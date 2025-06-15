/*
 *  Digital Video Recorder
 *  Copyright (C) 2008 Andreas Öman
 *  Copyright (C) 2014,2015 Jaroslav Kysela
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

#define _GNU_SOURCE
#include <sys/stat.h>
#include <strings.h>
#include <unistd.h>

#include "settings.h"

#include "tvheadend.h"
#include "dvr.h"
#include "htsp_server.h"
#include "streaming.h"
#include "intlconv.h"
#include "dbus.h"
#include "imagecache.h"
#include "access.h"

int dvr_iov_max;

struct dvr_config_list dvrconfigs;
static dvr_config_t *dvrdefaultconfig = NULL;

static void dvr_config_destroy(dvr_config_t *cfg, int delconf);
static void dvr_update_pathname_from_booleans(dvr_config_t *cfg);

/**
 * find a dvr config by name, return NULL if not found
 */
dvr_config_t *
dvr_config_find_by_name(const char *name)
{
  dvr_config_t *cfg;

  if (name == NULL)
    name = "";

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    if (cfg->dvr_enabled && !strcmp(name, cfg->dvr_config_name))
      return cfg;

  return NULL;
}

/**
 * find a dvr config by name, return the default config if not found
 */
dvr_config_t *
dvr_config_find_by_name_default(const char *name)
{
  dvr_config_t *cfg;

  if (dvrdefaultconfig == NULL)
    dvrdefaultconfig = dvr_config_find_by_name(NULL);

  if (dvrdefaultconfig == NULL) {
    cfg = dvr_config_create("", NULL, NULL);
    assert(cfg);
    idnode_changed(&cfg->dvr_id);
    dvrdefaultconfig = cfg;
  }

  if (tvh_str_default(name, NULL) == NULL)
    return dvrdefaultconfig;

  cfg = dvr_config_find_by_name(name);

  if (cfg == NULL) {
    if (tvh_str_default(name, NULL))
      tvhwarn(LS_DVR, "Configuration '%s' not found, using default", name);
    cfg = dvrdefaultconfig;
  } else if (!cfg->dvr_enabled) {
    tvhwarn(LS_DVR, "Configuration '%s' not enabled, using default", name);
    cfg = dvrdefaultconfig;
  }

  return cfg;
}

/*
 * find a dvr config by name using a filter list,
 * return the first config from list if name is not valid
 * return the default config if not found
 */
dvr_config_t *
dvr_config_find_by_list(htsmsg_t *uuids, const char *name)
{
  dvr_config_t *cfg, *res = NULL;
  htsmsg_field_t *f;
  const char *uuid, *uuid2;
  char ubuf[UUID_HEX_SIZE];

  cfg = dvr_config_find_by_uuid(name);
  if (!cfg)
    cfg  = dvr_config_find_by_name(name);
  uuid = cfg ? idnode_uuid_as_str(&cfg->dvr_id, ubuf) : "";
  if (uuids) {
    HTSMSG_FOREACH(f, uuids) {
      uuid2 = htsmsg_field_get_str(f) ?: "";
      if (strcmp(uuid, uuid2) == 0)
        return cfg;
      if (!res) {
        res = dvr_config_find_by_uuid(uuid2);
        if (res != NULL && !res->dvr_enabled)
          res = NULL;
      }
    }
  } else {
    if (cfg && cfg->dvr_enabled)
      res = cfg;
  }
  if (!res)
    res = dvr_config_find_by_name_default(NULL);
  return res;
}

/**
 *
 */
static int
dvr_charset_update(dvr_config_t *cfg, const char *charset)
{
  const char *s, *id;
  int change = strcmp(cfg->dvr_charset ?: "", charset ?: "");

  free(cfg->dvr_charset);
  free(cfg->dvr_charset_id);
  s = charset ? charset : intlconv_filesystem_charset();
  id = intlconv_charset_id(s, 1, 1);
  cfg->dvr_charset = s ? strdup(s) : NULL;
  cfg->dvr_charset_id = id ? strdup(id) : NULL;
  return change;
}

/**
 * create a new named dvr config; the caller is responsible
 * to avoid duplicates
 */
dvr_config_t *
dvr_config_create(const char *name, const char *uuid, htsmsg_t *conf)
{
  dvr_config_t *cfg;

  if (name == NULL)
    name = "";

  cfg = calloc(1, sizeof(dvr_config_t));
  LIST_INIT(&cfg->dvr_entries);
  LIST_INIT(&cfg->dvr_autorec_entries);
  LIST_INIT(&cfg->dvr_timerec_entries);
  LIST_INIT(&cfg->dvr_accesses);

  if (idnode_insert(&cfg->dvr_id, uuid, &dvr_config_class, 0)) {
    if (uuid)
      tvherror(LS_DVR, "invalid config uuid '%s'", uuid);
    free(cfg);
    return NULL;
  }

  cfg->dvr_enabled = 1;
  cfg->dvr_config_name = strdup(name);
  cfg->dvr_retention_days = DVR_RET_ONREMOVE;
  cfg->dvr_removal_days = DVR_RET_REM_FOREVER;
  cfg->dvr_clone = 1;
  cfg->dvr_tag_files = 1;
  cfg->dvr_create_scene_markers = 1;
  cfg->dvr_skip_commercials = 1;
  dvr_charset_update(cfg, intlconv_filesystem_charset());
  cfg->dvr_warm_time = 30;
  cfg->dvr_update_window = 24 * 3600;
  cfg->dvr_pathname = strdup("$t$n.$x");
  cfg->dvr_cleanup_threshold_free = 1000; // keep 1000 MiB of free space on disk by default
  cfg->dvr_cleanup_threshold_used = 0;    // disabled
  cfg->dvr_autorec_max_count = 50;
  cfg->dvr_format_tvmovies_subdir = strdup("tvmovies");
  cfg->dvr_format_tvshows_subdir = strdup("tvshows");
  cfg->dvr_autorec_dedup = DVR_AUTOREC_RECORD_ALL;

  /* Muxer config */
  cfg->dvr_muxcnf.m_cache  = MC_CACHE_SYSTEM;

  /* Default recording file and directory permissions */

  cfg->dvr_muxcnf.m_file_permissions      = 0664;
  cfg->dvr_muxcnf.m_directory_permissions = 0775;

  if (conf) {
    idnode_load(&cfg->dvr_id, conf);
    if (!htsmsg_field_find(conf, "pathname"))
      dvr_update_pathname_from_booleans(cfg);
    cfg->dvr_valid = 1;
  }

  tvhinfo(LS_DVR, "Creating new configuration '%s'", cfg->dvr_config_name);

  if (cfg->dvr_profile == NULL) {
    cfg->dvr_profile = profile_find_by_name("pass", NULL);
    assert(cfg->dvr_profile);
    LIST_INSERT_HEAD(&cfg->dvr_profile->pro_dvr_configs, cfg, profile_link);
  }

  if (dvr_config_is_default(cfg) && dvr_config_find_by_name(NULL)) {
    tvherror(LS_DVR, "Unable to create second default config, removing");
    LIST_INSERT_HEAD(&dvrconfigs, cfg, config_link);
    dvr_config_destroy(cfg, 0);
    cfg = NULL;
  }

  if (cfg) {
    LIST_INSERT_HEAD(&dvrconfigs, cfg, config_link);
    if (conf && dvr_config_is_default(cfg))
      cfg->dvr_enabled = 1;
  }

  return cfg;
}

