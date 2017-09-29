/*
 *  tvheadend, access control
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

#include <pthread.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "tvheadend.h"
#include "config.h"
#include "access.h"
#include "settings.h"
#include "channels.h"
#include "dvr/dvr.h"
#include "tcp.h"
#include "lang_codes.h"

#define TICKET_LIFETIME (5*60) /* in seconds */

struct access_entry_queue access_entries;
struct access_ticket_queue access_tickets;
struct passwd_entry_queue passwd_entries;
struct ipblock_entry_queue ipblock_entries;

const char *superuser_username;
const char *superuser_password;

int access_noacl;

static int passwd_verify(const char *username, verify_callback_t verify, void *aux);
static int passwd_verify2(const char *username, verify_callback_t verify, void *aux,
                          const char *username2, const char *passwd2);
static void access_ticket_destroy(access_ticket_t *at);
static void access_ticket_timeout(void *aux);

/**
 *
 */
static void
access_ticket_rearm(void)
{
  access_ticket_t *at;

  while ((at = TAILQ_FIRST(&access_tickets)) != NULL) {
    if (at->at_timer.mti_expire > mclk()) {
      mtimer_arm_abs(&at->at_timer, access_ticket_timeout, at, at->at_timer.mti_expire);
      break;
    }
    access_ticket_destroy(at);
  }
}

/**
 *
 */
static void
access_ticket_destroy(access_ticket_t *at)
{
  mtimer_disarm(&at->at_timer);
  free(at->at_id);
  free(at->at_resource);
  TAILQ_REMOVE(&access_tickets, at, at_link);
  access_destroy(at->at_access);
  free(at);
}

/**
 *
 */
static access_ticket_t *
access_ticket_find(const char *id)
{
  access_ticket_t *at = NULL;
  
  if(id != NULL) {
    /* assume that newer tickets are hit more probably */
    TAILQ_FOREACH_REVERSE(at, &access_tickets, access_ticket_queue, at_link)
      if(!strcmp(at->at_id, id))
	return at;
  }
  
  return NULL;
}

/**
 *
 */
static void
access_ticket_timeout(void *aux)
{
  access_ticket_t *at = aux;

  access_ticket_destroy(at);
  access_ticket_rearm();
}

/**
 * Create a new ticket for the requested resource and generate a id for it
 */
const char *
access_ticket_create(const char *resource, access_t *a)
{
  const int64_t lifetime = sec2mono(TICKET_LIFETIME);
  uint8_t buf[20];
  char id[41];
  uint_fast32_t i;
  access_ticket_t *at;
  static const char hex_string[16] = "0123456789ABCDEF";

  assert(a);

  /* try to find an existing ticket */
  TAILQ_FOREACH_REVERSE(at, &access_tickets, access_ticket_queue, at_link) {
    if (at->at_timer.mti_expire - lifetime + sec2mono(60) < mclk())
      break;
    if (strcmp(resource, at->at_resource))
      continue;
    if (!access_compare(at->at_access, a))
      return at->at_id;
  }


  at = calloc(1, sizeof(access_ticket_t));

  uuid_random(buf, 20);

  //convert to hexstring
  for (i=0; i < sizeof(buf); i++){
    id[i*2] = hex_string[((buf[i] >> 4) & 0xF)];
    id[(i*2)+1] = hex_string[(buf[i]) & 0x0F];
  }
  id[40] = '\0';

  at->at_id = strdup(id);
  at->at_resource = strdup(resource);

  at->at_access = access_copy(a);
  at->at_timer.mti_expire = mclk() + lifetime;

  i = TAILQ_FIRST(&access_tickets) != NULL;

  TAILQ_INSERT_TAIL(&access_tickets, at, at_link);

  if (i)
    access_ticket_rearm();

  return at->at_id;
}

/**
 *
 */
int
access_ticket_delete(const char *id)
{
  access_ticket_t *at;

  if((at = access_ticket_find(id)) == NULL)
    return -1;

  access_ticket_destroy(at);
  access_ticket_rearm();

  return 0;
}

/**
 *
 */
access_t *
access_ticket_verify2(const char *id, const char *resource)
{
  access_ticket_t *at;
  char buf[256], *r;

  if((at = access_ticket_find(id)) == NULL)
    return NULL;

  if (tvheadend_webroot) {
    snprintf(buf, sizeof(buf), "%s%s", tvheadend_webroot, at->at_resource);
    r = buf;
  } else {
    r = at->at_resource;
  }

  if(strcmp(r, resource))
    return NULL;

  return access_copy(at->at_access);
}

/**
 *
 */
int
access_verify_list(htsmsg_t *list, const char *item)
{
  htsmsg_field_t *f;

  if (list) {
    HTSMSG_FOREACH(f, list)
      if (!strcmp(htsmsg_field_get_str(f) ?: "", item))
        return 0;
    return -1;
  }
  return 0;
}

/**
 *
 */
int
access_compare(access_t *a, access_t *b)
{
  int r = strcmp(a->aa_username ?: "", b->aa_username ?: "");
  if (!r)
    r = strcmp(a->aa_representative ?: "", b->aa_representative ?: "");
  return r;
}

/**
 *
 */
access_t *
access_copy(access_t *src)
{
  access_t *dst = malloc(sizeof(*dst));
  *dst = *src;
  if (src->aa_username)
    dst->aa_username = strdup(src->aa_username);
  if (src->aa_representative)
    dst->aa_representative = strdup(src->aa_representative);
  if (src->aa_lang)
    dst->aa_lang = strdup(src->aa_lang);
  if (src->aa_lang_ui)
    dst->aa_lang_ui = strdup(src->aa_lang_ui);
  if (src->aa_theme)
    dst->aa_theme = strdup(src->aa_theme);
  if (src->aa_profiles)
    dst->aa_profiles = htsmsg_copy(src->aa_profiles);
  if (src->aa_dvrcfgs)
    dst->aa_dvrcfgs = htsmsg_copy(src->aa_dvrcfgs);
  if (src->aa_chrange) {
    size_t l = src->aa_chrange_count * sizeof(uint64_t);
    dst->aa_chrange = malloc(l);
    if (dst->aa_chrange == NULL)
      dst->aa_chrange_count = 0;
    else
      memcpy(dst->aa_chrange, src->aa_chrange, l);
  }
  if (src->aa_chtags)
    dst->aa_chtags  = htsmsg_copy(src->aa_chtags);
  return dst;
}

/**
 *
 */
char *
access_get_lang(access_t *a, const char *lang)
{
  if (lang == NULL) {
    if (a->aa_lang == NULL)
      return NULL;
    return strdup(a->aa_lang);
  } else {
    return lang_code_user(lang);
  }
}

/**
 *
 */
const char *
access_get_theme(access_t *a)
{
  if (a == NULL)
    return "blue";
  if (a->aa_theme == NULL || a->aa_theme[0] == '\0') {
    if (config.theme_ui == NULL || config.theme_ui[0] == '\0')
      return "blue";
     return config.theme_ui;
  }
  return a->aa_theme;
}

/**
 *
 */
void
access_destroy(access_t *a)
{
  if (a == NULL)
    return;
  free(a->aa_username);
  free(a->aa_representative);
  free(a->aa_lang);
  free(a->aa_lang_ui);
  free(a->aa_theme);
  free(a->aa_chrange);
  htsmsg_destroy(a->aa_profiles);
  htsmsg_destroy(a->aa_dvrcfgs);
  htsmsg_destroy(a->aa_chtags);
  free(a);
}

/**
 *
 */
