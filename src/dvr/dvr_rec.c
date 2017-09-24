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

#include <stdarg.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <libgen.h> /* basename */

#include "htsstr.h"

#include "tvheadend.h"
#include "streaming.h"
#include "tcp.h"
#include "dvr.h"
#include "spawn.h"
#include "service.h"
#include "htsp_server.h"
#include "atomic.h"
#include "intlconv.h"
#include "notify.h"

#include "muxer.h"

/**
 *
 */
static void *dvr_thread(void *aux);
static void dvr_thread_epilog(dvr_entry_t *de, const char *dvr_postproc);


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
  struct sockaddr_storage sa;
  access_t *aa;
  uint32_t rec_count, net_count;
  int pri, c1, c2;

  assert(de->de_s == NULL);
  assert(de->de_chain == NULL);

  pri = de->de_pri;
  if(pri == DVR_PRIO_NOTSET || pri == DVR_PRIO_DEFAULT)
    pri = de->de_config->dvr_pri;
  if(pri < 0 || pri >= ARRAY_SIZE(prio2weight))
    pri = DVR_PRIO_NORMAL;
  weight = prio2weight[pri];

  snprintf(buf, sizeof(buf), "DVR: %s", lang_str_get(de->de_title, NULL));

  if (de->de_owner && de->de_owner[0] != '\0')
    aa = access_get_by_username(de->de_owner);
  else if (de->de_creator && de->de_creator[0] != '\0' &&
           tcp_get_ip_from_str(de->de_creator, &sa) != NULL)
    aa = access_get_by_addr(&sa);
  else {
    tvherror(LS_DVR, "unable to find access (owner '%s', creator '%s')",
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
      tvherror(LS_DVR, "multiple connections are not allowed for user '%s' from '%s' "
                      "(limit %u, dvr limit %u, active DVR %u, streaming %u)",
               aa->aa_username ?: "", aa->aa_representative ?: "",
               aa->aa_conn_limit, aa->aa_conn_limit_dvr, rec_count, net_count);
      access_destroy(aa);
      return -EOVERFLOW;
    }
  }
  access_destroy(aa);

  pro = de->de_config->dvr_profile;
  prch = malloc(sizeof(*prch));
  profile_chain_init(prch, pro, de->de_channel);
  if (profile_chain_open(prch, &de->de_config->dvr_muxcnf, NULL, 0, 0)) {
    profile_chain_close(prch);
    tvherror(LS_DVR, "unable to create new channel streaming chain '%s' for '%s', using default",
             profile_get_name(pro), channel_get_name(de->de_channel, channel_blank_name));
    pro = profile_find_by_name(NULL, NULL);
    profile_chain_init(prch, pro, de->de_channel);
    if (profile_chain_open(prch, &de->de_config->dvr_muxcnf, NULL, 0, 0)) {
      tvherror(LS_DVR, "unable to create channel streaming default chain '%s' for '%s'",
               profile_get_name(pro), channel_get_name(de->de_channel, channel_blank_name));
      profile_chain_close(prch);
      free(prch);
      return -EINVAL;
    }
  }

  de->de_s = subscription_create_from_channel(prch, NULL, weight,
					      buf, prch->prch_flags,
					      NULL, NULL, NULL, NULL);
  if (de->de_s == NULL) {
    tvherror(LS_DVR, "unable to create new channel subcription for '%s' profile '%s'",
             channel_get_name(de->de_channel, channel_blank_name), profile_get_name(pro));
    profile_chain_close(prch);
    free(prch);
    return -EINVAL;
  }

  de->de_chain = prch;

  atomic_set(&de->de_thread_shutdown, 0);
  tvhthread_create(&de->de_thread, NULL, dvr_thread, de, "dvr");

  if (de->de_config->dvr_preproc)
    dvr_spawn_cmd(de, de->de_config->dvr_preproc, NULL, 1);
  return 0;
}

/**
 *
 */
void
dvr_rec_unsubscribe(dvr_entry_t *de)
{
  profile_chain_t *prch = de->de_chain;
  char *postproc = NULL;

  assert(de->de_s != NULL);
  assert(prch != NULL);

  de->de_in_unsubscribe = 1;

  streaming_target_deliver(prch->prch_st, streaming_msg_create(SMT_EXIT));

  atomic_add(&de->de_thread_shutdown, 1);

  pthread_join(de->de_thread, (void **)&postproc);

  if (prch->prch_muxer)
    dvr_thread_epilog(de, postproc);

  free(postproc);

  subscription_unsubscribe(de->de_s, UNSUBSCRIBE_FINAL);
  de->de_s = NULL;

  de->de_chain = NULL;
  profile_chain_close(prch);
  free(prch);

  dvr_vfs_refresh_entry(de);

  de->de_in_unsubscribe = 0;
}

/**
 *
 */
void
dvr_rec_migrate(dvr_entry_t *de_old, dvr_entry_t *de_new)
{
  lock_assert(&global_lock);

  de_new->de_s = de_old->de_s;
  de_new->de_chain = de_old->de_chain;
  de_new->de_thread = de_old->de_thread;
  de_old->de_s = NULL;
  de_old->de_chain = NULL;
  de_old->de_thread = 0;
}

/**
 * Replace various chars with a dash
 * - dosubs specifies if user demanded substitutions are performed
 */
