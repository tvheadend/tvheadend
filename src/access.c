/*
 *  tvheadend, access control
 *  Copyright (C) 2008 Andreas �man
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

const char *superuser_username;
const char *superuser_password;

int access_noacl;

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

  if((at = access_ticket_find(id)) == NULL)
    return NULL;

  if(strcmp(at->at_resource, resource))
    return NULL;

  return access_copy(at->at_access);
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
  int match = 0;

  if (access_noacl)
    return 0;

  if(username != NULL && superuser_username != NULL && 
     password != NULL && superuser_password != NULL && 
     !strcmp(username, superuser_username) &&
     !strcmp(password, superuser_password))
    return 0;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      if(username == NULL || password == NULL)
	continue; /* Didn't get one */

      if(strcmp(ae->ae_username, username) ||
	 strcmp(ae->ae_password, password))
	continue; /* username/password mismatch */
    }

    if(!netmask_verify(ae, src))
      continue; /* IP based access mismatches */

    if (ae->ae_username[0] != '*')
      match = 1;

    bits |= ae->ae_rights;
  }

  /* Username was not matched - no access */
  if (!match) {
    if (username && *username != '\0')
      bits = 0;
  }

  return (mask & bits) == mask ? 0 : -1;
}

/*
 *
 */
static void
access_update(access_t *a, access_entry_t *ae)
{
  if(a->aa_conn_limit < ae->ae_conn_limit)
    a->aa_conn_limit = ae->ae_conn_limit;

  if(ae->ae_chmin || ae->ae_chmax) {
    if(a->aa_chmin || a->aa_chmax) {
      if (a->aa_chmin < ae->ae_chmin)
        a->aa_chmin = ae->ae_chmin;
      if (a->aa_chmax > ae->ae_chmax)
        a->aa_chmax = ae->ae_chmax;
    } else {
      a->aa_chmin = ae->ae_chmin;
      a->aa_chmax = ae->ae_chmax;
    }
  }

  if(ae->ae_profile && ae->ae_profile->pro_name[0] != '\0') {
    if (a->aa_profiles == NULL)
      a->aa_profiles = htsmsg_create_list();
    htsmsg_add_str(a->aa_profiles, NULL, idnode_uuid_as_str(&ae->ae_profile->pro_id));
  }

  if(ae->ae_dvr_config && ae->ae_dvr_config->dvr_config_name[0] != '\0') {
    if (a->aa_dvrcfgs == NULL)
      a->aa_dvrcfgs = htsmsg_create_list();
    htsmsg_add_str(a->aa_dvrcfgs, NULL, idnode_uuid_as_str(&ae->ae_dvr_config->dvr_id));
  }

  if(ae->ae_chtag && ae->ae_chtag->ct_name[0] != '\0') {
    if (a->aa_chtags == NULL)
      a->aa_chtags = htsmsg_create_list();
    htsmsg_add_str(a->aa_chtags, NULL, idnode_uuid_as_str(&ae->ae_chtag->ct_id));
  }

  a->aa_rights |= ae->ae_rights;
}

/**
 *
 */
access_t *
access_get(const char *username, const char *password, struct sockaddr *src)
{
  access_t *a = calloc(1, sizeof(*a));
  access_entry_t *ae;

  if (username) {
    a->aa_username = strdup(username);
    a->aa_representative = strdup(username);
  } else {
    a->aa_representative = malloc(50);
    tcp_get_ip_str((struct sockaddr*)src, a->aa_representative, 50);
  }

  if (access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  if(username != NULL && superuser_username != NULL &&
     password != NULL && superuser_password != NULL &&
     !strcmp(username, superuser_username) &&
     !strcmp(password, superuser_password)) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      if(username == NULL || password == NULL)
	continue; /* Didn't get one */

      if(strcmp(ae->ae_username, username) ||
	 strcmp(ae->ae_password, password))
	continue; /* username/password mismatch */
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
    if (username && *username != '\0')
      a->aa_rights = 0;
  }

  return a;
}

/**
 *
 */