static int
netmask_verify(struct access_ipmask_queue *ais, struct sockaddr_storage *src)
{
  access_ipmask_t *ai;
  int isv4v6 = 0;
  uint32_t v4v6 = 0;
  
  if (src->ss_family == AF_INET6) {
    struct in6_addr *in6 = &(((struct sockaddr_in6 *)src)->sin6_addr);
    uint32_t *a32 = (uint32_t*)in6->s6_addr;
    if (a32[0] == 0 && a32[1] == 0 && ntohl(a32[2]) == 0x0000FFFFu) {
      isv4v6 = 1;
      v4v6 = ntohl(a32[3]);
    }
  }

  TAILQ_FOREACH(ai, ais, ai_link) {

    if (ai->ai_family == AF_INET && src->ss_family == AF_INET) {

      struct sockaddr_in *in4 = (struct sockaddr_in *)src;
      uint32_t b = ntohl(in4->sin_addr.s_addr);
      if ((b & ai->ai_netmask) == ai->ai_network)
        return 1;

    } else if (ai->ai_family == AF_INET && isv4v6) {

      if((v4v6 & ai->ai_netmask) == ai->ai_network)
        return 1;

    } else if (ai->ai_family == AF_INET6 && isv4v6) {

      continue;

    } else if (ai->ai_family == AF_INET6 && src->ss_family == AF_INET6) {

      struct in6_addr *in6 = &(((struct sockaddr_in6 *)src)->sin6_addr);
      uint8_t *a8 = (uint8_t*)in6->s6_addr;
      uint8_t *m8 = (uint8_t*)ai->ai_ip6.s6_addr;
      int slen = ai->ai_prefixlen;
      uint32_t apos = 0;
      uint8_t lastMask = (0xFFu << (8 - (slen % 8)));

      if(slen < 0 || slen > 128)
        continue;

      while(slen >= 8)
      {
        if(a8[apos] != m8[apos])
          break;

        apos += 1;
        slen -= 8;
      }
      if(slen >= 8)
        continue;

      if(slen == 0 || (a8[apos] & lastMask) == (m8[apos] & lastMask))
        return 1;
    }
  }

  return 0;
}

/**
 *
 */
static inline int
access_ip_blocked(struct sockaddr_storage *src)
{
  ipblock_entry_t *ib;

  TAILQ_FOREACH(ib, &ipblock_entries, ib_link)
    if (ib->ib_enabled && netmask_verify(&ib->ib_ipmasks, src))
      return 1;
  return 0;
}

/*
 *
 */
static void
access_dump_a(access_t *a)
{
  htsmsg_field_t *f;
  size_t l = 0;
  char buf[1024];
  int first;

  tvh_strlcatf(buf, sizeof(buf), l,
    "%s:%s [%c%c%c%c%c%c%c%c%c%c%c], conn=%u:s%u:r%u:l%u%s",
    a->aa_representative ?: "<no-id>",
    a->aa_username ?: "<no-user>",
    a->aa_rights & ACCESS_STREAMING          ? 'S' : ' ',
    a->aa_rights & ACCESS_ADVANCED_STREAMING ? 'A' : ' ',
    a->aa_rights & ACCESS_HTSP_STREAMING     ? 'T' : ' ',
    a->aa_rights & ACCESS_WEB_INTERFACE      ? 'W' : ' ',
    a->aa_rights & ACCESS_RECORDER           ? 'R' : ' ',
    a->aa_rights & ACCESS_HTSP_RECORDER      ? 'E' : ' ',
    a->aa_rights & ACCESS_ALL_RECORDER       ? 'L' : ' ',
    a->aa_rights & ACCESS_ALL_RW_RECORDER    ? 'D' : ' ',
    a->aa_rights & ACCESS_FAILED_RECORDER    ? 'F' : ' ',
    a->aa_rights & ACCESS_HTSP_ANONYMIZE     ? 'H' : ' ',
    a->aa_rights & ACCESS_ADMIN              ? '*' : ' ',
    a->aa_conn_limit,
    a->aa_conn_limit_streaming,
    a->aa_conn_limit_dvr,
    a->aa_uilevel,
    a->aa_match ? ", matched" : "");

  if (a->aa_profiles) {
    first = 1;
    HTSMSG_FOREACH(f, a->aa_profiles) {
      profile_t *pro = profile_find_by_uuid(htsmsg_field_get_str(f) ?: "");
      if (pro) {
        if (first)
          tvh_strlcatf(buf, sizeof(buf), l, ", profile=");
        tvh_strlcatf(buf, sizeof(buf), l, "%s'%s'",
                 first ? "" : ",", profile_get_name(pro));
        first = 0;
      }
    }
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, ", profile=ANY");
  }

  if (a->aa_dvrcfgs) {
    first = 1;
    HTSMSG_FOREACH(f, a->aa_dvrcfgs) {
      dvr_config_t *cfg = dvr_config_find_by_uuid(htsmsg_field_get_str(f) ?: "");
      if (cfg) {
        if (first)
          tvh_strlcatf(buf, sizeof(buf), l, ", dvr=");
        tvh_strlcatf(buf, sizeof(buf), l, "%s'%s'",
                 first ? "" : ",", cfg->dvr_config_name ?: "");
        first = 0;
      }
    }
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, ", dvr=ANY");
  }

  if (a->aa_chrange) {
    for (first = 0; first < a->aa_chrange_count; first += 2)
      tvh_strlcatf(buf, sizeof(buf), l, ", [chmin=%llu, chmax=%llu]",
                   (long long)a->aa_chrange[first],
                   (long long)a->aa_chrange[first+1]);
  }

  if (a->aa_chtags) {
    first = 1;
    HTSMSG_FOREACH(f, a->aa_chtags) {
      channel_tag_t *ct = channel_tag_find_by_uuid(htsmsg_field_get_str(f) ?: "");
      if (ct) {
        tvh_strlcatf(buf, sizeof(buf), l, "%s'%s'",
                 first ? ", tags=" : ",", ct->ct_name ?: "");
        first = 0;
      }
    }
  } else {
    tvh_strlcatf(buf, sizeof(buf), l, ", tag=ANY");
  }

  tvhtrace(LS_ACCESS, "%s", buf);
}

/*
 *
 */
static access_t *access_alloc(void)
{
  access_t *a = calloc(1, sizeof(access_t));
  a->aa_uilevel = -1;
  a->aa_uilevel_nochange = -1;
  return a;
}

/*
 *
 */
static access_t *access_full(access_t *a)
{
  a->aa_rights = ACCESS_FULL;
  a->aa_uilevel = UILEVEL_EXPERT;
  a->aa_uilevel_nochange = config.uilevel_nochange;
  return a;
}

/*
 *
 */
