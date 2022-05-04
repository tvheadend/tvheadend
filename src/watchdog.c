/*
 *  tvheadend, systemd watchdog support
 *  Copyright (C) 2017-2018 Erkki Seppälä
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

#include "watchdog.h"
#include "tvheadend.h"

#include <systemd/sd-daemon.h>

static pthread_t watchdog_tid;
static tvh_mutex_t watchdog_exiting_mutex;
static tvh_cond_t watchdog_exiting_cond;
static int watchdog_exiting;    /* 1 if exit has been requested */
static int watchdog_enabled;    /* 1 if watchdog was enabled for the systemd unit */
static uint64_t watchdog_interval_usec; /* value from .service divided by 2 */

static void* watchdog_thread(void* aux)
{
  int exiting = 0;
  (void) aux; /* ignore */
  sd_notify(0, "READY=1");
  tvh_mutex_lock(&watchdog_exiting_mutex);
  while (!exiting) {
    if (!watchdog_exiting) {
      /*
       * Just keep ticking regardless the return value; its intention is
       * to pace the loop down, and let us exit fast when the time comes
       */
      tvh_cond_timedwait(&watchdog_exiting_cond, &watchdog_exiting_mutex, mclk() + watchdog_interval_usec);
      if (!watchdog_exiting) {
        tvh_mutex_lock(&global_lock);
        tvh_mutex_unlock(&global_lock);
        sd_notify(0, "WATCHDOG=1");
      }
    }
    exiting = watchdog_exiting;
  }
  tvh_mutex_unlock(&watchdog_exiting_mutex);

  return NULL;
}

void watchdog_init(void)
{
  /*
   * I doubt MONOCLOCK_RESOLUTION is going to change, but if it does,
   * let's break this
   */
#if MONOCLOCK_RESOLUTION != 1000000LL
#error "Watchdog assumes MONOCLOCK_RESOLUTION == 1000000LL"
#endif

  watchdog_enabled = sd_watchdog_enabled(0, &watchdog_interval_usec) > 0;
  if (watchdog_enabled) {
     /* suggested by sd_watchdog_enabled documentation: */
    watchdog_interval_usec /= 2;

    watchdog_exiting = 0;
    tvh_cond_init(&watchdog_exiting_cond, 1);
    tvh_mutex_init(&watchdog_exiting_mutex, NULL);

    tvh_thread_create(&watchdog_tid, NULL, watchdog_thread, NULL, "systemd watchdog");
  }
}

void watchdog_done(void)
{
  if (watchdog_enabled) {
    tvh_mutex_lock(&watchdog_exiting_mutex);
    watchdog_exiting = 1;
    tvh_cond_signal(&watchdog_exiting_cond, 0);
    tvh_mutex_unlock(&watchdog_exiting_mutex);

    pthread_join(watchdog_tid, NULL);

    tvh_cond_destroy(&watchdog_exiting_cond);
    tvh_mutex_destroy(&watchdog_exiting_mutex);
  }
}
