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
#include "access.h"
#include "htsbuf.h"
#include "spawn.h"
#include "lock.h"
#include "profile.h"
#include "avahi.h"
#include "url.h"
#include "satip/server.h"
#include "channels.h"
#include "input/mpegts/scanfile.h"

#include <netinet/ip.h>
#define COMPAT_IPTOS
#include "compat.h"

static void config_muxconfpath_notify ( void *o, const char *lang );

void tvh_str_set(char **strp, const char *src);
int tvh_str_update(char **strp, const char *src);

/* *************************************************************************
 * Global data
 * ************************************************************************/

struct config config;
static char config_lock[PATH_MAX];
static int config_lock_fd;
static int config_scanfile_ok;

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

  if (hts_settings_makedirs(dst))
    return;

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
      htsmsg_add_u32(m, "psip", 1);
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
    htsmsg_destroy(c);
  }
  if ((c = hts_settings_load("capmt")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      hts_settings_remove("capmt/%s", f->hmf_name);
      hts_settings_save(e, "caclient/%s", f->hmf_name);
    }
    htsmsg_destroy(c);
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
    htsmsg_destroy(c);
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
    htsmsg_destroy(c);
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
    htsmsg_destroy(c);
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
    htsmsg_destroy(c);
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
    htsmsg_destroy(c);
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
  htsmsg_destroy(c);
}

static void
config_migrate_v20_helper ( htsmsg_t *e, const char *fname )
{
  htsmsg_t *l;
  const char *str;
  char *p;

  if ((str = htsmsg_get_str(e, fname)) != NULL) {
    p = strdup(str);
    htsmsg_delete_field(e, fname);
    l = htsmsg_create_list();
    htsmsg_add_str(l, NULL, p);
    htsmsg_add_msg(e, fname, l);
    free(p);
  }
}

static void
config_migrate_v20 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_migrate_v20_helper(e, "profile");
      config_migrate_v20_helper(e, "dvr_config");
      config_migrate_v20_helper(e, "channel_tag");
      hts_settings_save(e, "accesscontrol/%s", f->hmf_name);
    }
    htsmsg_destroy(c);
  }
}

/*
 * v20 -> v21 : epggrab changes
 */
static void
config_migrate_v21 ( void )
{
  htsmsg_t *c, *m, *e, *a;
  htsmsg_field_t *f;
  const char *str;
  int64_t s64;

  if ((c = hts_settings_load_r(1, "epggrab/config")) != NULL) {
    str = htsmsg_get_str(c, "module");
    m = htsmsg_get_map(c, "mod_enabled");
    e = htsmsg_create_map();
    if (m) {
      HTSMSG_FOREACH(f, m) {
        s64 = 0;
        htsmsg_field_get_s64(f, &s64);
        if ((s64 || !strcmp(str ?: "", f->hmf_name)) &&
            f->hmf_name && f->hmf_name[0]) {
          a = htsmsg_create_map();
          htsmsg_add_bool(a, "enabled", 1);
          htsmsg_add_msg(e, f->hmf_name, a);
        }
      }
    }
    htsmsg_delete_field(c, "mod_enabled");
    htsmsg_delete_field(c, "module");
    htsmsg_add_msg(c, "modules", e);
    hts_settings_save(c, "epggrab/config");
    htsmsg_destroy(c);
  }
}

/*
 * v21 -> v22 : epggrab missing changes
 */
static void
config_migrate_v22 ( void )
{
  htsmsg_t *c;
  uint32_t u32;

  if ((c = hts_settings_load("epggrab/config")) != NULL) {
    if (htsmsg_get_u32(c, "epgdb_periodicsave", &u32) == 0)
      htsmsg_set_u32(c, "epgdb_periodicsave", (u32 + 3600 - 1) / 3600);
    hts_settings_save(c, "epggrab/config");
    htsmsg_destroy(c);
  }
  if ((c = hts_settings_load("timeshift/config")) != NULL) {
    if (htsmsg_get_u32(c, "max_period", &u32) == 0)
      htsmsg_set_u32(c, "max_period", (u32 + 60 - 1) / 60);
    hts_settings_save(c, "timeshift/config");
    htsmsg_destroy(c);
  }
}

/*
 * v21 -> v23 : epggrab xmltv/pyepg channels
 */
