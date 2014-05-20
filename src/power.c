/*
 *  tvheadend, power management functions
 *  Copyright (C) 2014 Joerg Werner
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

#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvheadend.h"
#include "api.h"
#include "tcp.h"
#include "subscriptions.h"
#include "streaming.h"
#include "channels.h"
#include "service.h"
#include "htsmsg.h"
#include "notify.h"
#include "atomic.h"
#include "power.h"
#include "spawn.h"
#include "dvr/dvr.h"

/* **************************************************************************
 * Status monitoring
 * *************************************************************************/

uint32_t power_running=0;
uint32_t power_enabled;
uint32_t power_idletime;
uint32_t power_prerecordingwakeup;
char* power_epgwakeup;
char* power_rtcscript;
char* power_preshutdownscript;
char* power_shutdownscript;
time_t last_wakeup_time = 0;

static gtimer_t power_status_timer;

time_t idle_since;
int idle;

/**
 * Helper method for searching next upcoming recording
 */
static int
is_dvr_entry_upcoming(dvr_entry_t *entry)
{
  dvr_entry_sched_state_t state = entry->de_sched_state;
  return state == DVR_SCHEDULED;
}

/**
 * program wake-up time, also taking into account the next epg wake-up time
 * @param next_recording_time time of next recording
 */
void
power_schedule_poweron(time_t next_recording_time)
{
  if(power_enabled) {
    time_t now_time = time(0);
    if(next_recording_time > now_time) {
      time_t wakeup_time = next_recording_time - power_prerecordingwakeup * 60;

      // determine next wake-up due to epg wake-up setting
      time_t epg_wakeup_time = now_time;
      struct tm *lt;
      lt = localtime(&epg_wakeup_time);
      lt->tm_hour = atoi(power_epgwakeup);
      lt->tm_min = atoi(power_epgwakeup + 3);
      lt->tm_sec = 0;
      epg_wakeup_time = mktime(lt);
      if(epg_wakeup_time < now_time) {
        epg_wakeup_time += 3600 * 24;
      }
      tvhlog(LOG_DEBUG, "power", "EPG Wakup Time %s", ctime(&epg_wakeup_time));

      // check if the next epg-wakeup or recording wake-up is earlier
      if(epg_wakeup_time < wakeup_time)
        wakeup_time = epg_wakeup_time;
      tvhlog(LOG_DEBUG, "power", "Wakeup Time %s", ctime(&wakeup_time));

      // only update if wake-up time has changed since last time
      if(wakeup_time != last_wakeup_time) {
        last_wakeup_time = wakeup_time;
        if(power_rtcscript != NULL) {
          char seconds_since_epoch[14];
          snprintf(seconds_since_epoch, 14, "%ld", wakeup_time);

          const char *rtcscript_args[] = { power_rtcscript, seconds_since_epoch,
          NULL };

          if(spawnv(rtcscript_args[0], (void*) rtcscript_args)) {
            tvhlog(LOG_ERR, "power", "Could not execute %s script!", power_rtcscript);
          }
        } else {
          tvhlog(LOG_ERR, "power", "No RTC wake-up script was set!");
        }
      }
    }
  } else {
    // power management is disabled, remove previously programmed wake-up
    if(power_rtcscript != NULL) {
      if(last_wakeup_time != 0) {
        const char *rtcscript_args[] = { power_rtcscript, "-1", NULL };

        if(spawnv(rtcscript_args[0], (void*) rtcscript_args)) {
          tvhlog(LOG_ERR, "power", "Could not execute %s script!", power_rtcscript);
        }
      }
    } else {
      tvhlog(LOG_ERR, "power", "No RTC wake-up script was set!");
    }
    last_wakeup_time = 0;
  }
}

/**
 * Call this function if recordings have been rescheduled to update
 * the programmed wake-up time.
 */
