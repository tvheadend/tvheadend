/*
 *  TV headend - General configuration settings
 *  Copyright (C) 2012 Adam Sutton
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

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

#include "tvheadend.h"
#include "settings.h"
#include "config.h"
#include "uuid.h"
#include "htsbuf.h"
#include "spawn.h"
#include "lock.h"
#include "profile.h"
#include "avahi.h"

/* *************************************************************************
 * Global data
 * ************************************************************************/

static htsmsg_t *config;
static char config_lock[PATH_MAX];
static int config_lock_fd;

/* *************************************************************************
 * Config migration
 * ************************************************************************/

typedef void (*config_migrate_t) (void);

/*
 * Get channel UUID (by number)
 */
static const char *
config_migrate_v1_chn_id_to_uuid ( htsmsg_t *chns, uint32_t id )
{
  htsmsg_field_t *f;
  htsmsg_t *e;
  uint32_t u32;
  HTSMSG_FOREACH(f, chns) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    if (htsmsg_get_u32(e, "channelid", &u32)) continue;
    if (u32 == id) {
      return htsmsg_get_str(e, "uuid");
    }
  }
  return NULL;
}

/*
 * Get channel UUID (by name)
 */
static const char *
config_migrate_v1_chn_name_to_uuid ( htsmsg_t *chns, const char *name )
{
  htsmsg_t *chn = htsmsg_get_map(chns, name);
  if (chn)
    return htsmsg_get_str(chn, "uuid");
  return NULL;
}

/*
 * Helper to add service to channel
 */
static void
config_migrate_v1_chn_add_svc
  ( htsmsg_t *chns, const char *chnname, const char *svcuuid )
{
  htsmsg_t *chn = htsmsg_get_map(chns, chnname);
  if (chn) {
    htsmsg_t *l = htsmsg_get_list(chn, "services");
    if (l)
      htsmsg_add_str(l, NULL, svcuuid);
  }
}

/*
 * Helper function to migrate a muxes services
 */
static void
config_migrate_v1_dvb_svcs
  ( const char *name, const char *netu, const char *muxu, htsmsg_t *channels )
{
  tvh_uuid_t svcu;
  htsmsg_t *c, *e, *svc;
  htsmsg_field_t *f;
  const char *str;
  uint32_t u32;

  if ((c = hts_settings_load_r(1, "dvbtransports/%s", name))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;

      svc = htsmsg_create_map();
      uuid_init_hex(&svcu, NULL);
      if (!htsmsg_get_u32(e, "service_id", &u32))
        htsmsg_add_u32(svc, "sid", u32);
      if ((str = htsmsg_get_str(e, "servicename")))
        htsmsg_add_str(svc, "svcname", str);
      if ((str = htsmsg_get_str(e, "provider")))
        htsmsg_add_str(svc, "provider", str);
      if (!(htsmsg_get_u32(e, "type", &u32))) 
        htsmsg_add_u32(svc, "dvb_servicetype", u32);
      if (!htsmsg_get_u32(e, "channel", &u32))
        htsmsg_add_u32(svc, "lcn", u32);
      if (!htsmsg_get_u32(e, "disabled", &u32))
        htsmsg_add_u32(svc, "enabled", u32 ? 0 : 1);
      if ((str = htsmsg_get_str(e, "charset")))
        htsmsg_add_str(svc, "charset", str);
      if ((str = htsmsg_get_str(e, "default_authority"))) 
        htsmsg_add_str(svc, "cridauth", str);
  
      // TODO: dvb_eit_enable

      hts_settings_save(svc, "input/linuxdvb/networks/%s/muxes/%s/services/%s",
                        netu, muxu, svcu.hex);

      /* Map to channel */
      if ((str = htsmsg_get_str(e, "channelname")))
        config_migrate_v1_chn_add_svc(channels, str, svcu.hex);
    }
    htsmsg_destroy(c);
  }
}

/*
 * Helper to convert a DVB network
 */
static void
config_migrate_v1_dvb_network
  ( const char *name, htsmsg_t *c, htsmsg_t *channels )
{
  int i;
  tvh_uuid_t netu, muxu;
  htsmsg_t *e, *net, *mux, *tun;
  htsmsg_field_t *f;
  const char *str, *type;
  uint32_t u32;
  const char *mux_str_props[] = {
    "bandwidth",
    "consellation",
    "transmission_mode",
    "guard_interval",
    "hierarchy",
    "fec_hi",
    "fec_lo",
    "fec"
  };
    

  /* Load the adapter config */
  if (!(tun = hts_settings_load("dvbadapters/%s", name))) return;
  if (!(str = htsmsg_get_str(tun, "type"))) return;
  type = str;

  /* Create network entry */
  uuid_init_hex(&netu, NULL);
  net = htsmsg_create_map();
  if (!strcmp(str, "ATSC"))
    htsmsg_add_str(net, "class", "linuxdvb_network_atsc");
  else if (!strcmp(str, "DVB-S"))
    htsmsg_add_str(net, "class", "linuxdvb_network_dvbs");
  else if (!strcmp(str, "DVB-C"))
    htsmsg_add_str(net, "class", "linuxdvb_network_dvbc");
  else
    htsmsg_add_str(net, "class", "linuxdvb_network_dvbt");
  if (!htsmsg_get_u32(tun, "autodiscovery", &u32))
    htsmsg_add_u32(net, "autodiscovery",  u32);
  if (!htsmsg_get_u32(tun, "skip_initialscan", &u32))
    htsmsg_add_u32(net, "skipinitscan",   u32);

  /* Each mux */
  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    mux = htsmsg_create_map();

    if (!htsmsg_get_u32(e, "transportstreamid", &u32))
      htsmsg_add_u32(mux, "tsid", u32);
    if (!htsmsg_get_u32(e, "originalnetworkid", &u32))
      htsmsg_add_u32(mux, "onid", u32);
    if (!htsmsg_get_u32(e, "enabled", &u32))
      htsmsg_add_u32(mux, "enabled", u32);
    if (!htsmsg_get_u32(e, "initialscan", &u32))
      htsmsg_add_u32(mux, "initscan", u32);
    if ((str = htsmsg_get_str(e, "default_authority")))
      htsmsg_add_str(mux, "cridauth", str);
    if ((str = htsmsg_get_str(e, "network")) && *str)
      name = str;

    if (!htsmsg_get_u32(e, "symbol_rate", &u32))
      htsmsg_add_u32(mux, "symbolrate", u32);
    if (!htsmsg_get_u32(e, "frequency", &u32))
      htsmsg_add_u32(mux, "frequency", u32);

    for (i = 0; i < ARRAY_SIZE(mux_str_props); i++) {
      if ((str = htsmsg_get_str(e, mux_str_props[i])))
        htsmsg_add_str(mux, mux_str_props[i], str);
    }

    if ((str = htsmsg_get_str(e, "polarisation"))) {
      char tmp[2] = { *str, 0 };
      htsmsg_add_str(mux, "polarisation", tmp);
    }
    if ((str = htsmsg_get_str(e, "modulation"))) {
      if (!strcmp(str, "PSK_8"))
        htsmsg_add_str(mux, "modulation", "8PSK");
      else
        htsmsg_add_str(mux, "modulation", str);
    }
    if ((str = htsmsg_get_str(e, "rolloff")))
      if (strlen(str) > 8)
        htsmsg_add_str(mux, "rolloff", str+8);

    if ((str = htsmsg_get_str(e, "delivery_system")) &&
        strlen(str) > 4 )
      htsmsg_add_str(mux, "delsys", str+4);
    else if (!strcmp(type, "ATSC"))
      htsmsg_add_str(mux, "delsys", "ATSC");
    else if (!strcmp(type, "DVB-S"))
      htsmsg_add_str(mux, "delsys", "DVBS");
    else if (!strcmp(type, "DVB-C"))
      htsmsg_add_str(mux, "delsys", "DVBC_ANNEX_AC");
    else
      htsmsg_add_str(mux, "delsys", "DVBT");

    /* Save */
    uuid_init_hex(&muxu, NULL);
    hts_settings_save(mux, "input/linuxdvb/networks/%s/muxes/%s/config",
                      netu.hex, muxu.hex);
    htsmsg_destroy(mux);

    /* Services */
    config_migrate_v1_dvb_svcs(f->hmf_name, netu.hex, muxu.hex, channels);
  }

  /* Add properties derived from network */
  htsmsg_add_str(net, "networkname", name);
  hts_settings_save(net, "input/linuxdvb/networks/%s/config", netu.hex);
  htsmsg_destroy(net);
}

