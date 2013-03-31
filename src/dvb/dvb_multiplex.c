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

#include "tvheadend.h"
#include "dvb.h"
#include "channels.h"
#include "teletext.h"
#include "psi.h"
#include "dvb_support.h"
#include "notify.h"
#include "subscriptions.h"
#include "epggrab.h"

static struct strtab muxfestatustab[] = {
  { "Unknown",      TDMI_FE_UNKNOWN },
  { "No signal",    TDMI_FE_NO_SIGNAL },
  { "Faint signal", TDMI_FE_FAINT_SIGNAL },
  { "Bad signal",   TDMI_FE_BAD_SIGNAL },
  { "Constant FEC", TDMI_FE_CONSTANT_FEC },
  { "Bursty FEC",   TDMI_FE_BURSTY_FEC },
  { "OK",           TDMI_FE_OK },
};



static idnode_t **dvb_mux_get_childs(struct idnode *self);
static const char *dvb_mux_get_title(struct idnode *self);

static const idclass_t dvb_mux_class = {
  .ic_class = "dvbmux",
  .ic_get_title = dvb_mux_get_title,
  .ic_get_childs = dvb_mux_get_childs,
  .ic_save = (void *)dvb_mux_save,
  .ic_properties = (const property_t[]){
    {
      "enabled", "Enabled", PT_BOOL,
      offsetof(dvb_mux_t, dm_enabled)
    }, {}}
};



/**
 *
 */
static void
mux_link_initial(dvb_network_t *dn, dvb_mux_t *dm)
{
  assert(dm->dm_scan_status == DM_SCAN_DONE);

  dm->dm_scan_status = DM_SCAN_PENDING;
  TAILQ_INSERT_TAIL(&dn->dn_initial_scan_pending_queue, dm, dm_scan_link);
  dn->dn_initial_scan_num_mux++;
  dvb_network_schedule_initial_scan(dn);
}


/**
 *
 */
void
dvb_mux_initial_scan_done(dvb_mux_t *dm)
{
  dvb_network_t *dn = dm->dm_dn;
  gtimer_disarm(&dm->dm_initial_scan_timeout);
  assert(dm->dm_scan_status == DM_SCAN_CURRENT);
  dn->dn_initial_scan_num_mux--;
  dm->dm_scan_status = DM_SCAN_DONE;
  TAILQ_REMOVE(&dn->dn_initial_scan_current_queue, dm, dm_scan_link);
  dvb_network_schedule_initial_scan(dn);
  dvb_mux_save(dm); // Save to dm_scan_status is persisted
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
dmc_compare_key(const struct dvb_mux_conf *a,
                const struct dvb_mux_conf *b)
{
  int32_t fd = (int32_t)a->dmc_fe_params.frequency
             - (int32_t)b->dmc_fe_params.frequency;
  fd = labs(fd);
  return fd < 2000 &&
    a->dmc_polarisation             == b->dmc_polarisation;
}


/**
 * Return 0 if configuration does not differ. return 1 if it does
 */
static int
dcm_compare_conf(int adapter_type,
                 const struct dvb_mux_conf *a,
                 const struct dvb_mux_conf *b)
{
  switch(adapter_type) {
  case FE_OFDM:
    return memcmp(&a->dmc_fe_params.u.ofdm,
		  &b->dmc_fe_params.u.ofdm,
		  sizeof(a->dmc_fe_params.u.ofdm));
  case FE_QAM:
    return memcmp(&a->dmc_fe_params.u.qam,
		  &b->dmc_fe_params.u.qam,
		  sizeof(a->dmc_fe_params.u.qam));
  case FE_ATSC:
    return memcmp(&a->dmc_fe_params.u.vsb,
		  &b->dmc_fe_params.u.vsb,
		  sizeof(a->dmc_fe_params.u.vsb));
  case FE_QPSK:
    return memcmp(&a->dmc_fe_params.u.qpsk,
		  &b->dmc_fe_params.u.qpsk,
		  sizeof(a->dmc_fe_params.u.qpsk))
#if DVB_API_VERSION >= 5
      || a->dmc_fe_modulation != b->dmc_fe_modulation
      || a->dmc_fe_delsys != b->dmc_fe_delsys
      || a->dmc_fe_rolloff != b->dmc_fe_rolloff
#endif
;
  }
  return 0;
}

/**
 * Create a new mux on the given adapter, return NULL if it already exists
 */
dvb_mux_t *
dvb_mux_create(dvb_network_t *dn, const struct dvb_mux_conf *dmc,
	       uint16_t onid, uint16_t tsid, const char *network,
               const char *source, int enabled, int needscan,
               const char *uuid)
{
  dvb_mux_t *dm;

  lock_assert(&global_lock);

  /* HACK - we hash/compare based on 2KHz spacing and compare on +/-500Hz */
  LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link) {
    if(dmc_compare_key(&dm->dm_conf, dmc))
      break; /* Mux already exist */
  }

  if(dm != NULL) {
    /* Update stuff ... */
    int save = 0;
    char buf2[1024];
    buf2[0] = 0;

    if(dcm_compare_conf(dn->dn_fe_type, &dm->dm_conf, dmc)) {
#if DVB_API_VERSION >= 5
      snprintf(buf2, sizeof(buf2), " (");
      if (dm->dm_conf.dmc_fe_modulation != dmc->dmc_fe_modulation)
        sprintf(buf2, "%s %s->%s, ", buf2, 
            dvb_mux_qam2str(dm->dm_conf.dmc_fe_modulation), 
            dvb_mux_qam2str(dmc->dmc_fe_modulation));
      if (dm->dm_conf.dmc_fe_delsys != dmc->dmc_fe_delsys)
        sprintf(buf2, "%s %s->%s, ", buf2, 
            dvb_mux_delsys2str(dm->dm_conf.dmc_fe_delsys), 
            dvb_mux_delsys2str(dmc->dmc_fe_delsys));
      if (dm->dm_conf.dmc_fe_rolloff != dmc->dmc_fe_rolloff)
        sprintf(buf2, "%s %s->%s, ", buf2, 
            dvb_mux_rolloff2str(dm->dm_conf.dmc_fe_rolloff), 
            dvb_mux_rolloff2str(dmc->dmc_fe_rolloff));
      sprintf(buf2, "%s)", buf2);
#endif

      memcpy(&dm->dm_conf, dmc, sizeof(struct dvb_mux_conf));
      save = 1;
    }

    if(tsid != 0xFFFF && dm->dm_transport_stream_id != tsid) {
      dm->dm_transport_stream_id = tsid;
      save = 1;
    }
    if(onid && dm->dm_network_id != onid) {
      dm->dm_network_id = onid;
      save = 1;
    }

    if(save) {
      dvb_mux_save(dm);
      tvhlog(LOG_DEBUG, "dvb",
	     "Configuration for mux \"%s\" updated by %s%s",
	     dvb_mux_nicename(dm), source, buf2);
      dvb_mux_notify(dm);
    }

    return NULL;
  }

  dm = calloc(1, sizeof(dvb_mux_t));

  dm->dm_network_id = onid;
  dm->dm_transport_stream_id = tsid;
  dm->dm_network_name = network ? strdup(network) : NULL;
  TAILQ_INIT(&dm->dm_epg_grab);
  TAILQ_INIT(&dm->dm_table_queue);

  dm->dm_dn = dn;
  LIST_INSERT_HEAD(&dn->dn_muxes, dm, dm_network_link);

  idnode_insert(&dm->dm_id, uuid, &dvb_mux_class);

  char identifier[128];
  snprintf(identifier, sizeof(identifier),
           "%d%s", dmc->dmc_fe_params.frequency,
           dn->dn_fe_type == FE_QPSK ?
           dvb_polarisation_to_str(dmc->dmc_polarisation) : "");
  dm->dm_local_identifier = strdup(identifier);

  dm->dm_enabled = enabled;

  memcpy(&dm->dm_conf, dmc, sizeof(struct dvb_mux_conf));

  if(source != NULL) {
    tvhlog(LOG_NOTICE, "dvb", "New mux \"%s\" created by %s",
           dvb_mux_nicename(dm), source);

    dvb_mux_save(dm);
  }

  dvb_service_load(dm);

  if(enabled && needscan) {
    dn->dn_initial_scan_num_mux++;
    mux_link_initial(dn, dm);
  }

  return dm;
}


