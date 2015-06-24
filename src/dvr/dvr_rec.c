/*
 *  Digital Video Recorder
 *  Copyright (C) 2008 Andreas Öman
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

#include <stdarg.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <libgen.h> /* basename */

#if ENABLE_ANDROID
#include <sys/vfs.h>
#define statvfs statfs
#define fstatvfs fstatfs
#else
#include <sys/statvfs.h>
#endif

#include "htsstr.h"

#include "tvheadend.h"
#include "streaming.h"
#include "tcp.h"
#include "dvr.h"
#include "spawn.h"
#include "service.h"
#include "plumbing/tsfix.h"
#include "plumbing/globalheaders.h"
#include "htsp_server.h"
#include "atomic.h"
#include "intlconv.h"
#include "notify.h"

#include "muxer.h"

/**
 *
 */
static void *dvr_thread(void *aux);
static void dvr_spawn_postproc(dvr_entry_t *de, const char *dvr_postproc);
static void dvr_thread_epilog(dvr_entry_t *de);


const static int prio2weight[6] = {
  [DVR_PRIO_IMPORTANT]   = 500,
  [DVR_PRIO_HIGH]        = 400,
  [DVR_PRIO_NORMAL]      = 300,
  [DVR_PRIO_LOW]         = 200,
  [DVR_PRIO_UNIMPORTANT] = 100,
  [DVR_PRIO_NOTSET]      = 0,
};

/**
 *
 */
int
dvr_rec_subscribe(dvr_entry_t *de)
{
  char buf[100];
  int weight;
  profile_t *pro;
  profile_chain_t *prch;
  struct sockaddr sa;
  access_t *aa;
  uint32_t rec_count, net_count;
  int c1, c2;

  assert(de->de_s == NULL);
  assert(de->de_chain == NULL);

  if(de->de_pri > 0 && de->de_pri < ARRAY_SIZE(prio2weight))
    weight = prio2weight[de->de_pri];
  else
    weight = 300;

  snprintf(buf, sizeof(buf), "DVR: %s", lang_str_get(de->de_title, NULL));

  if (de->de_owner && de->de_owner[0] != '\0')
    aa = access_get_by_username(de->de_owner);
  else if (de->de_creator && de->de_creator[0] != '\0' &&
           tcp_get_ip_from_str(de->de_creator, &sa) != NULL)
    aa = access_get_by_addr(&sa);
  else {
    tvherror("dvr", "unable to find access (owner '%s', creator '%s')",
             de->de_owner, de->de_creator);
    return -EPERM;
 }

  if (aa->aa_conn_limit || aa->aa_conn_limit_dvr) {
    rec_count = dvr_usage_count(aa);
    net_count = aa->aa_conn_limit ? tcp_connection_count(aa) : 0;
    /* the rule is: allow if one condition is OK */
    c1 = aa->aa_conn_limit ? rec_count + net_count >= aa->aa_conn_limit : -1;
    c2 = aa->aa_conn_limit_dvr ? rec_count >= aa->aa_conn_limit_dvr : -1;
    if (c1 && c2) {
      tvherror("dvr", "multiple connections are not allowed for user '%s' from '%s' "
                      "(limit %u, dvr limit %u, active DVR %u, streaming %u)",
               aa->aa_username ?: "", aa->aa_representative ?: "",
               aa->aa_conn_limit, aa->aa_conn_limit_dvr, rec_count, net_count);
      return -EOVERFLOW;
    }
  }
  access_destroy(aa);

  pro = de->de_config->dvr_profile;
  prch = malloc(sizeof(*prch));
  profile_chain_init(prch, pro, de->de_channel);
  if (profile_chain_open(prch, &de->de_config->dvr_muxcnf, 0, 0)) {
    tvherror("dvr", "unable to create new channel streaming chain for '%s'",
             channel_get_name(de->de_channel));
    profile_chain_close(prch);
    free(prch);
    return -EINVAL;
  }

  de->de_s = subscription_create_from_channel(prch, NULL, weight,
					      buf, prch->prch_flags,
					      NULL, NULL, NULL, NULL);
  if (de->de_s == NULL) {
    tvherror("dvr", "unable to create new channel subcription for '%s'",
             channel_get_name(de->de_channel));
    profile_chain_close(prch);
    free(prch);
    return -EINVAL;
  }

  de->de_chain = prch;

  tvhthread_create(&de->de_thread, NULL, dvr_thread, de);
  return 0;
}

/**
 *
 */
void
dvr_rec_unsubscribe(dvr_entry_t *de)
{
  profile_chain_t *prch = de->de_chain;

  assert(de->de_s != NULL);
  assert(prch != NULL);

  streaming_target_deliver(prch->prch_st, streaming_msg_create(SMT_EXIT));
  
  pthread_join(de->de_thread, NULL);

  subscription_unsubscribe(de->de_s, 0);
  de->de_s = NULL;

  de->de_chain = NULL;
  profile_chain_close(prch);
  free(prch);
}


