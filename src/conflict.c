/*
 *  recording conflict detection
 *  Copyright (C) 2013 Adam Charrett
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "tvheadend.h"
#include "conflict.h"
#include "queue.h"
#include "service.h"
#include "dvb/dvb.h"
#include "dvr/dvr.h"

struct device{
    LIST_ENTRY(device) link;

    void *device_ptr;
    
    service_t **services;
    int service_count;
    
    dvr_query_result_t entries;
};

LIST_HEAD(device_list, device);

struct channel_alloc_state{
    struct device ip_dev;
    struct device_list v4l_devs;
    struct device_list dvb_devs;
};

static time_t overlap_start, overlap_stop;

static pthread_mutex_t overlap_mutex = PTHREAD_MUTEX_INITIALIZER;

static int overlap_filter(dvr_entry_t *entry)
{
    
    /* Case 1
     *    -----------
     *    | entry
     *    ----------
     * ========
     * overlap|
     * ========
     * 
     * Case 2
     * ----------
     * entry    | 
     * ----------
     *    =============
     *    | overlap   |
     *    =============
     * 
     * Case 3
     *  -----------------------------
     *  | entry                     |
     *  -----------------------------
     *      ================
     *      | overlap      |
     *      ================
     */
    if (((entry->de_start >= overlap_start) && (entry->de_start <= overlap_stop)) ||
        ((entry->de_stop >= overlap_start) && (entry->de_stop <= overlap_stop)) ||
        ((overlap_start > entry->de_start) && (overlap_stop < entry->de_stop)))
    {
        return 1;
    }
    return 0;
}

static void conflict_find_overlaps(time_t start, time_t stop, dvr_query_result_t *dqr)
{
    pthread_mutex_lock(&overlap_mutex);
    
    overlap_start = start;
    overlap_stop = stop;
    dvr_query_filter(dqr, overlap_filter);
    
    pthread_mutex_unlock(&overlap_mutex);
}

static void conflict_dev_add_service(struct device *dev, service_t *service)
{
    dev->service_count++;
    dev->services = realloc(dev->services,sizeof(service_t*) * dev->service_count);
    dev->services[dev->service_count - 1] = service;
}

static struct device *conflict_dev_dvb_alloc(struct device_list *dev_list, service_t *service)
{
    struct device *device = NULL, *result=NULL;
    bool found = false;

    LIST_FOREACH(device, dev_list, link)
    {
        if (device->device_ptr == service->s_dvb_mux_instance->tdmi_adapter)
        {
            found = true;
            if (device->services[0]->s_dvb_mux_instance == service->s_dvb_mux_instance)
            {
                conflict_dev_add_service(device, service);
                result = device;
            }
            break;
        }
    }
    
    if (!found)
    {
        result = calloc(1, sizeof(struct device));
        result->device_ptr = service->s_dvb_mux_instance->tdmi_adapter;
        result->service_count = 1;
        result->services = calloc(1, sizeof(service_t *));
        result->services[0] = service;
        LIST_INSERT_HEAD(dev_list, result, link);
    }
    return result;
    
}

static struct device *conflict_dev_v4l_alloc(struct device_list *dev_list, service_t *service)
{
    struct device *device = NULL;
    bool found = false;
    
    LIST_FOREACH(device, dev_list, link)
    {
        if (device->device_ptr == service->s_v4l_adapter)
        {
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        device = calloc(1, sizeof(struct device));
        device->device_ptr = service->s_v4l_adapter;
        device->service_count = 1;
        device->services = calloc(1, sizeof(service_t *));
        device->services[0] = service;
        LIST_INSERT_HEAD(dev_list, device, link);
    }
    return device;
}

static struct device *conflict_allocate_service(struct channel_alloc_state *state, channel_t *channel)
{
    service_t **services;
    int service_cnt = 0, i = 0;
    struct device *device = NULL;
    
    
    services = service_get_sorted_list(channel, &service_cnt);
    for (i = 0;i < service_cnt; i ++)
    {
        switch (services[i]->s_type)
        {
            case SERVICE_TYPE_DVB:
                device = conflict_dev_dvb_alloc(&state->dvb_devs, services[i]);
                break;
            case SERVICE_TYPE_IPTV:
                conflict_dev_add_service(&state->ip_dev, services[i]);
                device = &state->ip_dev;
                break;
            case SERVICE_TYPE_V4L:
                device = conflict_dev_v4l_alloc(&state->v4l_devs, services[i]);
                break;
        }
        if (device)
        {
            break;
        }
    }
    free(services);
    