access_t *
access_get_hashed(const char *username, const uint8_t digest[20],
		  const uint8_t *challenge, struct sockaddr *src)
{
  access_t *a = calloc(1, sizeof(*a));
  access_entry_t *ae;
  SHA_CTX shactx;
  uint8_t d[20];

  if (username) {
    a->aa_username = strdup(username);
    a->aa_representative = strdup(username);
  } else {
    a->aa_representative = malloc(50);
    tcp_get_ip_str((struct sockaddr*)src, a->aa_representative, 50);
  }

  if(access_noacl) {
    a->aa_rights = ACCESS_FULL;
    return a;
  }

  if(superuser_username != NULL && superuser_password != NULL) {

    SHA1_Init(&shactx);
    SHA1_Update(&shactx, (const uint8_t *)superuser_password,
		strlen(superuser_password));
    SHA1_Update(&shactx, challenge, 32);
    SHA1_Final(d, &shactx);

    if(!strcmp(superuser_username, username) && !memcmp(d, digest, 20)) {
      a->aa_rights = ACCESS_FULL;
      return a;
    }
  }

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if(!netmask_verify(ae, src))
      continue; /* IP based access mismatches */

    if(ae->ae_username[0] != '*') {
      SHA1_Init(&shactx);
      SHA1_Update(&shactx, (const uint8_t *)ae->ae_password,
                  strlen(ae->ae_password));
      SHA1_Update(&shactx, challenge, 32);
      SHA1_Final(d, &shactx);

      if(strcmp(ae->ae_username, username) || memcmp(d, digest, 20))
        continue;

      a->aa_match = 1;
    }

    access_update(a, ae);
  }

  /* Username was not matched - no access */
  if (!a->aa_match) {
    free(a->aa_username);
    a->aa_username = NULL;
    if (username && *username != '\0')
      a->aa_rights = 0;
  }

  return a;
}



/**
 *
 */
access_t *
access_get_by_addr(struct sockaddr *src)
{
  access_t *a = calloc(1, sizeof(*a));
  access_entry_t *ae;

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
  if (ae->ae_dvr)
    r |= ACCESS_RECORDER;
  if (ae->ae_webui)
    r |= ACCESS_WEB_INTERFACE;
  if (ae->ae_admin)
    r |= ACCESS_ADMIN;
  ae->ae_rights = r;
}

/**
 *
 */

static void access_entry_reindex(void);
static int access_entry_class_password_set(void *o, const void *v);

access_entry_t *
access_entry_create(const char *uuid, htsmsg_t *conf)
{
  access_entry_t *ae, *ae2;
  const char *s;

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
    idnode_load(&ae->ae_id, conf);
    /* note password has PO_NOSAVE, thus it must be set manually */
    if ((s = htsmsg_get_str(conf, "password")) != NULL)
      access_entry_class_password_set(ae, s);
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
  }

  if (ae->ae_username == NULL)
    ae->ae_username = strdup("*");
  if (ae->ae_comment == NULL)
    ae->ae_comment = strdup("New entry");
  if (ae->ae_password == NULL)
    access_entry_class_password_set(ae, "*");
  if (TAILQ_FIRST(&ae->ae_ipmasks) == NULL)
    access_set_prefix_default(ae);

  access_entry_reindex();

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

  if (ae->ae_profile)
    LIST_REMOVE(ae, ae_profile_link);
  if (ae->ae_dvr_config)
    LIST_REMOVE(ae, ae_dvr_config_link);
  if (ae->ae_chtag)
    LIST_REMOVE(ae, ae_channel_tag_link);

  while((ai = TAILQ_FIRST(&ae->ae_ipmasks)) != NULL)
  {
    TAILQ_REMOVE(&ae->ae_ipmasks, ai, ai_link);
    free(ai);
  }

  free(ae->ae_username);
  free(ae->ae_password);
  free(ae->ae_password2);
  free(ae->ae_comment);
  free(ae);
}

/*
 *
 */
void
access_destroy_by_profile(profile_t *pro, int delconf)
{
  access_entry_t *ae;

  while ((ae = LIST_FIRST(&pro->pro_accesses)) != NULL) {
    LIST_REMOVE(ae, ae_profile_link);
    ae->ae_dvr_config = NULL;
    if (delconf)
      access_entry_save(ae);
  }
}