/**
 * Replace various chars with a dash
 */
static char *
cleanup_filename(dvr_config_t *cfg, char *s)
{
  int len = strlen(s);
  char *s1, *p;

  s1 = intlconv_utf8safestr(cfg->dvr_charset_id, s, (len * 2) + 1);
  if (s1 == NULL) {
    tvherror("dvr", "Unsupported charset %s using ASCII", cfg->dvr_charset);
    s1 = intlconv_utf8safestr(intlconv_charset_id("ASCII", 1, 1),
                             s, len * 2);
    if (s1 == NULL)
      return NULL;
  }

  /* Do not create hidden files */
  if (s1[0] == '.')
    s1[0] = '_';
  if (s1[0] == '\\' && s1[1] == '.')
    s1[1] = '_';

  for (s = s1 ; *s; s++) {

    if (*s == '\\') {
      s++;
      if (*s == '\0')
        break;
    }

    if (*s == '/')
      *s = '-';

    else if (cfg->dvr_whitespace_in_title &&
             (*s == ' ' || *s == '\t'))
      *s = '-';	

    else if (cfg->dvr_clean_title &&
             ((*s < 32) || (*s > 122) ||
             (strchr("/:\\<>|*?\"", *s) != NULL)))
      *s = '_';

    else if (cfg->dvr_windows_compatible_filenames &&
             (strchr("/:\\<>|*?\"", *s) != NULL))
      *s = '_';
  }

  if (cfg->dvr_windows_compatible_filenames) {
    /* trim trailing spaces and dots */
    for (s = p = s1; *s; s++) {
      if (*s == '\\')
        s++;
      if (*s != ' ' && *s != '.')
        p = s + 1;
    }
    *p = '\0';
  }

  return s1;
}

/**
 *
 */
static char *dvr_clean_directory_separator(char *buf)
{
  char *p;

  /* replace directory separator */
  for (p = buf; *p; p++)
    if (*p == '/')
      *p = '-';
  return buf;
}

static const char *dvr_do_prefix(const char *id, const char *s)
{
  static char buf[128];

  if (s == NULL) {
    buf[0] = '\0';
  } else if (s[0] && !isalpha(id[0])) {
    snprintf(buf, sizeof(buf), "%c%s", id[0], s);
  } else {
    strncpy(buf, s, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
  }
  return dvr_clean_directory_separator(buf);
}


static const char *dvr_sub_title(const char *id, const void *aux)
{
  return dvr_do_prefix(id, lang_str_get(((dvr_entry_t *)aux)->de_title, NULL));
}

static const char *dvr_sub_subtitle(const char *id, const void *aux)
{
  return dvr_do_prefix(id, lang_str_get(((dvr_entry_t *)aux)->de_subtitle, NULL));
}

static const char *dvr_sub_description(const char *id, const void *aux)
{
  return dvr_do_prefix(id, lang_str_get(((dvr_entry_t *)aux)->de_desc, NULL));
}

static const char *dvr_sub_episode(const char *id, const void *aux)
{
  const dvr_entry_t *de = aux;
  static char buf[64];

  if (de->de_bcast == NULL || de->de_bcast->episode == NULL)
    return "";
  epg_episode_number_format(de->de_bcast->episode,
                            buf, sizeof(buf),
                            ".", "S%02d", NULL, "E%02d", NULL);
  return dvr_do_prefix(id, buf);
}

static const char *dvr_sub_channel(const char *id, const void *aux)
{
  return dvr_do_prefix(id, DVR_CH_NAME((dvr_entry_t *)aux));
}

static const char *dvr_sub_owner(const char *id, const void *aux)
{
  return dvr_do_prefix(id, ((dvr_entry_t *)aux)->de_owner);
}

static const char *dvr_sub_creator(const char *id, const void *aux)
{
  return dvr_do_prefix(id, ((dvr_entry_t *)aux)->de_creator);
}

static const char *dvr_sub_last_error(const char *id, const void *aux)
{
  return dvr_do_prefix(id, streaming_code2txt(((dvr_entry_t *)aux)->de_last_error));
}

static const char *dvr_sub_start(const char *id, const void *aux)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRItime_t, (time_t)dvr_entry_get_start_time((dvr_entry_t *)aux));
  return dvr_do_prefix(id, buf);
}

static const char *dvr_sub_stop(const char *id, const void *aux)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRItime_t, (time_t)dvr_entry_get_stop_time((dvr_entry_t *)aux));
  return dvr_do_prefix(id, buf);
}

