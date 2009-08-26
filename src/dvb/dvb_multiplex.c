/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2007 Andreas Öman
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
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

#include "settings.h"

#include "tvhead.h"
#include "dvb.h"
#include "channels.h"
#include "transports.h"
#include "teletext.h"
#include "psi.h"
#include "dvb_support.h"
#include "notify.h"

struct th_dvb_mux_instance_tree dvb_muxes;

static struct strtab muxfestatustab[] = {
  { "Unknown",      TDMI_FE_UNKNOWN },
  { "No signal",    TDMI_FE_NO_SIGNAL },
  { "Faint signal", TDMI_FE_FAINT_SIGNAL },
  { "Bad signal",   TDMI_FE_BAD_SIGNAL },
  { "Constant FEC", TDMI_FE_CONSTANT_FEC },
  { "Bursty FEC",   TDMI_FE_BURSTY_FEC },
  { "OK",           TDMI_FE_OK },
};

static void tdmi_set_enable(th_dvb_mux_instance_t *tdmi, int enabled);


/**
 *
 */
static void
mux_link_initial(th_dvb_adapter_t *tda, th_dvb_mux_instance_t *tdmi)
{
  int was_empty = TAILQ_FIRST(&tda->tda_initial_scan_queue) == NULL;

  tdmi->tdmi_scan_queue = &tda->tda_initial_scan_queue;
  TAILQ_INSERT_TAIL(tdmi->tdmi_scan_queue, tdmi, tdmi_scan_link);

  if(was_empty && (tda->tda_mux_current == NULL ||
		   tda->tda_mux_current->tdmi_table_initial == 0))
    dvb_adapter_mux_scanner(tda);
}

/**
 *  Return a readable status text for the given mux
 */
const char *
dvb_mux_status(th_dvb_mux_instance_t *tdmi)
{
  return val2str(tdmi->tdmi_fe_status, muxfestatustab) ?: "Invalid";
}


/**
 *
 */
static int
tdmi_global_cmp(th_dvb_mux_instance_t *a, th_dvb_mux_instance_t *b)
{
  return strcmp(a->tdmi_identifier, b->tdmi_identifier);
}


/**
 * Create a new mux on the given adapter, return NULL if it already exists
 */
th_dvb_mux_instance_t *
dvb_mux_create(th_dvb_adapter_t *tda, struct dvb_frontend_parameters *fe_param,
	       int polarisation, dvb_satconf_t *sc,
	       uint16_t tsid, const char *network, const char *source,
	       int enabled, const char *identifier)
{
  th_dvb_mux_instance_t *tdmi, *c;
  unsigned int hash;
  char buf[200];

  lock_assert(&global_lock);

  hash = (fe_param->frequency + polarisation) % TDA_MUX_HASH_WIDTH;
  
  LIST_FOREACH(tdmi, &tda->tda_mux_hash[hash], tdmi_adapter_hash_link) {
    if(tdmi->tdmi_fe_params.frequency == fe_param->frequency &&
       tdmi->tdmi_polarisation        == polarisation &&
       tdmi->tdmi_satconf             == sc)
    /* Mux already exist */
      return NULL;
  }

  tdmi = calloc(1, sizeof(th_dvb_mux_instance_t));

  if(identifier == NULL) {
    char qpsktxt[20];
    
    if(tda->tda_sat)
      snprintf(qpsktxt, sizeof(qpsktxt), "_%s",
	       dvb_polarisation_to_str(polarisation));
    else
      qpsktxt[0] = 0;
    
    snprintf(buf, sizeof(buf), "%s%d%s%s%s", 
	     tda->tda_identifier,fe_param->frequency, qpsktxt,
	     sc ? "_satconf_" : "", sc ? sc->sc_id : "");
    
    tdmi->tdmi_identifier = strdup(buf);
  } else {
    tdmi->tdmi_identifier = strdup(identifier);
  }

  c = RB_INSERT_SORTED(&dvb_muxes, tdmi, tdmi_global_link, tdmi_global_cmp);

  if(c != NULL) {
    /* Global identifier collision, not good, not good at all */

    tvhlog(LOG_ERR, "dvb",
	   "Multiple DVB multiplexes with same identifier \"%s\" "
	   "one is skipped", tdmi->tdmi_identifier);
    free(tdmi->tdmi_identifier);
    free(tdmi);
    return NULL;
  }



  tdmi->tdmi_enabled = enabled;
  
  TAILQ_INIT(&tdmi->tdmi_table_queue);

  tdmi->tdmi_transport_stream_id = tsid;
  tdmi->tdmi_adapter = tda;
  tdmi->tdmi_network = network ? strdup(network) : NULL;
  tdmi->tdmi_quality = 100;


  memcpy(&tdmi->tdmi_fe_params, fe_param, 
	 sizeof(struct dvb_frontend_parameters));
  tdmi->tdmi_polarisation = polarisation;

  if(sc != NULL) {
    tdmi->tdmi_satconf = sc;
    LIST_INSERT_HEAD(&sc->sc_tdmis, tdmi, tdmi_satconf_link);
  }

  LIST_INSERT_HEAD(&tda->tda_mux_hash[hash], tdmi, tdmi_adapter_hash_link);
  LIST_INSERT_HEAD(&tda->tda_muxes, tdmi, tdmi_adapter_link);

  if(source != NULL) {
    dvb_mux_nicename(buf, sizeof(buf), tdmi);
    tvhlog(LOG_NOTICE, "dvb", "New mux \"%s\" created by %s", buf, source);

    dvb_mux_save(tdmi);
    dvb_adapter_notify(tda);
  }

  dvb_transport_load(tdmi);
  dvb_mux_notify(tdmi);

  if(enabled) {
    tda->tda_initial_num_mux++;
    tdmi->tdmi_table_initial = 1;
    mux_link_initial(tda, tdmi);
  }

  return tdmi;
}