/**
 * destroy a dvr config
 */
static void
dvr_config_destroy(dvr_config_t *cfg, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&cfg->dvr_id, delconf);

  if (delconf) {
    tvhinfo(LS_DVR, "Deleting configuration '%s'", cfg->dvr_config_name);
    hts_settings_remove("dvr/config/%s", idnode_uuid_as_str(&cfg->dvr_id, ubuf));
  }
  LIST_REMOVE(cfg, config_link);
  idnode_unlink(&cfg->dvr_id);

  if (cfg->dvr_profile) {
    LIST_REMOVE(cfg, profile_link);
    cfg->dvr_profile = NULL;
  }

  dvr_entry_destroy_by_config(cfg, delconf);
  access_destroy_by_dvr_config(cfg, delconf);
  autorec_destroy_by_config(cfg, delconf);
  timerec_destroy_by_config(cfg, delconf);

  free(cfg->dvr_pathname);
  free(cfg->dvr_charset_id);
  free(cfg->dvr_charset);
  free(cfg->dvr_storage);
  free(cfg->dvr_config_name);
  free(cfg->dvr_preproc);
  free(cfg->dvr_postproc);
  free(cfg->dvr_postremove);
  free(cfg->dvr_format_tvmovies_subdir);
  free(cfg->dvr_format_tvshows_subdir);
  free(cfg);
}

/**
 *
 */
static void
dvr_config_storage_check(dvr_config_t *cfg)
{
  char recordings_dir[] = "/var/lib/tvheadend/recordings";
  char home_dir[PATH_MAX + sizeof("/Videos")];
  char dvr_dir[PATH_MAX];
  char buf[PATH_MAX];
  uid_t uid = getuid();
  char *xdg_dir;
  struct stat st;

  if(cfg->dvr_storage != NULL && cfg->dvr_storage[0])
    return;

  /* Try to figure out a good place to put them videos */
  snprintf(home_dir, sizeof(home_dir), "%s/Videos", getenv("HOME"));
  xdg_dir = hts_settings_get_xdg_dir_with_fallback("VIDEOS", home_dir);
  if (xdg_dir != NULL) {
    if (stat(xdg_dir, &st) == 0) {
      if (S_ISLNK(st.st_mode)) {
        char xdg_dir_link[PATH_MAX - sizeof('\0')];

        if (readlink(xdg_dir, xdg_dir_link, sizeof(xdg_dir_link)) == -1)
          tvhwarn(LS_DVR, "symlink '%s' error: %s\n", xdg_dir, strerror(errno));
        else
          strncpy(dvr_dir, xdg_dir_link, sizeof(dvr_dir));
      } else if (S_ISDIR(st.st_mode)) {
        strncpy(dvr_dir, xdg_dir, sizeof(dvr_dir) - sizeof('\0'));
      }
    }
    free(xdg_dir);
  }

  if ((stat(recordings_dir, &st) == 0) && (st.st_uid == uid))
    cfg->dvr_storage = strndup(recordings_dir, sizeof(recordings_dir));
  else if((stat(dvr_dir, &st) == 0) && S_ISDIR(st.st_mode))
      cfg->dvr_storage = strndup(dvr_dir, PATH_MAX);
  else if(stat(home_dir, &st) == 0 && S_ISDIR(st.st_mode))
      cfg->dvr_storage = strndup(home_dir, sizeof(home_dir));
  else
      cfg->dvr_storage = strdup(getcwd(buf, sizeof(buf)));

  tvhwarn(LS_DVR,
          "Output directory for video recording is not yet configured "
          "for DVR configuration \"%s\". "
          "Defaulting to to \"%s\". "
          "This can be changed from the web user interface.",
          cfg->dvr_config_name, cfg->dvr_storage);
}

/**
 *
 */
static int
dvr_filename_index(dvr_config_t *cfg)
{
  char *str = cfg->dvr_pathname, *p;

  for (str = p = cfg->dvr_pathname; *p; p++) {
    if (*p == '\\')
      p++;
    else if (*p == '/')
      str = p;
  }

  return str - cfg->dvr_pathname;
}

/**
 *
 */
static int
dvr_match_fmtstr(dvr_config_t *cfg, const char *needle, int nodir)
{
  char *str = cfg->dvr_pathname, *p;
  const char *x;

  if (needle == NULL)
    return -1;

  if (nodir)
    str += dvr_filename_index(cfg);

  while (*str) {
    if (*str == '\\') {
      str++;
      if (*str)
        str++;
      continue;
    }
    for (p = str, x = needle; *p && *x; p++, x++)
      if (*p != *x)
        break;
    if (*x == '\0')
      return str - cfg->dvr_pathname;
    else
      str++;
  }
  return -1;
}

/**
 *
 */
static void
dvr_insert_fmtstr(dvr_config_t *cfg, int idx, const char *str)
{
  size_t t = strlen(cfg->dvr_pathname);
  size_t l = strlen(str);
  char *n = malloc(t + l + 1);
  memcpy(n, cfg->dvr_pathname, idx);
  memcpy(n + idx, str, l);
  memcpy(n + idx + l, cfg->dvr_pathname + idx, t - idx);
  n[t + l] = '\0';
  free(cfg->dvr_pathname);
  cfg->dvr_pathname = n;
}

/**
 *
 */
static void
dvr_insert_fmtstr_before_extension(dvr_config_t *cfg, const char *str)
{
  int idx = dvr_match_fmtstr(cfg, "$n", 1);
  idx = idx < 0 ? dvr_match_fmtstr(cfg, "$x", 1) : idx;
  if (idx < 0) {
    idx = strlen(cfg->dvr_pathname);
  } else {
    while (idx > 0) {
      if (cfg->dvr_pathname[idx - 1] != '.')
        break;
      idx--;
    }
  }
  dvr_insert_fmtstr(cfg, idx, str);
}

/**
 *
 */
static void
dvr_remove_fmtstr(dvr_config_t *cfg, int idx, int len)
{
  char *pathname = cfg->dvr_pathname;
  size_t l = strlen(pathname);
  memmove(pathname + idx, pathname + idx + len, l - idx - len);
  pathname[l - len] = '\0';
}

/**
 *
 */