static htsstr_substitute_t dvr_subs_entry[] = {
  { .id = "t",  .getval = dvr_sub_title },
  { .id = " t", .getval = dvr_sub_title },
  { .id = "-t", .getval = dvr_sub_title },
  { .id = "_t", .getval = dvr_sub_title },
  { .id = ".t", .getval = dvr_sub_title },
  { .id = ",t", .getval = dvr_sub_title },
  { .id = ";t", .getval = dvr_sub_title },
  { .id = "s",  .getval = dvr_sub_subtitle },
  { .id = " s", .getval = dvr_sub_subtitle },
  { .id = "-s", .getval = dvr_sub_subtitle },
  { .id = "_s", .getval = dvr_sub_subtitle },
  { .id = ".s", .getval = dvr_sub_subtitle },
  { .id = ",s", .getval = dvr_sub_subtitle },
  { .id = ";s", .getval = dvr_sub_subtitle },
  { .id = "e",  .getval = dvr_sub_episode },
  { .id = " e", .getval = dvr_sub_episode },
  { .id = "-e", .getval = dvr_sub_episode },
  { .id = "_e", .getval = dvr_sub_episode },
  { .id = ".e", .getval = dvr_sub_episode },
  { .id = ",e", .getval = dvr_sub_episode },
  { .id = ";e", .getval = dvr_sub_episode },
  { .id = "c",  .getval = dvr_sub_channel },
  { .id = " c", .getval = dvr_sub_channel },
  { .id = "-c", .getval = dvr_sub_channel },
  { .id = "_c", .getval = dvr_sub_channel },
  { .id = ".c", .getval = dvr_sub_channel },
  { .id = ",c", .getval = dvr_sub_channel },
  { .id = ";c", .getval = dvr_sub_channel },
  { .id = NULL, .getval = NULL }
};

static const char *dvr_sub_strftime(const char *id, const void *aux)
{
  char fid[8], *p;
  static char buf[40];
  snprintf(fid, sizeof(fid), "%%%s", id);
  strftime(buf, sizeof(buf), fid, (struct tm *)aux);
  for (p = buf; *p; p++)
    if (*p == ':')
      *p = '-';
  return dvr_clean_directory_separator(buf);
}

static htsstr_substitute_t dvr_subs_time[] = {
  { .id = "a", .getval = dvr_sub_strftime }, /* The abbreviated name of the day of the week */
  { .id = "A", .getval = dvr_sub_strftime }, /* The full name of the day of the week */
  { .id = "b", .getval = dvr_sub_strftime }, /* The abbreviated month name */
  { .id = "B", .getval = dvr_sub_strftime }, /* The full month name */
  { .id = "c", .getval = dvr_sub_strftime }, /* The preferred date and time representation */
  { .id = "C", .getval = dvr_sub_strftime }, /* The century number (year/100) as a 2-digit integer */
  { .id = "d", .getval = dvr_sub_strftime }, /* The day of the month as a decimal number (range 01 to 31) */
  { .id = "D", .getval = dvr_sub_strftime }, /* Equivalent to %m/%d/%y */
  { .id = "e", .getval = dvr_sub_strftime }, /* The day of the month as a decimal number (range 01 to 31) */

  { .id = "Ec", .getval = dvr_sub_strftime }, /* alternatives */
  { .id = "EC", .getval = dvr_sub_strftime },
  { .id = "Ex", .getval = dvr_sub_strftime },
  { .id = "EX", .getval = dvr_sub_strftime },
  { .id = "Ey", .getval = dvr_sub_strftime },
  { .id = "EY", .getval = dvr_sub_strftime },

  { .id = "F", .getval = dvr_sub_strftime }, /* Equivalent to %m/%d/%y */
  { .id = "G", .getval = dvr_sub_strftime }, /* The ISO 8601 week-based year with century */
  { .id = "g", .getval = dvr_sub_strftime }, /* Like %G, but without century */
  { .id = "h", .getval = dvr_sub_strftime }, /* Equivalent to %b */
  { .id = "H", .getval = dvr_sub_strftime }, /* The hour (range 00 to 23) */
  { .id = "j", .getval = dvr_sub_strftime }, /* The day of the year (range 000 to 366) */
  { .id = "k", .getval = dvr_sub_strftime }, /* The hour (range 0 to 23) - with space */
  { .id = "l", .getval = dvr_sub_strftime }, /* The hour (range 1 to 12) - with space */
  { .id = "m", .getval = dvr_sub_strftime }, /* The month (range 01 to 12) */
  { .id = "M", .getval = dvr_sub_strftime }, /* The minute (range 00 to 59) */

  { .id = "Od", .getval = dvr_sub_strftime }, /* alternatives */
  { .id = "Oe", .getval = dvr_sub_strftime },
  { .id = "OH", .getval = dvr_sub_strftime },
  { .id = "OI", .getval = dvr_sub_strftime },
  { .id = "Om", .getval = dvr_sub_strftime },
  { .id = "OM", .getval = dvr_sub_strftime },
  { .id = "OS", .getval = dvr_sub_strftime },
  { .id = "Ou", .getval = dvr_sub_strftime },
  { .id = "OU", .getval = dvr_sub_strftime },
  { .id = "OV", .getval = dvr_sub_strftime },
  { .id = "Ow", .getval = dvr_sub_strftime },
  { .id = "OW", .getval = dvr_sub_strftime },
  { .id = "Oy", .getval = dvr_sub_strftime },

  { .id = "p", .getval = dvr_sub_strftime }, /* AM/PM */
  { .id = "P", .getval = dvr_sub_strftime }, /* am/pm */
  { .id = "r", .getval = dvr_sub_strftime }, /* a.m./p.m. */
  { .id = "R", .getval = dvr_sub_strftime }, /* %H:%M */
  { .id = "s", .getval = dvr_sub_strftime }, /* The number of seconds since the Epoch */
  { .id = "S", .getval = dvr_sub_strftime }, /* The seconds (range 00 to 60) */
  { .id = "T", .getval = dvr_sub_strftime }, /* %H:%M:%S */
  { .id = "u", .getval = dvr_sub_strftime }, /* The day of the week as a decimal, range 1 to 7, Monday being 1 */
  { .id = "U", .getval = dvr_sub_strftime }, /* The week number of the current year as a decimal number, range 00 to 53 (Sunday) */
  { .id = "V", .getval = dvr_sub_strftime }, /* The  ISO 8601  week  number (range 01 to 53) */
  { .id = "w", .getval = dvr_sub_strftime }, /* The day of the week as a decimal, range 0 to 6, Sunday being 0 */
  { .id = "W", .getval = dvr_sub_strftime }, /* The week number of the current year as a decimal number, range 00 to 53 (Monday) */
  { .id = "x", .getval = dvr_sub_strftime }, /* The preferred date representation */
  { .id = "X", .getval = dvr_sub_strftime }, /* The preferred time representation */
  { .id = "y", .getval = dvr_sub_strftime }, /* The year as a decimal number without a century (range 00 to 99) */
  { .id = "Y", .getval = dvr_sub_strftime }, /* The year as a decimal number including the century */
  { .id = "z", .getval = dvr_sub_strftime }, /* The +hhmm or -hhmm numeric timezone */
  { .id = "Z", .getval = dvr_sub_strftime }, /* The timezone name or abbreviation */

  { .id = NULL, .getval = NULL }
};