/**
 *
 */
static void
dvb_tdmi_destroy(th_dvb_mux_instance_t *tdmi)
{
  LIST_REMOVE(tdmi, tdmi_mux_link);
  LIST_REMOVE(tdmi, tdmi_adapter_link);
  free(tdmi);
}


/**
 * Destroy a DVB mux (it might come back by itself very soon though :)
 */
void
dvb_mux_destroy(dvb_mux_t *dm)
{
  th_dvb_mux_instance_t *tdmi;
  dvb_network_t *dn = dm->dm_dn;

  lock_assert(&global_lock);

  LIST_REMOVE(dm, dm_network_link);

  while((tdmi = LIST_FIRST(&dm->dm_tdmis)) != NULL)
    dvb_tdmi_destroy(tdmi);

  switch(dm->dm_scan_status) {
  case DM_SCAN_DONE:
    break;
  case DM_SCAN_CURRENT:
    TAILQ_REMOVE(&dn->dn_initial_scan_current_queue, dm, dm_scan_link);
    gtimer_disarm(&dm->dm_initial_scan_timeout);
    if(0) // Sorry but i can't resist these whenever i get an oppertunity // andoma
  case DM_SCAN_PENDING:
      TAILQ_REMOVE(&dn->dn_initial_scan_pending_queue, dm, dm_scan_link);
    dn->dn_initial_scan_num_mux--;
    dvb_network_schedule_initial_scan(dn);
    break;
  }


  epggrab_mux_delete(dm);

  free(dm->dm_local_identifier);
  idnode_unlink(&dm->dm_id);
  free(dm);
}


#if DVB_API_VERSION >= 5
static struct strtab rollofftab[] = {
  { "ROLLOFF_35",   ROLLOFF_35 },
  { "ROLLOFF_20",   ROLLOFF_20 },
  { "ROLLOFF_25",   ROLLOFF_25 },
  { "ROLLOFF_AUTO", ROLLOFF_AUTO }
};