static void
access_update(access_t *a, access_entry_t *ae)
{
  idnode_list_mapping_t *ilm;
  const char *s;
  char ubuf[UUID_HEX_SIZE];

  if (ae->ae_change_conn_limit) {
    switch (ae->ae_conn_limit_type) {
    case ACCESS_CONN_LIMIT_TYPE_ALL:
      a->aa_conn_limit = ae->ae_conn_limit;
    case ACCESS_CONN_LIMIT_TYPE_STREAMING:
      a->aa_conn_limit_streaming = ae->ae_conn_limit;
      break;
    case ACCESS_CONN_LIMIT_TYPE_DVR:
      a->aa_conn_limit_dvr = ae->ae_conn_limit;
      break;
    }
  }

  if (ae->ae_change_uilevel) {
    a->aa_uilevel = ae->ae_uilevel;
    a->aa_uilevel_nochange = ae->ae_uilevel_nochange;
  }

  if (ae->ae_change_chrange) {
    if (ae->ae_chmin || ae->ae_chmax) {
      uint64_t *p = realloc(a->aa_chrange, (a->aa_chrange_count + 2) * sizeof(uint64_t));
      if (p) {
        p[a->aa_chrange_count++] = ae->ae_chmin;
        p[a->aa_chrange_count++] = ae->ae_chmax;
        a->aa_chrange = p;
      }
    } else {
      free(a->aa_chrange);
      a->aa_chrange = NULL;
    }
  }

  if (ae->ae_change_profiles) {
    if (LIST_EMPTY(&ae->ae_profiles)) {
      idnode_list_destroy(&ae->ae_profiles, ae);
    } else {
      LIST_FOREACH(ilm, &ae->ae_profiles, ilm_in1_link) {
        profile_t *pro = (profile_t *)ilm->ilm_in2;
        if(pro && pro->pro_name && pro->pro_name[0] != '\0') {
          if (a->aa_profiles == NULL)
            a->aa_profiles = htsmsg_create_list();
          htsmsg_add_str_exclusive(a->aa_profiles, idnode_uuid_as_str(&pro->pro_id, ubuf));
        }
      }
    }
  }

  if (ae->ae_change_dvr_configs) {
    if (LIST_EMPTY(&ae->ae_dvr_configs)) {
      idnode_list_destroy(&ae->ae_dvr_configs, ae);
    } else {
      LIST_FOREACH(ilm, &ae->ae_dvr_configs, ilm_in1_link) {
        dvr_config_t *dvr = (dvr_config_t *)ilm->ilm_in2;
        if(dvr && dvr->dvr_config_name[0] != '\0') {
          if (a->aa_dvrcfgs == NULL)
            a->aa_dvrcfgs = htsmsg_create_list();
          htsmsg_add_str_exclusive(a->aa_dvrcfgs, idnode_uuid_as_str(&dvr->dvr_id, ubuf));
         }
      }
    }
  }

  if (ae->ae_change_chtags) {
    if (ae->ae_chtags_exclude && !LIST_EMPTY(&ae->ae_chtags)) {
      channel_tag_t *ct;
      TAILQ_FOREACH(ct, &channel_tags, ct_link) {
        if(ct && ct->ct_name[0] != '\0') {
          LIST_FOREACH(ilm, &ae->ae_chtags, ilm_in1_link) {
            channel_tag_t *ct2 = (channel_tag_t *)ilm->ilm_in2;
            if (ct == ct2) break;
          }
          if (ilm == NULL) {
            if (a->aa_chtags == NULL)
              a->aa_chtags = htsmsg_create_list();
            htsmsg_add_str_exclusive(a->aa_chtags, idnode_uuid_as_str(&ct->ct_id, ubuf));
          }
        }
      }
    } else {
      if (LIST_EMPTY(&ae->ae_chtags)) {
        idnode_list_destroy(&ae->ae_chtags, ae);
      } else {
        LIST_FOREACH(ilm, &ae->ae_chtags, ilm_in1_link) {
          channel_tag_t *ct = (channel_tag_t *)ilm->ilm_in2;
          if(ct && ct->ct_name[0] != '\0') {
            if (a->aa_chtags == NULL)
              a->aa_chtags = htsmsg_create_list();
            htsmsg_add_str_exclusive(a->aa_chtags, idnode_uuid_as_str(&ct->ct_id, ubuf));
          }
        }
      }
    }
  }

  if (ae->ae_change_lang) {
    free(a->aa_lang);
    if (ae->ae_lang && ae->ae_lang[0]) {
      a->aa_lang = lang_code_user(ae->ae_lang);
    } else {
      a->aa_lang = NULL;
    }
  }

  if (ae->ae_change_lang_ui) {
    free(a->aa_lang_ui);
    if (ae->ae_lang_ui && ae->ae_lang_ui[0])
      a->aa_lang_ui = lang_code_user(ae->ae_lang_ui);
    else if ((s = config_get_language_ui()) != NULL)
      a->aa_lang_ui = lang_code_user(s);
    else
      a->aa_lang_ui = NULL;
  }

  if (ae->ae_change_theme) {
    free(a->aa_theme);
    if (ae->ae_theme && ae->ae_theme[0])
      a->aa_theme = strdup(ae->ae_theme);
    else
      a->aa_theme = NULL;
  }

  if (ae->ae_change_rights) {
    if (ae->ae_rights == 0)
      a->aa_rights = 0;
    else
      a->aa_rights |= ae->ae_rights;
  }
}

/**
 */
static void
access_set_lang_ui(access_t *a)
{
  const char *s;
  if (!a->aa_lang_ui) {
    if ((s = config_get_language_ui()) != NULL)
      a->aa_lang_ui = lang_code_user(s);
    if (a->aa_lang)
      a->aa_lang_ui = strdup(a->aa_lang);
  }
  if (a->aa_uilevel < 0)
    a->aa_uilevel = config.uilevel;
  if (a->aa_uilevel_nochange < 0)
    a->aa_uilevel_nochange = config.uilevel_nochange;
}

/**
 *
 */
access_t *
access_get(struct sockaddr_storage *src, const char *username, verify_callback_t verify, void *aux)
{
  access_t *a = access_alloc();
  access_entry_t *ae;
  int nouser = username == NULL || username[0] == '\0';

  if (!access_noacl && access_ip_blocked(src))
    return a;

  if (!passwd_verify(username, verify, aux)) {
    a->aa_username = strdup(username);
    a->aa_representative = strdup(username);
    if(!passwd_verify2(username, verify, aux,
                       superuser_username, superuser_password))
      return access_full(a);
  } else {
    a->aa_representative = malloc(50);
    tcp_get_str_from_ip(src, a->aa_representative, 50);
    if(!passwd_verify2(username, verify, aux,
                       superuser_username, superuser_password))
      return access_full(a);
    username = NULL;
  }

  if (access_noacl)
    return access_full(a);

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      if(username == NULL || strcmp(username, ae->ae_username))
	continue; /* Didn't get one */
    }

    if(!netmask_verify(&ae->ae_ipmasks, src))
      continue; /* IP based access mismatches */

    if(ae->ae_username[0] != '*')
      a->aa_match = 1;

    access_update(a, ae);
  }

  /* Username was not matched - no access */
  if (!a->aa_match) {
    free(a->aa_username);
    a->aa_username = NULL;
    if (!nouser)
      a->aa_rights = 0;
  }

  access_set_lang_ui(a);

  if (tvhtrace_enabled())
    access_dump_a(a);
  return a;
}

/**
 *
 */
access_t *
access_get_by_username(const char *username)
{
  access_t *a = access_alloc();
  access_entry_t *ae;

  a->aa_username = strdup(username);
  a->aa_representative = strdup(username);

  if(access_noacl)
    return access_full(a);

  if (username[0] == '\0')
    return a;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] == '*' || strcmp(ae->ae_username, username))
      continue;

    access_update(a, ae);
  }

  access_set_lang_ui(a);

  return a;
}

/**
 *
 */
access_t *
access_get_by_addr(struct sockaddr_storage *src)
{
  access_t *a = access_alloc();
  access_entry_t *ae;

  a->aa_representative = malloc(50);
  tcp_get_str_from_ip(src, a->aa_representative, 50);

  if(access_noacl)
    return access_full(a);

  if (access_ip_blocked(src))
    return a;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*')
      continue;

    if(!netmask_verify(&ae->ae_ipmasks, src))
      continue; /* IP based access mismatches */

    access_update(a, ae);
  }

  access_set_lang_ui(a);

  return a;
}

/**
 *
 */
static void
access_set_prefix_default(struct access_ipmask_queue *ais)
{
  access_ipmask_t *ai;

  ai = calloc(1, sizeof(access_ipmask_t));
  ai->ai_family = AF_INET6;
  TAILQ_INSERT_HEAD(ais, ai, ai_link);

  ai = calloc(1, sizeof(access_ipmask_t));
  ai->ai_family = AF_INET;
  TAILQ_INSERT_HEAD(ais, ai, ai_link);
}

/**
 *
 */
static int access_addr4_empty(const char *s)
{
  int empty = 1;
  while (*s) {
    if (*s == '0') {
      /* nothing */
    } else if (isdigit(*s)) {
      empty = 0;
    } else if (*s == '.') {
      empty = 0;
    } else {
      return 1;
    }
    s++;
  }
  return empty;
}

/**
 *
 */
static int access_addr6_empty(const char *s)
{
  int empty = 1;
  while (*s) {
    if (*s == '0') {
      /* nothing */
    } else if (isdigit(*s)) {
      empty = 0;
    } else if (*s == ':') {
      empty = 0;
    } else {
      return 1;
    }
    s++;
  }
  return empty;
}

/**
 *
 */