static const char *dvr_sub_str(const char *id, const void *aux)
{
  return (const char *)aux;
}

static const char *dvr_sub_str_separator(const char *id, const void *aux)
{
  static char buf[128];
  strncpy(buf, (const char *)aux, sizeof(buf)-1);
  buf[sizeof(buf)-1] = '\0';
  return dvr_clean_directory_separator(buf);
}

static htsstr_substitute_t dvr_subs_extension[] = {
  { .id = "x", .getval = dvr_sub_str_separator },
  { .id = NULL, .getval = NULL }
};

static htsstr_substitute_t dvr_subs_tally[] = {
  { .id = "n", .getval = dvr_sub_str_separator },
  { .id = NULL, .getval = NULL }
};

static htsstr_substitute_t dvr_subs_postproc_entry[] = {
  { .id = "t",  .getval = dvr_sub_title },
  { .id = "s",  .getval = dvr_sub_subtitle },
  { .id = "p",  .getval = dvr_sub_episode },
  { .id = "d",  .getval = dvr_sub_description },
  { .id = "c",  .getval = dvr_sub_channel },
  { .id = "e",  .getval = dvr_sub_last_error },
  { .id = "C",  .getval = dvr_sub_creator },
  { .id = "O",  .getval = dvr_sub_owner },
  { .id = "S",  .getval = dvr_sub_start },
  { .id = "E",  .getval = dvr_sub_stop },
  { .id = NULL, .getval = NULL }
};

