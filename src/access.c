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

#include <openssl/sha.h>
#include <openssl/rand.h>

#include "tvheadend.h"
#include "access.h"
#include "settings.h"
#include "channels.h"
#include "dvr/dvr.h"
#include "tcp.h"

struct access_entry_queue access_entries;
struct access_ticket_queue access_tickets;
struct passwd_entry_queue passwd_entries;

const char *superuser_username;
const char *superuser_password;

int access_noacl;

static int passwd_verify(const char *username, const char *passwd);
static int passwd_verify2(const char *username, const char *passwd,
                          const char *username2, const char *passwd2);
static int passwd_verify_digest(const char *username, const uint8_t *digest,
                                const uint8_t *challenge);
static int passwd_verify_digest2(const char *username, const uint8_t *digest,
                                 const uint8_t *challenge,
                                 const char *username2, const char *passwd2);

/**
 *
 */
static void
access_ticket_destroy(access_ticket_t *at)
{
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
    TAILQ_FOREACH(at, &access_tickets, at_link)
      if(!strcmp(at->at_id, id))
	return at;
  }
  
  return NULL;
}

/**
 *
 */
static void
access_ticket_timout(void *aux)
{
  access_ticket_t *at = aux;

  access_ticket_destroy(at);
}

/**
 * Create a new ticket for the requested resource and generate a id for it
 */
