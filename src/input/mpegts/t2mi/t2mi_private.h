/*
 *  Tvheadend - T2-MI / TS piping decapsulation input
 *
 *  Copyright (C) 2026 Tvheadend Project
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

#ifndef __T2MI_PRIVATE_H__
#define __T2MI_PRIVATE_H__

#include "input.h"
#include "profile.h"
#include "subscriptions.h"

#include "t2mi_decap.h"

typedef struct t2mi_input   t2mi_input_t;
typedef struct t2mi_network t2mi_network_t;
typedef struct t2mi_mux     t2mi_mux_t;

struct t2mi_input
{
  mpegts_input_t;
};

struct t2mi_network
{
  mpegts_network_t;

  int tn_priority;
  int tn_streaming_priority;
  uint32_t tn_max_timeout;

  /* carrier discovery in the selected source muxes */
  idnode_list_head_t tn_src_muxes; /* selected source muxes (in1 side) */
  int      tn_src_private;      /* map private streams as carriers */
  uint32_t tn_scan_period;      /* rediscovery period (minutes) */
  mtimer_t tn_disc_timer;
};

struct t2mi_mux
{
  mpegts_mux_t;

  /* configuration */
  char     *mm_t2mi_src_mux;      /* UUID of the source (outer) mux */
  uint32_t  mm_t2mi_src_sid;      /* carrier service id, 0 = none (raw PID) */
  uint32_t  mm_t2mi_src_pid;      /* carrier PID, 0 = auto */
  int       mm_t2mi_format;       /* T2MI_DECAP_FORMAT_* */
  int       mm_t2mi_plp;          /* PLP id, -1 = auto */
  int       mm_t2mi_auto;         /* created by the auto-network */
  int       tm_seen;              /* transient: seen in last discovery */

  /* runtime */
  profile_chain_t     tm_prch;
  th_subscription_t  *tm_sub;
  streaming_target_t  tm_st;
  t2mi_decap_t       *tm_decap;
  sbuf_t              tm_buffer;
  int                 tm_carrier_pid; /* resolved carrier PID */
  uint8_t             tm_running;
};

extern const idclass_t t2mi_network_class;
extern const idclass_t t2mi_mux_class;
extern const idclass_t t2mi_input_class;

#endif /* __T2MI_PRIVATE_H__ */