/*
 * Migrate DVR/autorec entries
 */
static void
config_migrate_v1_dvr ( const char *path, htsmsg_t *channels )
{
  htsmsg_t *c, *e, *m;
  htsmsg_field_t *f;
  const char *str;
  
  if ((c = hts_settings_load_r(1, path))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if ((str = htsmsg_get_str(e, "channel"))) {
        m = htsmsg_copy(e);
        if (!htsmsg_get_str(e, "channelname"))
          htsmsg_add_str(m, "channelname", str);
        if ((str = config_migrate_v1_chn_name_to_uuid(channels, str))) {
          htsmsg_delete_field(m, "channel");
          htsmsg_add_str(m, "channel", str);
          hts_settings_save(m, "%s/%s", path, f->hmf_name);
        }
        htsmsg_destroy(m);
      }
    }
    htsmsg_destroy(c);
  }
}

/*
 * Migrate Epggrab entries
 */
static void
config_migrate_v1_epggrab ( const char *path, htsmsg_t *channels )
{
  htsmsg_t *c, *e, *m, *l, *chns;
  htsmsg_field_t *f, *f2;
  const char *str;
  uint32_t u32;
  
  if ((c = hts_settings_load_r(1, path))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      m    = htsmsg_copy(e);
      chns = htsmsg_create_list();
      if ((l = htsmsg_get_list(e, "channels"))) {
        htsmsg_delete_field(m, "channels");
        HTSMSG_FOREACH(f2, l) {
          if (!htsmsg_field_get_u32(f2, &u32))
            if ((str = config_migrate_v1_chn_id_to_uuid(channels, u32)))
              htsmsg_add_str(chns, NULL, str);
        }
      } else if (!htsmsg_get_u32(e, "channel", &u32)) {
        htsmsg_delete_field(m, "channel");
        if ((str = config_migrate_v1_chn_id_to_uuid(channels, u32)))
          htsmsg_add_str(chns, NULL, str);
      }
      htsmsg_add_msg(m, "channels", chns);
      hts_settings_save(m, "%s/%s", path, f->hmf_name);
      htsmsg_destroy(m);
    }
    htsmsg_destroy(c);
  }
}

/*
 * v0 -> v1 : 3.4 to initial 4.0)
 *
 * Strictly speaking there were earlier versions than this, but most people
 * using early versions of 4.0 would already have been on this version.
 */