static void
access_set_prefix(struct access_ipmask_queue *ais, const char *prefix, int dflt)
{
  static const char *delim = ",;| ";
  char buf[100];
  char tokbuf[4096];
  int prefixlen;
  char *p, *tok, *saveptr;
  in_addr_t s_addr;
  access_ipmask_t *ai = NULL;

  while((ai = TAILQ_FIRST(ais)) != NULL) {
    TAILQ_REMOVE(ais, ai, ai_link);
    free(ai);
  }

  strncpy(tokbuf, prefix, sizeof(tokbuf)-1);
  tokbuf[sizeof(tokbuf) - 1] = 0;
  tok = strtok_r(tokbuf, delim, &saveptr);

  while (tok != NULL) {
    if (ai == NULL)
      ai = calloc(1, sizeof(access_ipmask_t));

    if (strlen(tok) > sizeof(buf) - 1 || *tok == '\0')
      goto fnext;

    strcpy(buf, tok);

    if (strchr(buf, ':') != NULL)
      ai->ai_family = AF_INET6;
    else
      ai->ai_family = AF_INET;

    if (ai->ai_family == AF_INET6) {
      if ((p = strchr(buf, '/')) != NULL) {
        *p++ = 0;
        prefixlen = atoi(p);
        if (prefixlen < 0 || prefixlen > 128)
          goto fnext;
      } else {
        prefixlen = !access_addr6_empty(buf) ? 128 : 0;
      }

      ai->ai_prefixlen = prefixlen;
      inet_pton(AF_INET6, buf, &ai->ai_ip6);

      ai->ai_netmask = 0xffffffff;
      ai->ai_network = 0x00000000;
    } else {
      if ((p = strchr(buf, '/')) != NULL) {
        *p++ = 0;
        prefixlen = atoi(p);
        if (prefixlen < 0 || prefixlen > 32)
          goto fnext;
      } else {
        prefixlen = !access_addr4_empty(buf) ? 32 : 0;
      }

      s_addr = inet_addr(buf);
      ai->ai_prefixlen = prefixlen;

      ai->ai_netmask   = prefixlen ? 0xffffffff << (32 - prefixlen) : 0;
      ai->ai_network   = ntohl(s_addr) & ai->ai_netmask;
    }

    TAILQ_INSERT_TAIL(ais, ai, ai_link);
    ai = NULL;

    tok = strtok_r(NULL, delim, &saveptr);
    continue;

fnext:
    tok = strtok_r(NULL, delim, &saveptr);
    if (tok == NULL) {
      free(ai);
      ai = NULL;
    }
  }

  if (dflt && !TAILQ_FIRST(ais))
    access_set_prefix_default(ais);
}

/**
 *
 */
static const char *access_get_prefix(struct access_ipmask_queue *ais)
{
  char addrbuf[50];
  access_ipmask_t *ai;
  size_t pos = 0;
  uint32_t s_addr;

  prop_sbuf[0] = prop_sbuf[1] = '\0';
  TAILQ_FOREACH(ai, ais, ai_link)   {
    if(PROP_SBUF_LEN-pos <= 0)
      break;
    if(ai->ai_family == AF_INET6) {
      inet_ntop(AF_INET6, &ai->ai_ip6, addrbuf, sizeof(addrbuf));
    } else {
      s_addr = htonl(ai->ai_network);
      inet_ntop(AF_INET, &s_addr, addrbuf, sizeof(addrbuf));
    }
    tvh_strlcatf(prop_sbuf, PROP_SBUF_LEN, pos, ",%s/%d", addrbuf, ai->ai_prefixlen);
  }
  return prop_sbuf + 1;
}

/**
 *
 */
static void
access_entry_update_rights(access_entry_t *ae)
{
  uint32_t r = 0;

  if (ae->ae_streaming)
    r |= ACCESS_STREAMING;
  if (ae->ae_adv_streaming)
    r |= ACCESS_ADVANCED_STREAMING;
  if (ae->ae_htsp_streaming)
    r |= ACCESS_HTSP_STREAMING;
  if (ae->ae_webui)
    r |= ACCESS_WEB_INTERFACE;
  if (ae->ae_dvr)
    r |= ACCESS_RECORDER;
  if (ae->ae_htsp_dvr)
    r |= ACCESS_HTSP_RECORDER;
  if (ae->ae_all_dvr)
    r |= ACCESS_ALL_RECORDER;
  if (ae->ae_all_rw_dvr)
    r |= ACCESS_ALL_RW_RECORDER;
  if (ae->ae_failed_dvr)
    r |= ACCESS_FAILED_RECORDER;
  if (ae->ae_htsp_anonymize)
    r |= ACCESS_HTSP_ANONYMIZE;
  if (ae->ae_admin)
    r |= ACCESS_ADMIN;
  ae->ae_rights = r;
}

/**
 *
 */

static void access_entry_reindex(void);

access_entry_t *
access_entry_create(const char *uuid, htsmsg_t *conf)
{
  access_entry_t *ae, *ae2;

  lock_assert(&global_lock);

  ae = calloc(1, sizeof(access_entry_t));

  if (idnode_insert(&ae->ae_id, uuid, &access_entry_class, 0)) {
    if (uuid)
      tvherror(LS_ACCESS, "invalid uuid '%s'", uuid);
    free(ae);
    return NULL;
  }

  TAILQ_INIT(&ae->ae_ipmasks);

  ae->ae_uilevel = UILEVEL_DEFAULT;
  ae->ae_uilevel_nochange = -1;

  if (conf) {
    /* defaults */
    ae->ae_change_lang    = 1;
    ae->ae_change_lang_ui = 1;
    ae->ae_change_theme   = 1;
    ae->ae_change_uilevel = 1;
    ae->ae_change_profiles = 1;
    ae->ae_change_conn_limit = 1;
    ae->ae_change_dvr_configs = 1;
    ae->ae_change_chrange = 1;
    ae->ae_change_chtags  = 1;
    ae->ae_change_rights  = 1;
    ae->ae_htsp_streaming = 1;
    ae->ae_htsp_dvr       = 1;
    ae->ae_all_dvr        = 1;
    ae->ae_failed_dvr     = 1;
    idnode_load(&ae->ae_id, conf);
    access_entry_update_rights(ae);
    TAILQ_FOREACH(ae2, &access_entries, ae_link)
      if (ae->ae_index < ae2->ae_index)
        break;
    if (ae2)
      TAILQ_INSERT_BEFORE(ae2, ae, ae_link);
    else
      TAILQ_INSERT_TAIL(&access_entries, ae, ae_link);
  } else {
    TAILQ_INSERT_TAIL(&access_entries, ae, ae_link);
    access_entry_reindex();
  }

  if (ae->ae_username == NULL)
    ae->ae_username = strdup("*");
  if (ae->ae_comment == NULL)
    ae->ae_comment = strdup("New entry");
  if (TAILQ_FIRST(&ae->ae_ipmasks) == NULL)
    access_set_prefix_default(&ae->ae_ipmasks);

  return ae;
}

/**
 *
 */
void
access_entry_destroy(access_entry_t *ae, int delconf)
{
  access_ipmask_t *ai;
  char ubuf[UUID_HEX_SIZE];

  idnode_save_check(&ae->ae_id, delconf);

  if (delconf)
    hts_settings_remove("accesscontrol/%s", idnode_uuid_as_str(&ae->ae_id, ubuf));

  TAILQ_REMOVE(&access_entries, ae, ae_link);
  idnode_unlink(&ae->ae_id);

  idnode_list_destroy(&ae->ae_profiles, ae);
  idnode_list_destroy(&ae->ae_dvr_configs, ae);
  idnode_list_destroy(&ae->ae_chtags, ae);

  while((ai = TAILQ_FIRST(&ae->ae_ipmasks)) != NULL)
  {
    TAILQ_REMOVE(&ae->ae_ipmasks, ai, ai_link);
    free(ai);
  }

  free(ae->ae_username);
  free(ae->ae_comment);
  free(ae->ae_lang);
  free(ae->ae_lang_ui);
  free(ae->ae_theme);
  free(ae);
}

/*
 *
 */
