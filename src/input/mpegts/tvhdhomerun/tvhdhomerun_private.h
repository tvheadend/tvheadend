/*
 *  Tvheadend - HDHomeRun DVB private data
 *
 *  Copyright (C) 2014 Patric Karlstrom
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

#ifndef __TVH_tvhdhomerun_PRIVATE_H__
#define __TVH_tvhdhomerun_PRIVATE_H__

#include "input.h"
#include "htsbuf.h"
#include "tvhdhomerun.h"

#include <libhdhomerun/hdhomerun.h>

typedef struct tvhdhomerun_device_info tvhdhomerun_device_info_t;
typedef struct tvhdhomerun_device      tvhdhomerun_device_t;
typedef struct tvhdhomerun_frontend    tvhdhomerun_frontend_t;


static struct hdhomerun_debug_t* hdhomerun_debug_obj = 0;

// Defines for debugging locks...
#if 0

#define PTHREAD_MUTEX_LOCK(x) \
        printf("LOCK: %s:%s:%u "#x"\n",__FILE__,__FUNCTION__,__LINE__); \
        pthread_mutex_lock(x);

#define PTHREAD_MUTEX_UNLOCK(x) \
        printf("UNLOCK: %s:%s:%u "#x"\n",__FILE__,__FUNCTION__,__LINE__); \
        pthread_mutex_unlock(x);

#else

#define PTHREAD_MUTEX_LOCK(x) \
        pthread_mutex_lock(x);

#define PTHREAD_MUTEX_UNLOCK(x) \
        pthread_mutex_unlock(x);

#endif


// static pthread_mutex_t global_hdhomerun_device_mutex = PTHREAD_MUTEX_INITIALIZER;


struct tvhdhomerun_device_info
{
  char *ip_address;         /* IP address */
  uint32_t device_id;
  char *friendlyname;
  char *deviceModel;

  char *uuid;

};

struct tvhdhomerun_device
{
  tvh_hardware_t;

  gtimer_t                   hd_destroy_timer;

  /*
   * Adapter info
   */
  tvhdhomerun_device_info_t      hd_info;
  
  /*
   * Frontends
   */
  TAILQ_HEAD(,tvhdhomerun_frontend) hd_frontends;

  /*
    Flags...
  */
  int                        hd_fullmux_ok;
  
  int                        hd_pids_max;
  int                        hd_pids_len;
  int                        hd_pids_deladd;

  dvb_fe_type_t              hd_type;
  char                      *hd_override_type;
  
};

#define HDHOMERUN_MAX_PIDS 32

struct tvhdhomerun_frontend
{
  mpegts_input_t;

  /*
   * Device
   */
  tvhdhomerun_device_t          *hf_device;
  int                            hf_master;

  TAILQ_ENTRY(tvhdhomerun_frontend)  hf_link;

  /*
   * Frontend info
   */
  int                            hf_tunerNumber;
  dvb_fe_type_t                  hf_type; 
  pthread_mutex_t                hf_mutex;          // Anything that is used by both input-thread
                                                    // or monitor. Only for quick read/writes.

  // libhdhomerun objects.
  struct hdhomerun_device_t     *hf_hdhomerun_tuner;


  // Tuning information
  int                            hf_locked;
  int                            hf_status;

  pthread_mutex_t                hf_input_mux_lock;           // Lock to make sure we are not running the input-thread
                                                              // reader during a mux start/stop.


  // input thread..
  pthread_t                      hf_input_thread;
  pthread_mutex_t                hf_input_thread_mutex;        // Used for sending signals.
  uint8_t                        hf_input_thread_running;      // Indicates if input_thread is running.
  uint8_t                        hf_input_thread_terminating;  // Used for terminating the input_thread.


  // Global lock for the libhdhomerun library since it seems to have some threading-issues.
  pthread_mutex_t               hf_hdhomerun_device_mutex;


  /*
   * Reception
   */
  char                           hf_pid_filter_buf[1024];
  pthread_mutex_t                hf_pid_filter_mutex;

  gtimer_t                       hf_monitor_timer;

  mpegts_mux_instance_t         *hf_mmi;
 
};


/*
 * Methods
 */
  
void tvhdhomerun_device_init ( void );

void tvhdhomerun_device_done ( void );

void tvhdhomerun_device_destroy ( tvhdhomerun_device_t *sd );

void tvhdhomerun_device_destroy_later( tvhdhomerun_device_t *sd, int after_ms );



tvhdhomerun_frontend_t * 
tvhdhomerun_frontend_create( tvhdhomerun_device_t *hd, struct hdhomerun_discover_device_t *discover_info, htsmsg_t *conf, dvb_fe_type_t type, unsigned int frontend_number );

void tvhdhomerun_frontend_delete ( tvhdhomerun_frontend_t *lfe );

void tvhdhomerun_device_save ( tvhdhomerun_device_t *sd );
void tvhdhomerun_frontend_save ( tvhdhomerun_frontend_t *lfe, htsmsg_t *m );



#endif