/**
 * Destroy a DVB mux (it might come back by itself very soon though :)
 */
void
dvb_mux_destroy(th_dvb_mux_instance_t *tdmi)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  th_transport_t *t;

  lock_assert(&global_lock);
  
  hts_settings_remove("dvbmuxes/%s/%s",
		      tda->tda_identifier, tdmi->tdmi_identifier);

  while((t = LIST_FIRST(&tdmi->tdmi_transports)) != NULL) {
    hts_settings_remove("dvbtransports/%s/%s",
			t->tht_dvb_mux_instance->tdmi_identifier,
			t->tht_identifier);
    transport_destroy(t);
  }

  dvb_transport_notify_by_adapter(tda);

  if(tda->tda_mux_current == tdmi)
    dvb_fe_stop(tda->tda_mux_current);

  if(tdmi->tdmi_satconf != NULL)
    LIST_REMOVE(tdmi, tdmi_satconf_link);

  RB_REMOVE(&dvb_muxes, tdmi, tdmi_global_link);
  LIST_REMOVE(tdmi, tdmi_adapter_link);
  LIST_REMOVE(tdmi, tdmi_adapter_hash_link);

  if(tdmi->tdmi_scan_queue != NULL)
    TAILQ_REMOVE(tdmi->tdmi_scan_queue, tdmi, tdmi_scan_link);

  if(tdmi->tdmi_table_initial)
    tda->tda_initial_num_mux--;

  hts_settings_remove("dvbmuxes/%s", tdmi->tdmi_identifier);

  free(tdmi->tdmi_network);
  free(tdmi->tdmi_identifier);
  free(tdmi);

  dvb_adapter_notify(tda);
}


/**
 *
 */
th_dvb_mux_instance_t *
dvb_mux_find_by_identifier(const char *identifier)
{
  th_dvb_mux_instance_t skel;

  lock_assert(&global_lock);

  skel.tdmi_identifier = (char *)identifier;
  return RB_FIND(&dvb_muxes, &skel, tdmi_global_link, tdmi_global_cmp);
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
dvb_mux_save(th_dvb_mux_instance_t *tdmi)
{
  struct dvb_frontend_parameters *f = &tdmi->tdmi_fe_params;

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "quality", tdmi->tdmi_quality);
  htsmsg_add_u32(m, "enabled", tdmi->tdmi_enabled);
  htsmsg_add_str(m, "status", dvb_mux_status(tdmi));

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
     break;

  case FE_QAM:
    htsmsg_add_u32(m, "symbol_rate", f->u.qam.symbol_rate);

    htsmsg_add_str(m, "fec", 
	    val2str(f->u.qam.fec_inner, fectab));

    htsmsg_add_str(m, "constellation", 
	    val2str(f->u.qam.modulation, qamtab));
    break;

  case FE_ATSC:
    htsmsg_add_u32(m, "symbol_rate", f->u.qam.symbol_rate);

    htsmsg_add_str(m, "constellation", 
	    val2str(f->u.vsb.modulation, qamtab));
    break;
  }

  if(tdmi->tdmi_satconf != NULL)
    htsmsg_add_str(m, "satconf", tdmi->tdmi_satconf->sc_id);

  hts_settings_save(m, "dvbmuxes/%s/%s", 
		    tdmi->tdmi_adapter->tda_identifier, tdmi->tdmi_identifier);
  htsmsg_destroy(m);
}