static void
config_migrate_v23_one ( const char *modname )
{
  htsmsg_t *c, *m, *n;
  htsmsg_field_t *f;
  uint32_t maj, min;
  int64_t num;
  tvh_uuid_t u;

  if ((c = hts_settings_load_r(1, "epggrab/%s/channels", modname)) != NULL) {
    HTSMSG_FOREACH(f, c) {
      m = htsmsg_field_get_map(f);
      if (m == NULL) continue;
      n = htsmsg_copy(m);
      htsmsg_add_str(n, "id", f->hmf_name);
      maj = htsmsg_get_u32_or_default(m, "major", 0);
      min = htsmsg_get_u32_or_default(m, "minor", 0);
      num = (maj * CHANNEL_SPLIT) + min;
      if (num > 0)
        htsmsg_add_s64(n, "lcn", num);
      htsmsg_delete_field(n, "major");
      htsmsg_delete_field(n, "minor");
      uuid_init_hex(&u, NULL);
      hts_settings_remove("epggrab/%s/channels/%s", modname, f->hmf_name);
      hts_settings_save(n, "epggrab/%s/channels/%s", modname, u.hex);
      htsmsg_destroy(n);
    }
    htsmsg_destroy(c);
  }
}

static void
config_migrate_v23 ( void )
{
  config_migrate_v23_one("xmltv");
  config_migrate_v23_one("pyepg");
}

static void
config_migrate_v24_helper ( const char **list, htsmsg_t *e, const char *name )
{
  htsmsg_t *l = htsmsg_create_list();
  const char **p = list;
  if (!strcmp(name, "dvr") && !htsmsg_get_bool_or_default(e, "failed_dvr", 0))
    htsmsg_add_str(l, NULL, "failed");
  for (p = list; *p; p += 2)
    if (htsmsg_get_bool_or_default(e, p[0], 0))
      htsmsg_add_str(l, NULL, p[1]);
  for (p = list; *p; p += 2)
    htsmsg_delete_field(e, p[0]);
  htsmsg_add_msg(e, name, l);
}

static void
config_migrate_v24 ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  static const char *streaming_list[] = {
    "streaming", "basic",
    "adv_streaming", "advanced",
    "htsp_streaming", "htsp",
    NULL
  };
  static const char *dvr_list[] = {
    "dvr", "basic",
    "htsp_dvr", "htsp",
    "all_dvr", "all",
    "all_rw_dvr", "all_rw",
    "failed_dvr", "failed",
    NULL
  };
  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      config_migrate_v24_helper(streaming_list, e, "streaming");
      config_migrate_v24_helper(dvr_list, e, "dvr");
      hts_settings_save(e, "accesscontrol/%s", f->hmf_name);
    }
    htsmsg_destroy(c);
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
    "--exclude", "timeshift/buffer",
    ".", NULL
  };
  const char *root = hts_settings_get_root();
  char errtxt[128];
  const char **arg;
  pid_t pid;
  int code;

  assert(root);

  tvhinfo(LS_CONFIG, "backup: migrating config from %s (running %s)",
                    oldver, tvheadend_version);

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    tvherror(LS_CONFIG, "unable to get the current working directory");
    goto fatal;
  }

  if (!access("/bin/tar", X_OK))
    argv[0] = "/bin/tar";
  else if (!access("/usr/bin/tar", X_OK))
    argv[0] = "/usr/bin/tar";
  else if (!access("/usr/local/bin/tar", X_OK))
    argv[0] = "/usr/local/bin/tar";
  else {
    tvherror(LS_CONFIG, "unable to find tar program");
    goto fatal;
  }

  snprintf(outfile, sizeof(outfile), "%s/backup", root);
  if (makedirs(LS_CONFIG, outfile, 0700, 1, -1, -1))
    goto fatal;
  if (chdir(root)) {
    tvherror(LS_CONFIG, "unable to find directory '%s'", root);
    goto fatal;
  }

  snprintf(outfile, sizeof(outfile), "%s/backup/%s.tar.bz2",
                                     root, oldver);
  tvhinfo(LS_CONFIG, "backup: running, output file %s", outfile);

  if (spawnv(argv[0], (void *)argv, &pid, 1, 1)) {
    code = -ENOENT;
  } else {
    while ((code = spawn_reap(pid, errtxt, sizeof(errtxt))) == -EAGAIN)
      tvh_safe_usleep(20000);
    if (code == -ECHILD)
      code = 0;
    tvhinfo(LS_CONFIG, "backup: completed");
  }

  if (code) {
    htsbuf_queue_t q;
    char *s;
    htsbuf_queue_init(&q, 0);
    for (arg = argv; *arg; arg++) {
      htsbuf_append_str(&q, *arg);
      if (arg[1])
        htsbuf_append(&q, " ", 1);
    }
    s = htsbuf_to_string(&q);
    tvherror(LS_CONFIG, "command '%s' returned error code %d", s, code);
    tvherror(LS_CONFIG, "executed in directory '%s'", root);
    tvherror(LS_CONFIG, "please, do not report this as an error, you may use --nobackup option");
    tvherror(LS_CONFIG, "... or run the above command in the printed directory");
    tvherror(LS_CONFIG, "... using the same user/group as for the tvheadend executable");
    tvherror(LS_CONFIG, "... to check the reason for the unfinished backup");
    free(s);
    htsbuf_queue_flush(&q);
    goto fatal;
  }

  if (chdir(cwd)) {
    tvherror(LS_CONFIG, "unable to change directory to '%s'", cwd);
    goto fatal;
  }
  return;

