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

#define BUFFER_SIZE    (20000000 / 8)
#define UDP_PKTS        64
#define UDP_PKT_SIZE    1472

typedef struct tvhdhomerun_device_info tvhdhomerun_device_info_t;
typedef struct tvhdhomerun_device      tvhdhomerun_device_t;
typedef struct tvhdhomerun_frontend    tvhdhomerun_frontend_t;
typedef struct tvhdhomerun_tune_req    tvhdhomerun_tune_req_t;


static struct hdhomerun_debug_t* hdhomerun_debug_obj = 0;

struct tvhdhomerun_device_info
{
  struct sockaddr_storage ip_address;         /* IP address */
  char *friendlyname;
  char *deviceModel;
  char *uuid;
};

struct tvhdhomerun_device
{
  tvh_hardware_t;

  mtimer_t                   hd_destroy_timer;

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
  uint8_t                    hd_fullmux_ok;
  int                        hd_pids_max;

  dvb_fe_type_t              hd_type;
  char                      *hd_override_type;

};

struct tvhdhomerun_tune_req {
  mpegts_mux_instance_t      *hf_mmi;

  mpegts_apids_t             hf_pids;
};

struct tvhdhomerun_frontend
{
  mpegts_input_t;

  /*
   * Device
   */
  tvhdhomerun_device_t          *hf_device;

  TAILQ_ENTRY(tvhdhomerun_frontend)  hf_link;

  /*
   * Frontend info
   */
  int                            hf_tunerNumber;
  dvb_fe_type_t                  hf_type;
  int                            hf_grace_period;

  // libhdhomerun objects.
  struct hdhomerun_device_t     *hf_hdhomerun_tuner;

  // Tuning information
  uint8_t                        hf_locked;
  uint8_t                        hf_running;
  uint8_t                        hf_status;
  uint8_t                        hf_tables;
  uint64_t                       hf_last_tune;

  // input thread..
  pthread_t                      hf_dvr_thread;
  th_pipe_t                      hf_dvr_pipe;
  tvh_mutex_t                    hf_dvr_lock;

  tvhdhomerun_tune_req_t         *hf_req;
  tvhdhomerun_tune_req_t         *hf_req_thread;
  // Global lock for the libhdhomerun library since it seems to have some threading-issues.
  tvh_mutex_t                    hf_hdhomerun_device_mutex;

  /*
   * Reception
   */
  mtimer_t                       hf_monitor_timer;
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

static inline void tvhdhomerun_device_changed ( tvhdhomerun_device_t *sd )
  { idnode_changed(&sd->th_id); }

void tvhdhomerun_frontend_save ( tvhdhomerun_frontend_t *lfe, htsmsg_t *m );

#endif