time_t
power_reschedule_poweron(void)
{
  if (power_running) {
  tvhlog(LOG_DEBUG, "power", "Reschedule power-on called");

  // Find start time of next recording
  dvr_query_result_t dqr;
  dvr_query_filter(&dqr, is_dvr_entry_upcoming);
  dvr_query_sort_cmp(&dqr, dvr_sort_start_ascending);
  time_t next_recording_time = 0;
  if(dqr.dqr_entries > 0) {
    dvr_entry_t *de = dqr.dqr_array[0];
    next_recording_time = de->de_start;
  }
  dvr_query_free(&dqr);
  tvhlog(LOG_DEBUG, "power", "Next recording Time %s", ctime(&next_recording_time));

  power_schedule_poweron(next_recording_time);

  return next_recording_time;
  } else {
    tvhlog(LOG_DEBUG, "power", "Reschedule power-on ignored because not running!");
    return 0;
  }
}

/**
 * Check if computer is idle and needs to be shut down. The following checks are performed:
 * - All inputs are idle
 * - No active subscriptions
 * - No active connections
 * - No epg grabbers are running
 * - No running spawned child processes (e.g. post-processing)
 * - No upcoming recordings within the specified pre-record wake-up time
 * - The pre-shutdown script also allows the shutdown
 */
static void
power_status_callback(void *p)
{
  // schedule our routine to be called again after 60s!
  gtimer_arm(&power_status_timer, power_status_callback, NULL, 60);

  tvhlog(LOG_DEBUG, "power", "callback called");

  if((power_enabled) && (power_running)) {
    tvhlog(LOG_DEBUG, "power", "Power management enabled");
    tvhlog(LOG_DEBUG, "power", "Idle time %d min", power_idletime);
    tvhlog(LOG_DEBUG, "power", "Pre-Recording wake-up %d min", power_prerecordingwakeup);
    tvhlog(LOG_DEBUG, "power", "Epg-Wakeup %s o'clock", power_epgwakeup);
    tvhlog(LOG_DEBUG, "power", "Set RTC script %s", power_rtcscript);
    tvhlog(LOG_DEBUG, "power", "Pre-shutdown check script %s", power_preshutdownscript);
    tvhlog(LOG_DEBUG, "power", "Shutdown script %s", power_shutdownscript);

    time_t now_time = time(0);

    htsmsg_t *resp;
    htsmsg_t *req;
    req = htsmsg_create_map();
    htsmsg_add_str(req, "method", "");

    // Check if all inputs are idle
    api_exec("status/inputs", req, &resp);
    uint32_t active_inputs;
    if(htsmsg_get_u32(resp, "totalCount", &active_inputs)) {
      active_inputs = 99;
    }
    int inputs_idle = (active_inputs == 0);
    tvhlog(LOG_DEBUG, "power", "inputs active %d idle %d", active_inputs, inputs_idle);

    // Check for no active subscriptions
    api_exec("status/subscriptions", req, &resp);
    uint32_t active_subscriptions;
    if(htsmsg_get_u32(resp, "totalCount", &active_subscriptions)) {
      active_subscriptions = 99;
    }
    int subscriptions_idle = (active_subscriptions == 0);
    tvhlog(LOG_DEBUG, "power", "subscriptions active %d idle %d", active_subscriptions, subscriptions_idle);

    // Check for no active connections
    resp = tcp_server_connections();
    uint32_t active_connections;
    ;
    if(htsmsg_get_u32(resp, "totalCount", &active_connections)) {
      active_connections = 99;
    }
    int connections_idle = (active_connections == 0);
    tvhlog(LOG_DEBUG, "power", "connections active %d idle %d", active_connections, connections_idle);

    // Check for no running epg-grabs
    api_exec("status/epgs", req, &resp);
    uint32_t active_epgs;
    if(htsmsg_get_u32(resp, "totalCount", &active_epgs)) {
      active_epgs = 99;
    }
    int epg_idle = (active_epgs == 0);
    tvhlog(LOG_DEBUG, "power", "epgs active %d idle %d", active_epgs, epg_idle);

    // Check for no running spawned child processes
    uint32_t active_spawns = spawn_pending();
    int spawn_idle = (active_spawns == 0);
    tvhlog(LOG_DEBUG, "power", "child processes active %d idle %d", active_spawns, spawn_idle);

    // Check for no upcoming recordings within the pre-recording wake-up time
    time_t next_recording_time = power_reschedule_poweron();
    int no_recording_soon = ((next_recording_time - now_time) > power_prerecordingwakeup * 60);
    tvhlog(LOG_DEBUG, "power", "Upcoming recording idle %d", no_recording_soon);

    int now_idle = inputs_idle & subscriptions_idle & connections_idle & epg_idle & spawn_idle & no_recording_soon;
    tvhlog(LOG_DEBUG, "power", "Are we idle %d", now_idle);
    if(now_idle && !idle) { // we became idle
      tvhlog(LOG_DEBUG, "power", "We became idle");
      idle_since = now_time;
    }
    if(!now_idle && idle) {
      tvhlog(LOG_DEBUG, "power", "We are busy again!");
    }
    idle = now_idle;

    if(idle)
      tvhlog(LOG_DEBUG, "power", "Idle since %s", ctime(&idle_since));

    // Check that we have been idle long enough
    if(idle && ((now_time - idle_since) > power_idletime * 60)) {

      tvhlog(LOG_DEBUG, "power", "Executing pre-shutdown check");
      uint32_t okay_to_shutdown = 0;
      if(power_preshutdownscript != NULL) {
        const char *preshutdown_args[] = { power_preshutdownscript, NULL };

        char *outbuf;
        int outlen = spawn_and_store_stdout(preshutdown_args[0], (void*) preshutdown_args, &outbuf);
        if(outlen > 0) {
          if(strncasecmp("true", outbuf,4) == 0) {
            tvhlog(LOG_ERR, "power", "Pre-shutdown script allows shutdown");
            okay_to_shutdown = 1;
          }
          free(outbuf);
        } else if(outlen == 0) {
          tvhlog(LOG_ERR, "power", "No result obtained from pre-shutdown script %s!", power_preshutdownscript);
        } else {
          tvhlog(LOG_ERR, "power", "Could not execute pre-shutdown script %s!", power_preshutdownscript);
        }
      } else {
        tvhlog(LOG_DEBUG, "power", "No pre-shutdown script was configured!");
        okay_to_shutdown=1;
      }

      if(okay_to_shutdown > 0) {
        tvhlog(LOG_DEBUG, "power", "Executing shutdown");
        if(power_shutdownscript != NULL) {
          const char *shutdown_args[] = { power_shutdownscript, NULL };

          if(spawnv(shutdown_args[0], (void*) shutdown_args)) {
            tvhlog(LOG_ERR, "power", "Could not execute shutdown script %s!", power_shutdownscript);
          }
        } else {
          tvhlog(LOG_ERR, "power", "No shutdown script was configured!");
        }
      }
    }

  }

}