/**
 *
 */
static const char *
tdmi_create_by_msg(th_dvb_adapter_t *tda, htsmsg_t *m, const char *identifier)
{
  th_dvb_mux_instance_t *tdmi;
  struct dvb_frontend_parameters f;
  const char *s;
  int r;
  int polarisation = 0;
  unsigned int tsid, u32, enabled;
  dvb_satconf_t *sc;

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
    htsmsg_get_u32(m, "symbol_rate", &f.u.qam.symbol_rate);
    if(f.u.qam.symbol_rate == 0)
      return "Invalid symbol rate";

    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid VSB constellation";
    f.u.vsb.modulation = r;

    break;
  }

  if(htsmsg_get_u32(m, "transportstreamid", &tsid))
    tsid = 0xffff;

  if(htsmsg_get_u32(m, "enabled", &enabled))
    enabled = 1;

  if((s = htsmsg_get_str(m, "satconf")) != NULL)
    sc = dvb_satconf_entry_find(tda, s, 0);
  else
    sc = NULL;

  tdmi = dvb_mux_create(tda, &f, polarisation, sc,
			tsid, htsmsg_get_str(m, "network"), NULL, enabled,
			identifier);
  if(tdmi != NULL) {

    if((s = htsmsg_get_str(m, "status")) != NULL)
      tdmi->tdmi_fe_status = str2val(s, muxfestatustab);

    if(!htsmsg_get_u32(m, "quality", &u32))
      tdmi->tdmi_quality = u32;
  }
  return NULL;
}



/**
 *
 */
void
dvb_mux_load(th_dvb_adapter_t *tda)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load("dvbmuxes/%s", tda->tda_identifier)) == NULL)
    return;
 
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;
    
    tdmi_create_by_msg(tda, c, f->hmf_name);
  }
  htsmsg_destroy(l);
}


/**
 *
 */
void
dvb_mux_set_networkname(th_dvb_mux_instance_t *tdmi, const char *networkname)
{
  htsmsg_t *m;

  free(tdmi->tdmi_network);
  tdmi->tdmi_network = strdup(networkname);
  dvb_mux_save(tdmi);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);
  htsmsg_add_str(m, "network", tdmi->tdmi_network ?: "");
  notify_by_msg("dvbMux", m);
}


/**
 *
 */
void
dvb_mux_set_tsid(th_dvb_mux_instance_t *tdmi, uint16_t tsid)
{
  htsmsg_t *m;

  tdmi->tdmi_transport_stream_id = tsid;
 
  dvb_mux_save(tdmi);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);
  htsmsg_add_u32(m, "muxid", tdmi->tdmi_transport_stream_id);
  notify_by_msg("dvbMux", m);
}


/**
 *
 */
static void
tdmi_set_enable(th_dvb_mux_instance_t *tdmi, int enabled)
{
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  
  if(tdmi->tdmi_enabled == enabled)
    return;

  if(tdmi->tdmi_enabled) {

    if(tdmi->tdmi_scan_queue != NULL) {
      TAILQ_REMOVE(tdmi->tdmi_scan_queue, tdmi, tdmi_scan_link);
      tdmi->tdmi_scan_queue = NULL;
    }
  }

  tdmi->tdmi_enabled = enabled;

  if(enabled)
    mux_link_initial(tda, tdmi);
}

/**
 * Configurable by user
 */
void
dvb_mux_set_enable(th_dvb_mux_instance_t *tdmi, int enabled)
{
  tdmi_set_enable(tdmi, enabled);
  dvb_mux_save(tdmi);
}


/**
 *
 */
static void
dvb_mux_modulation(char *buf, size_t size, th_dvb_mux_instance_t *tdmi)
{
  struct dvb_frontend_parameters *f = &tdmi->tdmi_fe_params;

  switch(tdmi->tdmi_adapter->tda_type) {
  case FE_OFDM:
    snprintf(buf, size, "%s, %s, %s-mode",
	     val2str(f->u.ofdm.constellation, qamtab),
	     val2str(f->u.ofdm.bandwidth, bwtab),
	     val2str(f->u.ofdm.transmission_mode, modetab));
    break;

  case FE_QPSK:
    snprintf(buf, size, "%d kBaud", f->u.qpsk.symbol_rate / 1000);
    break;
	     
  case FE_QAM:
   snprintf(buf, size, "%s, %d kBaud",
	    val2str(f->u.qam.modulation, qamtab),
	    f->u.qpsk.symbol_rate / 1000);
   break;
   
  case FE_ATSC:
    snprintf(buf, size, "%s", val2str(f->u.vsb.modulation, qamtab));
    break;
  default:
    snprintf(buf, size, "Unknown");
    break;
  }
}