static void
config_migrate_v1 ( void )
{
  tvh_uuid_t netu, muxu, svcu, chnu;
  htsmsg_t *c, *m, *e, *l;
  htsmsg_field_t *f;
  uint32_t u32;
  const char *str;
  char buf[1024];
  htsmsg_t *channels = htsmsg_create_map();

  /* Channels */
  if ((c = hts_settings_load_r(1, "channels"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;

      /* Build entry */
      uuid_init_hex(&chnu, NULL);
      m = htsmsg_create_map();
      htsmsg_add_u32(m, "channelid", atoi(f->hmf_name));
      htsmsg_add_str(m, "uuid", chnu.hex);
      htsmsg_add_msg(m, "services", htsmsg_create_list());
      if (!htsmsg_get_u32(e, "dvr_extra_time_pre", &u32))
        htsmsg_add_u32(m, "dvr_pre_time", u32);
      if (!htsmsg_get_u32(e, "dvr_extra_time_pst", &u32))
        htsmsg_add_u32(m, "dvr_pst_time", u32);
      if (!htsmsg_get_u32(e, "channel_number", &u32))
        htsmsg_add_u32(m, "number", u32);
      if ((str = htsmsg_get_str(e, "icon")))
        htsmsg_add_str(m, "icon", str);
      if ((l = htsmsg_get_list(e, "tags")))
        htsmsg_add_msg(m, "tags", htsmsg_copy(l));
      if ((str = htsmsg_get_str(e, "name"))) {
        htsmsg_add_str(m, "name", str);
        htsmsg_add_msg(channels, str, m);
      }
    }
    htsmsg_destroy(c);
  }

  /* IPTV */
  // Note: this routine actually converts directly to v2 for simplicity
  if ((c = hts_settings_load_r(1, "iptvservices"))) {

    /* Create a network */
    uuid_init_hex(&netu, NULL);
    m = htsmsg_create_map();
    htsmsg_add_str(m, "networkname",    "IPTV Network");
    htsmsg_add_u32(m, "skipinitscan",   1);
    htsmsg_add_u32(m, "autodiscovery",  0);
    htsmsg_add_u32(m, "max_streams",    0);
    htsmsg_add_u32(m, "max_bandwidth",  0);
    hts_settings_save(m, "input/iptv/networks/%s/config", netu.hex);
    htsmsg_destroy(m);

    /* Process services */
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f)))       continue;
      if (!(str = htsmsg_get_str(e, "group")))  continue;
      if (htsmsg_get_u32(e, "port", &u32))      continue;

      /* Create mux entry */
      uuid_init_hex(&muxu, NULL);
      m = htsmsg_create_map();
      snprintf(buf, sizeof(buf), "udp://%s:%d", str, u32);
      htsmsg_add_str(m, "iptv_url", buf);
      if ((str = htsmsg_get_str(e, "interface")))
        htsmsg_add_str(m, "iptv_interface", str);
      if ((str = htsmsg_get_str(e, "channelname")))
        htsmsg_add_str(m, "iptv_svcname", str);
      htsmsg_add_u32(m, "enabled",
                     !!htsmsg_get_u32_or_default(e, "disabled", 0));
      htsmsg_add_u32(m, "initscan", 1);
      hts_settings_save(m, "input/iptv/networks/%s/muxes/%s/config",
                        netu.hex, muxu.hex);
      htsmsg_destroy(m);

      /* Create svc entry */
      uuid_init_hex(&svcu, NULL);
      m = htsmsg_create_map();
      if (!htsmsg_get_u32(e, "pmt", &u32))
        htsmsg_add_u32(m, "pmt", u32);
      if (!htsmsg_get_u32(e, "pcr", &u32))
        htsmsg_add_u32(m, "pcr", u32);
      if (!htsmsg_get_u32(e, "disabled", &u32))
        htsmsg_add_u32(m, "disabled", u32);
      if ((str = htsmsg_get_str(e, "channelname"))) {
        htsmsg_add_str(m, "svcname", str);
        config_migrate_v1_chn_add_svc(channels, str, svcu.hex);
      }
      hts_settings_save(m, "input/iptv/networks/%s/muxes/%s/services/%s",
                        netu.hex, muxu.hex, svcu.hex);
      htsmsg_destroy(m);
    }

    htsmsg_destroy(c);
  }

  /* DVB Networks */
  if ((c = hts_settings_load_r(2, "dvbmuxes"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_migrate_v1_dvb_network(f->hmf_name, e, channels);
    }
    htsmsg_destroy(c);
  }

  /* Update DVR records */
  config_migrate_v1_dvr("dvr/log", channels);
  config_migrate_v1_dvr("autorec", channels);

  /* Update EPG grabbers */
  hts_settings_remove("epggrab/otamux");
  config_migrate_v1_epggrab("epggrab/xmltv/channels", channels);
  
  /* Save the channels */
  // Note: UUID will be stored in the file (redundant) but that's no biggy
  HTSMSG_FOREACH(f, channels) {
    if (!(e   = htsmsg_field_get_map(f)))   continue;
    if (!(str = htsmsg_get_str(e, "uuid"))) continue;
    hts_settings_save(e, "channel/%s", str);
  }
  htsmsg_destroy(channels);
}

/*
 * v1 -> v2 : changes to IPTV arrangements
 */
static void
config_migrate_v2 ( void )
{
  htsmsg_t *m;
  tvh_uuid_t u;
  char src[1024], dst[1024];

  /* Do we have IPTV config to migrate ? */
  if (hts_settings_exists("input/iptv/muxes")) {
    
    /* Create a dummy network */
    uuid_init_hex(&u, NULL);
    m = htsmsg_create_map();
    htsmsg_add_str(m, "networkname", "IPTV Network");
    htsmsg_add_u32(m, "skipinitscan", 1);
    htsmsg_add_u32(m, "autodiscovery", 0);
    hts_settings_save(m, "input/iptv/networks/%s/config", u.hex);
    htsmsg_destroy(m);

    /* Move muxes */
    hts_settings_buildpath(src, sizeof(src),
                           "input/iptv/muxes");
    hts_settings_buildpath(dst, sizeof(dst),
                           "input/iptv/networks/%s/muxes", u.hex);
    rename(src, dst);
  }
}

/*
 * v2 -> v3 : changes to DVB layout
 */
static void
config_migrate_v3 ( void )
{
  char src[1024], dst[1024];

  /* Due to having to potentially run this twice! */
  hts_settings_buildpath(dst, sizeof(dst), "input/dvb/networks");
  if (!access(dst, R_OK | W_OK))
    return;

  hts_settings_makedirs(dst);
  hts_settings_buildpath(src, sizeof(src), "input/linuxdvb/networks");
  rename(src, dst);
}

/*
 * v3 -> v5 : fix broken DVB network / mux files
 */
static void
config_migrate_v5 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  const char *str;

  /* Remove linux prefix from class */
  if ((c = hts_settings_load_r(1, "input/dvb/networks"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e   = htsmsg_field_get_map(f)))    	continue;
      if (!(e   = htsmsg_get_map(e, "config"))) continue;
      if (!(str = htsmsg_get_str(e, "class"))) 	continue;
      if (!strncmp(str, "linux", 5)) {
        str = tvh_strdupa(str+5);
        htsmsg_delete_field(e, "class");
        htsmsg_add_str(e, "class", str);
        hts_settings_save(e, "input/dvb/networks/%s/config", f->hmf_name);
      }
    }
  }
  htsmsg_destroy(c);
}

/*
 * v5 -> v6 : epggrab changes, also xmltv/config
 */