static struct strtab delsystab[] = {
  { "SYS_UNDEFINED",        SYS_UNDEFINED },
  { "SYS_DVBC_ANNEX_AC",    SYS_DVBC_ANNEX_AC },
  { "SYS_DVBC_ANNEX_B",     SYS_DVBC_ANNEX_B },
  { "SYS_DVBT",             SYS_DVBT },
  { "SYS_DSS",              SYS_DSS },
  { "SYS_DVBS",             SYS_DVBS },
  { "SYS_DVBS2",            SYS_DVBS2 },
  { "SYS_DVBH",             SYS_DVBH },
  { "SYS_ISDBT",            SYS_ISDBT },
  { "SYS_ISDBS",            SYS_ISDBS },
  { "SYS_ISDBC",            SYS_ISDBC },
  { "SYS_ATSC",             SYS_ATSC },
  { "SYS_ATSCMH",           SYS_ATSCMH },
  { "SYS_DMBTH",            SYS_DMBTH },
  { "SYS_CMMB",             SYS_CMMB },
  { "SYS_DAB",              SYS_DAB }
};
#endif


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
  { "AUTO", FEC_AUTO },
#if DVB_API_VERSION >= 5
  { "3/5",  FEC_3_5 },
  { "9/10", FEC_9_10 }
#endif
};

static struct strtab qamtab[] = {
  { "QPSK",    QPSK },
  { "QAM16",   QAM_16 },
  { "QAM32",   QAM_32 },
  { "QAM64",   QAM_64 },
  { "QAM128",  QAM_128 },
  { "QAM256",  QAM_256 },
  { "AUTO",    QAM_AUTO },
  { "8VSB",    VSB_8 },
  { "16VSB",   VSB_16 },
#if DVB_API_VERSION >= 5
  { "PSK_8",   PSK_8 },
  { "APSK_16", APSK_16 },
  { "APSK_32", APSK_32 },
  { "DQPSK",   DQPSK }
#endif
};


static struct strtab bwtab[] = {
  { "8MHz", BANDWIDTH_8_MHZ },
  { "7MHz", BANDWIDTH_7_MHZ },
  { "6MHz", BANDWIDTH_6_MHZ },
  { "AUTO", BANDWIDTH_AUTO },
#if DVB_VER_ATLEAST(5,3)
  { "5MHz", BANDWIDTH_5_MHZ },
  { "10MHz", BANDWIDTH_10_MHZ },
  { "1712kHz", BANDWIDTH_1_712_MHZ},
#endif
};

static struct strtab modetab[] = {
  { "2k",   TRANSMISSION_MODE_2K },
  { "8k",   TRANSMISSION_MODE_8K },
  { "AUTO", TRANSMISSION_MODE_AUTO },
#if DVB_VER_ATLEAST(5,3)
  { "1k",   TRANSMISSION_MODE_1K },
  { "2k",   TRANSMISSION_MODE_16K },
  { "32k",  TRANSMISSION_MODE_32K },
#endif
};

static struct strtab guardtab[] = {
  { "1/32", GUARD_INTERVAL_1_32 },
  { "1/16", GUARD_INTERVAL_1_16 },
  { "1/8",  GUARD_INTERVAL_1_8 },
  { "1/4",  GUARD_INTERVAL_1_4 },
  { "AUTO", GUARD_INTERVAL_AUTO },
#if DVB_VER_ATLEAST(5,3)
  { "1/128", GUARD_INTERVAL_1_128 },
  { "19/128", GUARD_INTERVAL_19_128 },
  { "19/256", GUARD_INTERVAL_19_256},
#endif
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
  { "Left",          POLARISATION_CIRCULAR_LEFT },
  { "Right",         POLARISATION_CIRCULAR_RIGHT },
};

/**
 * for external use
 */
const char* dvb_mux_fec2str(int fec) {
  return val2str(fec, fectab);
}

#if DVB_API_VERSION >= 5
/**
 * for external use
 */
const char* dvb_mux_delsys2str(int delsys) {
  return val2str(delsys, delsystab);
}

/**
 * for external use
 */
const char* dvb_mux_qam2str(int qam) {
  return val2str(qam, qamtab);
}

/**
 * for external use
 */
const char* dvb_mux_rolloff2str(int rolloff) {
  return val2str(rolloff, rollofftab);
}
#endif

/**
 * for external use
 */
int dvb_mux_str2bw(const char *str)
{
  return str2val(str, bwtab);
}

int dvb_mux_str2qam(const char *str)
{
  return str2val(str, qamtab);
}

int dvb_mux_str2fec(const char *str)
{
  return str2val(str, fectab);
}

int dvb_mux_str2mode(const char *str)
{
  return str2val(str, modetab);
}

int dvb_mux_str2guard(const char *str)
{
  return str2val(str, guardtab);
}

int dvb_mux_str2hier(const char *str)
{
  return str2val(str, hiertab);
}

/**
 *
 */