void
access_destroy_by_profile(profile_t *pro, int delconf)
{
  idnode_list_destroy(&pro->pro_accesses, delconf ? pro : NULL);
}

/*
 *
 */
void
access_destroy_by_dvr_config(dvr_config_t *cfg, int delconf)
{
  idnode_list_destroy(&cfg->dvr_accesses, delconf ? cfg : NULL);
}

/*
 *
 */
void
access_destroy_by_channel_tag(channel_tag_t *ct, int delconf)
{
  idnode_list_destroy(&ct->ct_accesses, delconf ? ct : NULL);
}

/**
 *
 */
static void
access_entry_reindex(void)
{
  access_entry_t *ae;
  int i = 1;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {
    if (ae->ae_index != i) {
      ae->ae_index = i;
      idnode_changed(&ae->ae_id);
    }
    i++;
  }
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static htsmsg_t *
access_entry_class_save(idnode_t *self, char *filename, size_t fsize)
{
  access_entry_t *ae = (access_entry_t *)self;
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  access_entry_update_rights((access_entry_t *)self);
  idnode_save(&ae->ae_id, c);
  if (filename)
    snprintf(filename, fsize, "accesscontrol/%s", idnode_uuid_as_str(&ae->ae_id, ubuf));
  return c;
}

static void
access_entry_class_delete(idnode_t *self)
{
  access_entry_t *ae = (access_entry_t *)self;
  access_entry_destroy(ae, 1);
}

static void
access_entry_class_moveup(idnode_t *self)
{
  access_entry_t *ae = (access_entry_t *)self;
  access_entry_t *prev = TAILQ_PREV(ae, access_entry_queue, ae_link);
  if (prev) {
    TAILQ_REMOVE(&access_entries, ae, ae_link);
    TAILQ_INSERT_BEFORE(prev, ae, ae_link);
    access_entry_reindex();
  }
}

static void
access_entry_class_movedown(idnode_t *self)
{
  access_entry_t *ae = (access_entry_t *)self;
  access_entry_t *next = TAILQ_NEXT(ae, ae_link);
  if (next) {
    TAILQ_REMOVE(&access_entries, ae, ae_link);
    TAILQ_INSERT_AFTER(&access_entries, next, ae, ae_link);
    access_entry_reindex();
  }
}

static const char *
access_entry_class_get_title (idnode_t *self, const char *lang)
{
  access_entry_t *ae = (access_entry_t *)self;
  const char *s = ae->ae_username;

  if (ae->ae_comment && ae->ae_comment[0] != '\0') {
    if (ae->ae_username && ae->ae_username[0]) {
      snprintf(prop_sbuf, PROP_SBUF_LEN, "%s (%s)", ae->ae_username, ae->ae_comment);
      s = prop_sbuf;
    } else {
      s = ae->ae_comment;
    }
  }
  if (s == NULL || *s == '\0')
    s = "";
  return s;
}

static int
access_entry_class_prefix_set(void *o, const void *v)
{
  access_set_prefix(&((access_entry_t *)o)->ae_ipmasks, (const char *)v, 1);
  return 1;
}

static const void *
access_entry_class_prefix_get(void *o)
{
  prop_ptr = access_get_prefix(&((access_entry_t *)o)->ae_ipmasks);
  return &prop_ptr;
}

static int
access_entry_chtag_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  channel_tag_t *ct = (channel_tag_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_chtags, in2, &ct->ct_accesses, origin, 1);
  return ilm ? 1 : 0;
}

static int
access_entry_chtag_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  return idnode_list_set1(&ae->ae_id, &ae->ae_chtags,
                          &channel_tag_class, (htsmsg_t *)v,
                          access_entry_chtag_set_cb);
}

static const void *
access_entry_chtag_get(void *o)
{
  return idnode_list_get1(&((access_entry_t *)o)->ae_chtags);
}

static char *
access_entry_chtag_rend (void *o, const char *lang)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_chtags, lang);
}

static int
access_entry_dvr_config_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  dvr_config_t *dvr = (dvr_config_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_dvr_configs, in2, &dvr->dvr_accesses, origin, 1);
  return ilm ? 1 : 0;
}

static int
access_entry_dvr_config_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  return idnode_list_set1(&ae->ae_id, &ae->ae_dvr_configs,
                          &dvr_config_class, (htsmsg_t *)v,
                          access_entry_dvr_config_set_cb);
}

static const void *
access_entry_dvr_config_get(void *o)
{
  return idnode_list_get1(&((access_entry_t *)o)->ae_dvr_configs);
}

static char *
access_entry_dvr_config_rend (void *o, const char *lang)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_dvr_configs, lang);
}

static int
access_entry_profile_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  profile_t *pro = (profile_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_profiles, in2, &pro->pro_accesses, origin, 1);
  return ilm ? 1 : 0;
}

static int
access_entry_profile_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  return idnode_list_set1(&ae->ae_id, &ae->ae_profiles,
                          &profile_class, (htsmsg_t *)v,
                          access_entry_profile_set_cb);
}

static const void *
access_entry_profile_get(void *o)
{
  return idnode_list_get1(&((access_entry_t *)o)->ae_profiles);
}

static char *
access_entry_profile_rend (void *o, const char *lang)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_profiles, lang);
}

static htsmsg_t *
access_entry_conn_limit_type_enum ( void *p, const char *lang )
{
  static struct strtab
  conn_limit_type_tab[] = {
    { N_("All (Streaming plus DVR)"),  ACCESS_CONN_LIMIT_TYPE_ALL },
    { N_("Streaming"),                 ACCESS_CONN_LIMIT_TYPE_STREAMING   },
    { N_("DVR"),                       ACCESS_CONN_LIMIT_TYPE_DVR },
  };
  return strtab2htsmsg(conn_limit_type_tab, 1, lang);
}

htsmsg_t *
language_get_list ( void *obj, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "language/locale");
  return m;
}

htsmsg_t *
language_get_ui_list ( void *obj, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "language/ui_locale");
  return m;
}

htsmsg_t *
user_get_userlist ( void *obj, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "access/entry/userlist");
  htsmsg_add_str(m, "event", "access");
  return m;
}

static htsmsg_t *
uilevel_get_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Default"),  UILEVEL_DEFAULT },
    { N_("Basic"),    UILEVEL_BASIC },
    { N_("Advanced"), UILEVEL_ADVANCED },
    { N_("Expert"),   UILEVEL_EXPERT },
  };
  return strtab2htsmsg(tab, 1, lang);
}

static htsmsg_t *
uilevel_nochange_get_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Default"),  -1 },
    { N_("No"),        0 },
    { N_("Yes"),       1 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

htsmsg_t *
theme_get_ui_list ( void *p, const char *lang )
{
  static struct strtab_str tab[] = {
    { N_("Blue"),     "blue"  },
    { N_("Gray"),     "gray"  },
    { N_("Access"),   "access" },
  };
  return strtab2htsmsg_str(tab, 1, lang);
}

static idnode_slist_t access_entry_class_change_slist[] = {
  {
    .id   = "change_rights",
    .name = N_("Rights"),
    .off  = offsetof(access_entry_t, ae_change_rights),
  },
  {
    .id   = "change_chrange",
    .name = N_("Channel number range"),
    .off  = offsetof(access_entry_t, ae_change_chrange),
  },
  {
    .id   = "change_chtags",
    .name = N_("Channel tags"),
    .off  = offsetof(access_entry_t, ae_change_chtags),
  },
  {
    .id   = "change_dvr_configs",
    .name = N_("DVR configurations"),
    .off  = offsetof(access_entry_t, ae_change_dvr_configs),
  },
  {
    .id   = "change_profiles",
    .name = N_("Streaming profiles"),
    .off  = offsetof(access_entry_t, ae_change_profiles),
  },
  {
    .id   = "change_conn_limit",
    .name = N_("Connection limits"),
    .off  = offsetof(access_entry_t, ae_change_conn_limit),
  },
  {
    .id   = "change_lang",
    .name = N_("Language"),
    .off  = offsetof(access_entry_t, ae_change_lang),
  },
  {
    .id   = "change_lang_ui",
    .name = N_("Web interface language"),
    .off  = offsetof(access_entry_t, ae_change_lang_ui),
  },
  {
    .id   = "change_theme",
    .name = N_("Theme"),
    .off  = offsetof(access_entry_t, ae_change_theme),
  },
  {
    .id   = "change_uilevel",
    .name = N_("User interface level"),
   .off  = offsetof(access_entry_t, ae_change_uilevel),
  },
  {}
};

static htsmsg_t *
access_entry_class_change_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, access_entry_class_change_slist, lang);
}