static char *
cleanup_filename(dvr_config_t *cfg, char *s, int dosubs)
{
  int len = strlen(s);
  char *s1, *p;

  s1 = intlconv_utf8safestr(cfg->dvr_charset_id, s, (len * 2) + 1);
  if (s1 == NULL) {
    tvherror(LS_DVR, "Unsupported charset %s using ASCII", cfg->dvr_charset);
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
             (*s == ' ' || *s == '\t') &&
             dosubs)
      *s = '-';

    else if (cfg->dvr_clean_title &&
             ((*s < 32) || (*s > 122) ||
             (strchr("/:\\<>|*?\"", *s) != NULL)) &&
             dosubs)
      *s = '_';

    else if (cfg->dvr_windows_compatible_filenames &&
             (strchr("/:\\<>|*?\"", *s) != NULL) &&
             dosubs)
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
static char *
dvr_clean_directory_separator(char *s, char *tmp, size_t tmplen)
{
  char *p, *end;

  if (s != tmp) {
    end = tmp + tmplen - 1;
    /* replace directory separator */
    for (p = tmp; *s && p != end; s++, p++)
      if (*s == '/')
        *p = '-';
      else if (*s == '"')
        *p = '\'';
      else
        *p = *s;
    *p = '\0';
    return tmp;
  } else  {
    for (; *s; s++)
      if (*s == '/')
        *s = '-';
      else if (*s == '"')
        *s = '\'';
    return tmp;
  }
}

static const char *
dvr_do_prefix(const char *id, const char *fmt, const char *s, char *tmp, size_t tmplen)
{
  if (id[0] == '?') {
    id++;
    if (fmt && *fmt >= '0' && *fmt <= '9') {
      long l = strtol(fmt, NULL, 10);
      if (l && tmplen > l)
        tmplen = l;
    }
  }
  if (s == NULL) {
    tmp[0] = '\0';
  } else if (s[0] && !isalpha(id[0])) {
    snprintf(tmp, tmplen, "%c%s", id[0], s);
  } else {
    strncpy(tmp, s, tmplen-1);
    tmp[tmplen-1] = '\0';
  }
  return dvr_clean_directory_separator(tmp, tmp, tmplen);
}


static const char *
dvr_sub_title(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, lang_str_get(((dvr_entry_t *)aux)->de_title, NULL), tmp, tmplen);
}

static const char *
dvr_sub_subtitle(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, lang_str_get(((dvr_entry_t *)aux)->de_subtitle, NULL), tmp, tmplen);
}

static const char *
dvr_sub_description(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, lang_str_get(((dvr_entry_t *)aux)->de_desc, NULL), tmp, tmplen);
}

static const char *
dvr_sub_episode(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  const dvr_entry_t *de = aux;
  char buf[64];

  if (de->de_bcast == NULL || de->de_bcast->episode == NULL)
    return "";
  epg_episode_number_format(de->de_bcast->episode,
                            buf, sizeof(buf),
                            NULL, "S%02d", NULL, "E%02d", NULL);
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_channel(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, DVR_CH_NAME((dvr_entry_t *)aux), tmp, tmplen);
}

static const char *
dvr_sub_genre(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  const dvr_entry_t *de = aux;
  epg_genre_t *genre;
  char buf[64];

  if (de->de_bcast == NULL || de->de_bcast->episode == NULL)
    return "";
  genre = LIST_FIRST(&de->de_bcast->episode->genre);
  if (!genre || !genre->code)
    return "";
  epg_genre_get_str(genre, 0, 1, buf, sizeof(buf), "en");
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_owner(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, ((dvr_entry_t *)aux)->de_owner, tmp, tmplen);
}

static const char *
dvr_sub_creator(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, ((dvr_entry_t *)aux)->de_creator, tmp, tmplen);
}

static const char *
dvr_sub_last_error(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, streaming_code2txt(((dvr_entry_t *)aux)->de_last_error), tmp, tmplen);
}

static const char *
dvr_sub_start(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRItime_t, (time_t)dvr_entry_get_start_time((dvr_entry_t *)aux, 0));
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_errors(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRIu32, (uint32_t)((dvr_entry_t *)aux)->de_errors);
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_data_errors(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRIu32, (uint32_t)((dvr_entry_t *)aux)->de_data_errors);
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_stop(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%"PRItime_t, (time_t)dvr_entry_get_stop_time((dvr_entry_t *)aux));
  return dvr_do_prefix(id, fmt, buf, tmp, tmplen);
}

static const char *
dvr_sub_comment(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return dvr_do_prefix(id, fmt, ((dvr_entry_t *)aux)->de_comment, tmp, tmplen);
}