/*
 *
 */
void
access_destroy_by_dvr_config(dvr_config_t *cfg, int delconf)
{
  access_entry_t *ae;

  while ((ae = LIST_FIRST(&cfg->dvr_accesses)) != NULL) {
    LIST_REMOVE(ae, ae_dvr_config_link);
    ae->ae_profile = profile_find_by_name(NULL, NULL);
    if (delconf)
      access_entry_save(ae);
  }
}

/*
 *
 */
void
access_destroy_by_channel_tag(channel_tag_t *ct, int delconf)
{
  access_entry_t *ae;

  while ((ae = LIST_FIRST(&ct->ct_accesses)) != NULL) {
    LIST_REMOVE(ae, ae_channel_tag_link);
    ae->ae_chtag = NULL;
    if (delconf)
      access_entry_save(ae);
  }
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
    pos += snprintf(buf+pos, sizeof(buf)-pos, ",%s/%d", addrbuf, ai->ai_prefixlen);
  }
  return &ret;
}

static int
access_entry_class_password_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  char buf[256], result[300];

  if (strcmp(v ?: "", ae->ae_password ?: "")) {
    snprintf(buf, sizeof(buf), "TVHeadend-Hide-%s", (const char *)v ?: "");
    base64_encode(result, sizeof(result), (uint8_t *)buf, strlen(buf));
    free(ae->ae_password2);
    ae->ae_password2 = strdup(result);
    free(ae->ae_password);
    ae->ae_password = strdup((const char *)v ?: "");
    return 1;
  }
  return 0;
}

static int
access_entry_class_password2_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  char result[300];
  int l;

  if (strcmp(v ?: "", ae->ae_password2 ?: "")) {
    if (v && ((const char *)v)[0] != '\0') {
      l = base64_decode((uint8_t *)result, v, sizeof(result)-1);
      if (l < 0)
        l = 0;
      result[l] = '\0';
      free(ae->ae_password);
      ae->ae_password = strdup(result + 15);
      free(ae->ae_password2);
      ae->ae_password2 = strdup((const char *)v);
      return 1;
    }
  }
  return 0;
}

static int
access_entry_chtag_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  channel_tag_t *tag = v ? channel_tag_find_by_uuid(v) : NULL;
  if (tag == NULL && ae->ae_chtag) {
    LIST_REMOVE(ae, ae_channel_tag_link);
    ae->ae_chtag = NULL;
    return 1;
  } else if (ae->ae_chtag != tag) {
    if (ae->ae_chtag)
      LIST_REMOVE(ae, ae_channel_tag_link);
    ae->ae_chtag = tag;
    LIST_INSERT_HEAD(&tag->ct_accesses, ae, ae_channel_tag_link);
    return 1;
  }
  return 0;
}

static const void *
access_entry_chtag_get(void *o)
{
  static const char *ret;
  access_entry_t *ae = (access_entry_t *)o;
  if (ae->ae_chtag)
    ret = idnode_uuid_as_str(&ae->ae_chtag->ct_id);
  else
    ret = "";
  return &ret;
}

static int
access_entry_dvr_config_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  dvr_config_t *cfg = v ? dvr_config_find_by_uuid(v) : NULL;
  if (cfg == NULL && ae->ae_dvr_config) {
    LIST_REMOVE(ae, ae_dvr_config_link);
    ae->ae_dvr_config = NULL;
    return 1;
  } else if (ae->ae_dvr_config != cfg) {
    if (ae->ae_dvr_config)
      LIST_REMOVE(ae, ae_dvr_config_link);
    ae->ae_dvr_config = cfg;
    LIST_INSERT_HEAD(&cfg->dvr_accesses, ae, ae_dvr_config_link);
    return 1;
  }
  return 0;
}

static const void *
access_entry_dvr_config_get(void *o)
{
  static const char *ret;
  access_entry_t *ae = (access_entry_t *)o;
  if (ae->ae_dvr_config)
    ret = idnode_uuid_as_str(&ae->ae_dvr_config->dvr_id);
  else
    ret = "";
  return &ret;
}

