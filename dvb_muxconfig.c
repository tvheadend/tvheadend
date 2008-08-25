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
#include <ctype.h>

#include <libhts/htssettings.h>

#include <linux/dvb/frontend.h>

#include "tvhead.h"
#include "channels.h"
#include "dvb.h"
#include "dvb_dvr.h"
#include "dvb_muxconfig.h"
#include "dvb_support.h"
#include "transports.h"
#include "notify.h"



/**
 * Save config for the given adapter
 */
void
dvb_tda_save(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create();

  htsmsg_add_str(m, "type", dvb_adaptertype_to_str(tda->tda_type));
  htsmsg_add_str(m, "displayname", tda->tda_displayname);
  hts_settings_save(m, "dvbadapters/%s", tda->tda_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
static htsmsg_t *
dvb_tda_createmsg(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create();
  htsmsg_add_str(m, "id", tda->tda_identifier);
  return m;
}

/**
 *
 */
void
dvb_tda_set_displayname(th_dvb_adapter_t *tda, const char *s)
{
  htsmsg_t *m = dvb_tda_createmsg(tda);

  free(tda->tda_displayname);
  tda->tda_displayname = strdup(s);
  
  dvb_tda_save(tda);

  htsmsg_add_str(m, "name", tda->tda_displayname);

  notify_by_msg("dvbadapter", m);
}






static struct strtab fectab[] = {
  { "NONE", FEC_NONE },
  { "1/2",  FEC_1_2 },
  { "2/3",  FEC_2_3 },
  { "3/4",  FEC_3_4 },
  { "4/5",  FEC_4_5 },
  { "5/6",  FEC_5_6 },
  { "6/7",  FEC_6_7 },
  { "7/8",  FEC_7_8 },
  { "8/9",  FEC_8_9 },
  { "AUTO", FEC_AUTO }
};

static struct strtab qamtab[] = {
  { "QPSK",   QPSK },
  { "QAM16",  QAM_16 },
  { "QAM32",  QAM_32 },
  { "QAM64",  QAM_64 },
  { "QAM128", QAM_128 },
  { "QAM256", QAM_256 },
  { "AUTO",   QAM_AUTO },
  { "8VSB",   VSB_8 },
  { "16VSB",  VSB_16 }
};

static struct strtab bwtab[] = {
  { "8MHz", BANDWIDTH_8_MHZ },
  { "7MHz", BANDWIDTH_7_MHZ },
  { "6MHz", BANDWIDTH_6_MHZ },
  { "AUTO", BANDWIDTH_AUTO }
};

static struct strtab modetab[] = {
  { "2k",   TRANSMISSION_MODE_2K },
  { "8k",   TRANSMISSION_MODE_8K },
  { "AUTO", TRANSMISSION_MODE_AUTO }
};

static struct strtab guardtab[] = {
  { "1/32", GUARD_INTERVAL_1_32 },
  { "1/16", GUARD_INTERVAL_1_16 },
  { "1/8",  GUARD_INTERVAL_1_8 },
  { "1/4",  GUARD_INTERVAL_1_4 },
  { "AUTO", GUARD_INTERVAL_AUTO },
};

static struct strtab hiertab[] = {
  { "NONE", HIERARCHY_NONE },
  { "1",    HIERARCHY_1 },
  { "2",    HIERARCHY_2 },
  { "4",    HIERARCHY_4 },
  { "AUTO", HIERARCHY_AUTO }
};

static struct strtab poltab[] = {
  { "Vertical",      POLARISATION_VERTICAL },
  { "Horizontal",    POLARISATION_HORIZONTAL },
};


/**
 *
 */
void
dvb_tdmi_save(th_dvb_mux_instance_t *tdmi)
{
  struct dvb_frontend_parameters *f = &tdmi->tdmi_fe_params;

  htsmsg_t *m = htsmsg_create();

  htsmsg_add_u32(m, "transportstreamid", tdmi->tdmi_transport_stream_id);
  if(tdmi->tdmi_network != NULL)
    htsmsg_add_str(m, "network", tdmi->tdmi_network);

  htsmsg_add_u32(m, "frequency", f->frequency);

  switch(tdmi->tdmi_adapter->tda_type) {
  case FE_OFDM:
    htsmsg_add_str(m, "bandwidth",
		   val2str(f->u.ofdm.bandwidth, bwtab));

    htsmsg_add_str(m, "constellation", 
	    val2str(f->u.ofdm.constellation, qamtab));

    htsmsg_add_str(m, "transmission_mode", 
	    val2str(f->u.ofdm.transmission_mode, modetab));

    htsmsg_add_str(m, "guard_interval", 
	    val2str(f->u.ofdm.guard_interval, guardtab));

    htsmsg_add_str(m, "hierarchy", 
	    val2str(f->u.ofdm.hierarchy_information, hiertab));

    htsmsg_add_str(m, "fec_hi", 
	    val2str(f->u.ofdm.code_rate_HP, fectab));

    htsmsg_add_str(m, "fec_lo", 
	    val2str(f->u.ofdm.code_rate_LP, fectab));
    break;

  case FE_QPSK:
    htsmsg_add_u32(m, "symbol_rate", f->u.qpsk.symbol_rate);

    htsmsg_add_str(m, "fec", 
	    val2str(f->u.qpsk.fec_inner, fectab));

    htsmsg_add_str(m, "polarisation", 
	    val2str(tdmi->tdmi_polarisation, poltab));
 
    htsmsg_add_u32(m, "switchport", tdmi->tdmi_switchport);
    break;

  case FE_QAM:
    htsmsg_add_u32(m, "symbol_rate", f->u.qam.symbol_rate);

    htsmsg_add_str(m, "fec", 
	    val2str(f->u.qam.fec_inner, fectab));

    htsmsg_add_str(m, "constellation", 
	    val2str(f->u.qam.modulation, qamtab));
    break;

  case FE_ATSC:
    break;
  }

  hts_settings_save(m, "dvbmuxes/%s/%s", 
		    tdmi->tdmi_adapter->tda_identifier, tdmi->tdmi_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
static const char *
dvb_tdmi_create_by_msg(th_dvb_adapter_t *tda, htsmsg_t *m)
{
  struct dvb_frontend_parameters f;
  const char *s;
  int r;
  int polarisation = 0;
  unsigned int switchport = 0;
  unsigned int tsid;

  memset(&f, 0, sizeof(f));
  
  f.inversion = INVERSION_AUTO;
  htsmsg_get_u32(m, "frequency", &f.frequency);


  switch(tda->tda_type) {
  case FE_OFDM:
    s = htsmsg_get_str(m, "bandwidth");
    if(s == NULL || (r = str2val(s, bwtab)) < 0)
      return "Invalid bandwidth";
    f.u.ofdm.bandwidth = r;

    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid QAM constellation";
    f.u.ofdm.constellation = r;

    s = htsmsg_get_str(m, "transmission_mode");
    if(s == NULL || (r = str2val(s, modetab)) < 0)
      return "Invalid transmission mode";
    f.u.ofdm.transmission_mode = r;

    s = htsmsg_get_str(m, "guard_interval");
    if(s == NULL || (r = str2val(s, guardtab)) < 0)
      return "Invalid guard interval";
    f.u.ofdm.guard_interval = r;

    s = htsmsg_get_str(m, "hierarchy");
    if(s == NULL || (r = str2val(s, hiertab)) < 0)
      return "Invalid heirarchy information";
    f.u.ofdm.hierarchy_information = r;

    s = htsmsg_get_str(m, "fec_hi");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid hi-FEC";
    f.u.ofdm.code_rate_HP = r;

    s = htsmsg_get_str(m, "fec_lo");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid lo-FEC";
    f.u.ofdm.code_rate_LP = r;
    break;

  case FE_QPSK:
    htsmsg_get_u32(m, "symbol_rate", &f.u.qpsk.symbol_rate);
    if(f.u.qpsk.symbol_rate == 0)
      return "Invalid symbol rate";
    
    s = htsmsg_get_str(m, "fec");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid FEC";
    f.u.qpsk.fec_inner = r;

    s = htsmsg_get_str(m, "polarisation");
    if(s == NULL || (r = str2val(s, poltab)) < 0)
      return "Invalid polarisation";
    polarisation = r;
    
    htsmsg_get_u32(m, "switchport", &switchport);
    break;

  case FE_QAM:
    htsmsg_get_u32(m, "symbol_rate", &f.u.qam.symbol_rate);
    if(f.u.qam.symbol_rate == 0)
      return "Invalid symbol rate";
    
    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid QAM constellation";
    f.u.qam.modulation = r;

    s = htsmsg_get_str(m, "fec");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid FEC";
    f.u.qam.fec_inner = r;
    break;

  case FE_ATSC:
    break;
  }

  if(htsmsg_get_u32(m, "transportstreamid", &tsid))
    tsid = 0xffff;

  dvb_mux_create(tda, &f, polarisation, switchport,
		 tsid, htsmsg_get_str(m, "network"), NULL);
  return NULL;
}



/**
 *
 */
void
dvb_tdmi_load(th_dvb_adapter_t *tda)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load("dvbmuxes/%s", tda->tda_identifier)) == NULL)
    return;
 
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_msg_by_field(f)) == NULL)
      continue;
    
    dvb_tdmi_create_by_msg(tda, c);
  }
  htsmsg_destroy(l);
}




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

  printf("m = %p, num = %d\n", m, num);

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