static void
dvr_match_and_insert_or_remove(dvr_config_t *cfg, const char *str, int val, int idx)
{
  int i = dvr_match_fmtstr(cfg, str, idx < 0 ? 1 : 0);
  if (val) {
    if (i < 0) {
      if (idx < 0)
        dvr_insert_fmtstr_before_extension(cfg, str);
      else
        dvr_insert_fmtstr(cfg, idx, str);
    }
  } else {
    if (i >= 0) {
      if (idx < 0 && i >= dvr_filename_index(cfg))
        dvr_remove_fmtstr(cfg, i, strlen(str));
      else if (idx >= 0 && i < dvr_filename_index(cfg))
        dvr_remove_fmtstr(cfg, i, strlen(str));
    }
  }
}

/**
 *
 */
static void
dvr_update_pathname_safe(dvr_config_t *cfg)
{
  if (cfg->dvr_pathname[0] == '\0') {
    free(cfg->dvr_pathname);
    cfg->dvr_pathname = strdup("$t$n.$x");
  }

  if (strstr(cfg->dvr_pathname, "%n"))
    dvr_insert_fmtstr_before_extension(cfg, "%n");
}

/**
 *
 */
static void
dvr_update_pathname_from_fmtstr(dvr_config_t *cfg)
{
  if (cfg->dvr_pathname == NULL)
    return;

  dvr_update_pathname_safe(cfg);

  cfg->dvr_dir_per_day = dvr_match_fmtstr(cfg, "%F/", 0) >= 0;
  cfg->dvr_channel_dir = dvr_match_fmtstr(cfg, "$c/", 0) >= 0;
  cfg->dvr_title_dir   = dvr_match_fmtstr(cfg, "$t/", 0) >= 0;

  cfg->dvr_channel_in_title  = dvr_match_fmtstr(cfg, "$-c", 1) >= 0;
  cfg->dvr_date_in_title     = dvr_match_fmtstr(cfg, "%F", 1) >= 0;
  cfg->dvr_time_in_title     = dvr_match_fmtstr(cfg, "%R", 1) >= 0;
  cfg->dvr_episode_in_title  = dvr_match_fmtstr(cfg, "$-e", 1) >= 0;
  cfg->dvr_subtitle_in_title = dvr_match_fmtstr(cfg, "$.s", 1) >= 0;

  cfg->dvr_omit_title = dvr_match_fmtstr(cfg, "$t", 1) < 0;
}

/**
 *
 */
static void
dvr_update_pathname_from_booleans(dvr_config_t *cfg)
{
  int i;

  i = dvr_match_fmtstr(cfg, "$t", 1);
  if (cfg->dvr_omit_title) {
    if (i >= 0)
      dvr_remove_fmtstr(cfg, i, 2);
  } else if (i < 0) {
    i = dvr_match_fmtstr(cfg, "$n", 1);
    if (i >= 0)
      dvr_insert_fmtstr(cfg, i, "$t");
    else
      dvr_insert_fmtstr_before_extension(cfg, "$t");
  }

  dvr_match_and_insert_or_remove(cfg, "$t/", cfg->dvr_title_dir, 0);
  dvr_match_and_insert_or_remove(cfg, "$c/", cfg->dvr_channel_dir, 0);
  dvr_match_and_insert_or_remove(cfg, "%F/", cfg->dvr_dir_per_day, 0);

  dvr_match_and_insert_or_remove(cfg, "$-c", cfg->dvr_channel_in_title, -1);
  dvr_match_and_insert_or_remove(cfg, "$.s", cfg->dvr_subtitle_in_title, -1);
  dvr_match_and_insert_or_remove(cfg, "%F",  cfg->dvr_date_in_title, -1);
  dvr_match_and_insert_or_remove(cfg, "%R",  cfg->dvr_time_in_title, -1);
  dvr_match_and_insert_or_remove(cfg, "$-e",  cfg->dvr_episode_in_title, -1);

  dvr_update_pathname_safe(cfg);
}

/**
 *
 */
void
dvr_config_delete(const char *name)
{
  dvr_config_t *cfg;

  cfg = dvr_config_find_by_name(name);
  if (!dvr_config_is_default(cfg))
    dvr_config_destroy(cfg, 1);
  else
    tvhwarn(LS_DVR, "Attempt to delete default config ignored");
}

/**
 *
 */
void
dvr_config_changed(dvr_config_t *cfg)
{
  if (dvr_config_is_default(cfg))
    cfg->dvr_enabled = 1;
  cfg->dvr_valid = 1;
  if (cfg->dvr_pathname_changed) {
    cfg->dvr_pathname_changed = 0;
    dvr_update_pathname_from_fmtstr(cfg);
  } else {
    dvr_update_pathname_from_booleans(cfg);
  }
  dvr_config_storage_check(cfg);
  if (cfg->dvr_cleanup_threshold_free < 50)
    cfg->dvr_cleanup_threshold_free = 50; // as checking is only periodically, lower is not save
  if (cfg->dvr_removal_days > DVR_RET_REM_FOREVER)
    cfg->dvr_removal_days = DVR_RET_REM_FOREVER;
  if (cfg->dvr_retention_days > DVR_RET_REM_FOREVER)
    cfg->dvr_retention_days = DVR_RET_REM_FOREVER;
  if (cfg->dvr_profile && !strcmp(profile_get_name(cfg->dvr_profile), "htsp")) // htsp is for streaming only
    cfg->dvr_profile = profile_find_by_name("pass", NULL);
}


/* **************************************************************************
 * DVR Config Class definition
 * **************************************************************************/

static void
dvr_config_class_changed(idnode_t *self)
{
  dvr_config_changed((dvr_config_t *)self);
}

static htsmsg_t *
dvr_config_class_save(idnode_t *self, char *filename, size_t fsize)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  htsmsg_t *m = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&cfg->dvr_id, m);
  if (filename)
    snprintf(filename, fsize, "dvr/config/%s", idnode_uuid_as_str(&cfg->dvr_id, ubuf));
  return m;
}

static void
dvr_config_class_delete(idnode_t *self)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  if (!dvr_config_is_default(cfg))
      dvr_config_destroy(cfg, 1);
}

static int
dvr_config_class_perm(idnode_t *self, access_t *a, htsmsg_t *msg_to_write)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  htsmsg_field_t *f;
  const char *uuid, *my_uuid;
  char ubuf[UUID_HEX_SIZE];

  if (access_verify2(a, ACCESS_OR|ACCESS_ADMIN|ACCESS_RECORDER))
    return -1;
  if (!access_verify2(a, ACCESS_ADMIN))
    return 0;
  if (a->aa_dvrcfgs) {
    my_uuid = idnode_uuid_as_str(&cfg->dvr_id, ubuf);
    HTSMSG_FOREACH(f, a->aa_dvrcfgs) {
      uuid = htsmsg_field_get_str(f) ?: "";
      if (!strcmp(uuid, my_uuid))
        goto fine;
    }
    return -1;
  }
fine:
  return 0;
}

static int
dvr_config_class_enabled_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return 0;
  if (cfg->dvr_enabled != *(int *)v) {
    cfg->dvr_enabled = *(int *)v;
    return 1;
  }
  return 0;
}