static const void *
access_entry_class_change_get ( void *obj )
{
  return idnode_slist_get(obj, access_entry_class_change_slist);
}

static char *
access_entry_class_change_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, access_entry_class_change_slist, lang);
}

static int
access_entry_class_change_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, access_entry_class_change_slist, p);
}


static idnode_slist_t access_entry_class_streaming_slist[] = {
  {
    .id   = "basic",
    .name = N_("Basic"),
    .off  = offsetof(access_entry_t, ae_streaming),
  },
  {
    .id   = "advanced",
    .name = N_("Advanced"),
    .off  = offsetof(access_entry_t, ae_adv_streaming),
  },
  {
    .id   = "htsp",
    .name = N_("HTSP"),
    .off  = offsetof(access_entry_t, ae_htsp_streaming),
  },
  {}
};

static htsmsg_t *
access_entry_class_streaming_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, access_entry_class_streaming_slist, lang);
}

static const void *
access_entry_class_streaming_get ( void *obj )
{
  return idnode_slist_get(obj, access_entry_class_streaming_slist);
}

static char *
access_entry_class_streaming_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, access_entry_class_streaming_slist, lang);
}

static int
access_entry_class_streaming_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, access_entry_class_streaming_slist, p);
}

static idnode_slist_t access_entry_class_dvr_slist[] = {
  {
    .id   = "basic",
    .name = N_("Basic"),
    .off  = offsetof(access_entry_t, ae_dvr),
  },
  {
    .id   = "htsp",
    .name = N_("HTSP"),
    .off  = offsetof(access_entry_t, ae_htsp_dvr),
  },
  {
    .id   = "all",
    .name = N_("View all"),
    .off  = offsetof(access_entry_t, ae_all_dvr),
  },
  {
    .id   = "all_rw",
    .name = N_("Manage all"),
    .off  = offsetof(access_entry_t, ae_all_rw_dvr),
  },
  {
    .id   = "failed",
    .name = N_("Failed view"),
    .off  = offsetof(access_entry_t, ae_failed_dvr),
  },
  {}
};

static htsmsg_t *
access_entry_class_dvr_enum ( void *obj, const char *lang )
{
  return idnode_slist_enum(obj, access_entry_class_dvr_slist, lang);
}

static const void *
access_entry_class_dvr_get ( void *obj )
{
  return idnode_slist_get(obj, access_entry_class_dvr_slist);
}

static char *
access_entry_class_dvr_rend ( void *obj, const char *lang )
{
  return idnode_slist_rend(obj, access_entry_class_dvr_slist, lang);
}

static int
access_entry_class_dvr_set ( void *obj, const void *p )
{
  return idnode_slist_set(obj, access_entry_class_dvr_slist, p);
}

CLASS_DOC(access_entry)
PROP_DOC(viewlevel_access_entries)
PROP_DOC(themes)
PROP_DOC(connection_limit)
PROP_DOC(persistent_viewlevel)
PROP_DOC(streaming_profile)
PROP_DOC(change_parameters)

const idclass_t access_entry_class = {
  .ic_class      = "access",
  .ic_caption    = N_("Users - Access Entries"),
  .ic_event      = "access",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_doc        = tvh_doc_access_entry_class,
  .ic_save       = access_entry_class_save,
  .ic_get_title  = access_entry_class_get_title,
  .ic_delete     = access_entry_class_delete,
  .ic_moveup     = access_entry_class_moveup,
  .ic_movedown   = access_entry_class_movedown,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "index",
      .name     = N_("Index"),
      .off      = offsetof(access_entry_t, ae_index),
      .opts     = PO_RDONLY | PO_HIDDEN | PO_NOUI,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/Disable the entry."),
      .def.i    = 1,
      .off      = offsetof(access_entry_t, ae_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .desc     = N_("Username for the entry (login username)."),
      .off      = offsetof(access_entry_t, ae_username),
    },
    {
      .type     = PT_STR,
      .id       = "prefix",
      .name     = N_("Allowed networks"),
      .desc     = N_("List of allowed IPv4 or IPv6 hosts or networks (comma-separated)."),
      .set      = access_entry_class_prefix_set,
      .get      = access_entry_class_prefix_get,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "change",
      .name     = N_("Change parameters"),
      .desc     = N_("Specify the parameters to be changed. See Help for details."),
      .doc      = prop_doc_change_parameters,
      .list     = access_entry_class_change_enum,
      .get      = access_entry_class_change_get,
      .set      = access_entry_class_change_set,
      .rend     = access_entry_class_change_rend,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "uilevel",
      .name     = N_("User interface level"),
      .desc     = N_("Default user interface level."),
      .doc      = prop_doc_viewlevel_access_entries,
      .off      = offsetof(access_entry_t, ae_uilevel),
      .list     = uilevel_get_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "uilevel_nochange",
      .name     = N_("Persistent user interface level"),
      .desc     = N_("Prevent the user from overriding the default user "
                   "interface level setting and removes the view level "
                   "drop-dowm from the interface."),
      .doc      = prop_doc_persistent_viewlevel,
      .off      = offsetof(access_entry_t, ae_uilevel_nochange),
      .list     = uilevel_nochange_get_list,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .id       = "lang",
      .name     = N_("Language"),
      .desc     = N_("Default language."),
      .list     = language_get_list,
      .off      = offsetof(access_entry_t, ae_lang),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "langui",
      .name     = N_("Web interface language"),
      .desc     = N_("Web interface language."),
      .list     = language_get_ui_list,
      .off      = offsetof(access_entry_t, ae_lang_ui),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "themeui",
      .name     = N_("Web theme"),
      .desc     = N_("Web interface theme."),
      .doc      = prop_doc_themes,
      .list     = theme_get_ui_list,
      .off      = offsetof(access_entry_t, ae_theme),
      .opts     = PO_DOC_NLIST | PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "streaming",
      .name     = N_("Streaming"),
      .desc     = N_("Streaming flags, allow/disallow HTTP streaming, "
                     "advanced HTTP streaming (e.g, direct service or mux links), "
                     "HTSP protocol streaming (e.g, Kodi (via pvr.hts) or Movian."),
      .list     = access_entry_class_streaming_enum,
      .get      = access_entry_class_streaming_get,
      .set      = access_entry_class_streaming_set,
      .rend     = access_entry_class_streaming_rend,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "profile",
      .name     = N_("Streaming profiles"),
      .desc     = N_("The streaming profile to use/used. If not set, "
                     "the default will be used."),
      .doc      = prop_doc_streaming_profile,
      .set      = access_entry_profile_set,
      .get      = access_entry_profile_get,
      .list     = profile_class_get_list,
      .rend     = access_entry_profile_rend,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .islist   = 1,
      .id       = "dvr",
      .name     = N_("Video recorder"),
      .desc     = N_("Video recorder flags, allow/disallow access to video recorder "
                     "functionality (including Autorecs), allow/disallow users to "
                     "view other DVR entries, allow/disallow users to work with "
                     "DVR entries of other users (remove, edit) etc."),
      .list     = access_entry_class_dvr_enum,
      .get      = access_entry_class_dvr_get,
      .set      = access_entry_class_dvr_set,
      .rend     = access_entry_class_dvr_rend,
      .opts     = PO_DOC_NLIST,
    },
    {
      .type     = PT_BOOL,
      .id       = "htsp_anonymize",
      .name     = N_("Anonymize HTSP access"),
      .desc     = N_("Do not send any stream specific information to "
                     "the HTSP client like signal strength, input source "
                     "etc."),
      .off      = offsetof(access_entry_t, ae_htsp_anonymize),
      .opts     = PO_EXPERT | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "dvr_config",
      .name     = N_("DVR configuration profiles"),
      .desc     = N_("Allowed DVR profiles. This limits the profiles "
                     "the user has access to."),
      .set      = access_entry_dvr_config_set,
      .get      = access_entry_dvr_config_get,
      .list     = dvr_entry_class_config_name_list,
      .rend     = access_entry_dvr_config_rend,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "webui",
      .name     = N_("Web interface"),
      .desc     = N_("Allow/Disallow web interface access (this "
                     " includes access to the EPG)."),
      .off      = offsetof(access_entry_t, ae_webui),
    },
    {
      .type     = PT_BOOL,
      .id       = "admin",
      .name     = N_("Admin"),
      .desc     = N_("Allow/Disallow access to the 'Configuration' tab."),
      .off      = offsetof(access_entry_t, ae_admin),
    },
    {
      .type     = PT_INT,
      .id       = "conn_limit_type",
      .name     = N_("Connection limit type"),
      .desc     = N_("Restrict connections to this type."),
      .doc      = prop_doc_connection_limit,
      .off      = offsetof(access_entry_t, ae_conn_limit_type),
      .list     = access_entry_conn_limit_type_enum,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_U32,
      .id       = "conn_limit",
      .name     = N_("Limit connections"),
      .desc     = N_("The number of allowed connections this user can "
                     "make to the server."),
      .off      = offsetof(access_entry_t, ae_conn_limit),
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "channel_min",
      .name     = N_("Minimal channel number"),
      .desc     = N_("Lowest channel number the user can access."),
      .off      = offsetof(access_entry_t, ae_chmin),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "channel_max",
      .name     = N_("Maximal channel number"),
      .desc     = N_("Highest channel number the user can access."),
      .off      = offsetof(access_entry_t, ae_chmax),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "channel_tag_exclude",
      .name     = N_("Exclude channel tags"),
      .desc     = N_("Enable exclusion of user-config defined channel "
                     "tags. This will prevent the user from accessing "
                     "channels associated with the tags selected (below)."),
      .off      = offsetof(access_entry_t, ae_chtags_exclude),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channel_tag",
      .name     = N_("Channel tags"),
      .desc     = N_("Channel tags the user is allowed access to/excluded from."),
      .set      = access_entry_chtag_set,
      .get      = access_entry_chtag_get,
      .list     = channel_tag_class_get_list,
      .rend     = access_entry_chtag_rend,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .off      = offsetof(access_entry_t, ae_comment),
    },
    {
      .type     = PT_BOOL,
      .id       = "wizard",
      .name     = N_("Wizard"),
      .off      = offsetof(access_entry_t, ae_wizard),
      .opts     = PO_NOUI
    },
    {}
  }
};