fatal:
  tvherror(LS_CONFIG, "backup: fatal error");
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
  config_migrate_v20,
  config_migrate_v21,
  config_migrate_v22,
  config_migrate_v23,
  config_migrate_v24
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
  v = config.version;
  s = config.full_version ?: "unknown";

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
    tvhinfo(LS_CONFIG, "migrating config from v%d to v%d", v, v+1);
    config_migrate_table[v]();
  }

  /* Update */
update:
  config.version = v;
  tvh_str_set(&config.full_version, tvheadend_version);
  idnode_changed(&config.idnode);
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
      tvherror(LS_START, "filename %s/%s/%s is invalid", hts_settings_get_root(), dir, f->hmf_name);
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
  htsmsg_field_t *f;
  const char *s;

  memset(&config, 0, sizeof(config));
  config.idnode.in_class = &config_class;
  config.ui_quicktips = 1;
  config.digest = 1;
  config.proxy = 0;
  config.realm = strdup("tvheadend");
  config.info_area = strdup("login,storage,time");
  config.cookie_expires = 7;
  config.dscp = -1;
  config.descrambler_buffer = 9000;
  config.epg_compress = 1;
  config.epg_cut_window = 5*60;
  config.epg_update_window = 24*3600;
  config_scanfile_ok = 0;
  config.theme_ui = strdup("blue");

  idclass_register(&config_class);

  satip_server_boot();

  /* Generate default */
  if (!path) {
    const char *homedir = getenv("HOME");
    if (homedir == NULL) {
      tvherror(LS_START, "environment variable HOME is not set");
      exit(EXIT_FAILURE);
    }
    snprintf(buf, sizeof(buf), "%s/.hts/tvheadend", homedir);
    path = buf;
  }

  /* Ensure directory exists */
  if (stat(path, &st)) {
    config_newcfg = 1;
    if (makedirs(LS_CONFIG, path, 0700, 1, gid, uid)) {
      tvhwarn(LS_START, "failed to create settings directory %s,"
                       " settings will not be saved", path);
      return;
    }
  }

  /* And is usable */
  else if (access(path, R_OK | W_OK)) {
    tvhwarn(LS_START, "configuration path %s is not r/w"
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
    tvhwarn(LS_CONFIG, "unable to chown lock file %s UID:%d GID:%d", config_lock, uid, gid);

  /* Load global settings */
  config2 = hts_settings_load("config");
  if (!config2) {
    tvhlog(LOG_DEBUG, LS_CONFIG, "no configuration, loading defaults");
    config.wizard = strdup("hello");
    config_newcfg = 1;
  } else {
    f = htsmsg_field_find(config2, "language");
    if (f && f->hmf_type == HMF_STR) {
      s = htsmsg_get_str(config2, "language");
      if (s) {
        htsmsg_t *m = htsmsg_csv_2_list(s, ',');
        htsmsg_delete_field(config2, "language");
        htsmsg_add_msg(config2, "language", m);
      }
    }
    config.version = htsmsg_get_u32_or_default(config2, "config", 0);
    s = htsmsg_get_str(config2, "full_version");
    if (s)
      config.full_version = strdup(s);
    idnode_load(&config.idnode, config2);
#if ENABLE_SATIP_SERVER
    idnode_load(&satip_server_conf.idnode, config2);
#endif
  }
  htsmsg_destroy(config2);
  if (config.server_name == NULL || config.server_name[0] == '\0')
    config.server_name = strdup("Tvheadend");
  if (config.realm == NULL || config.realm[0] == '\0')
    config.realm = strdup("tvheadend");
  if (config.http_server_name == NULL || config.http_server_name[0] == '\0')
    config.http_server_name = strdup("HTS/tvheadend");
  if (!config_scanfile_ok)
    config_muxconfpath_notify(&config.idnode, NULL);
}

void
config_init ( int backup )
{
  const char *path = hts_settings_get_root();

  if (path == NULL || access(path, R_OK | W_OK)) {
    tvhwarn(LS_START, "configuration path %s is not r/w"
                     " for UID:%d GID:%d [e=%s],"
                     " settings will not be saved",
            path, getuid(), getgid(), strerror(errno));
    return;
  }

  /* Store version number */
  if (config_newcfg) {
    config.version = ARRAY_SIZE(config_migrate_table);
    tvh_str_set(&config.full_version, tvheadend_version);
    tvh_str_set(&config.server_name, "Tvheadend");
    tvh_str_set(&config.realm, "tvheadend");
    tvh_str_set(&config.http_server_name, "HTS/tvheadend");
    idnode_changed(&config.idnode);
  
  /* Perform migrations */
  } else {
    if (config_migrate(backup))
      config_check();
  }
  tvhinfo(LS_CONFIG, "loaded");
}

void config_done ( void )
{
  /* note: tvhlog is inactive !!! */
  free(config.wizard);
  free(config.full_version);
  free(config.http_server_name);
  free(config.server_name);
  free(config.language);
  free(config.language_ui);
  free(config.theme_ui);
  free(config.realm);
  free(config.info_area);
  free(config.muxconf_path);
  free(config.chicon_path);
  free(config.picon_path);
  free(config.cors_origin);
  file_unlock(config_lock, config_lock_fd);
}

/* **************************************************************************
 * Config Class
 * *************************************************************************/

static htsmsg_t *
config_class_save(idnode_t *self, char *filename, size_t fsize)
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&config.idnode, c);
#if ENABLE_SATIP_SERVER
  idnode_save(&satip_server_conf.idnode, c);
