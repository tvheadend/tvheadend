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



/**
 *
 */






/**
 * Switch the adapter (which is implicitly tied to our transport)
 * to receive the given transport.
 *
 * But we only do this if 'weight' is higher than all of the current
 * transports that is subscribing to the adapter
 */
static int
dvb_service_start(service_t *t, unsigned int weight, int force_start)
{
#if 0
  int w, r;
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;
  th_dvb_mux_instance_t *tdmi = tda->tda_current_tdmi;

  lock_assert(&global_lock);

  if(tda->tda_rootpath == NULL)
    return SM_CODE_NO_HW_ATTACHED;

  if(t->s_dvb_mux_instance && !t->s_dvb_mux_instance->tdmi_enabled)
    return SM_CODE_MUX_NOT_ENABLED; /* Mux is disabled */

  /* Check if adapter is idle, or already tuned */

  if(tdmi != NULL && 
     (tdmi != t->s_dvb_mux_instance ||
      tda->tda_hostconnection == HOSTCONNECTION_USB12)) {

    w = service_compute_weight(&tda->tda_transports);
    if(w && w >= weight && !force_start)
      /* We are outranked by weight, cant use it */
      return SM_CODE_NOT_FREE;

    if(LIST_FIRST(&tdmi->tdmi_subscriptions) != NULL)
      return SM_CODE_NOT_FREE;

    dvb_adapter_clean(tda);
  }

  pthread_mutex_lock(&tda->tda_delivery_mutex);

  r = dvb_fe_tune(t->s_dvb_mux_instance->tdmi_mux, "Transport start");
  if(!r)
    LIST_INSERT_HEAD(&tda->tda_transports, t, s_active_link);

  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  if(!r)
    tda->tda_open_service(tda, t);

  dvb_table_add_pmt(t->s_dvb_mux_instance->tdmi_mux, t->s_pmt_pid);

  return r;
#endif
  return SM_CODE_NO_HW_ATTACHED;
}


/**
 *
 */
static void
dvb_service_stop(service_t *t)
{
#if 0
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;

  lock_assert(&global_lock);

  pthread_mutex_lock(&tda->tda_delivery_mutex);
  LIST_REMOVE(t, s_active_link);
  pthread_mutex_unlock(&tda->tda_delivery_mutex);

  tda->tda_close_service(tda, t);

  t->s_status = SERVICE_IDLE;
#endif
}


/**
 *
 */
static void
dvb_service_refresh(service_t *t)
{
#if 0
  th_dvb_adapter_t *tda = t->s_dvb_mux_instance->tdmi_adapter;

  lock_assert(&global_lock);
  tda->tda_open_service(tda, t);
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

  pthread_mutex_lock(&t->s_stream_mutex);
  psi_save_service_settings(m, t);
  pthread_mutex_unlock(&t->s_stream_mutex);

  dvb_mux_t *dm = t->s_dvb_mux;

  hts_settings_save(m, "dvb/networks/%s/muxes/%s/services/%04x",
                    dm->dm_dn->dn_uuid,
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
                        dm->dm_dn->dn_uuid, dm->dm_local_identifier);
  if(l == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "service_id", &sid))
      continue;

    if(htsmsg_get_u32(c, "pmt", &pmt))
      continue;

    t = dvb_service_find(dm, sid, pmt, f->hmf_name);

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
  char buf[100];

  memset(si, 0, sizeof(struct source_info));

  lock_assert(&global_lock);

  si->si_type = S_MPEG_TS;

  if(dm->dm_network_name != NULL)
    si->si_network = strdup(dm->dm_network_name);

  dvb_mux_nicename(buf, sizeof(buf), dm);
  si->si_mux = strdup(buf);

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
      if (enabled    && !svc->s_enabled) continue;
      if (epgprimary && !service_is_primary_epg(svc)) continue;
      if (sid == svc->s_dvb_service_id) return svc;
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
  char buf[200];

  lock_assert(&global_lock);

  LIST_FOREACH(t, &dm->dm_services, s_group_link) {
    if(t->s_dvb_service_id == sid)
      return t;
  }

  if(pmt_pid == 0)
    return NULL;

  dvb_mux_nicename(buf, sizeof(buf), dm);
  tvhlog(LOG_DEBUG, "dvb", "Add service \"0x%x\" on \"%s\"", sid, buf);

  t = service_create(uuid, S_MPEG_TS);
  if (save) *save = 1;

  t->s_dvb_service_id = sid;
  t->s_pmt_pid        = pmt_pid;

  t->s_start_feed    = dvb_service_start;
  t->s_refresh_feed  = dvb_service_refresh;
  t->s_stop_feed     = dvb_service_stop;
  t->s_config_save   = dvb_service_save;
  t->s_setsourceinfo = dvb_service_setsourceinfo;
  t->s_grace_period  = dvb_grace_period;

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
htsmsg_t *
dvb_service_build_msg(service_t *t)
{
  dvb_mux_t *dm = t->s_dvb_mux;
  htsmsg_t *m = htsmsg_create_map();
  char buf[100];
 
  htsmsg_add_str(m, "uuid", t->s_uuid);
  htsmsg_add_u32(m, "enabled", t->s_enabled);
  htsmsg_add_u32(m, "channel", t->s_channel_number);

  htsmsg_add_u32(m, "sid", t->s_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->s_pmt_pid);
  htsmsg_add_u32(m, "pcr", t->s_pcr_pid);
  
  htsmsg_add_str(m, "type", service_servicetype_txt(t));

  htsmsg_add_str(m, "svcname", t->s_svcname ?: "");
  htsmsg_add_str(m, "provider", t->s_provider ?: "");

  htsmsg_add_str(m, "network", dm->dm_network_name ?: "");

  dvb_mux_nicefreq(buf, sizeof(buf), dm);
  htsmsg_add_str(m, "mux", buf);

  if(t->s_ch != NULL)
    htsmsg_add_str(m, "channelname", t->s_ch->ch_name);

  if(t->s_dvb_charset != NULL)
    htsmsg_add_str(m, "dvb_charset", t->s_dvb_charset);

  htsmsg_add_u32(m, "dvb_eit_enable", t->s_dvb_eit_enable);

  return m;
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