static uint32_t
dvr_config_class_enabled_opts(void *o, uint32_t opts)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (cfg && dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return PO_RDONLY;
  return 0;
}

static int
dvr_config_class_name_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (dvr_config_is_default(cfg) && dvr_config_is_valid(cfg))
    return 0;
  if (strcmp(cfg->dvr_config_name, v ?: "")) {
    if (dvr_config_is_valid(cfg) && tvh_str_default(v, NULL) == NULL)
      return 0;
    free(cfg->dvr_config_name);
    cfg->dvr_config_name = strdup(v ?: "");
    return 1;
  }
  return 0;
}

static int
dvr_config_class_profile_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  profile_t *pro;

  pro = v ? profile_find_by_uuid(v) : NULL;
  pro = pro ?: profile_find_by_name(v, "pass");
  if (pro == NULL) {
    if (cfg->dvr_profile) {
      LIST_REMOVE(cfg, profile_link);
      cfg->dvr_profile = NULL;
      return 1;
    }
  } else if (cfg->dvr_profile != pro) {
    if (cfg->dvr_profile)
      LIST_REMOVE(cfg, profile_link);
    cfg->dvr_profile = pro;
    LIST_INSERT_HEAD(&pro->pro_dvr_configs, cfg, profile_link);
    return 1;
  }
  return 0;
}

static const void *
dvr_config_class_profile_get(void *o)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (cfg->dvr_profile)
    idnode_uuid_as_str(&cfg->dvr_profile->pro_id, prop_sbuf);
  else
    prop_sbuf[0] = '\0';
  return &prop_sbuf_ptr;
}

static char *
dvr_config_class_profile_rend(void *o, const char *lang)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  if (cfg->dvr_profile)
    return strdup(profile_get_name(cfg->dvr_profile));
  return NULL;
}

static void
dvr_config_class_get_title
  (idnode_t *self, const char *lang, char *dst, size_t dstsize)
{
  dvr_config_t *cfg = (dvr_config_t *)self;
  if (!dvr_config_is_default(cfg)) {
    snprintf(dst, dstsize, "%s", cfg->dvr_config_name);
  } else {
    snprintf(dst, dstsize, "%s", tvh_gettext_lang(lang, N_("(Default profile)")));
  }
}

static int
dvr_config_class_charset_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  return dvr_charset_update(cfg, v);
}

static htsmsg_t *
dvr_config_class_charset_list(void *o, const char *lang)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "intlconv/charsets");
  return m;
}