const char *
access_ticket_create(const char *resource, access_t *a)
{
  uint8_t buf[20];
  char id[41];
  unsigned int i;
  access_ticket_t *at;
  static const char hex_string[16] = "0123456789ABCDEF";

  assert(a);

  at = calloc(1, sizeof(access_ticket_t));

  RAND_bytes(buf, 20);

  //convert to hexstring
  for(i=0; i<sizeof(buf); i++){
    id[i*2] = hex_string[((buf[i] >> 4) & 0xF)];
    id[(i*2)+1] = hex_string[(buf[i]) & 0x0F];
  }
  id[40] = '\0';

  at->at_id = strdup(id);
  at->at_resource = strdup(resource);

  at->at_access = access_copy(a);

  TAILQ_INSERT_TAIL(&access_tickets, at, at_link);
  gtimer_arm(&at->at_timer, access_ticket_timout, at, 60*5);

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

  gtimer_disarm(&at->at_timer);
  access_ticket_destroy(at);

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
access_t *
access_copy(access_t *src)
{
  access_t *dst = malloc(sizeof(*dst));
  *dst = *src;
  if (src->aa_username)
    dst->aa_username = strdup(src->aa_username);
  if (src->aa_representative)
    dst->aa_representative = strdup(src->aa_representative);
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
void
access_destroy(access_t *a)
{
  if (a == NULL)
    return;
  free(a->aa_username);
  free(a->aa_representative);
  htsmsg_destroy(a->aa_profiles);
  htsmsg_destroy(a->aa_dvrcfgs);
  htsmsg_destroy(a->aa_chtags);
  free(a);
}

/**
 *
 */
static int
netmask_verify(access_entry_t *ae, struct sockaddr *src)
{
  access_ipmask_t *ai;
  int isv4v6 = 0;
  uint32_t v4v6 = 0;
  
  if (src->sa_family == AF_INET6) {
    struct in6_addr *in6 = &(((struct sockaddr_in6 *)src)->sin6_addr);
    uint32_t *a32 = (uint32_t*)in6->s6_addr;
    if (a32[0] == 0 && a32[1] == 0 && ntohl(a32[2]) == 0x0000FFFFu) {
      isv4v6 = 1;
      v4v6 = ntohl(a32[3]);
    }
  }

  TAILQ_FOREACH(ai, &ae->ae_ipmasks, ai_link) {

    if (ai->ai_family == AF_INET && src->sa_family == AF_INET) {

      struct sockaddr_in *in4 = (struct sockaddr_in *)src;
      uint32_t b = ntohl(in4->sin_addr.s_addr);
      if ((b & ai->ai_netmask) == ai->ai_network)
        return 1;

    } else if (ai->ai_family == AF_INET && isv4v6) {

      if((v4v6 & ai->ai_netmask) == ai->ai_network)
        return 1;

    } else if (ai->ai_family == AF_INET6 && isv4v6) {

      continue;

    } else if (ai->ai_family == AF_INET6 && src->sa_family == AF_INET6) {

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
int
access_verify(const char *username, const char *password,
	      struct sockaddr *src, uint32_t mask)
{
  uint32_t bits = 0;
  access_entry_t *ae;
  int match = 0, nouser = username == NULL || username[0] == '\0';

  if (access_noacl)
    return 0;

  if (!passwd_verify2(username, password,
                      superuser_username, superuser_password))
    return 0;

  if (passwd_verify(username, password))
    username = NULL;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      if(username == NULL || strcmp(username, ae->ae_username))
	continue; /* Didn't get one */
    }

    if(!netmask_verify(ae, src))
      continue; /* IP based access mismatches */

    if (ae->ae_username[0] != '*')
      match = 1;

    bits |= ae->ae_rights;
  }

  /* Username was not matched - no access */
  if (!match && !nouser)
    bits = 0;

  return (mask & ACCESS_OR) ?
         ((mask & bits) ? 0 : -1) :
         ((mask & bits) == mask ? 0 : -1);
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
    "%s:%s [%c%c%c%c%c%c%c%c%c%c], conn=%u:s%u:r%u%s",
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
    a->aa_rights & ACCESS_ADMIN              ? '*' : ' ',
    a->aa_conn_limit,
    a->aa_conn_limit_streaming,
    a->aa_conn_limit_dvr,
    a->aa_match ? ", matched" : "");

  if (a->aa_profiles) {
    first = 1;
    HTSMSG_FOREACH(f, a->aa_profiles) {
      profile_t *pro = profile_find_by_uuid(htsmsg_field_get_str(f) ?: "");
      if (pro) {
        if (first)
          tvh_strlcatf(buf, sizeof(buf), l, ", profile=");
        tvh_strlcatf(buf, sizeof(buf), l, "%s'%s'",
                 first ? "" : ",", pro->pro_name ?: "");
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

  tvhtrace("access", "%s", buf);
}

/*
 *
 */
static access_t *access_alloc(void)
{
  return calloc(1, sizeof(access_t));
}

/*
 *
 */
static void
access_update(access_t *a, access_entry_t *ae)
{
  idnode_list_mapping_t *ilm;

  switch (ae->ae_conn_limit_type) {
  case ACCESS_CONN_LIMIT_TYPE_ALL:
    if (a->aa_conn_limit < ae->ae_conn_limit)
      a->aa_conn_limit = ae->ae_conn_limit;
    break;
  case ACCESS_CONN_LIMIT_TYPE_STREAMING:
    if (a->aa_conn_limit_streaming < ae->ae_conn_limit)
      a->aa_conn_limit_streaming = ae->ae_conn_limit;
    break;
  case ACCESS_CONN_LIMIT_TYPE_DVR:
    if (a->aa_conn_limit_dvr < ae->ae_conn_limit)
      a->aa_conn_limit_dvr = ae->ae_conn_limit;
    break;
  }

  if(ae->ae_chmin || ae->ae_chmax) {
    uint64_t *p = realloc(a->aa_chrange, (a->aa_chrange_count + 2) * sizeof(uint64_t));
    if (p) {
      p[a->aa_chrange_count++] = ae->ae_chmin;
      p[a->aa_chrange_count++] = ae->ae_chmax;
      a->aa_chrange = p;
    }
  }

  LIST_FOREACH(ilm, &ae->ae_profiles, ilm_in1_link) {
    profile_t *pro = (profile_t *)ilm->ilm_in2;
    if(pro && pro->pro_name[0] != '\0') {
      if (a->aa_profiles == NULL)
        a->aa_profiles = htsmsg_create_list();
      htsmsg_add_str_exclusive(a->aa_profiles, idnode_uuid_as_str(&pro->pro_id));
    }
  }

  LIST_FOREACH(ilm, &ae->ae_dvr_configs, ilm_in1_link) {
    dvr_config_t *dvr = (dvr_config_t *)ilm->ilm_in2;
    if(dvr && dvr->dvr_config_name[0] != '\0') {
      if (a->aa_dvrcfgs == NULL)
        a->aa_dvrcfgs = htsmsg_create_list();
      htsmsg_add_str_exclusive(a->aa_dvrcfgs, idnode_uuid_as_str(&dvr->dvr_id));
     }
  }

  if (ae->ae_chtags_exclude) {
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
          htsmsg_add_str_exclusive(a->aa_chtags, idnode_uuid_as_str(&ct->ct_id));
        }
      }
    }
  } else {
    LIST_FOREACH(ilm, &ae->ae_chtags, ilm_in1_link) {
      channel_tag_t *ct = (channel_tag_t *)ilm->ilm_in2;
      if(ct && ct->ct_name[0] != '\0') {
        if (a->aa_chtags == NULL)
          a->aa_chtags = htsmsg_create_list();
        htsmsg_add_str_exclusive(a->aa_chtags, idnode_uuid_as_str(&ct->ct_id));
      }
    }
  }

  a->aa_rights |= ae->ae_rights;
}

/**
 *
 */
access_t *
access_get(const char *username, const char *password, struct sockaddr *src)
{
  access_t *a = access_alloc();
  access_entry_t *ae;
  int nouser = username == NULL || username[0] == '\0';

  if (!passwd_verify(username, password)) {
    a->aa_username = strdup(username);
    a->aa_representative = strdup(username);
    if(!passwd_verify2(username, password,
                       superuser_username, superuser_password)) {
      a->aa_rights = ACCESS_FULL;
      return a;
    }
  } else {
    a->aa_representative = malloc(50);
    tcp_get_str_from_ip((struct sockaddr*)src, a->aa_representative, 50);
    if(!passwd_verify2(username, password,
                       superuser_username, superuser_password)) {
      a->aa_rights = ACCESS_FULL;
      return a;
    }
    username = NULL;
  }

  if (access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      if(username == NULL || strcmp(username, ae->ae_username))
	continue; /* Didn't get one */
    }

    if(!netmask_verify(ae, src))
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

  if (tvhtrace_enabled())
    access_dump_a(a);
  return a;
}

/**
 *
 */
access_t *
access_get_hashed(const char *username, const uint8_t digest[20],
		  const uint8_t *challenge, struct sockaddr *src)
{
  access_t *a = access_alloc();
  access_entry_t *ae;
  int nouser = username == NULL || username[0] == '\0';

  if (!passwd_verify_digest(username, digest, challenge)) {
    a->aa_username = strdup(username);
    a->aa_representative = strdup(username);
    if(!passwd_verify_digest2(username, digest, challenge,
                              superuser_username, superuser_password)) {
      a->aa_rights = ACCESS_FULL;
      return a;
    }
  } else {
    a->aa_representative = malloc(50);
    tcp_get_str_from_ip((struct sockaddr*)src, a->aa_representative, 50);
    if(!passwd_verify_digest2(username, digest, challenge,
                              superuser_username, superuser_password)) {
      a->aa_rights = ACCESS_FULL;
      return a;
    }
    username = NULL;
  }

  if(access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(!netmask_verify(ae, src))
      continue; /* IP based access mismatches */

    if(ae->ae_username[0] != '*') {

      if (username == NULL || strcmp(username, ae->ae_username))
        continue;

      a->aa_match = 1;
    }

    access_update(a, ae);
  }

  /* Username was not matched - no access */
  if (!a->aa_match) {
    free(a->aa_username);
    a->aa_username = NULL;
    if (!nouser)
      a->aa_rights = 0;
  }

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

  if(access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  if (username[0] == '\0')
    return a;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] == '*' || strcmp(ae->ae_username, username))
      continue;

    access_update(a, ae);
  }

  return a;
}

