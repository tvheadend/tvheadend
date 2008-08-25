/*
 *  TV headend - Code for configuring DVB muxes
 *  This code is based on code from linux dvb-apps
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <libhts/htssettings.h>

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "dvb.h"
#include "dvb_preconf.h"

/**
 * A big list of all known DVB networks (from linuxtv.org)
 */
#include "linuxtv_muxes.h"

/**
 *
 */
static void
dvb_mux_preconf_add(th_dvb_adapter_t *tda, const struct mux *m, int num,
		    const char *source)
{
  struct dvb_frontend_parameters f;
  int i;
  int polarisation;
  int switchport;

  for(i = 0; i < num; i++) {

    polarisation = 0;
    switchport = 0;

    memset(&f, 0, sizeof(f));
  
    f.inversion = INVERSION_AUTO;
    f.frequency = m->freq;
    
    switch(tda->tda_type) {
    case FE_OFDM:
      f.u.ofdm.bandwidth             = m->bw;
      f.u.ofdm.constellation         = m->constellation;
      f.u.ofdm.transmission_mode     = m->tmode;
      f.u.ofdm.guard_interval        = m->guard;
      f.u.ofdm.hierarchy_information = m->hierarchy;
      f.u.ofdm.code_rate_HP          = m->fechp;
      f.u.ofdm.code_rate_LP          = m->feclp;
      break;
      
    case FE_QPSK:
      f.u.qpsk.symbol_rate = m->symrate;
      f.u.qpsk.fec_inner   = m->fec;

      switch(m->polarisation) {
      case 'V':
	polarisation = POLARISATION_VERTICAL;
	break;
      case 'H':
	polarisation = POLARISATION_HORIZONTAL;
	break;
      default:
	abort();
      }

      break;

    case FE_QAM:
      f.u.qam.symbol_rate = m->symrate;
      f.u.qam.modulation  = m->constellation;
      f.u.qam.fec_inner   = m->fec;
      break;
    }
      
    dvb_mux_create(tda, &f, polarisation, switchport, 0xffff, NULL,
		   source);
    m++;
  }
}


/**
 *
 */
int
dvb_mux_preconf_add_network(th_dvb_adapter_t *tda, const char *id)
{
  const struct region *r;
  const struct network *n;
  int nr, nn, i, j;
  char source[100];

  snprintf(source, sizeof(source), "built-in configuration from \"%s\"", id);

  switch(tda->tda_type) {
  case FE_QAM:
    r = regions_DVBC;
    nr = sizeof(regions_DVBC) / sizeof(regions_DVBC[0]);
    break;
  case FE_QPSK:
    r = regions_DVBS;
    nr = sizeof(regions_DVBS) / sizeof(regions_DVBS[0]);
    break;
  case FE_OFDM:
    r = regions_DVBT;
    nr = sizeof(regions_DVBT) / sizeof(regions_DVBT[0]);
    break;
  default:
    return -1;
  }
  
  for(i = 0; i < nr; i++) {
    n = r[i].networks;
    nn = r[i].nnetworks;

    for(j = 0; j < nn; j++) {
      if(!strcmp(n[j].name, id)) {
	dvb_mux_preconf_add(tda, n[j].muxes, n[j].nmuxes, source);
	break;
      }
    }
  }
  return 0;
}

/**
 *
 */
htsmsg_t *
dvb_mux_preconf_get_node(int fetype, const char *node)
{
  const struct region *r;
  const struct network *n;
  int nr, nn, i;
  htsmsg_t *out, *e;

  switch(fetype) {
  case FE_QAM:
    r = regions_DVBC;
    nr = sizeof(regions_DVBC) / sizeof(regions_DVBC[0]);
    break;
  case FE_QPSK:
    r = regions_DVBS;
    nr = sizeof(regions_DVBS) / sizeof(regions_DVBS[0]);
    break;
  case FE_OFDM:
    r = regions_DVBT;
    nr = sizeof(regions_DVBT) / sizeof(regions_DVBT[0]);
    break;
  default:
    return NULL;
  }
  
  out = htsmsg_create_array();

  if(!strcmp(node, "root")) {

    for(i = 0; i < nr; i++) {
      e = htsmsg_create();
      htsmsg_add_u32(e, "leaf", 0);
      htsmsg_add_str(e, "text", r[i].name);
      htsmsg_add_str(e, "id", r[i].name);
      htsmsg_add_msg(out, NULL, e);
    }
    return out;
  }

  for(i = 0; i < nr; i++)
    if(!strcmp(node, r[i].name))
      break;

  if(i == nr)
    return out;
  n = r[i].networks;
  nn = r[i].nnetworks;

  for(i = 0; i < nn; i++) {
    e = htsmsg_create();
    htsmsg_add_u32(e, "leaf", 1);
    htsmsg_add_str(e, "text", n[i].name);
    htsmsg_add_str(e, "id", n[i].name);
    htsmsg_add_msg(out, NULL, e);
  }
      
  return out;
}
