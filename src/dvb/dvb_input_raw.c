/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2012 Andreas Öman
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

/**
 * DVB input from a raw transport stream
 */

#include <assert.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <sys/epoll.h>

#include "tvheadend.h"
#include "dvb.h"
#include "service.h"

/**
 * Install filters for a service
 *
 * global_lock must be held
 */
static void
open_service(th_dvb_adapter_t *tda, service_t *s)
{
}


/**
 * Remove filters for a service
 *
 * global_lock must be held
 */
static void
close_service(th_dvb_adapter_t *tda, service_t *s)
{
}


/**
 *
 */
static void
open_table(th_dvb_mux_instance_t *tdmi, th_dvb_table_t *tdt)
{
}


/**
 *
 */
static void
close_table(th_dvb_mux_instance_t *tdmi, th_dvb_table_t *tdt)
{
}

/**
 *
 */
void
dvb_input_raw_setup(th_dvb_adapter_t *tda)
{
  tda->tda_rawmode = 1;
  tda->tda_open_service  = open_service;
  tda->tda_close_service = close_service;
  tda->tda_open_table    = open_table;
  tda->tda_close_table   = close_table;
}

