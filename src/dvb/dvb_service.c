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
#include "subscriptions.h"
#include "psi.h"
#include "dvb_support.h"
#include "notify.h"

static const char *dvb_service_get_title(struct idnode *self);

const idclass_t dvb_service_class = {
  .ic_super = &service_class,
  .ic_class = "dvbservice",
  .ic_get_title = dvb_service_get_title,
  //  .ic_get_childs = dvb_service_get_childs,
  .ic_properties = (const property_t[]){
    {
      "dvb_eit_enable", "Use EPG", PT_BOOL,
      offsetof(service_t, s_dvb_eit_enable)
    }, {
    }}
};


/**
 * Switch the adapter (which is implicitly tied to our transport)
 * to receive the given transport.
 *
 * But we only do this if 'weight' is higher than all of the current
 * transports that is subscribing to the adapter
 */
static int
dvb_service_start(service_t *s, int instance)
{
  int r;
  dvb_mux_t *dm = s->s_dvb_mux;
  th_dvb_mux_instance_t *tdmi;

  assert(s->s_status == SERVICE_IDLE);

  lock_assert(&global_lock);

  LIST_FOREACH(tdmi, &dm->dm_tdmis, tdmi_mux_link)
    if(tdmi->tdmi_adapter->tda_instance == instance)
      break;

  assert(tdmi != NULL); // We should always find this instance

  r = dvb_fe_tune_tdmi(tdmi, "service start");

  if(!r) {
    th_dvb_adapter_t *tda = dm->dm_current_tdmi->tdmi_adapter;
    pthread_mutex_lock(&tda->tda_delivery_mutex);
    LIST_INSERT_HEAD(&tda->tda_transports, s, s_active_link);
    pthread_mutex_unlock(&tda->tda_delivery_mutex);
    if (tda->tda_locked) {
      tda->tda_open_service(tda, s);
      dvb_table_add_pmt(dm, s->s_pmt_pid);
    }
  }

  return r;
}

/**
 *
 */
static th_dvb_adapter_t *
dvb_service_get_tda(service_t *s)
{
  dvb_mux_t *dm = s->s_dvb_mux;
  assert(dm->dm_current_tdmi != NULL);
  th_dvb_adapter_t *tda = dm->dm_current_tdmi->tdmi_adapter;
  assert(tda != NULL);
  return tda;
}


/**
 *
 */
static void
dvb_service_stop(service_t *s)
{
  th_dvb_adapter_t *tda = dvb_service_get_tda(s);
  lock_assert(&global_lock);

  pthread_mutex_lock(&tda->tda_delivery_mutex);
  LIST_REMOVE(s, s_active_link);
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  tda->tda_close_service(tda, s);

  s->s_status = SERVICE_IDLE;
}


/**
 *
 */
static void
dvb_service_refresh(service_t *s)
{
  th_dvb_adapter_t *tda = dvb_service_get_tda(s);
  lock_assert(&global_lock);
  tda->tda_open_service(tda, s);
}


/**
 *
 */
static void
dvb_service_enlist(struct service *s, struct service_instance_list *sil)
{
  dvb_mux_t *dm = s->s_dvb_mux;
  th_dvb_mux_instance_t *tdmi;

  dvb_create_tdmis(dm);

  LIST_FOREACH(tdmi, &dm->dm_tdmis, tdmi_mux_link) {
    if(tdmi->tdmi_tune_failed)
      continue; // The hardware is not able to tune to this frequency, never consider it

    service_instance_add(sil, s, tdmi->tdmi_adapter->tda_instance, 100,
                         tdmi_current_weight(tdmi));
  }
}

/**
 *
 */
static int
dvb_service_is_enabled(service_t *t)
{
  return t->s_dvb_mux->dm_enabled;
#if TODO_FIX_THIS
  th_dvb_adapter_t *tda = tdmi->tdmi_adapter;
  return tda->tda_enabled && tdmi->tdmi_enabled && t->s_enabled && t->s_pmt_pid;
#endif
}


/**
 *
 */