#endif
  if (filename)
    snprintf(filename, fsize, "config");
  return c;
}

static int
config_class_cors_origin_set ( void *o, const void *v )
{
  const char *s = v;
  url_t u;

  while (s && *s && *s <= ' ')
    s++;
  if (*s == '*') {
    prop_sbuf[0] = '*';
    prop_sbuf[1] = '\0';
  } else {
    urlinit(&u);
    if (urlparse(s, &u))
      goto wrong;
    if (u.scheme && (!strcmp(u.scheme, "http") || !strcmp(u.scheme, "https")) && u.host) {
      if (u.port)
        snprintf(prop_sbuf, PROP_SBUF_LEN, "%s://%s:%d", u.scheme, u.host, u.port);
      else
        snprintf(prop_sbuf, PROP_SBUF_LEN, "%s://%s", u.scheme, u.host);
    } else {
wrong:
      prop_sbuf[0] = '\0';
    }
    urlreset(&u);
  }
  if (strcmp(prop_sbuf, config.cors_origin ?: "")) {
    free(config.cors_origin);
    config.cors_origin = strdup(prop_sbuf);
    return 1;
  }
  return 0;
}

static const void *
config_class_language_get ( void *o )
{
  return htsmsg_csv_2_list(config.language, ',');
}

static int
config_class_language_set ( void *o, const void *v )
{
  char *s = htsmsg_list_2_csv((htsmsg_t *)v, ',', 3);
  if (strcmp(s ?: "", config.language ?: "")) {
    free(config.language);
    config.language = s;
    return 1;
  }
  if (s)
    free(s);
  return 0;
}

static htsmsg_t *
config_class_language_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "language/list");
  return m;
}

static const void *
config_class_info_area_get ( void *o )
{
  return htsmsg_csv_2_list(config.info_area, ',');
}

static int
config_class_info_area_set ( void *o, const void *v )
{
  char *s = htsmsg_list_2_csv((htsmsg_t *)v, ',', 3);
  if (strcmp(s ?: "", config.info_area ?: "")) {
    free(config.info_area);
    config.info_area = s;
    return 1;
  }
  if (s)
    free(s);
  return 0;
}

static void
config_class_info_area_list1 ( htsmsg_t *m, const char *key,
                               const char *val, const char *lang )
{
  htsmsg_t *e = htsmsg_create_key_val(key, tvh_gettext_lang(lang, val));
  htsmsg_add_msg(m, NULL, e);
}

static htsmsg_t *
config_class_info_area_list ( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_list();
  config_class_info_area_list1(m, "login", N_("Login/Logout"), lang);
  config_class_info_area_list1(m, "storage", N_("Storage space"), lang);
  config_class_info_area_list1(m, "time", N_("Time"), lang);
  return m;
}