/**
 *
 */
access_t *
access_get_by_addr(struct sockaddr *src)
{
  access_t *a = access_alloc();
  access_entry_t *ae;

  a->aa_representative = malloc(50);
  tcp_get_str_from_ip(src, a->aa_representative, 50);

  if(access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*')
      continue;

    if(!netmask_verify(ae, src))
      continue; /* IP based access mismatches */

    access_update(a, ae);
  }

  return a;
}

/**
 *
 */
static void
access_set_prefix_default(access_entry_t *ae)
{
  access_ipmask_t *ai;

  ai = calloc(1, sizeof(access_ipmask_t));
  ai->ai_family = AF_INET6;
  TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);

  ai = calloc(1, sizeof(access_ipmask_t));
  ai->ai_family = AF_INET;
  TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);
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
access_set_prefix(access_entry_t *ae, const char *prefix)
{
  static const char *delim = ",;| ";
  char buf[100];
  char tokbuf[4096];
  int prefixlen;
  char *p, *tok, *saveptr;
  in_addr_t s_addr;
  access_ipmask_t *ai = NULL;

  while((ai = TAILQ_FIRST(&ae->ae_ipmasks)) != NULL) {
    TAILQ_REMOVE(&ae->ae_ipmasks, ai, ai_link);
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

    TAILQ_INSERT_TAIL(&ae->ae_ipmasks, ai, ai_link);
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

  if (!TAILQ_FIRST(&ae->ae_ipmasks))
    access_set_prefix_default(ae);
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
      tvherror("access", "invalid uuid '%s'", uuid);
    free(ae);
    return NULL;
  }

  TAILQ_INIT(&ae->ae_ipmasks);

  if (conf) {
    /* defaults */
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
    access_set_prefix_default(ae);

  return ae;
}

/**
 *
 */
static void
access_entry_destroy(access_entry_t *ae)
{
  access_ipmask_t *ai;

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
  free(ae);
}

/*
 *
 */
void
access_destroy_by_profile(profile_t *pro, int delconf)
{
  idnode_list_destroy(&pro->pro_accesses, pro);
}

/*
 *
 */
void
access_destroy_by_dvr_config(dvr_config_t *cfg, int delconf)
{
  idnode_list_destroy(&cfg->dvr_accesses, cfg);
}

/*
 *
 */
void
access_destroy_by_channel_tag(channel_tag_t *ct, int delconf)
{
  idnode_list_destroy(&ct->ct_accesses, ct);
}

/**
 *
 */
void
access_entry_save(access_entry_t *ae)
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&ae->ae_id, c);
  hts_settings_save(c, "accesscontrol/%s", idnode_uuid_as_str(&ae->ae_id));
  htsmsg_destroy(c);
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
      access_entry_save(ae);
    }
    i++;
  }
}