void
dvb_mux_save(dvb_mux_t *dm)
{
  const dvb_mux_conf_t *dmc = &dm->dm_conf;
  const struct dvb_frontend_parameters *f = &dmc->dmc_fe_params;

  htsmsg_t *m = htsmsg_create_map();

  htsmsg_add_u32(m, "enabled", dm->dm_enabled);
  htsmsg_add_str(m, "uuid",  idnode_uuid_as_str(&dm->dm_id));

  htsmsg_add_u32(m, "transportstreamid", dm->dm_transport_stream_id);
  htsmsg_add_u32(m, "originalnetworkid", dm->dm_network_id);
  if(dm->dm_network_name != NULL)
    htsmsg_add_str(m, "network", dm->dm_network_name);

  htsmsg_add_u32(m, "frequency", f->frequency);

  htsmsg_add_u32(m, "needscan", dm->dm_scan_status != DM_SCAN_DONE);

  if(dm->dm_default_authority)
    htsmsg_add_str(m, "default_authority", dm->dm_default_authority);

  switch(dm->dm_dn->dn_fe_type) {
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
      val2str(dmc->dmc_polarisation, poltab));

#if DVB_API_VERSION >= 5
    htsmsg_add_str(m, "modulation",
      val2str(dmc->dmc_fe_modulation, qamtab));

    htsmsg_add_str(m, "delivery_system",
      val2str(dmc->dmc_fe_delsys, delsystab));

    htsmsg_add_str(m, "rolloff",
      val2str(dmc->dmc_fe_rolloff, rollofftab));
#endif
    break;

  case FE_QAM:
    htsmsg_add_u32(m, "symbol_rate", f->u.qam.symbol_rate);

    htsmsg_add_str(m, "fec", 
		   val2str(f->u.qam.fec_inner, fectab));

    htsmsg_add_str(m, "constellation", 
		   val2str(f->u.qam.modulation, qamtab));
    break;

  case FE_ATSC:
    htsmsg_add_str(m, "constellation", 
		   val2str(f->u.vsb.modulation, qamtab));
    break;
  }

  hts_settings_save(m, "dvb/networks/%s/muxes/%s/config",
                    idnode_uuid_as_str(&dm->dm_dn->dn_id),
                    dm->dm_local_identifier);

  htsmsg_destroy(m);
}

/**
 *
 */
static const char *
dvb_mux_create_by_msg(dvb_network_t *dn, htsmsg_t *m, const char *fname)
{
  dvb_mux_t *dm;
  struct dvb_mux_conf dmc;
  const char *s;
  int r;
  unsigned int onid, tsid, enabled;

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_params.inversion = INVERSION_AUTO;
  htsmsg_get_u32(m, "frequency", &dmc.dmc_fe_params.frequency);

  switch(dn->dn_fe_type) {
  case FE_OFDM:
    s = htsmsg_get_str(m, "bandwidth");
    if(s == NULL || (r = str2val(s, bwtab)) < 0)
      return "Invalid bandwidth";
    dmc.dmc_fe_params.u.ofdm.bandwidth = r;

    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid QAM constellation";
    dmc.dmc_fe_params.u.ofdm.constellation = r;

    s = htsmsg_get_str(m, "transmission_mode");
    if(s == NULL || (r = str2val(s, modetab)) < 0)
      return "Invalid transmission mode";
    dmc.dmc_fe_params.u.ofdm.transmission_mode = r;

    s = htsmsg_get_str(m, "guard_interval");
    if(s == NULL || (r = str2val(s, guardtab)) < 0)
      return "Invalid guard interval";
    dmc.dmc_fe_params.u.ofdm.guard_interval = r;

    s = htsmsg_get_str(m, "hierarchy");
    if(s == NULL || (r = str2val(s, hiertab)) < 0)
      return "Invalid heirarchy information";
    dmc.dmc_fe_params.u.ofdm.hierarchy_information = r;

    s = htsmsg_get_str(m, "fec_hi");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid hi-FEC";
    dmc.dmc_fe_params.u.ofdm.code_rate_HP = r;

    s = htsmsg_get_str(m, "fec_lo");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid lo-FEC";
    dmc.dmc_fe_params.u.ofdm.code_rate_LP = r;
    break;

  case FE_QPSK:
    htsmsg_get_u32(m, "symbol_rate", &dmc.dmc_fe_params.u.qpsk.symbol_rate);
    if(dmc.dmc_fe_params.u.qpsk.symbol_rate == 0)
      return "Invalid symbol rate";
    
    s = htsmsg_get_str(m, "fec");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid FEC";
    dmc.dmc_fe_params.u.qpsk.fec_inner = r;

    s = htsmsg_get_str(m, "polarisation");
    if(s == NULL || (r = str2val(s, poltab)) < 0)
      return "Invalid polarisation";
    dmc.dmc_polarisation = r;

#if DVB_API_VERSION >= 5
    s = htsmsg_get_str(m, "modulation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0) {
      r = str2val("QPSK", qamtab);
      tvhlog(LOG_INFO,
          "dvb", "no modulation for mux found, defaulting to QPSK");
    } 
    dmc.dmc_fe_modulation = r;

    s = htsmsg_get_str(m, "delivery_system");
    if(s == NULL || (r = str2val(s, delsystab)) < 0) {
      r = str2val("SYS_DVBS", delsystab);
      tvhlog(LOG_INFO,
          "dvb", "no delivery system for mux found, defaulting to SYS_DVBS");
    }
    dmc.dmc_fe_delsys = r;

    s = htsmsg_get_str(m, "rolloff");
    if(s == NULL || (r = str2val(s, rollofftab)) < 0) {
      r = str2val("ROLLOFF_35", rollofftab);
      tvhlog(LOG_INFO,
          "dvb", "no rolloff for mux found, defaulting to ROLLOFF_35");
    }
    dmc.dmc_fe_rolloff = r;
#endif
    break;

  case FE_QAM:
    htsmsg_get_u32(m, "symbol_rate", &dmc.dmc_fe_params.u.qam.symbol_rate);
    if(dmc.dmc_fe_params.u.qam.symbol_rate == 0)
      return "Invalid symbol rate";
    
    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid QAM constellation";
    dmc.dmc_fe_params.u.qam.modulation = r;

    s = htsmsg_get_str(m, "fec");
    if(s == NULL || (r = str2val(s, fectab)) < 0)
      return "Invalid FEC";
    dmc.dmc_fe_params.u.qam.fec_inner = r;
    break;

  case FE_ATSC:
    s = htsmsg_get_str(m, "constellation");
    if(s == NULL || (r = str2val(s, qamtab)) < 0)
      return "Invalid VSB constellation";
    dmc.dmc_fe_params.u.vsb.modulation = r;

    break;
  }

  if(htsmsg_get_u32(m, "transportstreamid", &tsid))
    tsid = 0xffff;
  if(htsmsg_get_u32(m, "originalnetworkid", &onid))
    onid = 0;

  if(htsmsg_get_u32(m, "enabled", &enabled))
    enabled = 1;

  const char *uuid = htsmsg_get_str(m, "uuid");
  dm = dvb_mux_create(dn, &dmc,
                      onid, tsid, htsmsg_get_str(m, "network"), NULL, enabled,
                      htsmsg_get_u32_or_default(m, "needscan", 1),
                      uuid);
  if(dm != NULL) {

    if((s = htsmsg_get_str(m, "default_authority")))
      dm->dm_default_authority = strdup(s);


    if(uuid == NULL)
      // If mux didn't have UUID, write it to make sure UUID is stable
      dvb_mux_save(dm);
  }
  return NULL;
}