static void
config_migrate_v6 ( void )
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;
  const char *str;
  uint32_t interval;
  char buf[128];
  const char *s;
  int old = 0;

  c = hts_settings_load_r(1, "epggrab/config");

  /* xmltv/config -> egpgrab/config */
  if (!c && (c = hts_settings_load("xmltv/config")))
    old = 1;

  if (c) {
    if (!htsmsg_get_u32(c, old ? "grab-interval" : "interval", &interval)) {
      if (old) interval *= 3600;
      if (interval <= 600)
        strcpy(buf, "*/10 * * * *");
      else if (interval <= 900)
        strcpy(buf, "*/15 * * * *");
      else if (interval <= 1200)
        strcpy(buf, "*/30 * * * *");
      else if (interval <= 3600)
        strcpy(buf, "4 * * * *");
      else if (interval <= 7200)
        strcpy(buf, "4 */2 * * *");
      else if (interval <= 14400)
        strcpy(buf, "4 */4 * * *");
      else if (interval <= 28800)
        strcpy(buf, "4 */8 * * *");
      else if (interval <= 43200)
        strcpy(buf, "4 */12 * * *");
      else
        strcpy(buf, "4 0 * * *");
    } else
      strcpy(buf, "4 */12 * * *");
    htsmsg_add_str(c, "cron", buf);
    htsmsg_delete_field(c, old ? "grab-interval" : "interval");
    if (old) {
      s = htsmsg_get_str(c, "current-grabber");
      if (s) {
        htsmsg_add_str(c, "module", s);
        htsmsg_delete_field(c, "current-grabber");
      }
    }
    if (old) {
      m = htsmsg_get_map(c, "mod_enabled");
      if (!m) {
        m = htsmsg_create_map();
        htsmsg_add_msg(c, "mod_enabled", m);
        m = htsmsg_get_map(c, "mod_enabled");
      }
      htsmsg_add_u32(m, "eit", 1);
      htsmsg_add_u32(m, "uk_freesat", 1);
      htsmsg_add_u32(m, "uk_freeview", 1);
      htsmsg_add_u32(m, "viasat_baltic", 1);
      htsmsg_add_u32(m, "opentv-skyuk", 1);
      htsmsg_add_u32(m, "opentv-skyit", 1);
      htsmsg_add_u32(m, "opentv-ausat", 1);
    }

    hts_settings_save(c, "epggrab/config");
  }
  if (old) {
    hts_settings_remove("xmltv/config");

    /* Migrate XMLTV channels */
    htsmsg_t *xc, *ch;
    htsmsg_t *xchs = hts_settings_load("xmltv/channels");
    htsmsg_t *chs  = hts_settings_load_r(1, "channel");
    if (chs) {
      HTSMSG_FOREACH(f, chs) {
        if ((ch = htsmsg_get_map_by_field(f))) {
          if ((str = htsmsg_get_str(ch, "xmltv-channel"))) {
            if ((xc = htsmsg_get_map(xchs, str))) {
              htsmsg_add_u32(xc, "channel", atoi(f->hmf_name));
            }
          }
        }
      }
    }
    if (xchs) {
      HTSMSG_FOREACH(f, xchs) {
        if ((xc = htsmsg_get_map_by_field(f))) {
          hts_settings_save(xc, "epggrab/xmltv/channels/%s", f->hmf_name);
        }
      }
    }
  }
  htsmsg_destroy(c);
}


/*
 * v6 -> v7 : acesscontrol changes
 */
static void
config_migrate_simple ( const char *dir, htsmsg_t *list,
                        void (*modify)(htsmsg_t *record,
                                       uint32_t id,
                                       const char *uuid,
                                       const void *aux),
                        const void *aux )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  tvh_uuid_t u;
  uint32_t index = 1, id;

  if (!(c = hts_settings_load(dir)))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    uuid_init_hex(&u, NULL);
    if (htsmsg_get_u32(e, "id", &id))
      id = 0;
    else if (list) {
      htsmsg_t *m = htsmsg_create_map();
      char buf[16];
      snprintf(buf, sizeof(buf), "%d", id);
      htsmsg_add_str(m, "id", buf);
      htsmsg_add_str(m, "uuid", u.hex);
      htsmsg_add_msg(list, NULL, m);
    }
    htsmsg_delete_field(e, "id");
    htsmsg_add_u32(e, "index", index++);
    if (modify)
      modify(e, id, u.hex, aux);
    hts_settings_save(e, "%s/%s", dir, u.hex);
    hts_settings_remove("%s/%s", dir, f->hmf_name);
  }

  htsmsg_destroy(c);
}

static void
config_modify_acl( htsmsg_t *c, uint32_t id, const char *uuid, const void *aux )
{
  uint32_t a, b;
  const char *s;
  if (htsmsg_get_u32(c, "adv_streaming", &a))
    if (!htsmsg_get_u32(c, "streaming", &b))
      htsmsg_add_u32(c, "adv_streaming", b);
  if ((s = htsmsg_get_str(c, "password")) != NULL) {
    char buf[256], result[300];
    snprintf(buf, sizeof(buf), "TVHeadend-Hide-%s", s);
    base64_encode(result, sizeof(result), (uint8_t *)buf, strlen(buf));
    htsmsg_add_str(c, "password2", result);
    htsmsg_delete_field(c, "password");
  }
}

static void
config_migrate_v7 ( void )
{
  config_migrate_simple("accesscontrol", NULL, config_modify_acl, NULL);
}

static void
config_modify_tag( htsmsg_t *c, uint32_t id, const char *uuid, const void *aux )
{
  htsmsg_t *ch = (htsmsg_t *)aux;
  htsmsg_t *e, *m, *t;
  htsmsg_field_t *f, *f2;
  uint32_t u32;

  htsmsg_delete_field(c, "index");

  if (ch == NULL || uuid == NULL)
    return;

  HTSMSG_FOREACH(f, ch) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    m = htsmsg_get_list(e, "tags");
    if (m == NULL)
      continue;
    t = htsmsg_get_list(e, "tags_new");
    HTSMSG_FOREACH(f2, m) {
      if (!htsmsg_field_get_u32(f2, &u32) && u32 == id) {
        if (t == NULL) {
          t = htsmsg_create_list();
          htsmsg_add_msg(e, "tags_new", t);
          t = htsmsg_get_list(e, "tags_new");
        }
        if (t)
          htsmsg_add_str(t, NULL, uuid);
      }
    }
  }
}