/*
 * Password table
 */

static int passwd_entry_class_password_set(void *o, const void *v);

static int
passwd_verify2
  (const char *username, verify_callback_t verify, void *aux,
   const char *username2, const char *passwd2)
{
  if (username == NULL || username[0] == '\0' ||
      username2 == NULL || username2[0] == '\0' ||
      passwd2 == NULL)
    return -1;

  if (strcmp(username, username2))
    return -1;

  return verify(aux, passwd2) ? 0 : -1;
}

static int
passwd_verify
  (const char *username, verify_callback_t verify, void *aux)
{
  passwd_entry_t *pw;

  TAILQ_FOREACH(pw, &passwd_entries, pw_link)
    if (pw->pw_enabled &&
        !passwd_verify2(username, verify, aux,
                        pw->pw_username, pw->pw_password))
      return 0;
  return -1;
}

passwd_entry_t *
passwd_entry_create(const char *uuid, htsmsg_t *conf)
{
  passwd_entry_t *pw;
  const char *s;

  lock_assert(&global_lock);

  pw = calloc(1, sizeof(passwd_entry_t));

  if (idnode_insert(&pw->pw_id, uuid, &passwd_entry_class, 0)) {
    if (uuid)
      tvherror(LS_ACCESS, "invalid uuid '%s'", uuid);
    free(pw);
    return NULL;
  }

  if (conf) {
    pw->pw_enabled = 1;
    idnode_load(&pw->pw_id, conf);
    /* note password has PO_NOSAVE, thus it must be set manually */
    if ((s = htsmsg_get_str(conf, "password")) != NULL)
      passwd_entry_class_password_set(pw, s);
  }

  TAILQ_INSERT_TAIL(&passwd_entries, pw, pw_link);

  return pw;
}

void
passwd_entry_destroy(passwd_entry_t *pw, int delconf)
{
  char ubuf[UUID_HEX_SIZE];

  if (pw == NULL)
    return;

  idnode_save_check(&pw->pw_id, delconf);

  if (delconf)
    hts_settings_remove("passwd/%s", idnode_uuid_as_str(&pw->pw_id, ubuf));
  TAILQ_REMOVE(&passwd_entries, pw, pw_link);
  idnode_unlink(&pw->pw_id);
  free(pw->pw_username);
  free(pw->pw_password);
  free(pw->pw_password2);
  free(pw->pw_comment);
  free(pw);
}

static htsmsg_t *
passwd_entry_class_save(idnode_t *self, char *filename, size_t fsize)
{
  passwd_entry_t *pw = (passwd_entry_t *)self;
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&pw->pw_id, c);
  if (filename)
    snprintf(filename, fsize, "passwd/%s", idnode_uuid_as_str(&pw->pw_id, ubuf));
  return c;
}

static void
passwd_entry_class_delete(idnode_t *self)
{
  passwd_entry_t *pw = (passwd_entry_t *)self;
  passwd_entry_destroy(pw, 1);
}

static const char *
passwd_entry_class_get_title (idnode_t *self, const char *lang)
{
  passwd_entry_t *pw = (passwd_entry_t *)self;

  if (pw->pw_comment && pw->pw_comment[0] != '\0')
    return pw->pw_comment;
  return pw->pw_username ?: "";
}

static int
passwd_entry_class_password_set(void *o, const void *v)
{
  passwd_entry_t *pw = (passwd_entry_t *)o;
  char buf[256], result[300];

  if (strcmp(v ?: "", pw->pw_password ?: "")) {
    snprintf(buf, sizeof(buf), "TVHeadend-Hide-%s", (const char *)v ?: "");
    base64_encode(result, sizeof(result), (uint8_t *)buf, strlen(buf));
    free(pw->pw_password2);
    pw->pw_password2 = strdup(result);
    free(pw->pw_password);
    pw->pw_password = strdup((const char *)v ?: "");
    return 1;
  }
  return 0;
}

static int
passwd_entry_class_password2_set(void *o, const void *v)
{
  passwd_entry_t *pw = (passwd_entry_t *)o;
  char result[300];
  int l;

  if (strcmp(v ?: "", pw->pw_password2 ?: "")) {
    if (v && ((const char *)v)[0] != '\0') {
      l = base64_decode((uint8_t *)result, v, sizeof(result)-1);
      if (l < 0)
        l = 0;
      result[l] = '\0';
      free(pw->pw_password);
      pw->pw_password = strdup(result + 15);
      free(pw->pw_password2);
      pw->pw_password2 = strdup((const char *)v);
      return 1;
    }
  }
  return 0;
}

CLASS_DOC(passwd)