/**
 *
 */
void
dvb_mux_load(dvb_network_t *dn)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if((l = hts_settings_load_r(1, "dvb/networks/%s/muxes",
                              idnode_uuid_as_str(&dn->dn_id))) == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if((c = htsmsg_get_map(c, "config")) == NULL)
       continue;

    dvb_mux_create_by_msg(dn, c, f->hmf_name);
  }
  htsmsg_destroy(l);
}



/**
 *
 */
void
dvb_mux_set_networkname(dvb_mux_t *dm, const char *networkname)
{
  htsmsg_t *m;

  free(dm->dm_network_name);
  dm->dm_network_name = strdup(networkname);

  dvb_mux_save(dm);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "id", dm->dm_local_identifier);
  htsmsg_add_str(m, "network", dm->dm_network_name ?: "");
  notify_by_msg("dvbMux", m);
}


/**
 *
 */
void
dvb_mux_set_tsid(dvb_mux_t *dm, uint16_t tsid)
{
  htsmsg_t *m;

  dm->dm_transport_stream_id = tsid;

  dvb_mux_save(dm);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&dm->dm_id));
  htsmsg_add_u32(m, "muxid", dm->dm_transport_stream_id);
  notify_by_msg("dvbMux", m);
}

/**
 *
 */
void
dvb_mux_set_onid(dvb_mux_t *dm, uint16_t onid)
{
  htsmsg_t *m;

  dm->dm_network_id = onid;

  dvb_mux_save(dm);

  m = htsmsg_create_map();
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&dm->dm_id));
  htsmsg_add_u32(m, "onid", dm->dm_network_id);
  notify_by_msg("dvbMux", m);
}

#if 0
/**
 *
 */