static htsstr_substitute_t dvr_subs_entry[] = {
  { .id = "?t",  .getval = dvr_sub_title },
  { .id = "? t", .getval = dvr_sub_title },
  { .id = "?-t", .getval = dvr_sub_title },
  { .id = "?_t", .getval = dvr_sub_title },
  { .id = "?.t", .getval = dvr_sub_title },
  { .id = "?,t", .getval = dvr_sub_title },
  { .id = "?;t", .getval = dvr_sub_title },
  { .id = "?s",  .getval = dvr_sub_subtitle },
  { .id = "? s", .getval = dvr_sub_subtitle },
  { .id = "?-s", .getval = dvr_sub_subtitle },
  { .id = "?_s", .getval = dvr_sub_subtitle },
  { .id = "?.s", .getval = dvr_sub_subtitle },
  { .id = "?,s", .getval = dvr_sub_subtitle },
  { .id = "?;s", .getval = dvr_sub_subtitle },
  { .id = "e",   .getval = dvr_sub_episode },
  { .id = " e",  .getval = dvr_sub_episode },
  { .id = "-e",  .getval = dvr_sub_episode },
  { .id = "_e",  .getval = dvr_sub_episode },
  { .id = ".e",  .getval = dvr_sub_episode },
  { .id = ",e",  .getval = dvr_sub_episode },
  { .id = ";e",  .getval = dvr_sub_episode },
  { .id = "c",   .getval = dvr_sub_channel },
  { .id = " c",  .getval = dvr_sub_channel },
  { .id = "-c",  .getval = dvr_sub_channel },
  { .id = "_c",  .getval = dvr_sub_channel },
  { .id = ".c",  .getval = dvr_sub_channel },
  { .id = ",c",  .getval = dvr_sub_channel },
  { .id = ";c",  .getval = dvr_sub_channel },
  { .id = "g",   .getval = dvr_sub_genre },
  { .id = " g",  .getval = dvr_sub_genre },
  { .id = "-g",  .getval = dvr_sub_genre },
  { .id = "_g",  .getval = dvr_sub_genre },
  { .id = ".g",  .getval = dvr_sub_genre },
  { .id = ",g",  .getval = dvr_sub_genre },
  { .id = ";g",  .getval = dvr_sub_genre },
  { .id = NULL,  .getval = NULL }
};