static int
access_entry_profile_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  profile_t *pro = v ? profile_find_by_uuid(v) : NULL;
  if (pro == NULL && ae->ae_profile) {
    LIST_REMOVE(ae, ae_profile_link);
    ae->ae_profile = NULL;
    return 1;
  } else if (ae->ae_profile != pro) {
    if (ae->ae_profile)
      LIST_REMOVE(ae, ae_profile_link);
    ae->ae_profile = pro;
    LIST_INSERT_HEAD(&pro->pro_accesses, ae, ae_profile_link);
    return 1;
  }
  return 0;
}

static const void *
access_entry_profile_get(void *o)
{
  static const char *ret;
  access_entry_t *ae = (access_entry_t *)o;
  if (ae->ae_profile)
    ret = idnode_uuid_as_str(&ae->ae_profile->pro_id);
  else
    ret = "";
  return &ret;
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
      .id       = "password",
      .name     = "Password",
      .off      = offsetof(access_entry_t, ae_password),
      .opts     = PO_PASSWORD | PO_NOSAVE,
      .set      = access_entry_class_password_set,
    },
    {
      .type     = PT_STR,
      .id       = "password2",
      .name     = "Password2",
      .off      = offsetof(access_entry_t, ae_password2),
      .opts     = PO_PASSWORD | PO_HIDDEN | PO_ADVANCED | PO_WRONCE,
      .set      = access_entry_class_password2_set,
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
      .type     = PT_STR,
      .id       = "profile",
      .name     = "Streaming Profile",
      .set      = access_entry_profile_set,
      .get      = access_entry_profile_get,
      .list     = profile_class_get_list,
    },
    {
      .type     = PT_BOOL,
      .id       = "dvr",
      .name     = "Video Recorder",
      .off      = offsetof(access_entry_t, ae_dvr),
    },
    {
      .type     = PT_STR,
      .id       = "dvr_config",
      .name     = "DVR Config Profile",
      .set      = access_entry_dvr_config_set,
      .get      = access_entry_dvr_config_get,
      .list     = dvr_entry_class_config_name_list,
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
      .type     = PT_U32,
      .id       = "conn_limit",
      .name     = "Limit Connections",
      .off      = offsetof(access_entry_t, ae_conn_limit),
    },
    {
      .type     = PT_U32,
      .id       = "channel_min",
      .name     = "Min Channel Num",
      .off      = offsetof(access_entry_t, ae_chmin),
    },
    {
      .type     = PT_U32,
      .id       = "channel_max",
      .name     = "Max Channel Num",
      .off      = offsetof(access_entry_t, ae_chmax),
    },
    {
      .type     = PT_STR,
      .id       = "channel_tag",
      .name     = "Channel Tag",
      .set      = access_entry_chtag_set,
      .get      = access_entry_chtag_get,
      .list     = channel_tag_class_get_list,
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

  /* Load */
  if ((c = hts_settings_load("accesscontrol")) != NULL) {
    HTSMSG_FOREACH(f, c) {
      if (!(m = htsmsg_field_get_map(f))) continue;
      (void)access_entry_create(f->hmf_name, m);
    }
    htsmsg_destroy(c);
  }

  if(TAILQ_FIRST(&access_entries) == NULL) {
    /* No records available */
    ae = access_entry_create(NULL, NULL);

    free(ae->ae_comment);
    ae->ae_comment = strdup("Default access entry");

    ae->ae_enabled       = 1;
    ae->ae_streaming     = 1;
    ae->ae_adv_streaming = 1;
    ae->ae_dvr           = 1;
    ae->ae_webui         = 1;
    ae->ae_admin         = 1;
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

  pthread_mutex_lock(&global_lock);
  while ((ae = TAILQ_FIRST(&access_entries)) != NULL)
    access_entry_destroy(ae);
  while ((at = TAILQ_FIRST(&access_tickets)) != NULL)
    access_ticket_destroy(at);
  free((void *)superuser_username);
  superuser_username = NULL;
  free((void *)superuser_password);
  superuser_password = NULL;
  pthread_mutex_unlock(&global_lock);
}