static const char *dvr_sub_basename(const char *id, const void *aux)
{
  static char buf[PATH_MAX];
  strncpy(buf, (const char *)aux, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  return basename(buf);
}

static htsstr_substitute_t dvr_subs_postproc_filename[] = {
  { .id = "f",  .getval = dvr_sub_str },
  { .id = "b",  .getval = dvr_sub_basename },
  { .id = NULL, .getval = NULL }
};

static char *dvr_find_last_path_component(char *path)
{
  char *res, *p;
  for (p = path, res = NULL; *p; p++) {
    if (*p == '\\') {
      p++;
    } else {
      if (*p == '/')
        res = p;
    }
  }
  return res;
}

static char *dvr_find_next_path_component(char *path)
{
  char *res, *p;
  for (p = res = path; *p; p++) {
    if (*p == '\\') {
      p++;
    } else {
      if (*p == '/')
        return p + 1;
    }
  }
  return NULL;
}

/**
 * Filename generator
 *
 * - convert from utf8
 * - avoid duplicate filenames
 *
 */
static int
pvr_generate_filename(dvr_entry_t *de, const streaming_start_t *ss)
{
  char filename[PATH_MAX];
  char path[PATH_MAX];
  char ptmp[PATH_MAX];
  char number[16];
  int tally = 0;
  struct stat st;
  char *s, *x, *fmtstr, *dirsep;
  struct tm tm;
  dvr_config_t *cfg;
  htsmsg_t *m;
  size_t l, j;

  if (de == NULL)
    return -1;

  cfg = de->de_config;
  if (cfg->dvr_storage == NULL || cfg->dvr_storage == '\0')
    return -1;

  localtime_r(&de->de_start, &tm);

  strncpy(path, cfg->dvr_storage, sizeof(path));
  path[sizeof(path)-1] = '\0';
  l = strlen(path);
  if (l + 1 >= sizeof(path)) {
    tvherror("dvr", "wrong storage path");
    return -1;
  }

  /* Remove trailing slash */
  while (l > 0 && path[l-1] == '/')
    path[--l] = '\0';
  if (l + 1 >= sizeof(path))
    l--;
  path[l++] = '/';
  path[l] = '\0';

  fmtstr = cfg->dvr_pathname;
  while (*fmtstr == '/')
    fmtstr++;

  /* Substitute DVR entry formatters */
  htsstr_substitute(fmtstr, path + l, sizeof(path) - l, '$', dvr_subs_entry, de);

  /* Own directory? */
  if (de->de_directory) {
    dirsep = dvr_find_last_path_component(path + l);
    if (dirsep)
      strcpy(filename, dirsep + 1);
    else
      strcpy(filename, path + l);
    htsstr_substitute(de->de_directory, ptmp, sizeof(ptmp), '$', dvr_subs_entry, de);
    s = ptmp;
    while (*s == '/')
      s++;
    j = strlen(s);
    while (j >= 0 && s[j-1] == '/')
      j--;
    s[j] = '\0';
    snprintf(path + l, sizeof(path) - l, "%s", s);
    snprintf(path + l + j, sizeof(path) - l + j, "/%s", filename);
  }

  /* Substitute time formatters */
  htsstr_substitute(path + l, filename, sizeof(filename), '%', dvr_subs_time, &tm);

  /* Substitute extension */
  htsstr_substitute(filename, path + l, sizeof(path) - l, '$', dvr_subs_extension,
                    muxer_suffix(de->de_chain->prch_muxer, ss) ?: "");

  /* Cleanup all directory names */
  x = path + l;
  filename[j = 0] = '\0';
  while (1) {
    dirsep = dvr_find_next_path_component(x);
    if (dirsep == NULL || *dirsep == '\0')
      break;
    *(dirsep - 1) = '\0';
    if (*x) {
      s = cleanup_filename(cfg, x);
      tvh_strlcatf(filename, sizeof(filename), j, "%s/", s);
      free(s);
    }
    x = dirsep;
  }
  tvh_strlcatf(filename, sizeof(filename), j, "%s", x);
  snprintf(path + l, sizeof(path) - l, "%s", filename);

  /* Deescape directory path and create directory tree */
  dirsep = dvr_find_last_path_component(path + l);
  if (dirsep) {
    *dirsep = '\0';
    dirsep++;
  } else {
    if (l > 0) {
      assert(path[l-1] == '/');
      path[l-1] = '\0';
    }
    dirsep = path + l;
  }
  htsstr_unescape_to(path, filename, sizeof(filename));
  if (makedirs(filename, cfg->dvr_muxcnf.m_directory_permissions, -1, -1) != 0)
    return -1;
  j = strlen(filename);
  snprintf(filename + j, sizeof(filename) - j, "/%s", dirsep);
  if (filename[j] == '/')
    j++;
  
  /* Unique filename loop */
  while (1) {

    /* Prepare the name portion */
    if (tally > 0) {
      snprintf(number, sizeof(number), "-%d", tally);
    } else {
      number[0] = '\0';
    }
    htsstr_substitute(filename + j, ptmp, sizeof(ptmp), '$', dvr_subs_tally, number);
    s = cleanup_filename(cfg, ptmp);
    if (s == NULL)
      return -1;

    /* Construct the final filename */
    memcpy(path, filename, j);
    path[j] = '\0';
    htsstr_unescape_to(s, path + j, sizeof(path) - j);

    if (tally > 0) {
      htsstr_unescape_to(filename + j, ptmp, sizeof(ptmp));
      if (strcmp(ptmp, s) == 0) {
        free(s);
        tvherror("dvr", "unable to create unique name (missing $n in format string?)");
        return -1;
      }
    }

    free(s);

    if(stat(path, &st) == -1) {
      tvhlog(LOG_DEBUG, "dvr", "File \"%s\" -- %s -- Using for recording",
	     path, strerror(errno));
      break;
    }

    tvhlog(LOG_DEBUG, "dvr", "Overwrite protection, file \"%s\" exists",
	   path);

    tally++;
  }

  if (de->de_files == NULL)
    de->de_files = htsmsg_create_list();
  m = htsmsg_create_map();
  htsmsg_add_str(m, "filename", path);
  htsmsg_add_msg(de->de_files, NULL, m);

  return 0;
}

/**
 *
 */
static void
dvr_rec_fatal_error(dvr_entry_t *de, const char *fmt, ...)
{
  char msgbuf[256];

  va_list ap;
  va_start(ap, fmt);

  vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
  va_end(ap);

  tvhlog(LOG_ERR, "dvr", 
	 "Recording error: \"%s\": %s",
	 dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL), msgbuf);
}

