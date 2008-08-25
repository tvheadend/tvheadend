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

#include <libhts/htssettings.h>

#include "tvhead.h"
#include "dispatch.h"
#include "dvb.h"
#include "channels.h"
#include "transports.h"
#include "subscriptions.h"
#include "psi.h"
#include "dvb_support.h"
#include "dvb_dvr.h"
#include "notify.h"



/**
 * Load config for the given mux
 */
void
dvb_transport_load(th_dvb_mux_instance_t *tdmi)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;
  uint32_t sid, pmt;
  const char *s;
  unsigned int u32;
  th_transport_t *t;

  if((l = hts_settings_load("dvbtransports/%s", tdmi->tdmi_identifier)) == NULL)
    return;
  
  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_msg_by_field(f)) == NULL)
      continue;

    if(htsmsg_get_u32(c, "service_id", &sid))
      continue;

    if(htsmsg_get_u32(c, "pmt", &pmt))
      continue;
    
    t = dvb_transport_find(tdmi, sid, pmt, NULL);

    htsmsg_get_u32(c, "stype", &t->tht_servicetype);
    if(htsmsg_get_u32(c, "scrambled", &u32))
      u32 = 0;
    t->tht_scrambled = u32;

    s = htsmsg_get_str(c, "provider") ?: "unknown";
    t->tht_provider = strdup(s);

    s = htsmsg_get_str(c, "servicename") ?: "unknown";
    t->tht_svcname = strdup(s);

    s = htsmsg_get_str(c, "channelname");
    if(s != NULL) {
      t->tht_chname = strdup(s);
    } else {
      t->tht_chname = strdup(t->tht_svcname);
    }

    psi_load_transport_settings(c, t);

    if(!htsmsg_get_u32(c, "mapped", &u32) && u32)
      transport_map_channel(t, NULL);
  }
  htsmsg_destroy(l);
}

/**
 *
 */
static void
dvb_transport_save(th_transport_t *t)
{
  htsmsg_t *m = htsmsg_create();


  htsmsg_add_u32(m, "service_id", t->tht_dvb_service_id);
  htsmsg_add_u32(m, "pmt", t->tht_pmt);
  htsmsg_add_u32(m, "stype", t->tht_servicetype);
  htsmsg_add_u32(m, "scrambled", t->tht_scrambled);

  if(t->tht_provider != NULL)
    htsmsg_add_str(m, "provider", t->tht_provider);

  if(t->tht_svcname != NULL)
    htsmsg_add_str(m, "servicename", t->tht_svcname);

  if(t->tht_chname != NULL)
    htsmsg_add_str(m, "channelname", t->tht_chname);

  htsmsg_add_u32(m, "mapped", !!t->tht_ch);
  
  psi_save_transport_settings(m, t);
  
  hts_settings_save(m, "dvbtransports/%s/%s",
		    t->tht_dvb_mux_instance->tdmi_identifier,
		    t->tht_identifier);

  htsmsg_destroy(m);
}


/**
 * Called to get quality for the given transport
 *
 * We keep track of this for the entire mux (if we see errors), soo..
 * return that value 
 */
static int
dvb_transport_quality(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi = t->tht_dvb_mux_instance;
  return tdmi->tdmi_quality;
}


/**
 * Generate a descriptive name for the source
 */
static const char *
dvb_transport_name(th_transport_t *t)
{
  th_dvb_mux_instance_t *tdmi;
  static char buf[200];

  tdmi = t->tht_dvb_mux_instance;

  snprintf(buf, sizeof(buf), "\"%s\" on \"%s\"",
	   tdmi->tdmi_network ?: "Unknown network",
	   tdmi->tdmi_adapter->tda_rootpath);

  return buf;
}




/**
 * Find a transport based on 'serviceid' on the given mux
 *
 * If it cannot be found we create it if 'pmt_pid' is also set
 */
th_transport_t *
dvb_transport_find(th_dvb_mux_instance_t *tdmi, uint16_t sid, int pmt_pid,
		   int *created)
{
  th_transport_t *t;
  char tmp[200];

  if(created != NULL)
    *created = 0;

  LIST_FOREACH(t, &tdmi->tdmi_transports, tht_mux_link) {
    if(t->tht_dvb_service_id == sid)
      return t;
  }
  
  if(pmt_pid == 0)
    return NULL;

  if(created != NULL)
    *created = 1;

  snprintf(tmp, sizeof(tmp), "%s_%04x", tdmi->tdmi_identifier, sid);

  t = transport_create(tmp, TRANSPORT_DVB, THT_MPEG_TS);

  t->tht_dvb_service_id = sid;
  t->tht_pmt            = pmt_pid;

  t->tht_start_feed = dvb_start_feed;
  t->tht_stop_feed  = dvb_stop_feed;
  t->tht_config_change = dvb_transport_save;
  t->tht_sourcename = dvb_transport_name;
  t->tht_dvb_mux_instance = tdmi;
  t->tht_quality_index = dvb_transport_quality;

  LIST_INSERT_HEAD(&tdmi->tdmi_transports, t, tht_mux_link);
  return t;
}