static void
tvhpower_load(void)
{
  tvhlog(LOG_DEBUG, "power", "Power load called!");
  htsmsg_t *m = hts_settings_load("power/config");
  if(m == NULL) {
    power_enabled = 0;
    power_idletime = 10;
    power_prerecordingwakeup = 2;
    tvh_str_set(&power_epgwakeup, "03:00");
    tvh_str_set(&power_rtcscript, TVHEADEND_DATADIR "/bin/setwakeup.sh");
    tvh_str_set(&power_preshutdownscript,
    TVHEADEND_DATADIR "/bin/preshutdown.sh");
    tvh_str_set(&power_shutdownscript,
    TVHEADEND_DATADIR "/bin/shutdown.sh");
    return;
  }
  if(htsmsg_get_u32(m, "power_enabled", &power_enabled))
    power_enabled = 0;
  if(htsmsg_get_u32(m, "power_idletime", &power_idletime))
    power_idletime = 10;
  if(htsmsg_get_u32(m, "power_prerecordingwakeup", &power_prerecordingwakeup))
    power_prerecordingwakeup = 2;

  const char *str;
  str = htsmsg_get_str(m, "power_epgwakeup");
  if(str)
    tvh_str_set(&power_epgwakeup, str);
  else
    tvh_str_set(&power_epgwakeup, "03:00");

  str = htsmsg_get_str(m, "power_rtcscript");
  if(str)
    tvh_str_set(&power_rtcscript, str);
  else
    tvh_str_set(&power_rtcscript, TVHEADEND_DATADIR "/bin/setwakeup.sh");

  str = htsmsg_get_str(m, "power_preshutdownscript");
  if(str)
    tvh_str_set(&power_preshutdownscript, str);
  else
    tvh_str_set(&power_preshutdownscript,
    TVHEADEND_DATADIR "/bin/preshutdown.sh");

  str = htsmsg_get_str(m, "power_shutdownscript");
  if(str)
    tvh_str_set(&power_shutdownscript, str);
  else
    tvh_str_set(&power_shutdownscript,
    TVHEADEND_DATADIR "/bin/shutdown.sh");
}