static void
tdmi_set_enable(th_dvb_mux_instance_t *tdmi, int enabled)
{
  dvb_mux_t *dm = tdmi->tdmi_mux;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  if(tdmi->tdmi_enabled == enabled)
    return;

  if(tdmi->tdmi_enabled) {

    if(tda->tda_current_tdmi == tdmi)
      dvb_fe_stop(tda, 0);

    if(dm->dm_scan_queue != NULL) {
      TAILQ_REMOVE(dm->dm_scan_queue, dm, dm_scan_link);
      dm->dm_scan_queue = NULL;
    }
  }

  tdmi->tdmi_enabled = enabled;

  if(enabled)
    mux_link_initial(tda->tda_dn, dm);

  subscription_reschedule();
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
#endif

#if 0
/**
 *
 */
static void
dvb_mux_fe_status(char *buf, size_t size, th_dvb_mux_instance_t *tdmi) 
{
  switch (tdmi->tdmi_fe_status) {
    case TDMI_FE_UNKNOWN:
    default:
      snprintf(buf, size, "Unknown");
      break;
    case TDMI_FE_NO_SIGNAL:
      snprintf(buf, size, "No Signal");
      break;
    case TDMI_FE_FAINT_SIGNAL:
      snprintf(buf, size, "Faint Signal");
      break;
    case TDMI_FE_BAD_SIGNAL:
      snprintf(buf, size, "Bad Signal");
      break;
    case TDMI_FE_CONSTANT_FEC:
      snprintf(buf, size, "Constant FEC");
      break;
    case TDMI_FE_BURSTY_FEC:
      snprintf(buf, size, "Bursty FEC");
      break;
    case TDMI_FE_OK:
      snprintf(buf, size, "Ok");
      break;
  }
}
#endif

/**
 *
 */
static void
dvb_mux_modulation(char *buf, size_t size, const dvb_mux_t *dm)
{
  const dvb_mux_conf_t *dmc = &dm->dm_conf;
  const struct dvb_frontend_parameters *f = &dmc->dmc_fe_params;

  switch(dm->dm_dn->dn_fe_type) {
  case FE_OFDM:
    snprintf(buf, size, "%s, %s, %s-mode",
	     val2str(f->u.ofdm.constellation, qamtab),
	     val2str(f->u.ofdm.bandwidth, bwtab),
	     val2str(f->u.ofdm.transmission_mode, modetab));
    break;

  case FE_QPSK:
#if DVB_API_VERSION >= 5
    snprintf(buf, size, "%d kBaud, %s, %s", f->u.qpsk.symbol_rate / 1000,
      val2str(dmc->dmc_fe_delsys, delsystab),
      val2str(dmc->dmc_fe_modulation, qamtab));
#else
    snprintf(buf, size, "%d kBaud", f->u.qpsk.symbol_rate / 1000);
#endif
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
dvb_mux_build_msg(dvb_mux_t *dm)
{
  htsmsg_t *m = htsmsg_create_map();
  char buf[100];

  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&dm->dm_id));
  htsmsg_add_u32(m, "enabled",  dm->dm_enabled);
  htsmsg_add_str(m, "network", dm->dm_network_name ?: "");

  htsmsg_add_str(m, "freq",  dvb_mux_nicefreq(dm));

  dvb_mux_modulation(buf, sizeof(buf), dm);
  htsmsg_add_str(m, "mod",  buf);

  const dvb_mux_conf_t *dmc = &dm->dm_conf;

  htsmsg_add_str(m, "pol",
		 dvb_polarisation_to_str_long(dmc->dmc_polarisation));

  if(dm->dm_transport_stream_id != 0xffff)
    htsmsg_add_u32(m, "muxid", dm->dm_transport_stream_id);

  if(dm->dm_network_id)
    htsmsg_add_u32(m, "onid", dm->dm_network_id);

  return m;
}


/**
 *
 */
void
dvb_mux_notify(dvb_mux_t *dm)
{
  //  notify_by_msg("dvbMux", dvb_mux_build_msg(tdmi));
}


/**
 *
 */
const char *
dvb_mux_add_by_params(dvb_network_t *dn,
		      int freq,
		      int symrate,
		      int bw,
		      int constellation,
		      int delsys,
		      int tmode,
		      int guard,
		      int hier,
		      int fechi,
		      int feclo,
		      int fec,
		      int polarisation,
		      const char *satconf)
{
  struct dvb_mux_conf dmc;

  memset(&dmc, 0, sizeof(dmc));
  dmc.dmc_fe_params.inversion = INVERSION_AUTO;
   
  switch(dn->dn_fe_type) {
  case FE_OFDM:
    dmc.dmc_fe_params.frequency = freq * 1000;
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

    dmc.dmc_fe_params.u.ofdm.bandwidth             = bw;
    dmc.dmc_fe_params.u.ofdm.constellation         = constellation;
    dmc.dmc_fe_params.u.ofdm.transmission_mode     = tmode;
    dmc.dmc_fe_params.u.ofdm.guard_interval        = guard;
    dmc.dmc_fe_params.u.ofdm.hierarchy_information = hier;
    dmc.dmc_fe_params.u.ofdm.code_rate_HP          = fechi;
    dmc.dmc_fe_params.u.ofdm.code_rate_LP          = feclo;
    polarisation = 0;
    break;

  case FE_QAM:
    dmc.dmc_fe_params.frequency = freq * 1000;

    if(!val2str(constellation, qamtab))
      return "Invalid QAM constellation";

    if(!val2str(fec,           fectab))
      return "Invalid FEC";

    dmc.dmc_fe_params.u.qam.symbol_rate = symrate;
    dmc.dmc_fe_params.u.qam.modulation  = constellation;
    dmc.dmc_fe_params.u.qam.fec_inner   = fec;
    polarisation = 0;
    break;

  case FE_QPSK:
    dmc.dmc_fe_params.frequency = freq;

    if(!val2str(fec,          fectab))
      return "Invalid FEC";

    if(!val2str(polarisation, poltab))
      return "Invalid polarisation";

    dmc.dmc_fe_params.u.qpsk.symbol_rate = symrate;
    dmc.dmc_fe_params.u.qpsk.fec_inner   = fec;

#if DVB_API_VERSION >= 5
    if(!val2str(constellation, qamtab))
      return "Invalid QPSK constellation";

    if(!val2str(delsys, delsystab))
      return "Invalid delivery system";

    dmc.dmc_fe_delsys                    = delsys;
    dmc.dmc_fe_modulation                = constellation;
#endif
    break;

  case FE_ATSC:
    dmc.dmc_fe_params.frequency = freq;

    if(!val2str(constellation, qamtab))
      return "Invalid VSB constellation";

    dmc.dmc_fe_params.u.vsb.modulation = constellation;
    break;

  }

  dmc.dmc_polarisation = polarisation;

  dvb_mux_t *dm = dvb_mux_create(dn, &dmc, 0, 0xffff, NULL, NULL, 1, 1, NULL);

  if(dm == NULL)
    return "Mux already exist";

  return NULL;
}

#if 0
/**
 *
 */
int
dvb_mux_copy(th_dvb_adapter_t *dst, th_dvb_mux_instance_t *tdmi_src,
             dvb_satconf_t *satconf)
{
  th_dvb_mux_instance_t *tdmi_dst;
  service_t *t_src, *t_dst;
  elementary_stream_t *st_src, *st_dst;
  caid_t *caid_src, *caid_dst;

  tdmi_dst = dvb_mux_create(dst, 
			    &tdmi_src->tdmi_conf,
          tdmi_src->tdmi_network_id,
			    tdmi_src->tdmi_transport_stream_id,
			    tdmi_src->tdmi_network,
			    "copy operation", tdmi_src->tdmi_enabled,
			    1, NULL, satconf);

  if(tdmi_dst == NULL)
    return -1; // Already exist

  LIST_FOREACH(t_src, &tdmi_src->tdmi_mux->dm_services, s_group_link) {
    t_dst = dvb_service_find(tdmi_dst, 
			       t_src->s_dvb_service_id,
			       t_src->s_pmt_pid, NULL);

    t_dst->s_pcr_pid     = t_src->s_pcr_pid;
    t_dst->s_enabled     = t_src->s_enabled;
    t_dst->s_servicetype = t_src->s_servicetype;
    t_dst->s_scrambled   = t_src->s_scrambled;

    if(t_src->s_provider != NULL)
      t_dst->s_provider    = strdup(t_src->s_provider);

    if(t_src->s_svcname != NULL)
      t_dst->s_svcname = strdup(t_src->s_svcname);

    if(t_src->s_dvb_charset != NULL)
      t_dst->s_dvb_charset = strdup(t_src->s_dvb_charset);

    if(t_src->s_ch != NULL)
      service_map_channel(t_dst, t_src->s_ch, 0);

    pthread_mutex_lock(&t_src->s_stream_mutex);
    pthread_mutex_lock(&t_dst->s_stream_mutex);

    TAILQ_FOREACH(st_src, &t_src->s_components, es_link) {


      st_dst = service_stream_create(t_dst, 
				     st_src->es_pid,
				     st_src->es_type);
	
      memcpy(st_dst->es_lang, st_src->es_lang, 4);
      st_dst->es_frame_duration = st_src->es_frame_duration;

      LIST_FOREACH(caid_src, &st_src->es_caids, link) {
	caid_dst = malloc(sizeof(caid_t));

	caid_dst->caid       = caid_src->caid;
	caid_dst->providerid = caid_src->providerid;
	caid_dst->delete_me  = 0;
	LIST_INSERT_HEAD(&st_dst->es_caids, caid_dst, link);
      }
    }

    pthread_mutex_unlock(&t_dst->s_stream_mutex);
    pthread_mutex_unlock(&t_src->s_stream_mutex);

    t_dst->s_config_save(t_dst); // Save config

  }
  dvb_mux_save(tdmi_dst);
  return 0;
}
#endif

dvb_mux_t *
dvb_mux_find(dvb_network_t *dn, const char *netname, uint16_t onid,
             uint16_t tsid, int enabled)
{
  dvb_mux_t *dm;

  if(dn != NULL) {
    LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link) {

      if (enabled && !dm->dm_enabled)
        continue;

      if (onid    && onid != dm->dm_network_id)
        continue;

      if (tsid    && tsid != dm->dm_transport_stream_id)
        continue;

      if (netname && strcmp(netname, dm->dm_network_name ?: ""))
        continue;
      return dm;
    }
  } else {
    dvb_network_t *dn;
    LIST_FOREACH(dn, &dvb_networks, dn_global_link)
      if ((dm = dvb_mux_find(dn, netname, onid, tsid, enabled)))
        return dm;
  }
  return NULL;
}