static void
config_migrate_v8 ( void )
{
  htsmsg_t *ch, *e, *m;
  htsmsg_field_t *f;

  ch = hts_settings_load_r(1, "channel");
  config_migrate_simple("channeltags", NULL, config_modify_tag, ch);
  if (ch == NULL)
    return;
  HTSMSG_FOREACH(f, ch) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    htsmsg_delete_field(e, "tags");
    m = htsmsg_get_list(e, "tags_new");
    if (m) {
      htsmsg_add_msg(e, "tags", htsmsg_copy(m));
      htsmsg_delete_field(e, "tags_new");
    }
    hts_settings_save(e, "channel/%s", f->hmf_name);
  }
  htsmsg_destroy(ch);
}

static void
config_modify_autorec( htsmsg_t *c, uint32_t id, const char *uuid, const void *aux )
{
  uint32_t u32;
  htsmsg_delete_field(c, "index");
  if (!htsmsg_get_u32(c, "approx_time", &u32)) {
    htsmsg_delete_field(c, "approx_time");
    if (u32 != 0)
      htsmsg_add_u32(c, "start", u32);
    else
      htsmsg_add_str(c, "start", "");
  }
  if (!htsmsg_get_u32(c, "contenttype", &u32)) {
    htsmsg_delete_field(c, "contenttype");
    htsmsg_add_u32(c, "content_type", u32 / 16);
  }
}

static void
config_modify_dvr_log( htsmsg_t *c, uint32_t id, const char *uuid, const void *aux )
{
  const htsmsg_t *list = aux;
  const char *chname = htsmsg_get_str(c, "channelname");
  const char *chuuid = htsmsg_get_str(c, "channel");
  htsmsg_t *e;
  htsmsg_field_t *f;
  tvh_uuid_t uuid0;
  const char *s1;
  uint32_t u32;

  htsmsg_delete_field(c, "index");
  if (chname == NULL || (chuuid != NULL && uuid_init_bin(&uuid0, chuuid))) {
    chname = strdup(chuuid ?: "");
    htsmsg_delete_field(c, "channelname");
    htsmsg_delete_field(c, "channel");
    htsmsg_add_str(c, "channelname", chname);
    free((char *)chname);
    if (!htsmsg_get_u32(c, "contenttype", &u32)) {
      htsmsg_delete_field(c, "contenttype");
      htsmsg_add_u32(c, "content_type", u32 / 16);
    }
  }
  if ((s1 = htsmsg_get_str(c, "autorec")) != NULL) {
    s1 = strdup(s1);
    htsmsg_delete_field(c, "autorec");
    if (s1 != NULL) {
      HTSMSG_FOREACH(f, list) {
        if (!(e = htsmsg_field_get_map(f))) continue;
        if (strcmp(s1, htsmsg_get_str(e, "id") ?: "") == 0) {
          const char *s2 = htsmsg_get_str(e, "uuid");
          if (s2)
            htsmsg_add_str(c, "autorec", s2);
          break;
        }
      }
      free((char *)s1);
    }
  }
}

static void
config_migrate_v9 ( void )
{
  htsmsg_t *list = htsmsg_create_list();
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  tvh_uuid_t u;

  config_migrate_simple("autorec", list, config_modify_autorec, NULL);
  config_migrate_simple("dvr/log", NULL, config_modify_dvr_log, list);
  htsmsg_destroy(list);

  if ((c = hts_settings_load("dvr")) != NULL) {
    /* step 1: only "config" */
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (strcmp(f->hmf_name, "config")) continue;
      htsmsg_add_str(e, "name", f->hmf_name + 6);
      uuid_init_hex(&u, NULL);
      hts_settings_remove("dvr/%s", f->hmf_name);
      hts_settings_save(e, "dvr/config/%s", u.hex);
    }
    /* step 2: reset (without "config") */
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (strcmp(f->hmf_name, "config") == 0) continue;
      if (strncmp(f->hmf_name, "config", 6)) continue;
      htsmsg_add_str(e, "name", f->hmf_name + 6);
      uuid_init_hex(&u, NULL);
      hts_settings_remove("dvr/%s", f->hmf_name);
      hts_settings_save(e, "dvr/config/%s", u.hex);
    }
    htsmsg_destroy(c);
  }

  if ((c = hts_settings_load("autorec")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      hts_settings_remove("autorec/%s", f->hmf_name);
      hts_settings_save(e, "dvr/autorec/%s", f->hmf_name);
    }
  }
}

static void
config_migrate_move ( const char *dir,
                      const char *newdir )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if (!(c = hts_settings_load(dir)))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    hts_settings_save(e, "%s/%s", newdir, f->hmf_name);
    hts_settings_remove("%s/%s", dir, f->hmf_name);
  }

  htsmsg_destroy(c);
}

static void
config_migrate_v10 ( void )
{
  config_migrate_move("channel", "channel/config");
  config_migrate_move("channeltags", "channel/tag");
}

static const char *
config_find_uuid( htsmsg_t *map, const char *name, const char *value )
{
  htsmsg_t *e;
  htsmsg_field_t *f;
  const char *s;

  if (!map || !name || !value)
    return NULL;
  HTSMSG_FOREACH(f, map) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    if ((s = htsmsg_get_str(e, name)) != NULL) {
      if (!strcmp(s, value))
        return f->hmf_name;
    }
  }
  return NULL;
}

static void
config_modify_acl_dvallcfg( htsmsg_t *c, htsmsg_t *dvr_config )
{
  uint32_t a;
  const char *username, *uuid;

  username = htsmsg_get_str(c, "username");
  if (!htsmsg_get_u32(c, "dvallcfg", &a))
    if (a == 0) {
      uuid = username ? config_find_uuid(dvr_config, "name", username) : NULL;
      if (uuid)
        htsmsg_add_str(c, "dvr_config", uuid);
    }
  htsmsg_delete_field(c, "dvallcfg");
}

static void
config_modify_acl_tag_only( htsmsg_t *c, htsmsg_t *channel_tag )
{
  uint32_t a;
  const char *username, *tag, *uuid;

  username = htsmsg_get_str(c, "username");
  tag = htsmsg_get_str(c, "channel_tag");
  if (!tag || tag[0] == '\0')
    tag = NULL;
  if (tag == NULL && !htsmsg_get_u32(c, "tag_only", &a)) {
    if (a) {
      uuid = username ? config_find_uuid(channel_tag, "name", username) : NULL;
      if (uuid)
        htsmsg_add_str(c, "channel_tag", uuid);
    }
  } else if (tag) {
    uuid = config_find_uuid(channel_tag, "name", tag);
    if (uuid) {
      htsmsg_delete_field(c, "channel_tag");
      htsmsg_add_str(c, "channel_tag", uuid);
    }
  }
  htsmsg_delete_field(c, "tag_only");
}