static htsmsg_t *
config_class_dscp_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Default"), -1 },
    { N_("CS0"),  IPTOS_CLASS_CS0 },
    { N_("CS1"),  IPTOS_CLASS_CS1 },
    { N_("AF11"), IPTOS_DSCP_AF11 },
    { N_("AF12"), IPTOS_DSCP_AF12 },
    { N_("AF13"), IPTOS_DSCP_AF13 },
    { N_("CS2"),  IPTOS_CLASS_CS2 },
    { N_("AF21"), IPTOS_DSCP_AF21 },
    { N_("AF22"), IPTOS_DSCP_AF22 },
    { N_("AF23"), IPTOS_DSCP_AF23 },
    { N_("CS3"),  IPTOS_CLASS_CS3 },
    { N_("AF31"), IPTOS_DSCP_AF31 },
    { N_("AF32"), IPTOS_DSCP_AF32 },
    { N_("AF33"), IPTOS_DSCP_AF33 },
    { N_("CS4"),  IPTOS_CLASS_CS4 },
    { N_("AF41"), IPTOS_DSCP_AF41 },
    { N_("AF42"), IPTOS_DSCP_AF42 },
    { N_("AF43"), IPTOS_DSCP_AF43 },
    { N_("CS5"),  IPTOS_CLASS_CS5 },
    { N_("EF"),   IPTOS_DSCP_EF },
    { N_("CS6"),  IPTOS_CLASS_CS6 },
    { N_("CS7"),  IPTOS_CLASS_CS7 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
config_class_uilevel ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Basic"),    UILEVEL_BASIC },
    { N_("Advanced"), UILEVEL_ADVANCED },
    { N_("Expert"),   UILEVEL_EXPERT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
config_class_chiconscheme_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("No scheme"),           CHICON_NONE },
    { N_("All lower-case"),      CHICON_LOWERCASE },
    { N_("Service name picons"), CHICON_SVCNAME },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
config_class_piconscheme_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Standard"),                PICON_STANDARD },
    { N_("Force service type to 1"), PICON_ISVCTYPE },
  };
  return strtab2htsmsg(tab, 1, lang);
}

#if ENABLE_MPEGTS_DVB
static void
config_muxconfpath_notify_cb(void *opaque, int disarmed)
{
  char *muxconf_path = opaque;
  if (disarmed) {
    free(muxconf_path);
    return;
  }
  tvhinfo(LS_CONFIG, "scanfile (re)initialization with path %s", muxconf_path ?: "<none>");
  scanfile_init(muxconf_path, 1);
  free(muxconf_path);
}
#endif

static void
config_muxconfpath_notify ( void *o, const char *lang )
{
#if ENABLE_MPEGTS_DVB
  config_scanfile_ok = 1;
  tasklet_arm_alloc(config_muxconfpath_notify_cb,
                    config.muxconf_path ? strdup(config.muxconf_path) : NULL);
#endif
}


CLASS_DOC(config)
PROP_DOC(config_channelicon_path)
PROP_DOC(config_channelname_scheme)
PROP_DOC(config_picon_path)
PROP_DOC(config_picon_servicetype)
PROP_DOC(viewlevel_config)
PROP_DOC(themes)