static htsmsg_t *
dvr_config_class_cache_list(void *o, const char *lang)
{
  static struct strtab tab[] = {
    { N_("Unknown"),            MC_CACHE_UNKNOWN },
    { N_("System"),             MC_CACHE_SYSTEM },
    { N_("Don't keep"),         MC_CACHE_DONTKEEP },
    { N_("Sync"),               MC_CACHE_SYNC },
    { N_("Sync + Don't keep"),  MC_CACHE_SYNCDONTKEEP }
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
dvr_config_class_removal_list ( void *o, const char *lang )
{
  static const struct strtab_u32 tab[] = {
    { N_("1 day"),              DVR_RET_REM_1DAY },
    { N_("3 days"),             DVR_RET_REM_3DAY },
    { N_("5 days"),             DVR_RET_REM_5DAY },
    { N_("1 week"),             DVR_RET_REM_1WEEK },
    { N_("2 weeks"),            DVR_RET_REM_2WEEK },
    { N_("3 weeks"),            DVR_RET_REM_3WEEK },
    { N_("1 month"),            DVR_RET_REM_1MONTH },
    { N_("2 months"),           DVR_RET_REM_2MONTH },
    { N_("3 months"),           DVR_RET_REM_3MONTH },
    { N_("6 months"),           DVR_RET_REM_6MONTH },
    { N_("1 year"),             DVR_RET_REM_1YEAR },
    { N_("2 years"),            DVR_RET_REM_2YEARS },
    { N_("3 years"),            DVR_RET_REM_3YEARS },
    { N_("Maintained space"),   DVR_REM_SPACE },
    { N_("Forever"),            DVR_RET_REM_FOREVER },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
}

static htsmsg_t *
dvr_config_class_remove_after_playback_list ( void *o, const char *lang )
{
  enum {
    ONE_MINUTE = 60,
    ONE_HOUR = ONE_MINUTE * 60,
    ONE_DAY = ONE_HOUR * 24
  };

  /* We want a few "soon" options (other than immediately) since that
   * gives the user time to restart the playback if they accidentally
   * skipped to the end and marked it as watched, whereas immediately
   * would immediately delete that recording (and we don't yet support
   * undelete).
   */
  static const struct strtab_u32 tab[] = {
    { N_("Never"),              0 },
    { N_("Immediately"),        1 },
    { N_("1 minute"),           ONE_MINUTE },
    { N_("10 minutes"),         ONE_MINUTE * 10 },
    { N_("30 minutes"),         ONE_MINUTE * 30 },
    { N_("1 hour"),             ONE_HOUR },
    { N_("2 hours"),            ONE_HOUR * 2 },
    { N_("4 hour"),             ONE_HOUR * 4 },
    { N_("8 hour"),             ONE_HOUR * 8 },
    { N_("12 hours"),           ONE_HOUR * 12 },
    { N_("1 day"),              ONE_DAY },
    { N_("2 days"),             ONE_DAY * 2 },
    { N_("3 days"),             ONE_DAY * 3 },
    { N_("5 days"),             ONE_DAY * 5 },
    { N_("1 week"),             ONE_DAY * 7 },
    { N_("2 weeks"),            ONE_DAY * 14 },
    { N_("3 weeks"),            ONE_DAY * 21 },
    { N_("1 month"),            ONE_DAY * 31 }, /* Approximations based on RET_REM */
    { N_("2 months"),           ONE_DAY * 62 },
    { N_("3 months"),           ONE_DAY * 92 },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
}


static htsmsg_t *
dvr_config_class_retention_list ( void *o, const char *lang )
{
  static const struct strtab_u32 tab[] = {
    { N_("1 day"),              DVR_RET_REM_1DAY },
    { N_("3 days"),             DVR_RET_REM_3DAY },
    { N_("5 days"),             DVR_RET_REM_5DAY },
    { N_("1 week"),             DVR_RET_REM_1WEEK },
    { N_("2 weeks"),            DVR_RET_REM_2WEEK },
    { N_("3 weeks"),            DVR_RET_REM_3WEEK },
    { N_("1 month"),            DVR_RET_REM_1MONTH },
    { N_("2 months"),           DVR_RET_REM_2MONTH },
    { N_("3 months"),           DVR_RET_REM_3MONTH },
    { N_("6 months"),           DVR_RET_REM_6MONTH },
    { N_("1 year"),             DVR_RET_REM_1YEAR },
    { N_("2 years"),            DVR_RET_REM_2YEARS },
    { N_("3 years"),            DVR_RET_REM_3YEARS },
    { N_("On file removal"),    DVR_RET_ONREMOVE },
    { N_("Forever"),            DVR_RET_REM_FOREVER },
  };
  return strtab2htsmsg_u32(tab, 1, lang);
}

static htsmsg_t *
dvr_config_class_extra_list(void *o, const char *lang)
{
  return dvr_entry_class_duration_list(o,
           tvh_gettext_lang(lang, N_("Not set (none or channel configuration)")),
           4*60, 1, lang);
}

static htsmsg_t *
dvr_config_entry_class_update_window_list(void *o, const char *lang)
{
  return dvr_entry_class_duration_list(o,
           tvh_gettext_lang(lang, N_("Update disabled")),
           24*3600, 60, lang);
}

static htsmsg_t *
dvr_autorec_entry_class_record_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Record all"),
        DVR_AUTOREC_RECORD_ALL },
    { N_("All: Record if EPG/XMLTV indicates it is a unique programme"),
        DVR_AUTOREC_RECORD_UNIQUE },
    { N_("All: Record if different episode number"),
        DVR_AUTOREC_RECORD_DIFFERENT_EPISODE_NUMBER },
    { N_("All: Record if different subtitle"),
        DVR_AUTOREC_RECORD_DIFFERENT_SUBTITLE },
    { N_("All: Record if different description"),
        DVR_AUTOREC_RECORD_DIFFERENT_DESCRIPTION },
    { N_("All: Record once per month"),
        DVR_AUTOREC_RECORD_ONCE_PER_MONTH },
    { N_("All: Record once per week"),
        DVR_AUTOREC_RECORD_ONCE_PER_WEEK },
    { N_("All: Record once per day"),
        DVR_AUTOREC_RECORD_ONCE_PER_DAY },
    { N_("Local: Record if different episode number"),
        DVR_AUTOREC_LRECORD_DIFFERENT_EPISODE_NUMBER },
    { N_("Local: Record if different title"),
        DVR_AUTOREC_LRECORD_DIFFERENT_TITLE },
    { N_("Local: Record if different subtitle"),
        DVR_AUTOREC_LRECORD_DIFFERENT_SUBTITLE },
    { N_("Local: Record if different description"),
        DVR_AUTOREC_LRECORD_DIFFERENT_DESCRIPTION },
    { N_("Local: Record once per month"),
        DVR_AUTOREC_LRECORD_ONCE_PER_MONTH },
    { N_("Local: Record once per week"),
        DVR_AUTOREC_LRECORD_ONCE_PER_WEEK },
    { N_("Local: Record once per day"),
        DVR_AUTOREC_LRECORD_ONCE_PER_DAY },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static int
dvr_config_class_pathname_set(void *o, const void *v)
{
  dvr_config_t *cfg = (dvr_config_t *)o;
  const char *s = v;
  if (strcmp(cfg->dvr_pathname ?: "", s ?: "")) {
    free(cfg->dvr_pathname);
    cfg->dvr_pathname = s ? strdup(s) : NULL;
    cfg->dvr_pathname_changed = 1;
    return 1;
  }
  return 0;
}

CLASS_DOC(dvrconfig)
PROP_DOC(preprocessor)
PROP_DOC(postprocessor)
PROP_DOC(postremove)
PROP_DOC(pathname)
PROP_DOC(cache_scheme)
PROP_DOC(runningstate)
PROP_DOC(dvrconfig_whitespace)
PROP_DOC(dvrconfig_unsafe)
PROP_DOC(dvrconfig_windows)
PROP_DOC(dvrconfig_fanart)
PROP_DOC(duplicate_handling)

const idclass_t dvr_config_class = {
  .ic_class      = "dvrconfig",
  .ic_caption    = N_("DVR - Profiles"),
  .ic_event      = "dvrconfig",
  .ic_doc        = tvh_doc_dvrconfig_class,
  .ic_changed    = dvr_config_class_changed,
  .ic_save       = dvr_config_class_save,
  .ic_get_title  = dvr_config_class_get_title,
  .ic_delete     = dvr_config_class_delete,
  .ic_perm       = dvr_config_class_perm,
  .ic_groups     = (const property_group_t[]) {
      {
         .name   = N_("General Settings"),
         .number = 1,
      },
      {
         .name   = N_("Filesystem Settings"),
         .number = 2,
      },
      {
         .name   = N_("Subdirectory Settings"),
         .number = 3,
      },
      {
         .name   = N_("Filename/Tagging Settings"),
         .number = 4,
         .column = 1,
      },
      {
         .name   = "",
         .number = 5,
         .parent = 4,
         .column = 2,
      },
      {
         .name   = N_("EPG/Autorec Settings"),
         .number = 6,
      },
      {
         .name   = N_("Miscellaneous Settings"),
         .number = 7,
      },
      {
         .name   = N_("Artwork Settings"),
         .number = 8,
         .column = 1,
      },
      {}
  },
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable profile."),
      .set      = dvr_config_class_enabled_set,
      .off      = offsetof(dvr_config_t, dvr_enabled),
      .def.i    = 1,
      .group    = 1,
      .get_opts = dvr_config_class_enabled_opts,
    },
    {
      .type     = PT_STR,
      .id       = "name",
      .name     = N_("Configuration name"),
      .desc     = N_("Name of the profile."),
      .set      = dvr_config_class_name_set,
      .off      = offsetof(dvr_config_t, dvr_config_name),
      .def.s    = "! New config",
      .group    = 1,
      .get_opts = dvr_config_class_enabled_opts,
    },
    {
      .type     = PT_STR,
      .id       = "profile",
      .name     = N_("Stream profile"),
      .desc     = N_("The stream profile the DVR profile will use for "
                     "recordings."),
      .set      = dvr_config_class_profile_set,
      .get      = dvr_config_class_profile_get,
      .rend     = dvr_config_class_profile_rend,
      .list     = profile_class_get_list,
      .opts     = PO_ADVANCED,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "pri",
      .name     = N_("Priority"),
      .desc     = N_("Priority of the entry, higher-priority entries will "
                     "take precedence and cancel lower-priority events."),
      .list     = dvr_entry_class_pri_list,
      .def.i    = DVR_PRIO_NORMAL,
      .off      = offsetof(dvr_config_t, dvr_pri),
      .opts     = PO_SORTKEY | PO_ADVANCED | PO_DOC_NLIST,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "retention-days",
      .name     = N_("Recording info retention period"),
      .desc     = N_("Days to retain information about recordings. Once this period is exceeded, duplicate detection will not be possible."),
      .off      = offsetof(dvr_config_t, dvr_retention_days),
      .def.u32  = DVR_RET_ONREMOVE,
      .list     = dvr_config_class_retention_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "removal-days",
      .name     = N_("Recorded file(s) retention period"),
      .desc     = N_("Number of days to keep recorded files."),
      .off      = offsetof(dvr_config_t, dvr_removal_days),
      .def.u32  = DVR_RET_REM_FOREVER,
      .list     = dvr_config_class_removal_list,
      .opts     = PO_DOC_NLIST,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "remove-after-playback",
      .name     = N_("Automatically delete played recordings"),
      .desc     = N_("Number of minutes after playback has finished "
                     "before file should be automatically removed "
                     "(unless its retention is 'forever'). "
                     "Note that some clients may pre-cache playback "
                     "which means the recording will be marked as "
                     "played when the client has cached the data, "
                     "which may be before the end of the programme is "
                     "actually watched."
                    ),
      .off      = offsetof(dvr_config_t, dvr_removal_after_playback),
      .def.u32  = 0,
      .list     = dvr_config_class_remove_after_playback_list,
      .opts     = PO_ADVANCED,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "pre-extra-time",
      .name     = N_("Pre-recording padding"),
      .desc     = N_("Start recording earlier than the defined start "
                     "time by x minutes: for example, if a program is "
                     "to start at 13:00 and you set a padding of 5 "
                     "minutes it will start recording at 12:54:30 "
                     "(including a warm-up time of 30 seconds). If this "
                     "isn't specified, any pre-recording padding as set "
                     "in the channel or DVR entry will be used."),
      .off      = offsetof(dvr_config_t, dvr_extra_time_pre),
      .list     = dvr_config_class_extra_list,
      .opts     = PO_DOC_NLIST,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "post-extra-time",
      .name     = N_("Post-recording padding"),
      .desc     = N_("Continue recording for x minutes after scheduled "
                     "stop time."),
      .off      = offsetof(dvr_config_t, dvr_extra_time_post),
      .list     = dvr_config_class_extra_list,
      .opts     = PO_DOC_NLIST,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "clone",
      .name     = N_("Clone scheduled entry on error"),
      .desc     = N_("If an error occurs clone the scheduled entry and "
                     "try to record again (if possible)."),
      .off      = offsetof(dvr_config_t, dvr_clone),
      .opts     = PO_ADVANCED,
      .def.u32  = 1,
      .group    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "rerecord-errors",
      .name     = N_("Try re-scheduling recording if more errors than (0=off)"),
      .desc     = N_("If more than x errors occur during a recording "
                     "schedule a re-record (if possible)."),
      .off      = offsetof(dvr_config_t, dvr_rerecord_errors),
      .opts     = PO_ADVANCED,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "complex-scheduling",
      .name     = N_("For autorecs, attempt to find better time slots"),
      .desc     = N_("When scheduling an autorec, this option attempts "
                     "to schedule at the earliest time and on the 'best' "
                     "channel (such as channel with the most failover services). "
                     "This is useful when multiple timeshift "
                     "and repeat channels are available. Without this option "
                     "autorecs may get scheduled on timeshift channels "
                     "instead of on primary channels. "
                     "This scheduling "
                     "requires extra overhead so is disabled by default."),
      .off      = offsetof(dvr_config_t, dvr_complex_scheduling),
      .opts     = PO_ADVANCED,
      .group    = 1,
    },
    {
      .type     = PT_BOOL,
      .id       = "fetch-artwork",
      .name     = N_("Fetch artwork for new recordings."),
      .desc     = N_("Fetch additional artwork from installed providers. "
                     "Tvheadend has a 'tmdb' and `tvdb' provider which require "
                     "you to specify your authorized key in the options below."),
      .off      = offsetof(dvr_config_t, dvr_fetch_artwork),
      .opts     = PO_ADVANCED,
      .group    = 8,
    },
    {
      .type     = PT_BOOL,
      .id       = "fetch-artwork-known-broadcasts-allow-unknown",
      .name     = N_("Fetch artwork for unidentifiable broadcasts."),
      .desc     = N_("Artwork fetching requires broadcasts to have good quality "
                     "information that uniquely identifies them, such as "
                     "year, season and episode. "
                     "Without this information, lookups will frequently fail "
                     "or return incorrect artwork. "
                     "The default is to only lookup fanart for broadcasts that "
                     "have high quality identifiable information."
                    ),
      .off      = offsetof(dvr_config_t, dvr_fetch_artwork_allow_unknown),
      .opts     = PO_ADVANCED,
      .group    = 8,
    },
    {
      .type     = PT_STR,
      .id       = "fetch-artwork-options",
      .name     = N_("Additional command line options when fetching artwork for new recordings."),
      .desc     = N_("Some artwork providers require additional arguments such as "
                     "'--tmdb-key my_key_from_website'. These can be specified here. "
                     "See Help for full details."),
      .off      = offsetof(dvr_config_t, dvr_fetch_artwork_options),
      .doc      = prop_doc_dvrconfig_fanart,
      .opts     = PO_ADVANCED,
      .group    = 8,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form field, enter whatever you like here."),
      .off      = offsetof(dvr_config_t, dvr_comment),
      .group    = 1,
    },
    {
      .type     = PT_STR,
      .id       = "storage",
      .name     = N_("Storage path"),
      .desc     = N_("Path where the recordings are stored. If "
                     "components of the path do not exist, "
                     "Tvheadend will try to create them."),
      .off      = offsetof(dvr_config_t, dvr_storage),
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "storage-mfree",
      .name     = N_("Maintain free storage space in MiB"),
      .desc     = N_("Keep x amount of storage space free."),
      .off      = offsetof(dvr_config_t, dvr_cleanup_threshold_free),
      .def.i    = 1000,
      .opts     = PO_ADVANCED,
      .group    = 2,
    },
    {
      .type     = PT_U32,
      .id       = "storage-mused",
      .name     = N_("Maintain used storage space in MiB (0=disabled)"),
      .desc     = N_("Use x amount of storage space."),
      .off      = offsetof(dvr_config_t, dvr_cleanup_threshold_used),
      .def.i    = 0,
      .opts     = PO_EXPERT,
      .group    = 2,
    },
    {
      .type     = PT_PERM,
      .id       = "directory-permissions",
      .name     = N_("Directory permissions (octal, e.g. 0775)"),
      .desc     = N_("Create directories using these permissions."),
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_directory_permissions),
      .opts     = PO_EXPERT,
      .def.u32  = 0775,
      .group    = 2,
    },
    {
      .type     = PT_PERM,
      .id       = "file-permissions",
      .name     = N_("File permissions (octal, e.g. 0664)"),
      .desc     = N_("Create files using these permissions."),
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_file_permissions),
      .opts     = PO_EXPERT,
      .def.u32  = 0664,
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character set"),
      .desc     = N_("Use this character set when setting filenames."),
      .off      = offsetof(dvr_config_t, dvr_charset),
      .set      = dvr_config_class_charset_set,
      .list     = dvr_config_class_charset_list,
      .opts     = PO_EXPERT,
      .def.s    = "UTF-8",
      .group    = 2,
    },
    {
      .type     = PT_STR,
      .id       = "pathname",
      .name     = N_("Format string/Pathname specification"),
      .desc     = N_("The string allows you to manually specify the "
                     "full path generation using predefined "
                     "modifiers. See Help for full details."),
      .doc      = prop_doc_pathname,
      .set      = dvr_config_class_pathname_set,
      .off      = offsetof(dvr_config_t, dvr_pathname),
      .opts     = PO_EXPERT,
      .group    = 2,
    },
    {
      .type     = PT_INT,
      .id       = "cache",
      .name     = N_("Cache scheme"),
      .desc     = N_("The cache scheme to use/used to store recordings. "
                     "Leave as \"system\" unless you have a special use "
                     "case for one of the others. See Help for details."),
      .doc      = prop_doc_cache_scheme,
      .off      = offsetof(dvr_config_t, dvr_muxcnf.m_cache),
      .def.i    = MC_CACHE_DONTKEEP,
      .list     = dvr_config_class_cache_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .group    = 2,
    },
    {
      .type     = PT_BOOL,
      .id       = "day-dir",
      .name     = N_("Make subdirectories per day"),
      .desc     = N_("Create a new directory per day in the "
                     "recording system path. Folders will only be "
                     "created when something is recorded. The format "
                     "of the directory will be ISO standard YYYY-MM-DD."),
      .off      = offsetof(dvr_config_t, dvr_dir_per_day),
      .opts     = PO_EXPERT,
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "channel-dir",
      .name     = N_("Make subdirectories per channel"),
      .desc     = N_("Create a directory per channel when "
                     "storing recordings. If both this and the 'directory "
                     "per day' checkbox is enabled, the date-directory "
                     "will be the parent of the per-channel directory."),
      .off      = offsetof(dvr_config_t, dvr_channel_dir),
      .opts     = PO_EXPERT,
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "title-dir",
      .name     = N_("Make subdirectories per title"),
      .desc     = N_("Create a directory per title when "
                     "storing recordings. If the day/channel directory "
                     "checkboxes are also enabled, those directories "
                     "will be parents of this directory."),
      .off      = offsetof(dvr_config_t, dvr_title_dir),
      .opts     = PO_EXPERT,
      .group    = 3,
    },
    {
      .type     = PT_STR,
      .id       = "format-tvmovies-subdir",
      .name     = N_("Subdirectory for tvmovies for $q format specifier"),
      .desc     = N_("Subdirectory to use for tvmovies when using the $q specifier. "
                     "This can contain any alphanumeric "
                     "characters (A-Za-z0-9). Other characters may be supported depending "
                     "on your OS and filesystem."
                     ),
      .off      = offsetof(dvr_config_t, dvr_format_tvmovies_subdir),
      .def.s    = "tvmovies",
      .opts     = PO_EXPERT,
      .group    = 3,
    },
    {
      .type     = PT_STR,
      .id       = "format-tvshows-subdir",
      .name     = N_("Subdirectory for tvshows for $q format specifier"),
      .desc     = N_("Subdirectory to use for tvshows when using the $q specifier. "
                     "This can contain any alphanumeric "
                     "characters (A-Za-z0-9). Other characters may be supported depending "
                     "on your OS and filesystem."
                    ),
      .off      = offsetof(dvr_config_t, dvr_format_tvshows_subdir),
      .def.s    = "tvshows",
      .opts     = PO_EXPERT,
      .group    = 3,
    },
    {
      .type     = PT_BOOL,
      .id       = "channel-in-title",
      .name     = N_("Include channel name in filename"),
      .desc     = N_("Include the name of the channel in "
                     "the event title. This applies to both the title "
                     "stored in the file and to the filename itself."),
      .off      = offsetof(dvr_config_t, dvr_channel_in_title),
      .opts     = PO_ADVANCED,
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "date-in-title",
      .name     = N_("Include date in filename"),
      .desc     = N_("Include the date for the recording in "
                     "the event title. This applies to both the title "
                     "stored in the file and to the filename itself."),
      .off      = offsetof(dvr_config_t, dvr_date_in_title),
      .opts     = PO_ADVANCED,
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "time-in-title",
      .name     = N_("Include time in filename"),
      .desc     = N_("Include the time for the recording in "
                     "the event title. This applies to both the title "
                     "stored in the file and to the filename itself."),
      .off      = offsetof(dvr_config_t, dvr_time_in_title),
      .opts     = PO_ADVANCED,
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "episode-in-title",
      .name     = N_("Include episode in filename"),
      .desc     = N_("Include the season and episode in the "
                     "title (if available)."),
      .off      = offsetof(dvr_config_t, dvr_episode_in_title),
      .opts     = PO_EXPERT,
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "subtitle-in-title",
      .name     = N_("Include subtitle in filename"),
      .desc     = N_("Include the episode subtitle in the "
                     "title (if available)."),
      .off      = offsetof(dvr_config_t, dvr_subtitle_in_title),
      .opts     = PO_EXPERT,
      .group    = 4,
    },
    {
      .type     = PT_BOOL,
      .id       = "omit-title",
      .name     = N_("Don't include title in filename"),
      .desc     = N_("Don't include the title in the filename."),
      .off      = offsetof(dvr_config_t, dvr_omit_title),
      .opts     = PO_EXPERT,
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "clean-title",
      .name     = N_("Remove all unsafe characters from filename"),
      .desc     = N_("All characters that could possibly "
                     "cause problems for filenaming will be replaced "
                     "with an underscore. See Help for details."),
      .doc      = prop_doc_dvrconfig_unsafe,
      .off      = offsetof(dvr_config_t, dvr_clean_title),
      .opts     = PO_EXPERT,
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "whitespace-in-title",
      .name     = N_("Replace whitespace in title with '-'"),
      .desc     = N_("Replaces all whitespace in the title with '-'."),
      .doc      = prop_doc_dvrconfig_whitespace,
      .off      = offsetof(dvr_config_t, dvr_whitespace_in_title),
      .opts     = PO_EXPERT,
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "windows-compatible-filenames",
      .name     = N_("Use Windows-compatible filenames"),
      .desc     = N_("Characters not supported in Windows filenames "
                     "(e.g. for an SMB/CIFS share) will be stripped out "
                     "or converted."),
      .doc      = prop_doc_dvrconfig_windows,
      .off      = offsetof(dvr_config_t, dvr_windows_compatible_filenames),
      .opts     = PO_ADVANCED,
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "tag-files",
      .name     = N_("Tag files with metadata"),
      .desc     = N_("Create tags in recordings using media containers "
                     "that support metadata (if possible)."),
      .off      = offsetof(dvr_config_t, dvr_tag_files),
      .opts     = PO_ADVANCED,
      .def.i    = 1,
      .group    = 5,
    },
    {
      .type     = PT_BOOL,
      .id       = "create-scene-markers",
      .name     = N_("Create scene markers"),
      .desc     = N_("Create scene markers in recordings "
                     "based on the EPG start/stop times when available."),
      .off      = offsetof(dvr_config_t, dvr_create_scene_markers),
      .def.i    = 1,
      .group    = 5,
    },
    {
      .type     = PT_U32,
      .id       = "epg-update-window",
      .name     = N_("EPG update window"),
      .desc     = N_("Maximum allowed difference between event start time when "
                     "the EPG event is changed in seconds."),
      .off      = offsetof(dvr_config_t, dvr_update_window),
      .list     = dvr_config_entry_class_update_window_list,
      .def.u32  = 24*3600,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
      .group    = 6,
    },
    {
      .type     = PT_BOOL,
      .id       = "epg-running",
      .name     = N_("Use EPG running state"),
      .desc     = N_("Use EITp/f to decide event start/stop. This is "
                     "also known as \"Accurate Recording\". See Help "
                     "for details."),
      .doc      = prop_doc_runningstate,
      .off      = offsetof(dvr_config_t, dvr_running),
      .opts     = PO_ADVANCED,
      .def.u32  = 1,
      .group    = 6,
    },
    {
      .type     = PT_U32,
      .id       = "autorec-maxcount",
      .name     = N_("Autorec maximum count (0=unlimited)"),
      .desc     = N_("The maximum number of entries that can be matched."),
      .off      = offsetof(dvr_config_t, dvr_autorec_max_count),
      .opts     = PO_ADVANCED,
      .def.i    = 50,
      .group    = 6,
    },
    {
      .type     = PT_U32,
      .id       = "autorec-maxsched",
      .name     = N_("Autorec maximum schedules limit (0=unlimited)"),
      .desc     = N_("The maximum number of recordings that can be scheduled."),
      .off      = offsetof(dvr_config_t, dvr_autorec_max_sched_count),
      .opts     = PO_ADVANCED,
      .group    = 6,
    },
    {
      .type     = PT_U32,
      .id       = "record",
      .name     = N_("Duplicate handling"),
      .desc     = N_("How to handle duplicate recordings."),
      .def.i    = DVR_AUTOREC_RECORD_ALL,
      .doc      = prop_doc_duplicate_handling,
      .off      = offsetof(dvr_config_t, dvr_autorec_dedup),
      .list     = dvr_autorec_entry_class_record_list,
      .opts     = PO_ADVANCED | PO_DOC_NLIST | PO_HIDDEN,
      .group    = 6,
    },
    {
      .type     = PT_BOOL,
      .id       = "skip-commercials",
      .name     = N_("Skip commercials"),
      .desc     = N_("Commercials will be dropped from the "
                     "recordings. Commercial detection works using EITp/f "
                     "(EPG running state) and for the Swedish channel TV4 "
                     "(using teletext info)."),
      .off      = offsetof(dvr_config_t, dvr_skip_commercials),
      .opts     = PO_ADVANCED,
      .def.i    = 1,
      .group    = 6,
    },
    {
      .type     = PT_STR,
      .id       = "preproc",
      .name     = N_("Pre-processor command"),
      .desc     = N_("Script/program to run when a recording starts "
                     "(service is subscribed but no filename available)."),
      .doc      = prop_doc_preprocessor,
      .off      = offsetof(dvr_config_t, dvr_preproc),
      .opts     = PO_EXPERT,
      .group    = 7,
    },
    {
      .type     = PT_STR,
      .id       = "postproc",
      .name     = N_("Post-processor command"),
      .desc     = N_("Script/program to run when a recording completes."),
      .doc      = prop_doc_postprocessor,
      .off      = offsetof(dvr_config_t, dvr_postproc),
      .opts     = PO_ADVANCED,
      .group    = 7,
    },
    {
      .type     = PT_STR,
      .id       = "postremove",
      .name     = N_("Post-remove command"),
      .desc     = N_("Script/program to run when a recording gets removed."),
      .doc      = prop_doc_postremove,
      .off      = offsetof(dvr_config_t, dvr_postremove),
      .opts     = PO_EXPERT,
      .group    = 7,
    },
    {
      .type     = PT_U32,
      .id       = "warm-time",
      .name     = N_("Extra warming up time (seconds)"),
      .desc     = N_("Additional time (in seconds) in which to get "
                     "the tuner ready for recording. This is useful for "
                     "those with tuners that take some time to tune "
                     "and/or send garbage data at the beginning. "),
      .off      = offsetof(dvr_config_t, dvr_warm_time),
      .opts     = PO_EXPERT,
      .group    = 7,
      .def.u32  = 30
    },
    {}
  },
};