/**
 *
 */
static void
dvr_notify(dvr_entry_t *de)
{
  if (de->de_last_notify + 5 < dispatch_clock) {
    idnode_notify_changed(&de->de_id);
    de->de_last_notify = dispatch_clock;
    htsp_dvr_entry_update(de);
  }
}

/**
 *
 */
static void
dvr_rec_set_state(dvr_entry_t *de, dvr_rs_state_t newstate, int error)
{
  if (de->de_last_error != error && error)
    de->de_errors++;
  dvr_entry_set_state(de, de->de_sched_state, newstate, error);
}

/**
 *
 */
static int
dvr_rec_start(dvr_entry_t *de, const streaming_start_t *ss)
{
  const source_info_t *si = &ss->ss_si;
  const streaming_start_component_t *ssc;
  int i;
  dvr_config_t *cfg = de->de_config;
  profile_chain_t *prch = de->de_chain;
  muxer_t *muxer;

  if (!cfg) {
    dvr_rec_fatal_error(de, "Unable to determine config profile");
    return -1;
  }

  if (!prch) {
    dvr_rec_fatal_error(de, "Unable to determine stream profile");
    return -1;
  }

  if (!(muxer = prch->prch_muxer)) {
    if (profile_chain_reopen(prch, &cfg->dvr_muxcnf, 0)) {
      dvr_rec_fatal_error(de, "Unable to reopen muxer");
      return -1;
    }
    muxer = prch->prch_muxer;
  }

  if(!muxer) {
    dvr_rec_fatal_error(de, "Unable to create muxer");
    return -1;
  }

  if(pvr_generate_filename(de, ss) != 0) {
    dvr_rec_fatal_error(de, "Unable to create file");
    return -1;
  }

  if(muxer_open_file(muxer, dvr_get_filename(de))) {
    dvr_rec_fatal_error(de, "Unable to open file");
    return -1;
  }

  if(muxer_init(muxer, ss, lang_str_get(de->de_title, NULL))) {
    dvr_rec_fatal_error(de, "Unable to init file");
    return -1;
  }

  if(cfg->dvr_tag_files) {
    if(muxer_write_meta(muxer, de->de_bcast, de->de_comment)) {
      dvr_rec_fatal_error(de, "Unable to write meta data");
      return -1;
    }
  }

  tvhlog(LOG_INFO, "dvr", "%s from "
	 "adapter: \"%s\", "
	 "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	 "service: \"%s\"",
		
	 dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
	 si->si_adapter  ?: "<N/A>",
	 si->si_network  ?: "<N/A>",
	 si->si_mux      ?: "<N/A>",
	 si->si_provider ?: "<N/A>",
	 si->si_service  ?: "<N/A>");


  tvhlog(LOG_INFO, "dvr",
	 " #  %-16s  %-4s  %-10s  %-12s  %-11s  %-8s",
	 "type",
	 "lang",
	 "resolution",
	 "aspect ratio",
	 "sample rate",
	 "channels");

  for(i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];

    char res[11];
    char asp[6];
    char sr[6];
    char ch[7];

    if(SCT_ISAUDIO(ssc->ssc_type)) {
      if(ssc->ssc_sri)
	snprintf(sr, sizeof(sr), "%d", sri_to_rate(ssc->ssc_sri));
      else
	strcpy(sr, "?");

      if(ssc->ssc_channels == 6)
	snprintf(ch, sizeof(ch), "5.1");
      else if(ssc->ssc_channels == 0)
	strcpy(ch, "?");
      else
	snprintf(ch, sizeof(ch), "%d", ssc->ssc_channels);
    } else {
      sr[0] = 0;
      ch[0] = 0;
    }

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      if(ssc->ssc_width && ssc->ssc_height)
	snprintf(res, sizeof(res), "%dx%d",
		 ssc->ssc_width, ssc->ssc_height);
      else
	strcpy(res, "?");
    } else {
      res[0] = 0;
    }

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      if(ssc->ssc_aspect_num &&  ssc->ssc_aspect_den)
	snprintf(asp, sizeof(asp), "%d:%d",
		 ssc->ssc_aspect_num, ssc->ssc_aspect_den);
      else
	strcpy(asp, "?");
    } else {
      asp[0] = 0;
    }

    tvhlog(LOG_INFO, "dvr",
	   "%2d  %-16s  %-4s  %-10s  %-12s  %-11s  %-8s  %s",
	   ssc->ssc_index,
	   streaming_component_type2txt(ssc->ssc_type),
	   ssc->ssc_lang,
	   res,
	   asp,
	   sr,
	   ch,
	   ssc->ssc_disabled ? "<disabled, no valid input>" : "");
  }

  return 0;
}


/**
 *
 */