const idclass_t passwd_entry_class = {
  .ic_class      = "passwd",
  .ic_caption    = N_("Users - Passwords"),
  .ic_event      = "passwd",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_doc        = tvh_doc_passwd_class,
  .ic_save       = passwd_entry_class_save,
  .ic_get_title  = passwd_entry_class_get_title,
  .ic_delete     = passwd_entry_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the entry."),
      .def.i    = 1,
      .off      = offsetof(passwd_entry_t, pw_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = N_("Username"),
      .desc     = N_("Username of the entry (this should match a "
                     "username from within the \"Access Entries\" tab)."),
      .off      = offsetof(passwd_entry_t, pw_username),
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = N_("Password"),
      .desc     = N_("Password for the entry."),
      .off      = offsetof(passwd_entry_t, pw_password),
      .opts     = PO_PASSWORD | PO_NOSAVE,
      .set      = passwd_entry_class_password_set,
    },
    {
      .type     = PT_STR,
      .id       = "password2",
      .name     = N_("Password2"),
      .off      = offsetof(passwd_entry_t, pw_password2),
      .opts     = PO_PASSWORD | PO_HIDDEN | PO_EXPERT | PO_WRONCE | PO_NOUI,
      .set      = passwd_entry_class_password2_set,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = N_("Comment"),
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .off      = offsetof(passwd_entry_t, pw_comment),
    },
    {
      .type     = PT_BOOL,
      .id       = "wizard",
      .name     = N_("Wizard"),
      .off      = offsetof(passwd_entry_t, pw_wizard),
      .opts     = PO_NOUI
    },
    {}
  }
};

/**
 * IP block list
 */

ipblock_entry_t *
ipblock_entry_create(const char *uuid, htsmsg_t *conf)
{
  ipblock_entry_t *ib;

  lock_assert(&global_lock);

  ib = calloc(1, sizeof(ipblock_entry_t));

  TAILQ_INIT(&ib->ib_ipmasks);

  if (idnode_insert(&ib->ib_id, uuid, &ipblock_entry_class, 0)) {
    if (uuid)
      tvherror(LS_ACCESS, "invalid uuid '%s'", uuid);
    free(ib);
    return NULL;
  }

  if (conf) {
    ib->ib_enabled = 1;
    idnode_load(&ib->ib_id, conf);
  }

  TAILQ_INSERT_TAIL(&ipblock_entries, ib, ib_link);

  return ib;
}

static void
ipblock_entry_destroy(ipblock_entry_t *ib, int delconf)
{
  if (ib == NULL)
    return;
  idnode_save_check(&ib->ib_id, delconf);
  TAILQ_REMOVE(&ipblock_entries, ib, ib_link);
  idnode_unlink(&ib->ib_id);
  free(ib->ib_comment);
  free(ib);
}

static htsmsg_t *
ipblock_entry_class_save(idnode_t *self, char *filename, size_t fsize)
{
  ipblock_entry_t *ib = (ipblock_entry_t *)self;
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&ib->ib_id, c);
  if (filename)
    snprintf(filename, fsize, "ipblock/%s", idnode_uuid_as_str(&ib->ib_id, ubuf));
  return c;
}

static const char *
ipblock_entry_class_get_title (idnode_t *self, const char *lang)
{
  ipblock_entry_t *ib = (ipblock_entry_t *)self;

  if (ib->ib_comment && ib->ib_comment[0] != '\0')
    return ib->ib_comment;
  return N_("IP blocking");
}

static void
ipblock_entry_class_delete(idnode_t *self)
{
  ipblock_entry_t *ib = (ipblock_entry_t *)self;
  char ubuf[UUID_HEX_SIZE];

  hts_settings_remove("ipblock/%s", idnode_uuid_as_str(&ib->ib_id, ubuf));
  ipblock_entry_destroy(ib, 1);
}

static int
ipblock_entry_class_prefix_set(void *o, const void *v)
{
  access_set_prefix(&((ipblock_entry_t *)o)->ib_ipmasks, (const char *)v, 0);
  return 1;
}

static const void *
ipblock_entry_class_prefix_get(void *o)
{
  prop_ptr = access_get_prefix(&((ipblock_entry_t *)o)->ib_ipmasks);
  return &prop_ptr;
}

CLASS_DOC(ipblocking)

const idclass_t ipblock_entry_class = {
  .ic_class      = "ipblocking",
  .ic_caption    = N_("Users - IP Blocking"),
  .ic_event      = "ipblocking",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_doc        = tvh_doc_ipblocking_class,
  .ic_save       = ipblock_entry_class_save,
  .ic_get_title  = ipblock_entry_class_get_title,
  .ic_delete     = ipblock_entry_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .desc     = N_("Enable/disable the entry."),
      .off      = offsetof(ipblock_entry_t, ib_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "prefix",
      .name     = N_("Network prefix"),
      .desc     = N_("The network prefix(es) to block, "
                     "e.g.192.168.2.0/24 (comma-separated list)."),
      .set      = ipblock_entry_class_prefix_set,
      .get      = ipblock_entry_class_prefix_get,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .desc     = N_("Free-form text field, enter whatever you like here."),
      .name     = N_("Comment"),
      .off      = offsetof(ipblock_entry_t, ib_comment),
    },
    {}
  }
};

/**
 *
 */
void
access_init(int createdefault, int noacl)
{
  htsmsg_t *c, *m;
  htsmsg_field_t *f;
  access_entry_t *ae;
  const char *s;

  access_noacl = noacl;
  if (noacl)
    tvhwarn(LS_ACCESS, "Access control checking disabled");

  TAILQ_INIT(&access_entries);
  TAILQ_INIT(&access_tickets);
  TAILQ_INIT(&passwd_entries);
  TAILQ_INIT(&ipblock_entries);

  idclass_register(&access_entry_class);
  idclass_register(&passwd_entry_class);
  idclass_register(&ipblock_entry_class);

  /* Load ipblock entries */
  if ((c = hts_settings_load("ipblock")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)ipblock_entry_create(f->hmf_name, m);
    }
    htsmsg_destroy(c);
  }

  /* Load passwd entries */
  if ((c = hts_settings_load("passwd")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)passwd_entry_create(f->hmf_name, m);
    }
    htsmsg_destroy(c);
  }

  /* Load ACL entries */
  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)access_entry_create(f->hmf_name, m);
    }
    htsmsg_destroy(c);
    access_entry_reindex();
  }

  if(createdefault && TAILQ_FIRST(&access_entries) == NULL) {
    /* No records available */
    ae = access_entry_create(NULL, NULL);

    free(ae->ae_comment);
    ae->ae_comment = strdup(ACCESS_DEFAULT_COMMENT);

    ae->ae_enabled        = 1;
    ae->ae_change_rights  = 1;
    ae->ae_streaming      = 1;
    ae->ae_adv_streaming  = 1;
    ae->ae_htsp_streaming = 1;
    ae->ae_webui          = 1;
    ae->ae_dvr            = 1;
    ae->ae_htsp_dvr       = 1;
    ae->ae_all_dvr        = 1;
    ae->ae_all_rw_dvr     = 1;
    ae->ae_failed_dvr     = 1;
    ae->ae_admin          = 1;
    access_entry_update_rights(ae);

    idnode_changed(&ae->ae_id);

    tvhwarn(LS_ACCESS, "Created default wide open access controle entry");
  }

  if(!TAILQ_FIRST(&access_entries) && !noacl)
    tvherror(LS_ACCESS, "No access entries loaded");

  /* Load superuser account */

  if((m = hts_settings_load("superuser")) != NULL) {
    s = htsmsg_get_str(m, "username");
    superuser_username = s ? strdup(s) : NULL;
    s = htsmsg_get_str(m, "password");
    superuser_password = s ? strdup(s) : NULL;
    htsmsg_destroy(m);
  }
}

void
access_done(void)
{
  access_entry_t *ae;
  access_ticket_t *at;
  passwd_entry_t *pw;
  ipblock_entry_t *ib;

  pthread_mutex_lock(&global_lock);
  while ((ae = TAILQ_FIRST(&access_entries)) != NULL)
    access_entry_destroy(ae, 0);
  while ((at = TAILQ_FIRST(&access_tickets)) != NULL)
    access_ticket_destroy(at);
  while ((pw = TAILQ_FIRST(&passwd_entries)) != NULL)
    passwd_entry_destroy(pw, 0);
  while ((ib = TAILQ_FIRST(&ipblock_entries)) != NULL)
    ipblock_entry_destroy(ib, 0);
  free((void *)superuser_username);
  superuser_username = NULL;
  free((void *)superuser_password);
  superuser_password = NULL;
  pthread_mutex_unlock(&global_lock);
}