/**
 *
 */
th_subscription_t *
dvb_subscription_create_from_tdmi(th_dvb_mux_instance_t *tdmi,
				  const char *name,
				  streaming_target_t *st)
{
  th_subscription_t *s;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;

  s = subscription_create(INT32_MAX, name, st, SUBSCRIPTION_RAW_MPEGTS, 
			  NULL, NULL, NULL, NULL);
  

  s->ths_tdmi = tdmi;
  LIST_INSERT_HEAD(&tdmi->tdmi_subscriptions, s, ths_tdmi_link);

  dvb_mux_tune(tdmi->tdmi_mux, "Full mux subscription", 99999);
  abort();
  pthread_mutex_lock(&tda->tda_delivery_mutex);
  streaming_target_connect(&tda->tda_streaming_pad, &s->ths_input);
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  notify_reload("subscriptions");
  return s;
}



/**
 *
 */
static const char *
dvb_mux_get_title(struct idnode *self)
{
  return dvb_mux_nicename((dvb_mux_t *)self);
}

/**
 *
 */
static int
svcsortcmp(const void *A, const void *B)
{
  const service_t *a = *(service_t **)A;
  const service_t *b = *(service_t **)B;
  return (int)a->s_dvb_service_id - (int)b->s_dvb_service_id;
}


/**
 *
 */
static idnode_t **
dvb_mux_get_childs(struct idnode *self)
{
  dvb_mux_t *dm = (dvb_mux_t *)self;
  service_t *s;
  int cnt = 1;

  LIST_FOREACH(s, &dm->dm_services, s_group_link)
    cnt++;

  idnode_t **v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  LIST_FOREACH(s, &dm->dm_services, s_group_link)
    v[cnt++] = (idnode_t *)s;
  qsort(v, cnt, sizeof(idnode_t *), svcsortcmp);
  v[cnt] = NULL;
  return v;
}