static void
config_modify_dvr_config_name( htsmsg_t *c, htsmsg_t *dvr_config )
{
  const char *config_name, *uuid;

  config_name = htsmsg_get_str(c, "config_name");
  uuid = config_name ? config_find_uuid(dvr_config, "name", config_name) : NULL;
  htsmsg_delete_field(c, "config_name");
  htsmsg_add_str(c, "config_name", uuid ?: "");
}


static void
config_migrate_v11 ( void )
{
  htsmsg_t *dvr_config;
  htsmsg_t *channel_tag;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  dvr_config = hts_settings_load("dvr/config");
  channel_tag = hts_settings_load("channel/tag");

  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_modify_acl_dvallcfg(e, dvr_config);
      config_modify_acl_tag_only(e, channel_tag);
    }
    htsmsg_destroy(c);
  }

  if ((c = hts_settings_load("dvr/log")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_modify_dvr_config_name(e, dvr_config);
    }
    htsmsg_destroy(c);
  }

  htsmsg_destroy(channel_tag);
  htsmsg_destroy(dvr_config);
}

static void
config_modify_caclient( htsmsg_t *c, uint32_t id, const char *uuid, const void *aux )
{
  uint32_t u;

  htsmsg_delete_field(c, "index");
  htsmsg_delete_field(c, "connected");
  htsmsg_add_str(c, "class", aux);
  if (!htsmsg_get_u32(c, "oscam", &u)) {
    htsmsg_delete_field(c, "oscam");
    htsmsg_add_u32(c, "mode", u);
  }
}

static void
config_migrate_v12 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  config_migrate_simple("cwc", NULL, config_modify_caclient, "caclient_cwc");
  config_migrate_simple("capmt", NULL, config_modify_caclient, "caclient_capmt");

  if ((c = hts_settings_load("cwc")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      hts_settings_remove("cwc/%s", f->hmf_name);
      hts_settings_save(e, "caclient/%s", f->hmf_name);
    }
  }
  if ((c = hts_settings_load("capmt")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      hts_settings_remove("capmt/%s", f->hmf_name);
      hts_settings_save(e, "caclient/%s", f->hmf_name);
    }
  }
}

static void
config_migrate_v13 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i;

  if ((c = hts_settings_load("dvr/config")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (!htsmsg_get_bool(e, "container", &i)) {
        htsmsg_delete_field(e, "container");
        if (i == 1)
          htsmsg_add_str(e, "profile", "matroska");
        else if (i == 4)
          htsmsg_add_str(e, "profile", "pass");
      }
      htsmsg_delete_field(e, "rewrite-pat");
      htsmsg_delete_field(e, "rewrite-pmt");
      hts_settings_save(e, "dvr/config/%s", f->hmf_name);
    }
  }
}

static const char *
config_migrate_v14_codec(int i)
{
  switch (i) {
  case 1: return "mpeg2video";
  case 2: return "mp2";
  case 3: return "libx264";
  case 4: return "ac3";
  case 8: return "aac";
  case 13: return "libvpx";
  case 14: return "libvorbis";
  default: return "";
  }
}

static void
config_migrate_v14 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i;

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (!htsmsg_get_s32(e, "vcodec", &i)) {
        htsmsg_delete_field(e, "vcodec");
        htsmsg_set_str(e, "vcodec", config_migrate_v14_codec(i));
      }
      if (!htsmsg_get_s32(e, "acodec", &i)) {
        htsmsg_delete_field(e, "acodec");
        htsmsg_set_str(e, "acodec", config_migrate_v14_codec(i));
      }
      if (!htsmsg_get_s32(e, "scodec", &i))
        htsmsg_delete_field(e, "scodec");
      hts_settings_save(e, "profile/%s", f->hmf_name);
    }
  }
}

static void
config_migrate_v15 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i;

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (htsmsg_get_s32(e, "timeout", &i)) {
        htsmsg_set_s32(e, "timeout", 5);
        hts_settings_save(e, "profile/%s", f->hmf_name);
      }
    }
  }
}

static int
config_dvr_autorec_start_set(const char *s, int *tm)
{
  int t;

  if(s == NULL || s[0] == '\0' || !isdigit(s[0]))
    t = -1;
  else if(strchr(s, ':') != NULL)
    // formatted time string - convert
    t = (atoi(s) * 60) + atoi(s + 3);
  else {
    t = atoi(s);
  }
  if (t >= 24 * 60)
    t = -1;
  if (t != *tm) {
    *tm = t;
    return 1;
  }
  return 0;
}

static void
config_modify_dvrauto( htsmsg_t *c )
{
  int tm = -1, tw = -1;
  char buf[16];

  if (config_dvr_autorec_start_set(htsmsg_get_str(c, "start"), &tm) > 0 && tm >= 0) {
    tm -= 15;
    if (tm < 0)
      tm += 24 * 60;
    tw = tm + 30;
    if (tw >= 24 * 60)
      tw -= 24 * 60;
    snprintf(buf, sizeof(buf), "%02d:%02d", tm / 60, tm % 60);
    htsmsg_set_str(c, "start", buf);
    snprintf(buf, sizeof(buf), "%02d:%02d", tw / 60, tw % 60);
    htsmsg_set_str(c, "start_window", buf);
  } else {
    htsmsg_delete_field(c, "start");
  }
}

static void
config_migrate_v16 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if ((c = hts_settings_load("dvr/autorec")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_modify_dvrauto(e);
      hts_settings_save(e, "dvr/autorec/%s", f->hmf_name);
    }
    htsmsg_destroy(c);
  }
}

static void
config_migrate_v17 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i, p;

  if ((c = hts_settings_load("profile")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (htsmsg_get_s32(e, "priority", &i)) {
        p = PROFILE_SPRIO_NORMAL;
        if (strcmp(htsmsg_get_str(e, "name") ?: "", "htsp") == 0)
          p = PROFILE_SPRIO_IMPORTANT;
        htsmsg_set_s32(e, "priority", p);
        hts_settings_save(e, "profile/%s", f->hmf_name);
      }
    }
  }
}