/* **************************************************************************
 * Class definition
 * **************************************************************************/

static void
access_entry_class_save(idnode_t *self)
{
  access_entry_update_rights((access_entry_t *)self);
  access_entry_save((access_entry_t *)self);
}

static void
access_entry_class_delete(idnode_t *self)
{
  access_entry_t *ae = (access_entry_t *)self;

  hts_settings_remove("accesscontrol/%s", idnode_uuid_as_str(&ae->ae_id));
  access_entry_destroy(ae);
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
access_entry_class_get_title (idnode_t *self)
{
  access_entry_t *ae = (access_entry_t *)self;

  if (ae->ae_comment && ae->ae_comment[0] != '\0')
    return ae->ae_comment;
  return ae->ae_username ?: "";
}

static int
access_entry_class_prefix_set(void *o, const void *v)
{
  access_set_prefix((access_entry_t *)o, (const char *)v);
  return 1;
}

static const void *
access_entry_class_prefix_get(void *o)
{
  static char buf[4096], addrbuf[50], *ret = buf+1;
  access_entry_t *ae = (access_entry_t *)o;
  access_ipmask_t *ai;
  size_t pos = 0;
  uint32_t s_addr;

  buf[0] = buf[1] = '\0';
  TAILQ_FOREACH(ai, &ae->ae_ipmasks, ai_link)   {
    if(sizeof(buf)-pos <= 0)
      break;

    if(ai->ai_family == AF_INET6) {
      inet_ntop(AF_INET6, &ai->ai_ip6, addrbuf, sizeof(addrbuf));
    } else {
      s_addr = htonl(ai->ai_network);
      inet_ntop(AF_INET, &s_addr, addrbuf, sizeof(addrbuf));
    }
    tvh_strlcatf(buf, sizeof(buf), pos, ",%s/%d", addrbuf, ai->ai_prefixlen);
  }
  return &ret;
}

static int
access_entry_chtag_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  channel_tag_t *ct = (channel_tag_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_chtags, in2, &ct->ct_accesses, origin);
  if (ilm) {
    ilm->ilm_in1_save = 1;
    return 1;
  }
  return 0;
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
access_entry_chtag_rend (void *o)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_chtags);
}