/**
 *
 */
void
dvr_config_destroy_by_profile(profile_t *pro, int delconf)
{
  dvr_config_t *cfg;

  while((cfg = LIST_FIRST(&pro->pro_dvr_configs)) != NULL) {
    LIST_REMOVE(cfg, profile_link);
    cfg->dvr_profile = profile_find_by_name(NULL, "pass");
  }
}

/**
 *
 */
void
dvr_config_init(void)
{
  htsmsg_t *m, *l;
  htsmsg_field_t *f;
  dvr_config_t *cfg;

  dvr_iov_max = sysconf(_SC_IOV_MAX);
  if( dvr_iov_max == -1 )
#ifdef IOV_MAX
    dvr_iov_max = IOV_MAX;
#else
    dvr_iov_max = 16;
#endif

  /* Default settings */

  LIST_INIT(&dvrconfigs);
  idclass_register(&dvr_config_class);

  if ((l = hts_settings_load("dvr/config")) != NULL) {
    HTSMSG_FOREACH(f, l) {
      if ((m = htsmsg_get_map_by_field(f)) == NULL) continue;
      (void)dvr_config_create(NULL, htsmsg_field_name(f), m);
    }
    htsmsg_destroy(l);
  }

  /* Create the default entry */

  cfg = dvr_config_find_by_name_default(NULL);
  assert(cfg);

  LIST_FOREACH(cfg, &dvrconfigs, config_link)
    dvr_config_storage_check(cfg);
}

void
dvr_init(void)
{
#if ENABLE_INOTIFY
  dvr_inotify_init();
#endif
  dvr_disk_space_boot();
  dvr_autorec_init();
  dvr_timerec_init();
  dvr_entry_init();

  /* We update the autorec entries so any that are no longer matching
   * the current schedule get deleted. This avoids the problem where
   * autorec entries remain even when user has deleted the epgdb
   * or modified their settings between runs.
   */
  tvhinfo(LS_DVR, "Purging obsolete autorec entries for current schedule");
  dvr_autorec_purge_obsolete_timers();

  dvr_autorec_update();
  dvr_timerec_update();
  dvr_disk_space_init();
}

/**
 *
 */
void
dvr_done(void)
{
  dvr_config_t *cfg;

#if ENABLE_INOTIFY
  dvr_inotify_done();
#endif
  tvh_mutex_lock(&global_lock);
  dvr_entry_done();
  while ((cfg = LIST_FIRST(&dvrconfigs)) != NULL)
    dvr_config_destroy(cfg, 0);
  tvh_mutex_unlock(&global_lock);
  dvr_autorec_done();
  dvr_timerec_done();
  dvr_disk_space_done();
}
