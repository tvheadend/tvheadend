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
#include "muxes.h"

/**
 *
 */
static void
dvb_mux_preconf_add(dvb_network_t *dn, const network_t *net,
		    const char *source)
{
  const mux_t *m;
  struct dvb_mux_conf dmc;

  LIST_FOREACH(m, &net->muxes, link) {

    memset(&dmc, 0, sizeof(dmc));
  
    dmc.dmc_fe_params.inversion = INVERSION_AUTO;
    dmc.dmc_fe_params.frequency = m->freq;
    
    switch(dn->dn_fe_type) {
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
#if DVB_API_VERSION >= 5
      dmc.dmc_fe_delsys                    = SYS_DVBS;
#endif
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

    dvb_mux_create(dn, &dmc, 0, 0xffff, NULL, source, 1, 1, NULL);
  }
}


/**
 *
 */
int
dvb_mux_preconf_add_network(dvb_network_t *dn, const char *id)
{
  region_list_t *list;
  const region_t *r;
  const network_t *n;
  char source[100];

  snprintf(source, sizeof(source), "built-in configuration from \"%s\"", id);

  switch(dn->dn_fe_type) {
    case FE_QAM:
      list = &regions_DVBC;
      break;
    case FE_QPSK:
      list = &regions_DVBS;
      break;
    case FE_OFDM:
      list = &regions_DVBT;
      break;
    case FE_ATSC:
      list = &regions_ATSC;
      break;
    default:
      return -1;
  }

  LIST_FOREACH(r, list, link) {
    LIST_FOREACH(n, &r->networks, link) {
      if(!strcmp(n->id, id)) {
        dvb_mux_preconf_add(dn, n, source);
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
  region_list_t *list = NULL;
  const region_t *r;
  const network_t *n;
  htsmsg_t *out, *e;

  switch(fetype) {
    case FE_QAM:
      list = &regions_DVBC;
      break;
    case FE_QPSK:
      list = &regions_DVBS;
      break;
    case FE_OFDM:
      list = &regions_DVBT;
      break;
    case FE_ATSC:
      list = &regions_ATSC;
      break;
    default:
      tvhlog(LOG_ERR, "DVB", "No built-in config for fetype %d", fetype);
      return NULL;
  }
  
  out = htsmsg_create_list();

  if(!strcmp(node, "root")) {
    LIST_FOREACH(r, list, link) {
      e = htsmsg_create_map();
      htsmsg_add_u32(e, "leaf", 0);
      htsmsg_add_str(e, "text", r->name);
      htsmsg_add_str(e, "id", r->id);
      htsmsg_add_msg(out, NULL, e);
    }
    return out;
  }

  LIST_FOREACH(r, list, link)
    if (!strcmp(node, r->id))
      break;
  if (!r) return out;

  LIST_FOREACH(n, &r->networks, link) {
    e = htsmsg_create_map();
    htsmsg_add_u32(e, "leaf", 1);
    htsmsg_add_str(e, "text", n->name);
    htsmsg_add_str(e, "id", n->id);
    htsmsg_add_msg(out, NULL, e);
  }
      
  return out;
}