static int
access_entry_dvr_config_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  dvr_config_t *dvr = (dvr_config_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_dvr_configs, in2, &dvr->dvr_accesses, origin);
  if (ilm) {
    ilm->ilm_in1_save = 1;
    return 1;
  }
  return 0;
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
access_entry_dvr_config_rend (void *o)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_dvr_configs);
}

static int
access_entry_profile_set_cb ( idnode_t *in1, idnode_t *in2, void *origin )
{
  access_entry_t *ae = (access_entry_t *)in1;
  idnode_list_mapping_t *ilm;
  profile_t *pro = (profile_t *)in2;
  ilm = idnode_list_link(in1, &ae->ae_profiles, in2, &pro->pro_accesses, origin);
  if (ilm) {
    ilm->ilm_in1_save = 1;
    return 1;
  }
  return 0;
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
access_entry_profile_rend (void *o)
{
  return idnode_list_get_csv1(&((access_entry_t *)o)->ae_profiles);
}

static htsmsg_t *
access_entry_conn_limit_type_enum ( void *p )
{
  static struct strtab
  conn_limit_type_tab[] = {
    { "All (Streaming + DVR)",  ACCESS_CONN_LIMIT_TYPE_ALL },
    { "Streaming",              ACCESS_CONN_LIMIT_TYPE_STREAMING   },
    { "DVR",                    ACCESS_CONN_LIMIT_TYPE_DVR },
  };
  return strtab2htsmsg(conn_limit_type_tab);
}

const idclass_t access_entry_class = {
  .ic_class      = "access",
  .ic_caption    = "Access",
  .ic_event      = "access",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = access_entry_class_save,
  .ic_get_title  = access_entry_class_get_title,
  .ic_delete     = access_entry_class_delete,
  .ic_moveup     = access_entry_class_moveup,
  .ic_movedown   = access_entry_class_movedown,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "index",
      .name     = "Index",
      .off      = offsetof(access_entry_t, ae_index),
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(access_entry_t, ae_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = "Username",
      .off      = offsetof(access_entry_t, ae_username),
    },
    {
      .type     = PT_STR,
      .id       = "prefix",
      .name     = "Network prefix",
      .set      = access_entry_class_prefix_set,
      .get      = access_entry_class_prefix_get,
    },
    {
      .type     = PT_BOOL,
      .id       = "streaming",
      .name     = "Streaming",
      .off      = offsetof(access_entry_t, ae_streaming),
    },
    {
      .type     = PT_BOOL,
      .id       = "adv_streaming",
      .name     = "Advanced Streaming",
      .off      = offsetof(access_entry_t, ae_adv_streaming),
    },
    {
      .type     = PT_BOOL,
      .id       = "htsp_streaming",
      .name     = "HTSP Streaming",
      .off      = offsetof(access_entry_t, ae_htsp_streaming),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "profile",
      .name     = "Streaming Profiles",
      .set      = access_entry_profile_set,
      .get      = access_entry_profile_get,
      .list     = profile_class_get_list,
      .rend     = access_entry_profile_rend,
    },
    {
      .type     = PT_BOOL,
      .id       = "dvr",
      .name     = "Video Recorder",
      .off      = offsetof(access_entry_t, ae_dvr),
    },
    {
      .type     = PT_BOOL,
      .id       = "htsp_dvr",
      .name     = "HTSP DVR",
      .off      = offsetof(access_entry_t, ae_htsp_dvr),
    },
    {
      .type     = PT_BOOL,
      .id       = "all_dvr",
      .name     = "All DVR",
      .off      = offsetof(access_entry_t, ae_all_dvr),
    },
    {
      .type     = PT_BOOL,
      .id       = "all_rw_dvr",
      .name     = "All DVR (rw)",
      .off      = offsetof(access_entry_t, ae_all_rw_dvr),
    },
    {
      .type     = PT_BOOL,
      .id       = "failed_dvr",
      .name     = "Failed DVR",
      .off      = offsetof(access_entry_t, ae_failed_dvr),
      .opts     = PO_ADVANCED | PO_HIDDEN,
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "dvr_config",
      .name     = "DVR Config Profiles",
      .set      = access_entry_dvr_config_set,
      .get      = access_entry_dvr_config_get,
      .list     = dvr_entry_class_config_name_list,
      .rend     = access_entry_dvr_config_rend,
    },
    {
      .type     = PT_BOOL,
      .id       = "webui",
      .name     = "Web Interface",
      .off      = offsetof(access_entry_t, ae_webui),
    },
    {
      .type     = PT_BOOL,
      .id       = "admin",
      .name     = "Admin",
      .off      = offsetof(access_entry_t, ae_admin),
    },
    {
      .type     = PT_INT,
      .id       = "conn_limit_type",
      .name     = "Connection Limit Type",
      .off      = offsetof(access_entry_t, ae_conn_limit_type),
      .list     = access_entry_conn_limit_type_enum,
    },
    {
      .type     = PT_U32,
      .id       = "conn_limit",
      .name     = "Limit Connections",
      .off      = offsetof(access_entry_t, ae_conn_limit),
    },
    {
      .type     = PT_S64,
      .intsplit = CHANNEL_SPLIT,
      .id       = "channel_min",
      .name     = "Min Channel Num",
      .off      = offsetof(access_entry_t, ae_chmin),
    },
    {
      .type     = PT_S64,
      .intsplit = CHANNEL_SPLIT,
      .id       = "channel_max",
      .name     = "Max Channel Num",
      .off      = offsetof(access_entry_t, ae_chmax),
    },
    {
      .type     = PT_BOOL,
      .id       = "channel_tag_exclude",
      .name     = "Exclude Channel Tags",
      .off      = offsetof(access_entry_t, ae_chtags_exclude),
    },
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "channel_tag",
      .name     = "Channel Tags",
      .set      = access_entry_chtag_set,
      .get      = access_entry_chtag_get,
      .list     = channel_tag_class_get_list,
      .rend     = access_entry_chtag_rend,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
      .off      = offsetof(access_entry_t, ae_comment),
    },
    {}
  }
};

