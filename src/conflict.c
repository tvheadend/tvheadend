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

struct device{
    LIST_ENTRY(device) link;

    void *device_ptr;
    
    service_t **services;
    int service_count;
};

LIST_HEAD(device_list, device);

struct conflict_state{
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

static bool conflict_dev_dvb_alloc(struct device_list *dev_list, service_t *service)
{
    struct device *device;
    bool found = false;
    bool allocated = false;

    LIST_FOREACH(device, dev_list, link)
    {
        if (device->device_ptr == service->s_dvb_mux_instance->tdmi_adapter)
        {
            found = true;
            if (device->services[0]->s_dvb_mux_instance == service->s_dvb_mux_instance)
            {
                conflict_dev_add_service(device, service);
                allocated = true;
            }
            break;
        }
    }
    
    if (!found)
    {
        device = calloc(1, sizeof(struct device));
        device->device_ptr = service->s_dvb_mux_instance->tdmi_adapter;
        device->service_count = 1;
        device->services = calloc(1, sizeof(service_t *));
        device->services[0] = service;
        LIST_INSERT_HEAD(dev_list, device, link);
        allocated = true;
    }
    return allocated;
    
}

static bool conflict_dev_v4l_alloc(struct device_list *dev_list, service_t *service)
{
    struct device *device;
    bool found = false;
    bool allocated = false;
    
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
        allocated = true;
    }
    return allocated;
}

static bool conflict_allocate_service(struct conflict_state *state, channel_t *channel)
{
    service_t **services;
    int service_cnt = 0, i = 0;
    bool allocated = false;
    
    
    services = service_get_sorted_list(channel, &service_cnt);
    for (i = 0;i < service_cnt; i ++)
    {
        bool dev_found = false;
        switch (services[i]->s_type)
        {
            case SERVICE_TYPE_DVB:
                dev_found = conflict_dev_dvb_alloc(&state->dvb_devs, services[i]);
                break;
            case SERVICE_TYPE_IPTV:
                conflict_dev_add_service(&state->ip_dev, services[i]);
                dev_found = true;
                break;
            case SERVICE_TYPE_V4L:
                dev_found = conflict_dev_v4l_alloc(&state->v4l_devs, services[i]);
                break;
        }
        if (dev_found)
        {
            allocated = true;
            break;
        }
    }
    free(services);
    
    return allocated;
}

static void conflict_free_state(struct conflict_state *state)
{
    struct device *device;
    
    if (state->ip_dev.services)
    {
        free(state->ip_dev.services);
    }
    
    device = LIST_FIRST(&state->v4l_devs);
    while(device)
    {
        struct device *next;
        free(device->services);
        next = LIST_NEXT(device, link);
        free(device);
        device = next;
    }
    
    device = LIST_FIRST(&state->dvb_devs);
    while(device)
    {
        struct device *next;
        free(device->services);
        next = LIST_NEXT(device, link);
        free(device);
        device = next;
    }
}

bool conflict_check_epg(epg_broadcast_t *broadcast, dvr_query_result_t *dqr)
{
    dvr_query_result_t overlaps;
    bool result = false;
    struct conflict_state state;
    memset(dqr, 0, sizeof(dvr_query_result_t));
    memset(&state, 0, sizeof(state));
    
    conflict_find_overlaps(broadcast->start, broadcast->stop, &overlaps);
    if (overlaps.dqr_entries)
    {
        int i;
        for (i = 0; i < overlaps.dqr_entries; i ++)
        {
            /* Don't worry about existing conflicts, we only care if the 
             * specified event can not be recorded.
             */
            (void)conflict_allocate_service(&state, overlaps.dqr_array[i]->de_channel);
        }
        if (!conflict_allocate_service(&state, broadcast->channel))
        {
            result = true;
            *dqr = overlaps;
        }
        else
        {
            dvr_query_free(&overlaps);
        }
        conflict_free_state(&state);
    }
    
    return result;
}

enum conflict_status conflict_check_dvr(dvr_entry_t *entry, dvr_query_result_t *dqr)
{
    memset(dqr, 0, sizeof(dvr_query_result_t));
    
    return CONFLICT_NO_CONFLICT;
}