/**
 * These are created on the fly
 */
void
dvb_create_tdmis(dvb_mux_t *dm)
{
  th_dvb_mux_instance_t *tdmi;
  dvb_network_t *dn = dm->dm_dn;
  th_dvb_adapter_t *tda;

  LIST_FOREACH(tda, &dn->dn_adapters, tda_network_link) {

    LIST_FOREACH(tdmi, &dm->dm_tdmis, tdmi_mux_link) {
      if(tdmi->tdmi_adapter != NULL)
        break;
    }

    if(tdmi == NULL) {
      tdmi = calloc(1, sizeof(th_dvb_mux_instance_t));
      tdmi->tdmi_adapter = tda;
      tdmi->tdmi_mux = dm;
      LIST_INSERT_HEAD(&tda->tda_tdmis, tdmi, tdmi_adapter_link);
      LIST_INSERT_HEAD(&dm->dm_tdmis,   tdmi, tdmi_mux_link);
    }
  }
}



/**
 *
 */
void
dvb_mux_stop(th_dvb_mux_instance_t *tdmi)
{
  dvb_mux_t *dm = tdmi->tdmi_mux;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  service_t *s;

  assert(dm->dm_current_tdmi == tdmi);
  assert(tda->tda_current_tdmi == tdmi);

  lock_assert(&global_lock);

  /* Flush all subscribers */
  while((s = LIST_FIRST(&tda->tda_transports)) != NULL)
    service_remove_subscriber(s, NULL, SM_CODE_SUBSCRIPTION_OVERRIDDEN);

  dvb_table_flush_all(dm);
  epggrab_mux_stop(dm, 0);

  if(dm->dm_scan_status == DM_SCAN_CURRENT) {
    /*
     * If we were currently doing initial scan but lost the adapter
     * before finishing, put us back on the pending queue
     */
    dvb_network_t *dn = dm->dm_dn;
    TAILQ_REMOVE(&dn->dn_initial_scan_current_queue, dm, dm_scan_link);
    dm->dm_scan_status = DM_SCAN_PENDING;
    TAILQ_INSERT_TAIL(&dn->dn_initial_scan_pending_queue, dm, dm_scan_link);
    dvb_network_schedule_initial_scan(dn);
  }

  dm->dm_current_tdmi = NULL;
  tda->tda_current_tdmi = NULL;
}


/**
 *
 */
int
tdmi_current_weight(const th_dvb_mux_instance_t *tdmi)
{
  int w = 0;
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  const service_t *s;


  // This max weight of all subscriptions could be kept in the adapter struct
  pthread_mutex_lock(&tda->tda_delivery_mutex);
  LIST_FOREACH(s, &tda->tda_transports, s_active_link) {
    const th_subscription_t *ths;
    LIST_FOREACH(ths, &s->s_subscriptions, ths_service_link)
      w = MAX(w, ths->ths_weight);
  }
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  w = MAX(w, tdmi->tdmi_mux->dm_scan_status == DM_SCAN_CURRENT);
  return w;
}


/**
 *
 */
static void
dvb_mux_initial_scan_timeout(void *aux)
{
  dvb_mux_t *dm = aux;
  tvhlog(LOG_DEBUG, "dvb", "Initial scan timed out for \"%s\"",
         dvb_mux_nicename(dm));

  dvb_mux_initial_scan_done(dm);
}


/**
 *
 */
int
dvb_mux_tune(dvb_mux_t *dm, const char *reason, int weight)
{
  dvb_network_t *dn = dm->dm_dn;
  th_dvb_mux_instance_t *tdmi;
  int r;

  lock_assert(&global_lock);

  if(dm->dm_current_tdmi == NULL) {

    dvb_create_tdmis(dm);

    while(1) {
      // Figure which adapter to use
      LIST_FOREACH(tdmi, &dm->dm_tdmis, tdmi_mux_link)
        if(!tdmi->tdmi_tune_failed && tdmi->tdmi_adapter->tda_current_tdmi == NULL)
          break;

      if(tdmi == NULL) {
        // None available, need to strike one out
        LIST_FOREACH(tdmi, &dm->dm_tdmis, tdmi_mux_link) {
          if(tdmi->tdmi_tune_failed)
            continue;

          th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
          th_dvb_mux_instance_t *t2 = tda->tda_current_tdmi;
          assert(t2 != NULL);
          assert(t2 != tdmi);

          if(tdmi_current_weight(t2) < weight) {
            break;
          }
        }

        if(tdmi == NULL)
          return SM_CODE_NO_FREE_ADAPTER;
      }

      r = dvb_fe_tune_tdmi(tdmi, reason);
      if(!r)
        break;
    }
  }

  if(dm->dm_scan_status == DM_SCAN_PENDING) {
    TAILQ_REMOVE(&dn->dn_initial_scan_pending_queue, dm, dm_scan_link);
    dm->dm_scan_status = DM_SCAN_CURRENT;
    TAILQ_INSERT_TAIL(&dn->dn_initial_scan_current_queue, dm, dm_scan_link);

    gtimer_arm(&dm->dm_initial_scan_timeout, dvb_mux_initial_scan_timeout, dm, 10);
  }

  return 0;
}