/*
 * Password table
 */

static int passwd_entry_class_password_set(void *o, const void *v);

static int
passwd_verify_digest2(const char *username, const uint8_t *digest,
                      const uint8_t *challenge,
                      const char *username2, const char *passwd2)
{
  SHA_CTX shactx;
  uint8_t d[20];

  if (username == NULL || username[0] == '\0' ||
      username2 == NULL || username2[0] == '\0' ||
      passwd2 == NULL || passwd2[0] == '\0')
    return -1;

  if (strcmp(username, username2))
    return -1;

  SHA1_Init(&shactx);
  SHA1_Update(&shactx, (const uint8_t *)passwd2, strlen(passwd2));
  SHA1_Update(&shactx, challenge, 32);
  SHA1_Final(d, &shactx);

  return memcmp(d, digest, 20) ? -1 : 0;
}

static int
passwd_verify_digest(const char *username, const uint8_t *digest,
                     const uint8_t *challenge)
{
  passwd_entry_t *pw;

  TAILQ_FOREACH(pw, &passwd_entries, pw_link)
    if (pw->pw_enabled &&
        !passwd_verify_digest2(username, digest, challenge,
                               pw->pw_username, pw->pw_password))
      return 0;
  return -1;
}

static int
passwd_verify2(const char *username, const char *passwd,
               const char *username2, const char *passwd2)
{
  if (username == NULL || username[0] == '\0' ||
      username2 == NULL || username2[0] == '\0' ||
      passwd == NULL || passwd2 == NULL)
    return -1;

  if (strcmp(username, username2))
    return -1;

  return strcmp(passwd, passwd2) ? -1 : 0;
}