static void
config_migrate_v18 ( void )
{
  htsmsg_t *c, *e, *l, *m;
  htsmsg_field_t *f;
  const char *filename;

  if ((c = hts_settings_load("dvr/log")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if ((filename = htsmsg_get_str(e, "filename")) == NULL)
        continue;
      if ((l = htsmsg_get_list(e, "files")) != NULL)
        continue;
      l = htsmsg_create_list();
      m = htsmsg_create_map();
      htsmsg_add_str(m, "filename", filename);
      htsmsg_add_msg(l, NULL, m);
      htsmsg_delete_field(e, "filename");
      htsmsg_add_msg(e, "files", l);
      hts_settings_save(e, "dvr/log/%s", f->hmf_name);
    }
  }
}

static void
config_migrate_v19 ( void )
{
  htsmsg_t *c, *e, *m;
  htsmsg_field_t *f;
  const char *username, *passwd;
  tvh_uuid_t u;

  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if ((username = htsmsg_get_str(e, "username")) == NULL)
        continue;
      if ((passwd = htsmsg_get_str(e, "password2")) == NULL)
        continue;
      m = htsmsg_create_map();
      htsmsg_add_str(m, "username", username);
      htsmsg_add_str(m, "password2", passwd);
      uuid_init_hex(&u, NULL);
      hts_settings_save(m, "passwd/%s", u.hex);
      htsmsg_delete_field(e, "password2");
      hts_settings_save(e, "accesscontrol/%s", f->hmf_name);
    }
  }
}

/*
 * Perform backup
 */
static void
dobackup(const char *oldver)
{
  char outfile[PATH_MAX], cwd[PATH_MAX];
  const char *argv[] = {
    "/usr/bin/tar", "cjf", outfile,
    "--exclude", "backup", "--exclude", "epggrab/*.sock",
    ".", NULL
  };
  const char *root = hts_settings_get_root();
  char errtxt[128];
  const char **arg;
  pid_t pid;
  int code;

  assert(root);

  tvhinfo("config", "backup: migrating config from %s (running %s)",
                    oldver, tvheadend_version);

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    tvherror("config", "unable to get the current working directory");
    goto fatal;
  }

  if (!access("/bin/tar", X_OK))
    argv[0] = "/bin/tar";
  else if (!access("/usr/bin/tar", X_OK))
    argv[0] = "/usr/bin/tar";
  else if (!access("/usr/local/bin/tar", X_OK))
    argv[0] = "/usr/local/bin/tar";
  else {
    tvherror("config", "unable to find tar program");
    goto fatal;
  }

  snprintf(outfile, sizeof(outfile), "%s/backup", root);
  if (makedirs(outfile, 0700, -1, -1))
    goto fatal;
  if (chdir(root)) {
    tvherror("config", "unable to find directory '%s'", root);
    goto fatal;
  }

  snprintf(outfile, sizeof(outfile), "%s/backup/%s.tar.bz2",
                                     root, oldver);
  tvhinfo("config", "backup: running, output file %s", outfile);

  if (spawnv(argv[0], (void *)argv, &pid, 1, 1)) {
    code = -ENOENT;
  } else {
    while ((code = spawn_reap(pid, errtxt, sizeof(errtxt))) == -EAGAIN)
      usleep(20000);
    if (code == -ECHILD)
      code = 0;
  }

  if (code) {
    htsbuf_queue_t q;
    char *s;
    htsbuf_queue_init(&q, 0);
    for (arg = argv; *arg; arg++) {
      htsbuf_append(&q, *arg, strlen(*arg));
      if (arg[1])
        htsbuf_append(&q, " ", 1);
    }
    s = htsbuf_to_string(&q);
    tvherror("config", "command '%s' returned error code %d", s, code);
    tvherror("config", "executed in directory '%s'", root);
    tvherror("config", "please, do not report this as an error, you may use --nobackup option");
    tvherror("config", "... or run the above command in the printed directory");
    tvherror("config", "... using the same user/group as for the tvheadend executable");
    tvherror("config", "... to check the reason for the unfinished backup");
    free(s);
    htsbuf_queue_flush(&q);
    goto fatal;
  }

  if (chdir(cwd)) {
    tvherror("config", "unable to change directory to '%s'", cwd);
    goto fatal;
  }
  return;

fatal:
  tvherror("config", "backup: fatal error");
  exit(EXIT_FAILURE);
}

/*
 * Migration table
 */
static const config_migrate_t config_migrate_table[] = {
  config_migrate_v1,
  config_migrate_v2,
  config_migrate_v3,
  config_migrate_v3, // Re-run due to bug in previous version of function
  config_migrate_v5,
  config_migrate_v6,
  config_migrate_v7,
  config_migrate_v8,
  config_migrate_v9,
  config_migrate_v10,
  config_migrate_v11,
  config_migrate_v12,
  config_migrate_v13,
  config_migrate_v14,
  config_migrate_v15,
  config_migrate_v16,
  config_migrate_v17,
  config_migrate_v18,
  config_migrate_v19,
};

/*
 * Perform migrations (if required)
 */
static int
config_migrate ( int backup )
{
  uint32_t v;
  const char *s;

  /* Get the current version */
  v = htsmsg_get_u32_or_default(config, "version", 0);
  s = htsmsg_get_str(config, "fullversion") ?: "unknown";

  if (backup && strcmp(s, tvheadend_version))
    dobackup(s);
  else
    backup = 0;

  /* Attempt to auto-detect versions prior to v2 */
  if (!v) {
    if (hts_settings_exists("input/iptv/networks"))
      v = 2;
    else if (hts_settings_exists("input"))
      v = 1;
  }

  /* No changes required */
  if (v == ARRAY_SIZE(config_migrate_table)) {
    if (backup)
      goto update;
    return 0;
  }

  /* Run migrations */
  for ( ; v < ARRAY_SIZE(config_migrate_table); v++) {
    tvhinfo("config", "migrating config from v%d to v%d", v, v+1);
    config_migrate_table[v]();
  }

  /* Update */
update:
  htsmsg_set_u32(config, "version", v);
  htsmsg_set_str(config, "fullversion", tvheadend_version);
  config_save();
  return 1;
}