static void *
dvr_thread(void *aux)
{
  dvr_entry_t *de = aux;
  dvr_config_t *cfg = de->de_config;
  profile_chain_t *prch = de->de_chain;
  streaming_queue_t *sq = &prch->prch_sq;
  streaming_message_t *sm;
  th_subscription_t *ts;
  th_pkt_t *pkt;
  int run = 1;
  int started = 0;
  int comm_skip = cfg->dvr_skip_commercials;
  int commercial = COMMERCIAL_UNKNOWN;

  pthread_mutex_lock(&sq->sq_mutex);

  while(run) {
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      pthread_cond_wait(&sq->sq_cond, &sq->sq_mutex);
      continue;
    }

    if ((ts = de->de_s) != NULL && started) {
      pktbuf_t *pb = NULL;
      if (sm->sm_type == SMT_PACKET) {
        pb = ((th_pkt_t*)sm->sm_data)->pkt_payload;
        if (((th_pkt_t*)sm->sm_data)->pkt_err) {
          de->de_data_errors += ((th_pkt_t*)sm->sm_data)->pkt_err;
          dvr_notify(de);
        }
      }
      else if (sm->sm_type == SMT_MPEGTS) {
        pb = sm->sm_data;
        if (pb->pb_err) {
          de->de_data_errors += pb->pb_err;
          dvr_notify(de);
        }
      }
      if (pb)
        atomic_add(&ts->ths_bytes_out, pktbuf_len(pb));
    }

    TAILQ_REMOVE(&sq->sq_queue, sm, sm_link);

    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {

    case SMT_PACKET:
      pkt = sm->sm_data;
      if(pkt->pkt_commercial == COMMERCIAL_YES)
	dvr_rec_set_state(de, DVR_RS_COMMERCIAL, 0);
      else
	dvr_rec_set_state(de, DVR_RS_RUNNING, 0);

      if(pkt->pkt_commercial == COMMERCIAL_YES && comm_skip)
	break;

      if(commercial != pkt->pkt_commercial)
	muxer_add_marker(prch->prch_muxer);

      commercial = pkt->pkt_commercial;

      if(started) {
	muxer_write_pkt(prch->prch_muxer, sm->sm_type, sm->sm_data);
	sm->sm_data = NULL;
	dvr_notify(de);
      }
      break;

    case SMT_MPEGTS:
      if(started) {
	dvr_rec_set_state(de, DVR_RS_RUNNING, 0);
	muxer_write_pkt(prch->prch_muxer, sm->sm_type, sm->sm_data);
	sm->sm_data = NULL;
	dvr_notify(de);
      }
      break;

    case SMT_START:
      if(started &&
	 muxer_reconfigure(prch->prch_muxer, sm->sm_data) < 0) {
	tvhlog(LOG_WARNING,
	       "dvr", "Unable to reconfigure \"%s\"",
	       dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL));

	// Try to restart the recording if the muxer doesn't
	// support reconfiguration of the streams.
	dvr_thread_epilog(de);
	started = 0;
      }

      if(!started) {
        pthread_mutex_lock(&global_lock);
        dvr_rec_set_state(de, DVR_RS_WAIT_PROGRAM_START, 0);
        if(dvr_rec_start(de, sm->sm_data) == 0)
          started = 1;
        else
          dvr_stop_recording(de, SM_CODE_INVALID_TARGET, 1);
        pthread_mutex_unlock(&global_lock);
      } 
      break;

    case SMT_STOP:
       if(sm->sm_code == SM_CODE_SOURCE_RECONFIGURED) {
	 // Subscription is restarting, wait for SMT_START

       } else if(sm->sm_code == 0) {
	 // Recording is completed

	dvr_entry_set_state(de, de->de_sched_state, de->de_rec_state, SM_CODE_OK);
	tvhlog(LOG_INFO, 
	       "dvr", "Recording completed: \"%s\"",
	       dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL));

	dvr_thread_epilog(de);
	started = 0;

      }else if(de->de_last_error != sm->sm_code) {
	 // Error during recording

	 dvr_rec_set_state(de, DVR_RS_ERROR, sm->sm_code);
	 tvhlog(LOG_ERR,
		"dvr", "Recording stopped: \"%s\": %s",
		dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
		streaming_code2txt(sm->sm_code));

	 dvr_thread_epilog(de);
	 started = 0;
      }
      break;

    case SMT_SERVICE_STATUS:
      if(sm->sm_code & TSS_PACKETS) {
	
      } else if(sm->sm_code & TSS_ERRORS) {

	int code = SM_CODE_UNDEFINED_ERROR;

	if(sm->sm_code & TSS_NO_DESCRAMBLER)
	  code = SM_CODE_NO_DESCRAMBLER;

	if(sm->sm_code & TSS_NO_ACCESS)
	  code = SM_CODE_NO_ACCESS;

	if(de->de_last_error != code) {
	  dvr_rec_set_state(de, DVR_RS_ERROR, code);
	  tvhlog(LOG_ERR,
		 "dvr", "Streaming error: \"%s\": %s",
		 dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
		 streaming_code2txt(code));
	}
      }
      break;

    case SMT_NOSTART:

      if(de->de_last_error != sm->sm_code) {
	dvr_rec_set_state(de, DVR_RS_PENDING, sm->sm_code);

	tvhlog(LOG_ERR,
	       "dvr", "Recording unable to start: \"%s\": %s",
	       dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
	       streaming_code2txt(sm->sm_code));
      }
      break;

    case SMT_GRACE:
    case SMT_SPEED:
    case SMT_SKIP:
    case SMT_SIGNAL_STATUS:
    case SMT_TIMESHIFT_STATUS:
      break;

    case SMT_EXIT:
      run = 0;
      break;
    }

    streaming_msg_free(sm);
    pthread_mutex_lock(&sq->sq_mutex);
  }
  pthread_mutex_unlock(&sq->sq_mutex);

  if(prch->prch_muxer)
    dvr_thread_epilog(de);

  return NULL;
}

