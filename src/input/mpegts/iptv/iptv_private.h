/*
 *  IPTV Input
 *
 *  Copyright (C) 2013 Andreas Öman
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

#ifndef __IPTV_PRIVATE_H__
#define __IPTV_PRIVATE_H__

#include "input/mpegts.h"
#include "input/mpegts/iptv.h"
#include "htsbuf.h"

typedef struct iptv_input   iptv_input_t;
typedef struct iptv_network iptv_network_t;
typedef struct iptv_mux     iptv_mux_t;
typedef struct iptv_service iptv_service_t;

struct iptv_input
{
  mpegts_input_t;
};

struct iptv_network
{
  mpegts_network_t;
};

struct iptv_mux
{
  mpegts_mux_t;

  int                   mm_iptv_fd;
  mpegts_mux_instance_t mm_iptv_instance;
  char                 *mm_iptv_url;
  char                 *mm_iptv_interface;
};

iptv_mux_t* iptv_mux_create ( const char *uuid, htsmsg_t *conf );

struct iptv_service
{
  mpegts_service_t;
};

iptv_service_t *iptv_service_create 
  ( const char *uuid, iptv_mux_t *im, uint16_t sid, uint16_t pmt_pid );

extern iptv_input_t   iptv_input;
extern iptv_network_t iptv_network;

void iptv_mux_load_all     ( void );
void iptv_service_load_all ( iptv_mux_t *im, const char *n );

#endif /* __IPTV_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
