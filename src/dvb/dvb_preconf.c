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

#include "settings.h"

#include <linux/dvb/frontend.h>

#include "tvheadend.h"
#include "dvb.h"
#include "dvb_preconf.h"

/**
 * A big list of all known DVB networks (from linuxtv.org)
 */
#include "linuxtv_muxes.h"
#if 0
/**
 *
 */
static void
dvb_mux_preconf_add(th_dvb_adapter_t *tda, const struct mux *m, int num,
		    const char *source, const char *satconf)
{
  struct dvb_mux_conf dmc;
  int i;

  for(i = 0; i < num; i++) {

    memset(&dmc, 0, sizeof(dmc));
  
    dmc.dmc_fe_params.inversion = INVERSION_AUTO;
    dmc.dmc_fe_params.frequency = m->freq;
    
    switch(tda->tda_type) {
    case FE_OFDM:
      dmc.dmc_fe_params.u.ofdm.bandwidth             = m->bw;
      dmc.dmc_fe_params.u.ofdm.constellation         = m->constellation;
      dmc.dmc_fe_params.u.ofdm.transmission_mode     = m->tmode;
      dmc.dmc_fe_params.u.ofdm.guard_interval        = m->guard;
      dmc.dmc_fe_params.u.ofdm.hierarchy_information = m->hierarchy;
      dmc.dmc_fe_params.u.ofdm.code_rate_HP          = m->fechp;
      dmc.dmc_fe_params.u.ofdm.code_rate_LP          = m->feclp;
      break;
      
    case FE_QPSK:
      dmc.dmc_fe_params.u.qpsk.symbol_rate = m->symrate;
      dmc.dmc_fe_params.u.qpsk.fec_inner   = m->fec;

      switch(m->polarisation) {
      case 'V':
	dmc.dmc_polarisation = POLARISATION_VERTICAL;
	break;
      case 'H':
	dmc.dmc_polarisation = POLARISATION_HORIZONTAL;
	break;
      case 'L':
	dmc.dmc_polarisation = POLARISATION_CIRCULAR_LEFT;
	break;
      case 'R':
	dmc.dmc_polarisation = POLARISATION_CIRCULAR_RIGHT;
	break;
      default:
	abort();
      }

      break;

    case FE_QAM:
      dmc.dmc_fe_params.u.qam.symbol_rate = m->symrate;
      dmc.dmc_fe_params.u.qam.modulation  = m->constellation;
      dmc.dmc_fe_params.u.qam.fec_inner   = m->fec;
      break;

    case FE_ATSC:
      dmc.dmc_fe_params.u.vsb.modulation  = m->constellation;
      break;
    }

    dmc.dmc_satconf = dvb_satconf_entry_find(tda, satconf, 0);
      
    dvb_mux_create(tda, &dmc, 0xffff, NULL, source, 1, 1, NULL);
    m++;
  }
}


/**
 *
 */
int
dvb_mux_preconf_add_network(th_dvb_adapter_t *tda, const char *id,
			    const char *satconf)
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
  case FE_ATSC:
    r = regions_ATSC;
    nr = sizeof(regions_ATSC) / sizeof(regions_ATSC[0]);
    break;
  default:
    return -1;
  }
  
  for(i = 0; i < nr; i++) {
    n = r[i].networks;
    nn = r[i].nnetworks;

    for(j = 0; j < nn; j++) {
      if(!strcmp(n[j].name, id)) {
	dvb_mux_preconf_add(tda, n[j].muxes, n[j].nmuxes, source, satconf);
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
  case FE_ATSC:
    r = regions_ATSC;
    nr = sizeof(regions_ATSC) / sizeof(regions_ATSC[0]);
    break;
  default:
    tvhlog(LOG_ERR, "DVB", "No built-in config for fetype %d", fetype);
    return NULL;
  }
  
  out = htsmsg_create_list();

  if(!strcmp(node, "root")) {

    for(i = 0; i < nr; i++) {
      e = htsmsg_create_map();
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
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "leaf", 1);
    htsmsg_add_str(e, "text", n[i].name);
    htsmsg_add_str(e, "id", n[i].name);
    htsmsg_add_msg(out, NULL, e);
  }
      
  return out;
}
#endif
