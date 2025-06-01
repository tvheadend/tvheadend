/*
 *  Tvheadend - TS file input system
 *
 *  Copyright (C) 2014 Adam Sutton
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

#ifndef __TVH_MPEGTS_MUX_SCHED_H__
#define __TVH_MPEGTS_MUX_SCHED_H__

#include "tvheadend.h"
#include "cron.h"
#include "idnode.h"
#include "subscriptions.h"

struct profile_chain;

typedef LIST_HEAD(,mpegts_mux_sched) mpegts_mux_sched_list_t;

extern mpegts_mux_sched_list_t mpegts_mux_sched_all;

extern const idclass_t mpegts_mux_sched_class;

typedef struct mpegts_mux_sched
{
  idnode_t mms_id;

  LIST_ENTRY(mpegts_mux_sched) mms_link;

  /*
   * Configuration
   */
  int             mms_enabled;  ///< Enabled
  char           *mms_cronstr;  ///< Cron configuration string
  char           *mms_mux;      ///< Mux UUID
  char           *mms_creator;  ///< Creator of entry
  int             mms_timeout;  ///< Timeout (in seconds)
  int             mms_restart;  ///< Restart subscription when overriden
  int             mms_weight;   ///< Weighting

  /*
   * Cron handling
   */
  int             mms_active;   ///< Subscription is active
  time_t          mms_start;    ///< Start time
  gtimer_t        mms_timer;    ///< Timer for start/end
  cron_t          mms_cronjob;  ///< Cron spec
  
  /*
   * Subscription
   */
  struct profile_chain *mms_prch;     ///< Dummy profile chain
  th_subscription_t    *mms_sub;      ///< Subscription handler
  streaming_target_t    mms_input;    ///< Streaming input

} mpegts_mux_sched_t;

mpegts_mux_sched_t *mpegts_mux_sched_create ( const char *uuid, htsmsg_t *c );
void mpegts_mux_sched_delete ( mpegts_mux_sched_t *mms, int delconf );

void mpegts_mux_sched_init ( void );
void mpegts_mux_sched_done ( void );
time_t mpegts_mux_sched_next(void);


#endif /* __TVH_MPEGTS_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
