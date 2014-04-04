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
 * Helper to add service to channel
 */
static void
config_migrate_chn_add_svc
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
 * v0 -> v1 : 3.4 to initial 4.0)
 *
 * Strictly speaking there were earlier versions than this, but most people
 * using early versions of 4.0 would already have been on this version.
 */
static void
config_migrate_v1 ( void )
{
  uuid_t netu, muxu, svcu, chnu;
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
        config_migrate_chn_add_svc(channels, str, svcu.hex);
      }
      hts_settings_save(m, "input/iptv/networks/%s/muxes/%s/services/%s",
                        netu.hex, muxu.hex, svcu.hex);
      htsmsg_destroy(m);
    }

    htsmsg_destroy(c);
  }

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
  uuid_t u;
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

    /* Move muxes */
    hts_settings_buildpath(src, sizeof(src),
                           "input/iptv/muxes");
    hts_settings_buildpath(dst, sizeof(dst),
                           "input/iptv/networks/%s/muxes", u.hex);
    rename(src, dst);
  }
}

/*
 * Migration table
 */
static const config_migrate_t config_migrate_table[] = {
  config_migrate_v1,
  config_migrate_v2
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