static void
dvb_service_save(service_t *t)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_str(m, "uuid",  idnode_uuid_as_str(&t->s_id));
  htsmsg_add_u32(m, "service_id", t->s_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);
  htsmsg_add_u32(m, "stype", t->s_servicetype);
  htsmsg_add_u32(m, "scrambled", t->s_scrambled);
  htsmsg_add_u32(m, "channel", t->s_channel_number);

  if(t->s_provider != NULL)
    htsmsg_add_str(m, "provider", t->s_provider);

  if(t->s_svcname != NULL)
    htsmsg_add_str(m, "servicename", t->s_svcname);

  if(t->s_ch != NULL) {
    htsmsg_add_str(m, "channelname", t->s_ch->ch_name);
    htsmsg_add_u32(m, "mapped", 1);
  }

  if(t->s_dvb_charset != NULL)
    htsmsg_add_str(m, "dvb_charset", t->s_dvb_charset);

  htsmsg_add_u32(m, "dvb_eit_enable", t->s_dvb_eit_enable);

  if(t->s_default_authority)
    htsmsg_add_str(m, "default_authority", t->s_default_authority);

  if(t->s_prefcapid)
    htsmsg_add_u32(m, "prefcapid", t->s_prefcapid);

  pthread_mutex_lock(&t->s_stream_mutex);
  psi_save_service_settings(m, t);
  pthread_mutex_unlock(&t->s_stream_mutex);

  dvb_mux_t *dm = t->s_dvb_mux;

  hts_settings_save(m, "dvb/networks/%s/muxes/%s/services/%04x",
                    idnode_uuid_as_str(&dm->dm_dn->dn_id),
                    dm->dm_local_identifier,
                    t->s_dvb_service_id);

  htsmsg_destroy(m);
}


/**
 * Load config for the given mux
 */
void
dvb_service_load(dvb_mux_t *dm)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  uint32_t sid, pmt;
  const char *s;
  unsigned int u32;
  service_t *t;

  lock_assert(&global_lock);

  l = hts_settings_load("dvb/networks/%s/muxes/%s/services",
                        idnode_uuid_as_str(&dm->dm_dn->dn_id),
                        dm->dm_local_identifier);
  if(l == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "service_id", &sid))
      continue;

    if(htsmsg_get_u32(c, "pmt", &pmt))
      continue;

    const char *uuid = htsmsg_get_str(c, "uuid");
    t = dvb_service_find(dm, sid, pmt, uuid);

    htsmsg_get_u32(c, "stype", &t->s_servicetype);
    if(htsmsg_get_u32(c, "scrambled", &u32))
      u32 = 0;
    t->s_scrambled = u32;

    if(htsmsg_get_u32(c, "channel", &u32))
      u32 = 0;
    t->s_channel_number = u32;

    s = htsmsg_get_str(c, "provider");
    t->s_provider = s ? strdup(s) : NULL;

    s = htsmsg_get_str(c, "servicename");
    t->s_svcname = s ? strdup(s) : NULL;

    pthread_mutex_lock(&t->s_stream_mutex);
    service_make_nicename(t);
    psi_load_service_settings(c, t);
    pthread_mutex_unlock(&t->s_stream_mutex);

    if (!(s = htsmsg_get_str(c, "dvb_charset")))
      s = htsmsg_get_str(c, "dvb_default_charset");
    t->s_dvb_charset = s ? strdup(s) : NULL;

    s = htsmsg_get_str(c, "default_authority");
    t->s_default_authority = s ? strdup(s) : NULL;

    if(htsmsg_get_u32(c, "dvb_eit_enable", &u32))
      u32 = 1;
    t->s_dvb_eit_enable = u32;

    s = htsmsg_get_str(c, "channelname");
    if(htsmsg_get_u32(c, "mapped", &u32))
      u32 = 0;

    if(s && u32)
      service_map_channel(t, channel_find_by_name(s, 1, 0), 0);

    if(htsmsg_get_u32(c, "prefcapid", &u32))
      u32 = 0;
    t->s_prefcapid = u32;

    /* HACK - force save for old config */
    if(uuid == NULL) {
      // If service config on disk lacked UUID (for whatever reason),
      // write it back
      dvb_service_save(t);
    }
  }
  htsmsg_destroy(l);
}



/**
 * Generate a descriptive name for the source
 */
static void
dvb_service_setsourceinfo(service_t *t, struct source_info *si)
{
  dvb_mux_t *dm = t->s_dvb_mux;

  memset(si, 0, sizeof(struct source_info));

  lock_assert(&global_lock);

  si->si_type = S_MPEG_TS;

  if(dm->dm_network_name != NULL)
    si->si_network = strdup(dm->dm_network_name);

  si->si_mux = strdup(dvb_mux_nicename(dm));

  if(t->s_provider != NULL)
    si->si_provider = strdup(t->s_provider);

  if(t->s_svcname != NULL)
    si->si_service = strdup(t->s_svcname);
}