/*
 *
 */
static void
config_check_one ( const char *dir )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if (!(c = hts_settings_load(dir)))
    return;

  HTSMSG_FOREACH(f, c) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    if (strlen(f->hmf_name) != UUID_HEX_SIZE - 1) {
      tvherror("START", "filename %s/%s/%s is invalid", hts_settings_get_root(), dir, f->hmf_name);
      exit(1);
    }
  }
  htsmsg_destroy(c);
}

/*
 * Perform a simple check for UUID files
 */
static void
config_check ( void )
{
  config_check_one("accesscontrol");
  config_check_one("channel/config");
  config_check_one("channel/tag");
  config_check_one("dvr/config");
  config_check_one("dvr/log");
  config_check_one("dvr/autorec");
  config_check_one("esfilter");
}

/* **************************************************************************
 * Initialisation / Shutdown / Saving
 * *************************************************************************/

static int config_newcfg = 0;

void
config_boot ( const char *path, gid_t gid, uid_t uid )
{
  struct stat st;
  char buf[1024];
  htsmsg_t *config2;

  config = htsmsg_create_map();

  /* Generate default */
  if (!path) {
    const char *homedir = getenv("HOME");
    if (homedir == NULL) {
      tvherror("START", "environment variable HOME is not set");
      exit(EXIT_FAILURE);
    }
    snprintf(buf, sizeof(buf), "%s/.hts/tvheadend", homedir);
    path = buf;
  }

  /* Ensure directory exists */
  if (stat(path, &st)) {
    config_newcfg = 1;
    if (makedirs(path, 0700, gid, uid)) {
      tvhwarn("START", "failed to create settings directory %s,"
                       " settings will not be saved", path);
      return;
    }
  }

  /* And is usable */
  else if (access(path, R_OK | W_OK)) {
    tvhwarn("START", "configuration path %s is not r/w"
                     " for UID:%d GID:%d [e=%s],"
                     " settings will not be saved",
            path, getuid(), getgid(), strerror(errno));
    return;
  }

  /* Configure settings routines */
  hts_settings_init(path);

  /* Lock it */
  hts_settings_buildpath(config_lock, sizeof(config_lock), ".lock");
  if ((config_lock_fd = file_lock(config_lock, 3)) < 0)
    exit(78); /* config error */

  if (chown(config_lock, uid, gid))
    tvhwarn("config", "unable to chown lock file %s UID:%d GID:%d", config_lock, uid, gid);

  /* Load global settings */
  config2 = hts_settings_load("config");
  if (!config2) {
    tvhlog(LOG_DEBUG, "config", "no configuration, loading defaults");
  } else {
    htsmsg_destroy(config);
    config = config2;
  }
}

void
config_init ( int backup )
{
  const char *path = hts_settings_get_root();

  if (path == NULL || access(path, R_OK | W_OK)) {
    tvhwarn("START", "configuration path %s is not r/w"
                     " for UID:%d GID:%d [e=%s],"
                     " settings will not be saved",
            path, getuid(), getgid(), strerror(errno));
    return;
  }

  /* Store version number */
  if (config_newcfg) {
    htsmsg_set_u32(config, "version", ARRAY_SIZE(config_migrate_table));
    htsmsg_set_str(config, "fullversion", tvheadend_version);
    htsmsg_set_str(config, "server_name", "Tvheadend");
    config_save();
  
  /* Perform migrations */
  } else {
    if (config_migrate(backup))
      config_check();
  }
  tvhinfo("config", "loaded");
}

void config_done ( void )
{
  /* note: tvhlog is inactive !!! */
  htsmsg_destroy(config);
  file_unlock(config_lock, config_lock_fd);
}

void config_save ( void )
{
  hts_settings_save(config, "config");
}

/* **************************************************************************
 * Access / Update routines
 * *************************************************************************/

htsmsg_t *config_get_all ( void )
{
  return htsmsg_copy(config);
}

const char *
config_get_str ( const char *fld )
{
  return htsmsg_get_str(config, fld);
}

int
config_set_str ( const char *fld, const char *val )
{
  const char *c = htsmsg_get_str(config, fld);
  if (!c || strcmp(c, val)) {
    if (c) htsmsg_delete_field(config, fld);
    htsmsg_add_str(config, fld, val);
    return 1;
  }
  return 0;
}

int
config_get_int ( const char *fld, int deflt )
{
  return htsmsg_get_s32_or_default(config, fld, deflt);
}

int
config_set_int ( const char *fld, int val )
{
  const char *c = htsmsg_get_str(config, fld);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", val);
  if (!c || strcmp(c, buf)) {
    if (c) htsmsg_delete_field(config, fld);
    htsmsg_add_s32(config, fld, val);
    return 1;
  }
  return 0;
}

const char *config_get_server_name ( void )
{
  const char *s = htsmsg_get_str(config, "server_name");
  if (s == NULL || *s == '\0')
    return "Tvheadend";
  return s;
}

int config_set_server_name ( const char *name )
{
  int r = config_set_str("server_name", name);
  avahi_restart();
  return r;
}

const char *config_get_language ( void )
{
  const char *s = htsmsg_get_str(config, "language");
  if (s == NULL || *s == '\0')
    return "eng";
  return s;
}

int config_set_language ( const char *lang )
{
  return config_set_str("language", lang);
}

const char *config_get_muxconfpath ( void )
{
  return htsmsg_get_str(config, "muxconfpath");
}

int config_set_muxconfpath ( const char *path )
{
  return config_set_str("muxconfpath", path);
}

int config_get_prefer_picon ( void )
{
  int b = 0;
  htsmsg_get_bool(config, "prefer_picon", &b);
  return b;
}

int config_set_prefer_picon ( const char *str )
{
  return config_set_str("prefer_picon", str);
}

const char *config_get_chicon_path ( void )
{
  return htsmsg_get_str(config, "chiconpath");
}

int config_set_chicon_path ( const char *str )
{
  return config_set_str("chiconpath", str);
}

const char *config_get_picon_path ( void )
{
  return htsmsg_get_str(config, "piconpath");
}

int config_set_picon_path ( const char *str )
{
  return config_set_str("piconpath", str);
}