static int
passwd_verify(const char *username, const char *passwd)
{
  passwd_entry_t *pw;

  TAILQ_FOREACH(pw, &passwd_entries, pw_link)
    if (pw->pw_enabled &&
        !passwd_verify2(username, passwd,
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
      tvherror("access", "invalid uuid '%s'", uuid);
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

static void
passwd_entry_destroy(passwd_entry_t *pw)
{
  if (pw == NULL)
    return;
  TAILQ_REMOVE(&passwd_entries, pw, pw_link);
  idnode_unlink(&pw->pw_id);
  free(pw->pw_username);
  free(pw->pw_password);
  free(pw->pw_password2);
  free(pw->pw_comment);
  free(pw);
}

void
passwd_entry_save(passwd_entry_t *pw)
{
  htsmsg_t *c = htsmsg_create_map();
  idnode_save(&pw->pw_id, c);
  hts_settings_save(c, "passwd/%s", idnode_uuid_as_str(&pw->pw_id));
  htsmsg_destroy(c);
}

static void
passwd_entry_class_save(idnode_t *self)
{
  passwd_entry_save((passwd_entry_t *)self);
}

static void
passwd_entry_class_delete(idnode_t *self)
{
  passwd_entry_t *pw = (passwd_entry_t *)self;

  hts_settings_remove("passwd/%s", idnode_uuid_as_str(&pw->pw_id));
  passwd_entry_destroy(pw);
}

static const char *
passwd_entry_class_get_title (idnode_t *self)
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

const idclass_t passwd_entry_class = {
  .ic_class      = "passwd",
  .ic_caption    = "Passwords",
  .ic_event      = "passwd",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = passwd_entry_class_save,
  .ic_get_title  = passwd_entry_class_get_title,
  .ic_delete     = passwd_entry_class_delete,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = "Enabled",
      .off      = offsetof(passwd_entry_t, pw_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "username",
      .name     = "Username",
      .off      = offsetof(passwd_entry_t, pw_username),
    },
    {
      .type     = PT_STR,
      .id       = "password",
      .name     = "Password",
      .off      = offsetof(passwd_entry_t, pw_password),
      .opts     = PO_PASSWORD | PO_NOSAVE,
      .set      = passwd_entry_class_password_set,
    },
    {
      .type     = PT_STR,
      .id       = "password2",
      .name     = "Password2",
      .off      = offsetof(passwd_entry_t, pw_password2),
      .opts     = PO_PASSWORD | PO_HIDDEN | PO_ADVANCED | PO_WRONCE,
      .set      = passwd_entry_class_password2_set,
    },
    {
      .type     = PT_STR,
      .id       = "comment",
      .name     = "Comment",
      .off      = offsetof(passwd_entry_t, pw_comment),
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

  static struct {
    pid_t pid;
    struct timeval tv;
  } randseed;

  access_noacl = noacl;
  if (noacl)
    tvhlog(LOG_WARNING, "access", "Access control checking disabled");

  randseed.pid = getpid();
  gettimeofday(&randseed.tv, NULL);
  RAND_seed(&randseed, sizeof(randseed));

  TAILQ_INIT(&access_entries);
  TAILQ_INIT(&access_tickets);
  TAILQ_INIT(&passwd_entries);

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
    ae->ae_comment = strdup("Default access entry");

    ae->ae_enabled        = 1;
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

    TAILQ_INIT(&ae->ae_ipmasks);

    access_set_prefix_default(ae);

    access_entry_save(ae);

    tvhlog(LOG_WARNING, "access",
	   "Created default wide open access controle entry");
  }

  if(!TAILQ_FIRST(&access_entries) && !noacl)
    tvherror("access", "No access entries loaded");

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

  pthread_mutex_lock(&global_lock);
  while ((ae = TAILQ_FIRST(&access_entries)) != NULL)
    access_entry_destroy(ae);
  while ((at = TAILQ_FIRST(&access_tickets)) != NULL)
    access_ticket_destroy(at);
  while ((pw = TAILQ_FIRST(&passwd_entries)) != NULL)
    passwd_entry_destroy(pw);
  free((void *)superuser_username);
  superuser_username = NULL;
  free((void *)superuser_password);
  superuser_password = NULL;
  pthread_mutex_unlock(&global_lock);
}