static void
tvhpower_save(void)
{
  tvhlog(LOG_DEBUG, "power", "Power save called!");
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "power_enabled", power_enabled);
  htsmsg_add_u32(m, "power_idletime", power_idletime);
  htsmsg_add_u32(m, "power_prerecordingwakeup", power_prerecordingwakeup);
  htsmsg_add_str(m, "power_epgwakeup", power_epgwakeup);
  htsmsg_add_str(m, "power_rtcscript", power_rtcscript);
  htsmsg_add_str(m, "power_preshutdownscript", power_preshutdownscript);
  htsmsg_add_str(m, "power_shutdownscript", power_shutdownscript);
  hts_settings_save(m, "power/config");
}

void
tvhpower_set_power_enabled(uint32_t on)
{
  if(power_enabled == on)
    return;

  power_enabled = on;
  tvhlog(LOG_DEBUG, "power", "set_power_enabled set to %d", power_enabled);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_idletime(uint32_t idletime)
{
  if(power_idletime == idletime)
    return;
  power_idletime = idletime;
  tvhlog(LOG_DEBUG, "power", "power_idletime set to %d", power_idletime);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_prerecordingwakeup(uint32_t prerecordwakeup)
{
  if(power_prerecordingwakeup == prerecordwakeup)
    return;
  power_prerecordingwakeup = prerecordwakeup;
  tvhlog(LOG_DEBUG, "power", "power_prerecordingwakeup set to %d", power_prerecordingwakeup);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_epgwakeup(const char* epgwakeup)
{
  if(power_epgwakeup != NULL && !strcmp(power_epgwakeup, epgwakeup))
    return;
  tvh_str_set(&power_epgwakeup, epgwakeup);
  tvhlog(LOG_DEBUG, "power", "epgwakeup set to %s", epgwakeup);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_rtcscript(const char* rtcscript)
{
  if(power_rtcscript != NULL && !strcmp(power_rtcscript, rtcscript))
    return;
  tvh_str_set(&power_rtcscript, rtcscript);
  tvhlog(LOG_DEBUG, "power", "power_rtcscript set to %s", power_rtcscript);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_preshutdownscript(const char* preshutdownscript)
{
  if(power_preshutdownscript != NULL && !strcmp(power_preshutdownscript, preshutdownscript))
    return;
  tvh_str_set(&power_preshutdownscript, preshutdownscript);
  tvhlog(LOG_DEBUG, "power", "power_preshutdownscript set to %s", power_preshutdownscript);
  tvhpower_save();
  power_reschedule_poweron();
}

void
tvhpower_set_power_shutdownscript(const char* shutdownscript)
{
  if(power_shutdownscript != NULL && !strcmp(power_shutdownscript, shutdownscript))
    return;
  tvh_str_set(&power_shutdownscript, shutdownscript);
  tvhlog(LOG_DEBUG, "power", "power_shutdownscript set to %s", power_shutdownscript);
  tvhpower_save();
  power_reschedule_poweron();
}

/**
 * Initialise subsystem
 */
void
power_init(void)
{
  tvhpower_load();
  power_running=1;
  power_status_callback(NULL);
}

/**
 * Shutdown subsystem
 */
void
power_done(void)
{
  power_running=0;
}