/**
 *
 */
static void
dvr_spawn_postproc(dvr_entry_t *de, const char *dvr_postproc)
{
  char buf1[2048], *buf2;
  const char *filename;
  char **args;

  filename = dvr_get_filename(de);
  if (filename == NULL)
    return;

  /* Substitute DVR entry formatters */
  htsstr_substitute(dvr_postproc, buf1, sizeof(buf1), '%', dvr_subs_postproc_entry, de);
  buf2 = tvh_strdupa(buf1);
  /* Substitute filename formatters */
  htsstr_substitute(buf2, buf1, sizeof(buf1), '%', dvr_subs_postproc_filename, filename);

  args = htsstr_argsplit(buf1);
  /* no arguments at all */
  if(!args[0]) {
    htsstr_argsplit_free(args);
    return;
  }

  spawnv(args[0], (void *)args, NULL, 1, 1);
    
  htsstr_argsplit_free(args);
}

/**
 *
 */
static void
dvr_thread_epilog(dvr_entry_t *de)
{
  profile_chain_t *prch = de->de_chain;

  muxer_close(prch->prch_muxer);
  muxer_destroy(prch->prch_muxer);
  prch->prch_muxer = NULL;

  dvr_config_t *cfg = de->de_config;
  if(cfg && cfg->dvr_postproc)
    dvr_spawn_postproc(de,cfg->dvr_postproc);
}

/**
 *
 */
static int64_t dvr_bfree;
static int64_t dvr_btotal;
static pthread_mutex_t dvr_disk_space_mutex;
static gtimer_t dvr_disk_space_timer;
static tasklet_t dvr_disk_space_tasklet;

/**
 *
 */
static void
dvr_get_disk_space_update(const char *path)
{
  struct statvfs diskdata;

  if(statvfs(path, &diskdata) == -1)
    return;

  dvr_bfree = diskdata.f_bsize * (int64_t)diskdata.f_bavail;
  dvr_btotal = diskdata.f_bsize * (int64_t)diskdata.f_blocks;
}

/**
 *
 */
static void
dvr_get_disk_space_tcb(void *s, int dearmed)
{
  dvr_config_t *cfg;
  htsmsg_t *m;
  char *path;

  pthread_mutex_lock(&global_lock);
  cfg = dvr_config_find_by_name_default(NULL);
  path = tvh_strdupa(cfg->dvr_storage);
  pthread_mutex_unlock(&global_lock);

  m = htsmsg_create_map();
  pthread_mutex_lock(&dvr_disk_space_mutex);
  dvr_get_disk_space_update(path);
  htsmsg_add_s64(m, "freediskspace", dvr_bfree);
  htsmsg_add_s64(m, "totaldiskspace", dvr_btotal);
  pthread_mutex_unlock(&dvr_disk_space_mutex);

  notify_by_msg("diskspaceUpdate", m);
}

static void
dvr_get_disk_space_cb(void *aux)
{
  tasklet_arm(&dvr_disk_space_tasklet, dvr_get_disk_space_tcb, NULL);
  gtimer_arm(&dvr_disk_space_timer, dvr_get_disk_space_cb, NULL, 60);
}

/**
 *
 */
void
dvr_disk_space_init(void)
{
  dvr_config_t *cfg = dvr_config_find_by_name_default(NULL);
  pthread_mutex_init(&dvr_disk_space_mutex, NULL);
  dvr_get_disk_space_update(cfg->dvr_storage);
  gtimer_arm(&dvr_disk_space_timer, dvr_get_disk_space_cb, NULL, 60);
}

/**
 *
 */
void
dvr_disk_space_done(void)
{
  tasklet_disarm(&dvr_disk_space_tasklet);
  gtimer_disarm(&dvr_disk_space_timer);
}

/**
 *
 */
int
dvr_get_disk_space(int64_t *bfree, int64_t *btotal)
{
  int res = 0;

  pthread_mutex_lock(&dvr_disk_space_mutex);
  if (dvr_bfree || dvr_btotal) {
    *bfree = dvr_bfree;
    *btotal = dvr_btotal;
  } else {
    res = -EINVAL;
  }
  pthread_mutex_unlock(&dvr_disk_space_mutex);
  return res;
}