static const char *
dvr_sub_strftime(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  char fid[8], *p;
  snprintf(fid, sizeof(fid), "%%%s", id);
  strftime(tmp, tmplen, fid, (struct tm *)aux);
  for (p = tmp; *p; p++)
    if (*p == ':')
      *p = '-';
  return dvr_clean_directory_separator(tmp, tmp, tmplen);
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

static const char *
dvr_sub_str(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  return (const char *)aux;
}

static const char *
dvr_sub_str_separator(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  strncpy(tmp, (const char *)aux, tmplen-1);
  tmp[tmplen-1] = '\0';
  return dvr_clean_directory_separator(tmp, tmp, tmplen);
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
  { .id = "g",  .getval = dvr_sub_genre },
  { .id = "c",  .getval = dvr_sub_channel },
  { .id = "e",  .getval = dvr_sub_last_error },
  { .id = "C",  .getval = dvr_sub_creator },
  { .id = "O",  .getval = dvr_sub_owner },
  { .id = "S",  .getval = dvr_sub_start },
  { .id = "E",  .getval = dvr_sub_stop },
  { .id = "r",  .getval = dvr_sub_errors },
  { .id = "R",  .getval = dvr_sub_data_errors },
  { .id = "Z",  .getval = dvr_sub_comment },
  { .id = NULL, .getval = NULL }
};

static const char *
dvr_sub_basename(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  strncpy(tmp, (const char *)aux, tmplen);
  tmp[tmplen-1] = '\0';
  return basename(tmp);
}

static htsstr_substitute_t dvr_subs_postproc_filename[] = {
  { .id = "f",  .getval = dvr_sub_str },
  { .id = "b",  .getval = dvr_sub_basename },
  { .id = NULL, .getval = NULL }
};

static const char *
dvr_sub_basic_info(const char *id, const char *fmt, const void *aux, char *tmp, size_t tmplen)
{
  htsmsg_t *info = (htsmsg_t *)aux, *e;
  htsmsg_field_t *f;
  const char *s;
  size_t l = 0;

  if (info)
    info = htsmsg_get_list(info, "info");
  if (!info)
    return "";

  tmp[0] = '\0';
  HTSMSG_FOREACH(f, info) {
    if (!(e = htsmsg_field_get_map(f))) continue;
    if ((s = htsmsg_get_str(e, "type")) != NULL)
      tvh_strlcatf(tmp, tmplen, l, "%s%s", l > 0 ? "," : "", s);
  }
  return tmp;
}

static htsstr_substitute_t dvr_subs_postproc_info[] = {
  { .id = "i",  .getval = dvr_sub_basic_info },
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
  char tmp[MAX(PATH_MAX, 512)];
  char *lastpath = NULL;
  int tally = 0;
  struct stat st;
  char *s, *x, *fmtstr, *dirsep;
  struct tm tm;
  dvr_config_t *cfg;
  htsmsg_t *m;
  size_t l, j, k;
  long max;
  int dir_dosubs;

  if (de == NULL)
    return -1;

  cfg = de->de_config;
  if (cfg->dvr_storage == NULL || *(cfg->dvr_storage) == '\0')
    return -1;

  dir_dosubs = de->de_directory == NULL ||
              (de->de_directory[0] == '$' &&
               de->de_directory[1] == '$');

  localtime_r(&de->de_start, &tm);

  strncpy(path, cfg->dvr_storage, sizeof(path));
  path[sizeof(path)-1] = '\0';
  l = strlen(path);
  if (l + 1 >= sizeof(path)) {
    tvherror(LS_DVR, "wrong storage path");
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
  htsstr_substitute(fmtstr, path + l, sizeof(path) - l, '$', dvr_subs_entry, de, tmp, sizeof(tmp));

  /* Own directory? */
  if (de->de_directory) {
    dirsep = dvr_find_last_path_component(path + l);
    if (dirsep)
      strcpy(filename, dirsep + 1);
    else
      strcpy(filename, path + l);
    if (dir_dosubs) {
      htsstr_substitute(de->de_directory+2, ptmp, sizeof(ptmp), '$', dvr_subs_entry, de, tmp, sizeof(tmp));
    } else {
      strncpy(ptmp, de->de_directory, sizeof(ptmp)-1);
      ptmp[sizeof(ptmp)-1] = '\0';
    }
    s = ptmp;
    while (*s == '/')
      s++;
    j = strlen(s);
    while (j > 0 && s[j-1] == '/')
      j--;
    s[j] = '\0';
    snprintf(path + l, sizeof(path) - l, "%s", s);
    snprintf(path + l + j, sizeof(path) - l + j, "/%s", filename);
  }

  /* Substitute time formatters */
  htsstr_substitute(path + l, filename, sizeof(filename), '%', dvr_subs_time, &tm, tmp, sizeof(tmp));

  /* Substitute extension */
  htsstr_substitute(filename, path + l, sizeof(path) - l, '$', dvr_subs_extension,
                    muxer_suffix(de->de_chain->prch_muxer, ss) ?: "", tmp, sizeof(tmp));

  /* Cleanup all directory names */
  x = path + l;
  filename[j = 0] = '\0';
  while (1) {
    dirsep = dvr_find_next_path_component(x);
    if (dirsep == NULL || *dirsep == '\0')
      break;
    *(dirsep - 1) = '\0';
    if (*x) {
      s = cleanup_filename(cfg, x, dir_dosubs);
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
  if (makedirs(LS_DVR, filename,
               cfg->dvr_muxcnf.m_directory_permissions, 0, -1, -1) != 0)
    return -1;
  max = pathconf(filename, _PC_NAME_MAX);
  if (max < 8)
    max = NAME_MAX;
  if (max > 255 && cfg->dvr_windows_compatible_filenames)
    max = 255;
  max -= 2;
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
    /* Check the maximum filename length */
    l = strlen(number);
    k = strlen(filename + j);
    if (l + k > max) {
      s = (char *)htsstr_substitute_find(filename + j, '$');
      if (s == NULL || s - (filename + j) < (l + k) - max) {
cut1:
        l = j + (max - l);
        if (filename[l - 1] == '$') /* not optimal */
          filename[l + 1] = '\0';
        else
          filename[l] = '\0';
      } else {
        x = (char *)htsstr_escape_find(filename + j, s - (filename + j) - ((l + k) - max));
        if (x == NULL)
          goto cut1;
        k = strlen(s);
        memmove(x, s, k);
        x[k] = '\0';
      }
    }

    htsstr_substitute(filename + j, ptmp, sizeof(ptmp), '$', dvr_subs_tally, number, tmp, sizeof(tmp));
    s = cleanup_filename(cfg, ptmp, 1);
    if (s == NULL) {
      free(lastpath);
      return -1;
    }

    /* Construct the final filename */
    memcpy(path, filename, j);
    path[j] = '\0';
    htsstr_unescape_to(s, path + j, sizeof(path) - j);
    free(s);

    if (lastpath) {
      if (strcmp(path, lastpath) == 0) {
        free(lastpath);
        tvherror(LS_DVR, "unable to create unique name (missing $n in format string?)");
        return -1;
      }
    }

    if(stat(path, &st) == -1) {
      tvhdebug(LS_DVR, "File \"%s\" -- %s -- Using for recording",
	       path, strerror(errno));
      break;
    }

    tvhdebug(LS_DVR, "Overwrite protection, file \"%s\" exists", path);

    free(lastpath);
    lastpath = strdup(path);
    tally++;
  }

  free(lastpath);
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

  tvherror(LS_DVR, "Recording error: \"%s\": %s",
	   dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL), msgbuf);
}

/**
 *
 */
static void
dvr_notify(dvr_entry_t *de)
{
  if (de->de_last_notify + sec2mono(5) < mclk()) {
    idnode_notify_changed(&de->de_id);
    de->de_last_notify = mclk();
    htsp_dvr_entry_update_stats(de);
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
  streaming_start_t *ss_copy;
  const streaming_start_component_t *ssc;
  char res[14], asp[14], sr[6], ch[7];
  dvr_config_t *cfg = de->de_config;
  profile_chain_t *prch = de->de_chain;
  htsmsg_t *info, *e;
  htsmsg_field_t *f;
  muxer_t *muxer;
  int i;

  if (!cfg) {
    dvr_rec_fatal_error(de, "Unable to determine config profile");
    return -1;
  }

  if (!prch) {
    dvr_rec_fatal_error(de, "Unable to determine stream profile");
    return -1;
  }

  if (!dvr_vfs_rec_start_check(cfg)) {
    dvr_rec_fatal_error(de, "Not enough free disk space");
    return SM_CODE_NO_SPACE;
  }

  if (!(muxer = prch->prch_muxer)) {
    if (profile_chain_reopen(prch, &cfg->dvr_muxcnf, NULL, 0)) {
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

  dvr_vfs_refresh_entry(de);

  ss_copy = streaming_start_copy(ss);

  if(muxer_init(muxer, ss_copy, lang_str_get(de->de_title, NULL))) {
    dvr_rec_fatal_error(de, "Unable to init file");
    goto _err;
  }

  if(cfg->dvr_tag_files) {
    if(muxer_write_meta(muxer, de->de_bcast, de->de_comment)) {
      dvr_rec_fatal_error(de, "Unable to write meta data");
      goto _err;
    }
  }

  tvhinfo(LS_DVR, "%s from "
	 "adapter: \"%s\", "
	 "network: \"%s\", mux: \"%s\", provider: \"%s\", "
	 "service: \"%s\"",

	 dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
	 si->si_adapter  ?: "<N/A>",
	 si->si_network  ?: "<N/A>",
	 si->si_mux      ?: "<N/A>",
	 si->si_provider ?: "<N/A>",
	 si->si_service  ?: "<N/A>");


  tvhinfo(LS_DVR,
	 " #  %-16s  %-4s  %-10s  %-12s  %-11s  %-8s",
	 "type",
	 "lang",
	 "resolution",
	 "aspect ratio",
	 "sample rate",
	 "channels");

  info = htsmsg_create_list();
  for(i = 0; i < ss_copy->ss_num_components; i++) {
    ssc = &ss_copy->ss_components[i];

    if(ssc->ssc_muxer_disabled)
      continue;

    e = htsmsg_create_map();
    htsmsg_add_str(e, "type", streaming_component_type2txt(ssc->ssc_type));
    if (ssc->ssc_lang[0])
       htsmsg_add_str(e, "language", ssc->ssc_lang);

    if(SCT_ISAUDIO(ssc->ssc_type)) {
      htsmsg_add_u32(e, "audio_type", ssc->ssc_audio_type);
      if(ssc->ssc_audio_version)
        htsmsg_add_u32(e, "audio_version", ssc->ssc_audio_version);
      if(ssc->ssc_sri < 16)
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
      sr[0] = ch[0] = 0;
    }

    if(SCT_ISVIDEO(ssc->ssc_type)) {
      if(ssc->ssc_width && ssc->ssc_height)
	snprintf(res, sizeof(res), "%dx%d",
		 ssc->ssc_width, ssc->ssc_height);
      else
	strcpy(res, "?");

      if(ssc->ssc_aspect_num &&  ssc->ssc_aspect_den)
	snprintf(asp, sizeof(asp), "%d:%d",
		 ssc->ssc_aspect_num, ssc->ssc_aspect_den);
      else
	strcpy(asp, "?");

      htsmsg_add_u32(e, "width",      ssc->ssc_width);
      htsmsg_add_u32(e, "height",     ssc->ssc_height);
      htsmsg_add_u32(e, "duration",   ssc->ssc_frameduration);
      htsmsg_add_u32(e, "aspect_num", ssc->ssc_aspect_num);
      htsmsg_add_u32(e, "aspect_den", ssc->ssc_aspect_den);
    } else {
      res[0] = asp[0] = 0;
    }

    if (SCT_ISSUBTITLE(ssc->ssc_type)) {
      htsmsg_add_u32(e, "composition_id", ssc->ssc_composition_id);
      htsmsg_add_u32(e, "ancillary_id",   ssc->ssc_ancillary_id);
    }

    tvhinfo(LS_DVR,
	   "%2d  %-16s  %-4s  %-10s  %-12s  %-11s  %-8s  %s",
	   ssc->ssc_index,
	   streaming_component_type2txt(ssc->ssc_type),
	   ssc->ssc_lang,
	   res,
	   asp,
	   sr,
	   ch,
	   ssc->ssc_disabled ? "<disabled, no valid input>" : "");
    htsmsg_add_msg(info, NULL, e);
  }

  streaming_start_unref(ss_copy);

  /* update the info field for a filename */
  if ((f = htsmsg_field_last(de->de_files)) != NULL &&
      (e = htsmsg_field_get_map(f)) != NULL) {
    htsmsg_set_msg(e, "info", info);
    htsmsg_set_s64(e, "start", gclk());
  } else {
    htsmsg_destroy(info);
  }
  return 0;

_err:
  streaming_start_unref(ss_copy);
  return -1;
}

/**
 *
 */
static inline int
dvr_thread_global_lock(dvr_entry_t *de, int *run)
{
  if (atomic_add(&de->de_thread_shutdown, 1) > 0) {
    *run = 0;
    return 0;
  }
  pthread_mutex_lock(&global_lock);
  return 1;
}

/**
 *
 */
static inline void
dvr_thread_global_unlock(dvr_entry_t *de)
{
  pthread_mutex_unlock(&global_lock);
  atomic_dec(&de->de_thread_shutdown, 1);
}

/**
 *
 */
static void
dvr_streaming_restart(dvr_entry_t *de, int *run)
{
  if (dvr_thread_global_lock(de, run)) {
    service_restart(de->de_s->ths_service);
    dvr_thread_global_unlock(de);
  }
}

/**
 *
 */
static int
dvr_thread_pkt_stats(dvr_entry_t *de, th_pkt_t *pkt, int payload)
{
  th_subscription_t *ts;
  int ret = 0;

  if ((ts = de->de_s) != NULL) {
    if (pkt->pkt_err) {
      de->de_data_errors += pkt->pkt_err;
      ret = 1;
    }
    if (payload && pkt->pkt_payload)
      subscription_add_bytes_out(ts, pktbuf_len(pkt->pkt_payload));
  }
  return ret;
}

/**
 *
 */
static int
dvr_thread_mpegts_stats(dvr_entry_t *de, void *sm_data)
{
  th_subscription_t *ts;
  pktbuf_t *pb = sm_data;
  int ret = 0;

  if (pb) {
    if ((ts = de->de_s) != NULL) {
      if (pb->pb_err) {
        de->de_data_errors += pb->pb_err;
        ret = 1;
      }
      subscription_add_bytes_out(ts, pktbuf_len(pb));
    }
  }
  return ret;
}

/**
 *
 */
static int
dvr_thread_rec_start(dvr_entry_t **_de, streaming_start_t *ss,
                     int *run, int *started, int64_t *dts_offset,
                     const char *postproc)
{
  dvr_entry_t *de = *_de;
  profile_chain_t *prch = de->de_chain;
  int ret = 0;

  if (*started &&
      muxer_reconfigure(prch->prch_muxer, ss) < 0) {
    tvhwarn(LS_DVR, "Unable to reconfigure \"%s\"",
            dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL));

    // Try to restart the recording if the muxer doesn't
    // support reconfiguration of the streams.
    if (!dvr_thread_global_lock(de, run)) {
      *dts_offset = PTS_UNSET;
      *started = 0;
      return 0;
    }
    dvr_thread_epilog(de, postproc);
    *dts_offset = PTS_UNSET;
    *started = 0;
    if (de->de_config->dvr_clone)
      *_de = dvr_entry_clone(de);
    dvr_thread_global_unlock(de);
    de = *_de;
  }

  if (!*started) {
    if (!dvr_thread_global_lock(de, run))
      return 0;
    dvr_rec_set_state(de, DVR_RS_WAIT_PROGRAM_START, 0);
    int code = dvr_rec_start(de, ss);
    if(code == 0) {
      ret = 1;
      *started = 1;
    } else
      dvr_stop_recording(de, code == SM_CODE_NO_SPACE ? SM_CODE_NO_SPACE : SM_CODE_INVALID_TARGET, 1, 0);
    dvr_thread_global_unlock(de);
  }
  return ret;
}

/**
 *
 */
static inline int
dts_pts_valid(th_pkt_t *pkt, int64_t dts_offset)
{
  if (pkt->pkt_dts == PTS_UNSET ||
      pkt->pkt_pts == PTS_UNSET ||
      dts_offset == PTS_UNSET ||
      pkt->pkt_dts < dts_offset ||
      pkt->pkt_pts < dts_offset)
    return 0;
  return 1;
}

/**
 *
 */
static int64_t
get_dts_ref(th_pkt_t *pkt, streaming_start_t *ss)
{
  const streaming_start_component_t *ssc;
  int64_t audio = PTS_UNSET;
  int i;

  if (pkt->pkt_dts == PTS_UNSET)
    return PTS_UNSET;
  for (i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    if (ssc->ssc_index == pkt->pkt_componentindex) {
      if (SCT_ISVIDEO(ssc->ssc_type))
        return pkt->pkt_dts;
      if (audio == PTS_UNSET && SCT_ISAUDIO(ssc->ssc_type))
        audio = pkt->pkt_dts;
    }
  }
  return audio;
}

/**
 *
 */
static void *
dvr_thread(void *aux)
{
  dvr_entry_t *de = aux;
  profile_chain_t *prch = de->de_chain;
  streaming_queue_t *sq = &prch->prch_sq;
  struct streaming_message_queue backlog;
  streaming_message_t *sm, *sm2;
  th_pkt_t *pkt, *pkt2, *pkt3;
  streaming_start_t *ss = NULL;
  int run = 1, started = 0, muxing = 0, comm_skip, rs;
  int epg_running = 0, epg_pause = 0;
  int commercial = COMMERCIAL_UNKNOWN;
  int running_disabled;
  int64_t packets = 0, dts_offset = PTS_UNSET;
  time_t real_start, start_time = 0, running_start = 0, running_stop = 0;
  char *postproc;

  if (!dvr_thread_global_lock(de, &run))
    return NULL;
  comm_skip = de->de_config->dvr_skip_commercials;
  postproc  = de->de_config->dvr_postproc ? strdup(de->de_config->dvr_postproc) : NULL;
  running_disabled = dvr_entry_get_epg_running(de) <= 0;
  real_start = dvr_entry_get_start_time(de, 0);
  dvr_thread_global_unlock(de);

  TAILQ_INIT(&backlog);

  pthread_mutex_lock(&sq->sq_mutex);
  while(run) {
    sm = TAILQ_FIRST(&sq->sq_queue);
    if(sm == NULL) {
      tvh_cond_wait(&sq->sq_cond, &sq->sq_mutex);
      continue;
    }
    streaming_queue_remove(sq, sm);

    if (running_disabled) {
      epg_running = real_start <= gclk();
    } else if (sm->sm_type == SMT_PACKET || sm->sm_type == SMT_MPEGTS) {
      running_start = atomic_get_time_t(&de->de_running_start);
      running_stop  = atomic_get_time_t(&de->de_running_stop);
      if (running_start > 0) {
        epg_running = running_start >= running_stop ? 1 : 0;
        if (epg_running && atomic_get_time_t(&de->de_running_pause) >= running_start)
          epg_running = 2;
      } else if (running_stop == 0) {
        if (start_time + 2 >= gclk()) {
          TAILQ_INSERT_TAIL(&backlog, sm, sm_link);
          continue;
        } else {
          if (TAILQ_FIRST(&backlog))
            streaming_queue_clear(&backlog);
          epg_running = real_start <= gclk();
        }
      } else {
        epg_running = 0;
      }
    }

    pthread_mutex_unlock(&sq->sq_mutex);

    switch(sm->sm_type) {

    case SMT_PACKET:
      pkt = sm->sm_data;

      rs = DVR_RS_RUNNING;
      if (!epg_running)
        rs = DVR_RS_EPG_WAIT;
      else if (pkt->pkt_commercial == COMMERCIAL_YES || epg_running == 2)
        rs = DVR_RS_COMMERCIAL;
      dvr_rec_set_state(de, rs, 0);

      if ((rs == DVR_RS_COMMERCIAL && comm_skip) || !epg_running) {
        if (ss && packets && running_start == 0) {
          dvr_streaming_restart(de, &run);
          packets = 0;
          started = 0;
        }
        break;
      }

      if (epg_pause != (epg_running == 2)) {
        epg_pause = epg_running == 2;
	if (muxing) muxer_add_marker(prch->prch_muxer);
      } else if (commercial != pkt->pkt_commercial) {
        commercial = pkt->pkt_commercial;
        if (muxing) muxer_add_marker(prch->prch_muxer);
      } else if (atomic_exchange(&de->de_running_change, 0)) {
        if (muxing) muxer_add_marker(prch->prch_muxer);
      }

      if (ss == NULL)
        break;

      if (muxing == 0 &&
          !dvr_thread_rec_start(&de, ss, &run, &started, &dts_offset, postproc))
        break;

      muxing = 1;
      while ((sm2 = TAILQ_FIRST(&backlog)) != NULL) {
        TAILQ_REMOVE(&backlog, sm2, sm_link);
        pkt2 = sm2->sm_data;
        if (pkt2->pkt_dts != PTS_UNSET) {
          if (dts_offset == PTS_UNSET)
            dts_offset = get_dts_ref(pkt2, ss);
          if (dts_pts_valid(pkt2, dts_offset)) {
            pkt3 = pkt_copy_shallow(pkt2);
            pkt3->pkt_dts -= dts_offset;
            if (pkt3->pkt_pts != PTS_UNSET)
              pkt3->pkt_pts -= dts_offset;
            dvr_thread_pkt_stats(de, pkt3, 1);
            muxer_write_pkt(prch->prch_muxer, sm2->sm_type, pkt3);
          } else {
            dvr_thread_pkt_stats(de, pkt2, 0);
          }
        }
        streaming_msg_free(sm2);
      }
      if (dts_offset == PTS_UNSET)
        dts_offset = get_dts_ref(pkt, ss);
      if (dts_pts_valid(pkt, dts_offset)) {
        pkt3 = pkt_copy_shallow(pkt);
        pkt3->pkt_dts -= dts_offset;
        if (pkt3->pkt_pts != PTS_UNSET)
          pkt3->pkt_pts -= dts_offset;
        dvr_thread_pkt_stats(de, pkt3, 1);
        muxer_write_pkt(prch->prch_muxer, sm->sm_type, pkt3);
      } else {
        dvr_thread_pkt_stats(de, pkt, 0);
      }
      dvr_notify(de);
      packets++;
      break;

    case SMT_MPEGTS:
      rs = DVR_RS_RUNNING;
      if (!epg_running)
        rs = DVR_RS_EPG_WAIT;
      else if (epg_running == 2)
        rs = DVR_RS_COMMERCIAL;
      dvr_rec_set_state(de, rs, 0);

      if (ss == NULL)
        break;

      if ((rs == DVR_RS_COMMERCIAL && comm_skip) || !epg_running) {
        if (packets && running_start == 0) {
          dvr_streaming_restart(de, &run);
          packets = 0;
          started = 0;
        }
        break;
      }

      if (epg_pause != (epg_running == 2)) {
        epg_pause = epg_running == 2;
	if (muxing) muxer_add_marker(prch->prch_muxer);
      } else if (atomic_exchange(&de->de_running_change, 0)) {
        if (muxing) muxer_add_marker(prch->prch_muxer);
      }

      if (muxing == 0 &&
          !dvr_thread_rec_start(&de, ss, &run, &started, &dts_offset, postproc))
        break;

      muxing = 1;
      while ((sm2 = TAILQ_FIRST(&backlog)) != NULL) {
        TAILQ_REMOVE(&backlog, sm2, sm_link);
        dvr_thread_mpegts_stats(de, sm2->sm_data);
        muxer_write_pkt(prch->prch_muxer, sm2->sm_type, sm2->sm_data);
        sm2->sm_data = NULL;
        streaming_msg_free(sm2);
      }
      dvr_thread_mpegts_stats(de, sm->sm_data);
      muxer_write_pkt(prch->prch_muxer, sm->sm_type, sm->sm_data);
      sm->sm_data = NULL;
      dvr_notify(de);
      packets++;
      break;

    case SMT_START:
      start_time = gclk();
      packets = 0;
      if (ss)
	streaming_start_unref(ss);
      ss = streaming_start_copy((streaming_start_t *)sm->sm_data);
      break;

    case SMT_STOP:
       if (sm->sm_code == SM_CODE_SOURCE_RECONFIGURED) {
	 // Subscription is restarting, wait for SMT_START
	 muxing = 0; // reconfigure muxer

       } else if(sm->sm_code == 0) {
	 // Recording is completed

	dvr_entry_set_state(de, de->de_sched_state, de->de_rec_state, SM_CODE_OK);
	tvhinfo(LS_DVR, "Recording completed: \"%s\"",
	        dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL));

        goto fin;

      } else if (de->de_last_error != sm->sm_code) {
	 // Error during recording

	dvr_rec_set_state(de, DVR_RS_ERROR, sm->sm_code);
	tvherror(LS_DVR, "Recording stopped: \"%s\": %s",
                dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
                streaming_code2txt(sm->sm_code));

fin:
        streaming_queue_clear(&backlog);
        if (!dvr_thread_global_lock(de, &run))
          break;
        dvr_thread_epilog(de, postproc);
        dvr_thread_global_unlock(de);
	start_time = 0;
	started = 0;
	muxing = 0;
	if (ss) {
	  streaming_start_unref(ss);
	  ss = NULL;
        }
      }
      break;

    case SMT_SERVICE_STATUS:
      if (sm->sm_code & TSS_PACKETS) {

      } else if (sm->sm_code & TSS_ERRORS) {

	int code = SM_CODE_UNDEFINED_ERROR;

	if(sm->sm_code & TSS_NO_DESCRAMBLER)
	  code = SM_CODE_NO_DESCRAMBLER;

	if(sm->sm_code & TSS_NO_ACCESS)
	  code = SM_CODE_NO_ACCESS;

	if(de->de_last_error != code) {
	  dvr_rec_set_state(de, DVR_RS_ERROR, code);
	  tvherror(LS_DVR, "Streaming error: \"%s\": %s",
		   dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
		   streaming_code2txt(code));
	}
      }
      break;

    case SMT_NOSTART:

      if (de->de_last_error != sm->sm_code) {
	dvr_rec_set_state(de, DVR_RS_PENDING, sm->sm_code);

	tvherror(LS_DVR, "Recording unable to start: \"%s\": %s",
	         dvr_get_filename(de) ?: lang_str_get(de->de_title, NULL),
	         streaming_code2txt(sm->sm_code));
      }
      break;

    case SMT_GRACE:
    case SMT_NOSTART_WARN:
    case SMT_SPEED:
    case SMT_SKIP:
    case SMT_SIGNAL_STATUS:
    case SMT_TIMESHIFT_STATUS:
    case SMT_DESCRAMBLE_INFO:
      break;

    case SMT_EXIT:
      run = 0;
      break;
    }

    streaming_msg_free(sm);
    pthread_mutex_lock(&sq->sq_mutex);
  }
  pthread_mutex_unlock(&sq->sq_mutex);

  streaming_queue_clear(&backlog);

  if (ss)
    streaming_start_unref(ss);

  return postproc;
}

/**
 *
 */
void
dvr_spawn_cmd(dvr_entry_t *de, const char *cmd, const char *filename, int pre)
{
  char buf1[MAX(PATH_MAX, 2048)], *buf2;
  char tmp[MAX(PATH_MAX, 512)];
  htsmsg_t *info = NULL, *e;
  htsmsg_field_t *f;
  char **args;

  if (!pre) {
    if ((f = htsmsg_field_last(de->de_files)) != NULL &&
        (e = htsmsg_field_get_map(f)) != NULL) {
      if (filename == NULL) {
        filename = htsmsg_get_str(e, "filename");
        if (filename == NULL)
          return;
      }
      info = htsmsg_get_list(e, "info");
    } else {
      return;
    }
  }

  /* Substitute DVR entry formatters */
  htsstr_substitute(cmd, buf1, sizeof(buf1), '%', dvr_subs_postproc_entry, de, tmp, sizeof(tmp));
  buf2 = tvh_strdupa(buf1);
  /* Substitute filename formatters */
  if (!pre) {
    htsstr_substitute(buf2, buf1, sizeof(buf1), '%', dvr_subs_postproc_filename, filename, tmp, sizeof(tmp));
    buf2 = tvh_strdupa(buf1);
  }
  /* Substitute info formatters */
  if (info)
    htsstr_substitute(buf2, buf1, sizeof(buf1), '%', dvr_subs_postproc_info, info, tmp, sizeof(tmp));

  args = htsstr_argsplit(buf1);
  if(args[0])
    spawnv(args[0], (void *)args, NULL, 1, 1);

  htsstr_argsplit_free(args);
}

/**
 *
 */
static void
dvr_thread_epilog(dvr_entry_t *de, const char *dvr_postproc)
{
  profile_chain_t *prch = de->de_chain;
  htsmsg_t *e;
  htsmsg_field_t *f;

  lock_assert(&global_lock);

  if (prch == NULL)
    return;

  muxer_close(prch->prch_muxer);
  muxer_destroy(prch->prch_muxer);
  prch->prch_muxer = NULL;

  if ((f = htsmsg_field_last(de->de_files)) != NULL &&
      (e = htsmsg_field_get_map(f)) != NULL)
    htsmsg_set_s64(e, "stop", gclk());

  if(dvr_postproc && dvr_postproc[0])
    dvr_spawn_cmd(de, dvr_postproc, NULL, 0);

  idnode_changed(&de->de_id);
}