    return device;
}

static void conflict_add_suggestion(conflict_state_t *state, dvr_query_result_t *dqr)
{
    state->suggestion_count ++;
    state->suggestions = realloc(state->suggestions, sizeof(dvr_query_result_t) * state->suggestion_count);
    state->suggestions[state->suggestion_count - 1] = *dqr;
    /* We've taken over the entries for the suggestions, so ensure we don't free
     * the array when cleaning up.
     */
    dqr->dqr_alloced = 0;
    dqr->dqr_array= NULL;
    dqr->dqr_entries = 0;
}

static void conflict_generate_suggestions(struct channel_alloc_state *alloc_state, conflict_state_t *state, channel_t *channel)
{
    service_t **services;
    int service_cnt = 0, i = 0;
    struct device *device = NULL;
    
    
    services = service_get_sorted_list(channel, &service_cnt);
    for (i = 0;i < service_cnt; i ++)
    {
        switch (services[i]->s_type)
        {
            case SERVICE_TYPE_DVB:
                LIST_FOREACH(device, &alloc_state->dvb_devs, link)
                {
                    if (device->device_ptr == services[i]->s_dvb_mux_instance->tdmi_adapter)
                    {
                        conflict_add_suggestion(state, &device->entries);
                        break;
                    }
                }
                break;

            case SERVICE_TYPE_IPTV:
                break;

            case SERVICE_TYPE_V4L:
                LIST_FOREACH(device, &alloc_state->v4l_devs, link)
                {
                    if (device->device_ptr == services[i]->s_v4l_adapter)
                    {
                        conflict_add_suggestion(state, &device->entries);
                        break;
                    }
                }
                break;
        }
    }
}

static void conflict_free_alloc_state(struct channel_alloc_state *state)
{
    struct device *device;
    
    if (state->ip_dev.services)
    {
        dvr_query_free(&state->ip_dev.entries);
        free(state->ip_dev.services);
    }
    
    device = LIST_FIRST(&state->v4l_devs);
    while(device)
    {
        struct device *next;
        dvr_query_free(&device->entries);
        free(device->services);
        next = LIST_NEXT(device, link);
        free(device);
        device = next;
    }
    
    device = LIST_FIRST(&state->dvb_devs);
    while(device)
    {
        struct device *next;
        dvr_query_free(&device->entries);
        free(device->services);
        next = LIST_NEXT(device, link);
        free(device);
        device = next;
    }
}

void conflict_check(channel_t *channel, time_t start, time_t stop, 
                    conflict_state_t *state)
{
    dvr_query_result_t overlaps;
    struct channel_alloc_state alloc_state;
    struct device *device;
    memset(state, 0, sizeof(conflict_state_t));
    memset(&alloc_state, 0, sizeof(alloc_state));

    conflict_find_overlaps(start, stop, &overlaps);
    if (overlaps.dqr_entries)
    {
        int i;
        for (i = 0; i < overlaps.dqr_entries; i ++)
        {
            /* Don't worry about existing conflicts, we only care if the 
             * specified event can not be recorded.
             */
            device = conflict_allocate_service(&alloc_state, 
                                               overlaps.dqr_array[i]->de_channel);
            tvhlog(LOG_INFO, "CONFLICT", "Entry: %s (%s) => %p", 
                    lang_str_get(overlaps.dqr_array[i]->de_title, "EN"), 
                    overlaps.dqr_array[i]->de_channel->ch_name, device);
            if (device)
            {
                dvr_query_add_entry(&device->entries, overlaps.dqr_array[i]);
            }
        }
        
        if (!conflict_allocate_service(&alloc_state, channel))
        {
            tvhlog(LOG_INFO, "CONFLICT", "Conflict detected");
            state->status = CONFLICT_CONFLICT_DETECTED;
            conflict_generate_suggestions(&alloc_state, state, channel);
        }
        else
        {
            tvhlog(LOG_INFO, "CONFLICT", "No conflict detected");
            dvr_query_free(&overlaps);
        }
        conflict_free_alloc_state(&alloc_state);
    }

}

void conflict_check_epg(epg_broadcast_t *broadcast, conflict_state_t *state)
{
    conflict_check(broadcast->channel, broadcast->start, broadcast->stop, state);
}

void conflict_free_state(conflict_state_t *state)
{
    int i;
    if (state->suggestions)
    {
        for (i = 0; i < state->suggestion_count; i ++)
        {
            dvr_query_free(&state->suggestions[i]);
        }
        free(state->suggestions);
    }
}