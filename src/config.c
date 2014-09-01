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

#include "tvheadend.h"
#include "settings.h"
#include "config.h"
#include "uuid.h"

#include <string.h>
#include <sys/stat.h>

/* *************************************************************************
 * Global data
 * ************************************************************************/

static htsmsg_t *config;

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
    if (xchs) {
      HTSMSG_FOREACH(f, chs) {
        if ((ch = htsmsg_get_map_by_field(f))) {
          if ((str = htsmsg_get_str(ch, "xmltv-channel"))) {
            if ((xc = htsmsg_get_map(xchs, str))) {
              htsmsg_add_u32(xc, "channel", atoi(f->hmf_name));
            }
          }
        }
      }
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
                                       void *aux),
                        void *aux )
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
config_modify_acl( htsmsg_t *c, uint32_t id, const char *uuid, void *aux )
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
config_modify_tag( htsmsg_t *c, uint32_t id, const char *uuid, void *aux )
{
  htsmsg_t *ch = (htsmsg_t *)aux;
  htsmsg_t *e, *m, *t;
  htsmsg_field_t *f, *f2;
  uint32_t u32;

  htsmsg_delete_field(c, "index");

  if (ch == NULL)
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
config_modify_autorec( htsmsg_t *c, uint32_t id, const char *uuid, void *aux )
{
  uint32_t u32;
  htsmsg_delete_field(c, "index");
  if (!htsmsg_get_u32(c, "approx_time", &u32)) {
    if (u32 == 0)
      u32 = -1;
    htsmsg_delete_field(c, "approx_time");
    htsmsg_add_u32(c, "start", u32);
  }
  if (!htsmsg_get_u32(c, "contenttype", &u32)) {
    htsmsg_delete_field(c, "contenttype");
    htsmsg_add_u32(c, "content_type", u32 / 16);
  }
}

static void
config_modify_dvr_log( htsmsg_t *c, uint32_t id, const char *uuid, void *aux )
{
  htsmsg_t *list = aux;
  const char *chname = htsmsg_get_str(c, "channelname");
  const char *chuuid = htsmsg_get_str(c, "channel");
  htsmsg_t *e;
  htsmsg_field_t *f;
  tvh_uuid_t uuid0;
  const char *s1;
  uint32_t u32;

  htsmsg_delete_field(c, "index");
  if (chname == NULL || (chuuid != NULL && uuid_init_bin(&uuid0, chuuid))) {
    chname = strdup(chuuid);
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
    HTSMSG_FOREACH(f, list) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (strcmp(s1, htsmsg_get_str(e, "id")) == 0) {
        htsmsg_add_str(c, "autorec", htsmsg_get_str(e, "uuid"));
        break;
      }
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
};

/*
 * Perform migrations (if required)
 */
static void
config_migrate ( void )
{
  uint32_t v;

  /* Get the current version */
  v = htsmsg_get_u32_or_default(config, "version", 0);

  /* Attempt to auto-detect versions prior to v2 */
  if (!v) {
    if (hts_settings_exists("input/iptv/networks"))
      v = 2;
    else if (hts_settings_exists("input"))
      v = 1;
  }

  /* No changes required */
  if (v == ARRAY_SIZE(config_migrate_table))
    return;

  /* Run migrations */
  for ( ; v < ARRAY_SIZE(config_migrate_table); v++) {
    tvhinfo("config", "migrating config from v%d to v%d", v, v+1);
    config_migrate_table[v]();
  }

  /* Update */
  htsmsg_set_u32(config, "version", v);
  config_save();
}

/* **************************************************************************
 * Initialisation / Shutdown / Saving
 * *************************************************************************/

void
config_init ( const char *path )
{
  struct stat st;
  char buf[1024];
  const char *homedir = getenv("HOME");
  int new = 0;

  /* Generate default */
  if (!path) {
    snprintf(buf, sizeof(buf), "%s/.hts/tvheadend", homedir);
    path = buf;
  }

  /* Ensure directory exists */
  if (stat(path, &st)) {
    new = 1;
    if (makedirs(path, 0700)) {
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

  /* Load global settings */
  config = hts_settings_load("config");
  if (!config) {
    tvhlog(LOG_DEBUG, "config", "no configuration, loading defaults");
    config = htsmsg_create_map();
  }

  /* Store version number */
  if (new) {
    htsmsg_set_u32(config, "version", ARRAY_SIZE(config_migrate_table));
    config_save();
  
  /* Perform migrations */
  } else {
    config_migrate();
  }
}

void config_done ( void )
{
  htsmsg_destroy(config);
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

static int
_config_set_str ( const char *fld, const char *val )
{
  const char *c = htsmsg_get_str(config, fld);
  if (!c || strcmp(c, val)) {
    if (c) htsmsg_delete_field(config, fld);
    htsmsg_add_str(config, fld, val);
    return 1;
  }
  return 0;
}

const char *config_get_language ( void )
{
  return htsmsg_get_str(config, "language");
}

int config_set_language ( const char *lang )
{
  return _config_set_str("language", lang);
}

const char *config_get_muxconfpath ( void )
{
  return htsmsg_get_str(config, "muxconfpath");
}

int config_set_muxconfpath ( const char *path )
{
  return _config_set_str("muxconfpath", path);
}