const idclass_t config_class = {
  .ic_snode      = &config.idnode,
  .ic_class      = "config",
  .ic_caption    = N_("Configuration - Base"),
  .ic_event      = "config",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_doc        = tvh_doc_config_class,
  .ic_save       = config_class_save,
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("Server settings"),
         .number = 1,
      },
      {
         .name   = N_("Web interface settings"),
         .number = 2,
      },
      {
         .name   = N_("EPG settings"),
         .number = 3,
      },
      {
         .name   = N_("Channel icon/Picon settings"),
         .number = 4,
      },
      {
         .name   = N_("HTTP server settings"),
         .number = 5,
      },
      {
         .name   = N_("System time update"),
         .number = 6,
      },
      {
         .name   = N_("Misc settings"),
         .number = 7,
      },
      {}
  },
  .ic_properties = (const property_t[]){
    {
      .type   = PT_STR,
      .id     = "server_name",
      .name   = N_("Tvheadend server name"),
      .desc   = N_("Set the name of the server so you can distinguish "
                   "multiple instances apart."),
      .off    = offsetof(config_t, server_name),
      .group  = 1
    },
    {
      .type   = PT_U32,
      .id     = "version",
      .name   = N_("Configuration version"),
      .desc   = N_("The current configuration version."),
      .off    = offsetof(config_t, version),
      .opts   = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
      .group  = 1
    },
    {
      .type   = PT_STR,
      .id     = "full_version",
      .name   = N_("Last updated from"),
      .desc   = N_("The version of Tvheadend that last updated the "
                   "config."),
      .off    = offsetof(config_t, full_version),
      .opts   = PO_RDONLY | PO_HIDDEN | PO_EXPERT,
      .group  = 1
    },
    {
      .type   = PT_STR,
      .id     = "language_ui",
      .name   = N_("Default language"),
      .desc   = N_("The default language to use if the user "
                   " language isn't set (in the Access Entries tab)."),
      .list   = language_get_ui_list,
      .off    = offsetof(config_t, language_ui),
      .group  = 2
    },
    {
      .type   = PT_STR,
      .id     = "theme_ui",
      .name   = N_("Theme"),
      .desc   = N_("The default web interface theme, if a user-specific "
                   "one isn't set (in the Access Entries tab)."),
      .doc    = prop_doc_themes,
      .list   = theme_get_ui_list,
      .off    = offsetof(config_t, theme_ui),
      .opts   = PO_DOC_NLIST,
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "ui_quicktips",
      .name   = N_("Tooltips"),
      .desc   = N_("Enable/Disable web interface mouse-over tooltips."),
      .off    = offsetof(config_t, ui_quicktips),
      .opts   = PO_ADVANCED,
      .group  = 2
    },
    {
      .type   = PT_INT,
      .id     = "uilevel",
      .name   = N_("Default view level"),
      .desc   = N_("The default interface view level (next to the "
                   "Help button)."),
      .doc    = prop_doc_viewlevel_config,
      .off    = offsetof(config_t, uilevel),
      .list   = config_class_uilevel,
      .opts   = PO_DOC_NLIST,
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "uilevel_nochange",
      .name   = N_("Persistent view level"),
      .desc   = N_("Prevent users from overriding the view level "
                   "setting. This option shows or hides the View level "
                   "drop-down (next to the Help button)."),
      .off    = offsetof(config_t, uilevel_nochange),
      .opts   = PO_ADVANCED,
      .group  = 2
    },
    {
      .type   = PT_BOOL,
      .id     = "caclient_ui",
      .name   = N_("Conditional Access (for advanced view level)"),
      .desc   = N_("Enable/Disable the CAs (conditional accesses) tab "
                   "for the advanced view level. By default, it's "
                   "visible only to the Expert level."),
      .off    = offsetof(config_t, caclient_ui),
      .opts   = PO_ADVANCED,
      .group  = 2
    },
    {
      .type   = PT_STR,
      .islist = 1,
      .id     = "info_area",
      .name   = N_("Information area"),
      .desc   = N_("Show, hide and sort the various details that "
                   "appear on the interface next to the About tab."),
      .set    = config_class_info_area_set,
      .get    = config_class_info_area_get,
      .list   = config_class_info_area_list,
      .opts   = PO_LORDER | PO_ADVANCED | PO_DOC_NLIST,
      .group  = 2
    },
    {
      .type   = PT_STR,
      .islist = 1,
      .id     = "language",
      .name   = N_("Default language(s)"),
      .desc   = N_("Select the list of languages (in order of "
                   "priority) to be used for supplying EPG information "
                   "to clients that don't provide their own "
                   "configuration."),
      .set    = config_class_language_set,
      .get    = config_class_language_get,
      .list   = config_class_language_list,
      .opts   = PO_LORDER,
      .group  = 3
    },
#if ENABLE_ZLIB
    {
      .type   = PT_BOOL,
      .id     = "epg_compress",
      .name   = N_("Compress EPG database"),
      .desc   = N_("Compress the EPG database to reduce disk I/O "
                   "and space."),
      .off    = offsetof(config_t, epg_compress),
      .opts   = PO_EXPERT,
      .def.i  = 1,
      .group  = 3
    },
#endif
    {
      .type   = PT_U32,
      .id     = "epg_cutwindow",
      .name   = N_("EPG overlap cut"),
      .desc   = N_("The time window to cut the stop time from the overlapped event in seconds."),
      .off    = offsetof(config_t, epg_cut_window),
      .opts   = PO_EXPERT,
      .group  = 3
    },
    {
      .type   = PT_U32,
      .id     = "epg_window",
      .name   = N_("EPG update window"),
      .desc   = N_("Maximum allowed difference between event start time when "
                   "the EPG event is changed (in seconds)."),
      .off    = offsetof(config_t, epg_update_window),
      .opts   = PO_EXPERT,
      .group  = 3
    },
    {
      .type   = PT_BOOL,
      .id     = "prefer_picon",
      .name   = N_("Prefer picons over channel icons"),
      .desc   = N_("If both a picon and a channel-specific "
      "(e.g. channelname.jpg) icon are defined, prefer the picon."),
      .off    = offsetof(config_t, prefer_picon),
      .opts   = PO_ADVANCED,
      .group  = 4,
    },
    {
      .type   = PT_STR,
      .id     = "chiconpath",
      .name   = N_("Channel icon path"),
      .desc   = N_("Path to an icon for this channel. This can be "
                   "named however you wish, as either a local "
                   "(file://) or remote (http://) image. "
                   "See Help for more infomation."),
      .off    = offsetof(config_t, chicon_path),
      .doc    = prop_doc_config_channelicon_path,
      .opts   = PO_ADVANCED,
      .group  = 4,
    },
    {
      .type   = PT_INT,
      .id     = "chiconscheme",
      .name   = N_("Channel icon name scheme"),
      .desc   = N_("Scheme to generate the channel icon names "
                   "(all lower-case, service name picons etc.)."),
      .list   = config_class_chiconscheme_list,
      .doc    = prop_doc_config_channelname_scheme,
      .off    = offsetof(config_t, chicon_scheme),
      .opts   = PO_ADVANCED | PO_DOC_NLIST,
      .group  = 4,
    },
    {
      .type   = PT_STR,
      .id     = "piconpath",
      .name   = N_("Picon path"),
      .desc   = N_("Path to a directory (folder) containing your picon "
                   "collection. See Help for more detailed "
                   "information."),
      .doc    = prop_doc_config_picon_path,
      .off    = offsetof(config_t, picon_path),
      .opts   = PO_ADVANCED,
      .group  = 4,
    },
    {
      .type   = PT_INT,
      .id     = "piconscheme",
      .name   = N_("Picon name scheme"),
      .desc   = N_("Select scheme to generate the picon names "
                   "(standard, force service type to 1)"),
      .list   = config_class_piconscheme_list,
      .doc    = prop_doc_config_picon_servicetype,
      .off    = offsetof(config_t, picon_scheme),
      .opts   = PO_ADVANCED | PO_DOC_NLIST,
      .group  = 4,
    },
    {
      .type   = PT_STR,
      .id     = "http_server_name",
      .name   = N_("Server name"),
      .desc   = N_("The server name for 'Server:' HTTP headers."),
      .off    = offsetof(config_t, http_server_name),
      .opts   = PO_HIDDEN | PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_STR,
      .id     = "http_realm_name",
      .name   = N_("Realm name"),
      .desc   = N_("The realm name for HTTP authorization."),
      .off    = offsetof(config_t, realm),
      .opts   = PO_HIDDEN | PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_BOOL,
      .id     = "digest",
      .name   = N_("Digest authentication"),
      .desc   = N_("Digest access authentication is intended as a security trade-off. "
                   "It is intended to replace unencrypted HTTP basic access authentication. "
                   "This option should be enabled for standard usage."),
      .off    = offsetof(config_t, digest),
      .opts   = PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_U32,
      .intextra = INTEXTRA_RANGE(1, 0x7ff, 1),
      .id     = "cookie_expires",
      .name   = N_("Cookie expiration (days)"),
      .desc   = N_("The number of days cookies set by Tvheadend should "
                   "expire."),
      .off    = offsetof(config_t, cookie_expires),
      .opts   = PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_BOOL,
      .id     = "proxy",
      .name   = N_("PROXY protocol & X-Forwarded-For"),
      .desc   = N_("PROXY protocol is an extension for support incoming "
                   "TCP connections from a remote server (like a firewall) "
                   "sending the original IP address of the client. "
                   "The HTTP header 'X-Forwarded-For' do the same with "
                   "HTTP connections. Both enable tunneled connections."
                   "This option should be disabled for standard usage."),
      .off    = offsetof(config_t, proxy),
      .opts   = PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_STR,
      .id     = "cors_origin",
      .name   = N_("CORS origin"),
      .desc   = N_("HTTP CORS (cross-origin resource sharing) origin. This "
                   "option is usually set when Tvheadend is behind a "
                   "proxy. Enter a domain (or IP) to allow "
                   "cross-domain requests."),
      .set    = config_class_cors_origin_set,
      .off    = offsetof(config_t, cors_origin),
      .opts   = PO_EXPERT,
      .group  = 5
    },
    {
      .type   = PT_BOOL,
      .id     = "tvhtime_update_enabled",
      .name   = N_("Update time"),
      .desc   = N_("Enable system time updates. This will only work if "
                   "the user running Tvheadend has rights to update "
                   "the system clock (normally only root)."),
      .off    = offsetof(config_t, tvhtime_update_enabled),
      .opts   = PO_EXPERT,
      .group  = 6,
    },
    {
      .type   = PT_BOOL,
      .id     = "tvhtime_ntp_enabled",
      .name   = N_("Enable NTP driver"),
      .desc   = N_("This will create an NTP driver (using shmem "
                   "interface) that you can feed into ntpd. This can "
                   "be run without root privileges, but generally the "
                   "performance is not that great."),
      .off    = offsetof(config_t, tvhtime_ntp_enabled),
      .opts   = PO_EXPERT,
      .group  = 6,
    },
    {
      .type   = PT_U32,
      .id     = "tvhtime_tolerance",
      .name   = N_("Update tolerance (ms)"),
      .desc   = N_("Only update the system clock (doesn't affect NTP "
                   "driver) if the delta between the system clock and "
                   "DVB time is greater than this. This can help stop "
                   "excessive oscillations on the system clock."),
      .off    = offsetof(config_t, tvhtime_tolerance),
      .opts   = PO_EXPERT,
      .group  = 6,
    },
    {
      .type   = PT_INT,
      .id     = "dscp",
      .name   = N_("DSCP/TOS for streaming"),
      .desc   = N_("Differentiated Services Code Point / Type of "
                   "Service: Set the service class Tvheadend sends "
                   "with each packet. Depending on the option selected "
                   "this tells your router the prority in which to "
                   "give packets sent from Tvheadend, this option does "
                   "not usually need changing. See "
                   "https://en.wikipedia.org/wiki/"
                   "Differentiated_services for more information. "),
      .off    = offsetof(config_t, dscp),
      .list   = config_class_dscp_list,
      .opts   = PO_EXPERT | PO_DOC_NLIST,
      .group  = 7
    },
    {
      .type   = PT_U32,
      .id     = "descrambler_buffer",
      .name   = N_("Descrambler buffer (TS packets)"),
      .desc   = N_("The number of MPEG-TS packets Tvheadend buffers in case "
                   "there is a delay receiving CA keys. "),
      .off    = offsetof(config_t, descrambler_buffer),
      .opts   = PO_EXPERT,
      .group  = 7
    },
    {
      .type   = PT_BOOL,
      .id     = "parser_backlog",
      .name   = N_("Packet backlog"),
      .desc   = N_("Send previous stream frames to upper layers "
                   "(before frame start is signalled in the stream). "
                   "It may cause issues with some clients / players."),
      .off    = offsetof(config_t, parser_backlog),
      .opts   = PO_EXPERT,
      .group  = 7
    },
    {
      .type   = PT_STR,
      .id     = "muxconfpath",
      .name   = N_("DVB scan files path"),
      .desc   = N_("Select the path to use for DVB scan configuration "
                   "files. Typically dvb-apps stores these in "
                   "/usr/share/dvb/. Leave blank to use the "
                   "internal file set."),
      .off    = offsetof(config_t, muxconf_path),
      .notify = config_muxconfpath_notify,
      .opts   = PO_ADVANCED,
      .group  = 7
    },
    {
      .type   = PT_BOOL,
      .id     = "hbbtv",
      .name   = N_("Parse HbbTV info"),
      .desc   = N_("Parse HbbTV information from services."),
      .off    = offsetof(config_t, hbbtv),
      .group  = 7
    },
    {
      .type   = PT_STR,
      .id     = "wizard",
      .name   = "Wizard level", /* untranslated */
      .off    = offsetof(config_t, wizard),
      .opts   = PO_NOUI,
    },
    {}
  }
};

/* **************************************************************************
 * Access routines
 * *************************************************************************/

const char *config_get_server_name ( void )
{
  return config.server_name ?: "Tvheadend";
}

const char *config_get_language ( void )
{
  const char *s = config.language;
  if (s == NULL || *s == '\0')
    return "eng";
  return s;
}

const char *config_get_language_ui ( void )
{
  const char *s = config.language_ui;
  if (s == NULL || *s == '\0')
    return NULL;
  return s;
}