/**
 *
 */
static int
dvb_grace_period(service_t *t)
{
#if TODO_FIX_THIS
  if (t->s_dvb_mux_instance && t->s_dvb_mux_instance->tdmi_adapter)
    return t->s_dvb_mux_instance->tdmi_adapter->tda_grace_period ?: 10;
#endif
  return 10;
}



/*
 * Find transport based on the DVB identification
 */
service_t *
dvb_service_find3(dvb_network_t *dn, dvb_mux_t *dm,
                  const char *netname, uint16_t onid, uint16_t tsid,
                  uint16_t sid, int enabled, int epgprimary)
{
  service_t *svc;
  if (dm != NULL) {
    LIST_FOREACH(svc, &dm->dm_services, s_group_link) {
      if (sid != svc->s_dvb_service_id) continue;
      if (enabled    && !svc->s_enabled) continue;
      if (epgprimary && !service_is_primary_epg(svc)) continue;
      return svc;
    }
  } else if (dn) {
    LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link) {
      if (enabled && !dm->dm_enabled) continue;
      if (onid    && onid != dm->dm_network_id) continue;
      if (tsid    && tsid != dm->dm_transport_stream_id) continue;
      if (netname && strcmp(netname, dm->dm_network_name ?: "")) continue;
      if ((svc = dvb_service_find3(dn, dm, NULL, 0, 0, sid,
                                   enabled, epgprimary)))
        return svc;
    }
  } else {
    LIST_FOREACH(dn, &dvb_networks, dn_global_link)
      if ((svc = dvb_service_find3(dn, NULL, netname, onid, tsid,
                                   sid, enabled, epgprimary)))
        return svc;
  }
  return NULL;
}


/**
 * Find a transport based on 'serviceid' on the given mux
 *
 * If it cannot be found we create it if 'pmt_pid' is also set
 */
service_t *
dvb_service_find(dvb_mux_t *dm, uint16_t sid, int pmt_pid, const char *uuid)
{
  return dvb_service_find2(dm, sid, pmt_pid, uuid, NULL);
}


/**
 *
 */
service_t *
dvb_service_find2(dvb_mux_t *dm, uint16_t sid, int pmt_pid,
		   const char *uuid, int *save)
{
  service_t *t;

  lock_assert(&global_lock);

  LIST_FOREACH(t, &dm->dm_services, s_group_link) {
    if(t->s_dvb_service_id == sid)
      break;
  }

  /* Existing - updated PMT_PID if required */
  if (t) {
    if (pmt_pid && pmt_pid != t->s_pmt_pid) {
      t->s_pmt_pid = pmt_pid;
      *save = 1;
    }
    return t;
  }
  
  if(pmt_pid == 0)
    return NULL;

  tvhlog(LOG_DEBUG, "dvb", "Add service \"0x%x\" on \"%s\"", sid,
         dvb_mux_nicename(dm));

  t = service_create(uuid, S_MPEG_TS, &dvb_service_class);

  if (save) *save = 1;

  t->s_dvb_service_id = sid;
  t->s_pmt_pid        = pmt_pid;

  t->s_start_feed    = dvb_service_start;
  t->s_refresh_feed  = dvb_service_refresh;
  t->s_stop_feed     = dvb_service_stop;
  t->s_config_save   = dvb_service_save;
  t->s_setsourceinfo = dvb_service_setsourceinfo;
  t->s_grace_period  = dvb_grace_period;
  t->s_is_enabled    = dvb_service_is_enabled;
  t->s_enlist        = dvb_service_enlist;

  t->s_dvb_mux = dm;
  LIST_INSERT_HEAD(&dm->dm_services, t, s_group_link);

  pthread_mutex_lock(&t->s_stream_mutex);
  service_make_nicename(t);
  pthread_mutex_unlock(&t->s_stream_mutex);
  return t;
}


/**
 *
 */
static const char *
dvb_service_get_title(struct idnode *self)
{
  service_t *s = (service_t *)self;
  static char buf[100];

  if(s->s_svcname) {
    return s->s_svcname;
  } else {
    snprintf(buf, sizeof(buf), "Service-0x%04x", s->s_dvb_service_id);
    return buf;
  }
}


/**
 *
 */
void
dvb_service_notify_by_adapter(th_dvb_adapter_t *tda)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "adapterId", tda->tda_identifier);
  notify_by_msg("dvbService", m);
}