/**
 *
 */
htsmsg_t *
dvb_mux_build_msg(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *m = htsmsg_create_map();
  char buf[100];

  htsmsg_add_str(m, "id", tdmi->tdmi_identifier);
  htsmsg_add_u32(m, "enabled",  tdmi->tdmi_enabled);
  htsmsg_add_str(m, "network", tdmi->tdmi_network ?: "");

  dvb_mux_nicefreq(buf, sizeof(buf), tdmi);
  htsmsg_add_str(m, "freq",  buf);

  dvb_mux_modulation(buf, sizeof(buf), tdmi);
  htsmsg_add_str(m, "mod",  buf);

  htsmsg_add_str(m, "pol", 
		 dvb_polarisation_to_str_long(tdmi->tdmi_polarisation));

  if(tdmi->tdmi_satconf != NULL)
    htsmsg_add_str(m, "satconf", tdmi->tdmi_satconf->sc_id);

  if(tdmi->tdmi_transport_stream_id != 0xffff)
    htsmsg_add_u32(m, "muxid", tdmi->tdmi_transport_stream_id);

  htsmsg_add_u32(m, "quality", tdmi->tdmi_quality);
  return m;
}


/**
 *
 */
void
dvb_mux_notify(th_dvb_mux_instance_t *tdmi)
{
  notify_by_msg("dvbMux", dvb_mux_build_msg(tdmi));
}


/**
 *
 */
const char *
dvb_mux_add_by_params(th_dvb_adapter_t *tda,
		      int freq,
		      int symrate,
		      int bw,
		      int constellation,
		      int tmode,
		      int guard,
		      int hier,
		      int fechi,
		      int feclo,
		      int fec,
		      int polarisation,
		      const char *satconf)
{
  dvb_satconf_t *sc;
  struct dvb_frontend_parameters f;
  th_dvb_mux_instance_t *tdmi;

  memset(&f, 0, sizeof(f));
  
  f.inversion = INVERSION_AUTO;
   
  switch(tda->tda_type) {
  case FE_OFDM:
    f.frequency = freq * 1000;
    if(!val2str(bw,            bwtab))
      return "Invalid bandwidth";

    if(!val2str(constellation, qamtab))
      return "Invalid QAM constellation";

    if(!val2str(tmode,         modetab))
      return "Invalid transmission mode";

    if(!val2str(guard,         guardtab))
      return "Invalid guard interval";

    if(!val2str(hier,          hiertab))
      return "Invalid hierarchy";

    if(!val2str(fechi,         fectab))
      return "Invalid FEC Hi";

    if(!val2str(feclo,         fectab))
      return "Invalid FEC Lo";

    f.u.ofdm.bandwidth             = bw;
    f.u.ofdm.constellation         = constellation;
    f.u.ofdm.transmission_mode     = tmode;
    f.u.ofdm.guard_interval        = guard;
    f.u.ofdm.hierarchy_information = hier;
    f.u.ofdm.code_rate_HP          = fechi;
    f.u.ofdm.code_rate_LP          = feclo;
    polarisation = 0;
    break;

  case FE_QAM:
    f.frequency = freq * 1000;

    if(!val2str(constellation, qamtab))
      return "Invalid QAM constellation";

    if(!val2str(fec,           fectab))
      return "Invalid FEC";

    f.u.qam.symbol_rate = symrate;
    f.u.qam.modulation  = constellation;
    f.u.qam.fec_inner   = fec;
    polarisation = 0;
    break;

  case FE_QPSK:
    f.frequency = freq;

    if(!val2str(fec,          fectab))
      return "Invalid FEC";

    if(!val2str(polarisation, poltab))
      return "Invalid polarisation";

    f.u.qpsk.symbol_rate = symrate;
    f.u.qpsk.fec_inner   = fec;
    break;

  case FE_ATSC:
    f.frequency = freq;

    if(!val2str(constellation, qamtab))
      return "Invalid VSB constellation";

    f.u.vsb.modulation = constellation;
    break;

  }

  if(satconf != NULL) {
    sc = dvb_satconf_entry_find(tda, satconf, 0);
    if(sc == NULL)
      return "Satellite configuration not found";
  } else {
    sc = NULL;
  }

  tdmi = dvb_mux_create(tda, &f, polarisation, sc, 
			0xffff, NULL, NULL, 1, NULL);

  if(tdmi == NULL)
    return "Mux already exist";

  return NULL;
}
			
