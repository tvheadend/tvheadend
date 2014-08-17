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

struct access_entry_queue access_entries;
struct access_ticket_queue access_tickets;

const char *superuser_username;
const char *superuser_password;

static int access_noacl;

/**
 *
 */
static void
access_ticket_destroy(access_ticket_t *at)
{
  free(at->at_id);
  free(at->at_resource);
  TAILQ_REMOVE(&access_tickets, at, at_link);
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
access_ticket_create(const char *resource)
{
  uint8_t buf[20];
  char id[41];
  unsigned int i;
  access_ticket_t *at;
  static const char hex_string[16] = "0123456789ABCDEF";

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
int
access_ticket_verify(const char *id, const char *resource)
{
  access_ticket_t *at;

  if((at = access_ticket_find(id)) == NULL)
    return -1;

  if(strcmp(at->at_resource, resource))
    return -1;

  return 0;
}

/**
 *
 */
void
access_destroy(access_t *a)
{
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
  
  if(src->sa_family == AF_INET6)
  {
    struct in6_addr *in6 = &(((struct sockaddr_in6 *)src)->sin6_addr);
    uint32_t *a32 = (uint32_t*)in6->s6_addr;
    if(a32[0] == 0 && a32[1] == 0 && ntohl(a32[2]) == 0x0000FFFFu)
    {
      isv4v6 = 1;
      v4v6 = ntohl(a32[3]);
    }
  }

  TAILQ_FOREACH(ai, &ae->ae_ipmasks, ai_link)
  {
    if(ai->ai_ipv6 == 0 && src->sa_family == AF_INET)
    {
      struct sockaddr_in *in4 = (struct sockaddr_in *)src;
      uint32_t b = ntohl(in4->sin_addr.s_addr);
      if((b & ai->ai_netmask) == ai->ai_network)
        return 1;
    }
    else if(ai->ai_ipv6 == 0 && isv4v6)
    {
      if((v4v6 & ai->ai_netmask) == ai->ai_network)
        return 1;
    }
    else if(ai->ai_ipv6 && isv4v6)
    {
      continue;
    }
    else if(ai->ai_ipv6 && src->sa_family == AF_INET6)
    {
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

    bits |= ae->ae_rights;
  }
  return (mask & bits) == mask ? 0 : -1;
}

/*
 *
 */
static void
access_update(access_t *a, access_entry_t *ae)
{
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

  if(ae->ae_chtag) {
    if (a->aa_chtags == NULL)
      a->aa_chtags = htsmsg_create_list();
    htsmsg_add_str(a->aa_chtags, NULL, ae->ae_chtag);
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

    a->aa_match = 1;
    access_update(a, ae);
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
    }

    a->aa_match = 1;
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
access_set_prefix(access_entry_t *ae, const char *prefix)
{
  char buf[100];
  char tokbuf[4096];
  int prefixlen;
  char *p, *tok, *saveptr;
  access_ipmask_t *ai;

  while((ai = TAILQ_FIRST(&ae->ae_ipmasks)) != NULL)
  {
    TAILQ_REMOVE(&ae->ae_ipmasks, ai, ai_link);
    free(ai);
  }

  strncpy(tokbuf, prefix, 4095);
  tokbuf[4095] = 0;
  tok = strtok_r(tokbuf, ",;| ", &saveptr);

  while(tok != NULL)
  {
    ai = calloc(1, sizeof(access_ipmask_t));

    if(strlen(tok) > 90 || strlen(tok) == 0)
    {
      free(ai);
      tok = strtok_r(NULL, ",;| ", &saveptr);
      continue;
    }

    strcpy(buf, tok);

    if(strchr(buf, ':') != NULL)
      ai->ai_ipv6 = 1;
    else
      ai->ai_ipv6 = 0;

    if(ai->ai_ipv6)
    {
      p = strchr(buf, '/');
      if(p)
      {
        *p++ = 0;
        prefixlen = atoi(p);
        if(prefixlen > 128)
        {
          free(ai);
          tok = strtok_r(NULL, ",;| ", &saveptr);
          continue;
        }
      } else {
        prefixlen = 128;
      }

      ai->ai_prefixlen = prefixlen;
      inet_pton(AF_INET6, buf, &ai->ai_ip6);

      ai->ai_netmask = 0xffffffff;
      ai->ai_network = 0x00000000;
    }
    else
    {
      p = strchr(buf, '/');
      if(p)
      {
        *p++ = 0;
        prefixlen = atoi(p);
        if(prefixlen > 32)
        {
          free(ai);
          tok = strtok_r(NULL, ",;| ", &saveptr);
          continue;
        }
      } else {
        prefixlen = 32;
      }

      ai->ai_ip.s_addr = inet_addr(buf);
      ai->ai_prefixlen = prefixlen;

      ai->ai_netmask   = prefixlen ? 0xffffffff << (32 - prefixlen) : 0;
      ai->ai_network   = ntohl(ai->ai_ip.s_addr) & ai->ai_netmask;
    }

    TAILQ_INSERT_TAIL(&ae->ae_ipmasks, ai, ai_link);

    tok = strtok_r(NULL, ",;| ", &saveptr);
  }
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
  if (ae->ae_dvrallcfg)
    r |= ACCESS_RECORDER_ALL;
  if (ae->ae_webui)
    r |= ACCESS_WEB_INTERFACE;
  if (ae->ae_tag_only)
    r |= ACCESS_TAG_ONLY;
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
  access_ipmask_t *ai;
  access_entry_t *ae, *ae2;

  lock_assert(&global_lock);

  ae = calloc(1, sizeof(access_entry_t));

  idnode_insert(&ae->ae_id, uuid, &access_entry_class, 0);

  TAILQ_INIT(&ae->ae_ipmasks);

  if (conf) {
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
  }

  if (ae->ae_username == NULL)
    ae->ae_username = strdup("*");
  if (ae->ae_comment == NULL)
    ae->ae_comment = strdup("New entry");
  if (ae->ae_password == NULL)
    access_entry_class_password_set(ae, "*");
  if (TAILQ_FIRST(&ae->ae_ipmasks) == NULL) {
    ai = calloc(1, sizeof(access_ipmask_t));
    ai->ai_ipv6 = 1;
    TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);
    ai = calloc(1, sizeof(access_ipmask_t));
    ai->ai_ipv6 = 0;
    TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);
  }

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

  while((ai = TAILQ_FIRST(&ae->ae_ipmasks)) != NULL)
  {
    TAILQ_REMOVE(&ae->ae_ipmasks, ai, ai_link);
    free(ai);
  }

  free(ae->ae_username);
  free(ae->ae_password);
  free(ae->ae_password2);
  free(ae->ae_comment);
  free(ae->ae_chtag);
  free(ae);
}

/**
 *
 */
static void
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

  buf[0] = buf[1] = '\0';
  TAILQ_FOREACH(ai, &ae->ae_ipmasks, ai_link)   {
    if(sizeof(buf)-pos <= 0)
      break;

    if(ai->ai_ipv6) {
      inet_ntop(AF_INET6, &ai->ai_ip6, addrbuf, sizeof(addrbuf));
      pos += snprintf(buf+pos, sizeof(buf)-pos, ",%s/%d", addrbuf, ai->ai_prefixlen);
    } else {
      pos += snprintf(buf+pos, sizeof(buf)-pos, ",%s/%d", inet_ntoa(ai->ai_ip), ai->ai_prefixlen);
    }
  }
  return &ret;
}

static int
access_entry_class_password_set(void *o, const void *v)
{
  access_entry_t *ae = (access_entry_t *)o;
  char buf[256], result[300];

  if (strcmp(v ?: "", ae->ae_password ?: "")) {
    snprintf(buf, sizeof(buf), "TVHeadend-Hide-%s", (const char *)v);
    base64_encode(result, sizeof(result), (uint8_t *)buf, strlen(buf));
    free(ae->ae_password2);
    ae->ae_password2 = strdup(result);
    free(ae->ae_password);
    ae->ae_password = strdup((const char *)v);
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

static htsmsg_t *
access_entry_chtag_list ( void *o )
{
  channel_tag_t *ct;
  htsmsg_t *m = htsmsg_create_list();
  TAILQ_FOREACH(ct, &channel_tags, ct_link)
    htsmsg_add_str(m, NULL, ct->ct_name);
  return m;
}

const idclass_t access_entry_class = {
  .ic_class      = "access",
  .ic_caption    = "Access",
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
      .type     = PT_BOOL,
      .id       = "dvr",
      .name     = "Video Recorder",
      .off      = offsetof(access_entry_t, ae_dvr),
    },
    {
      .type     = PT_BOOL,
      .id       = "dvrallcfg",
      .name     = "Username Configs (VR)",
      .off      = offsetof(access_entry_t, ae_dvrallcfg),
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
      .type     = PT_BOOL,
      .id       = "tag_only",
      .name     = "Username Channel Tag Match",
      .off      = offsetof(access_entry_t, ae_tag_only),
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
      .name     = "Min Channel Max",
      .off      = offsetof(access_entry_t, ae_chmax),
    },
    {
      .type     = PT_STR,
      .id       = "channel_tag",
      .name     = "Channel Tag",
      .off      = offsetof(access_entry_t, ae_chtag),
      .list     = access_entry_chtag_list,
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
  access_ipmask_t *ai;
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
  if ((c = hts_settings_load_r(1, "accesscontrol")) != NULL) {
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

    ai = calloc(1, sizeof(access_ipmask_t));
    ai->ai_ipv6 = 1;
    TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);

    ai = calloc(1, sizeof(access_ipmask_t));
    ai->ai_ipv6 = 0;
    TAILQ_INSERT_HEAD(&ae->ae_ipmasks, ai, ai_link);

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

  pthread_mutex_lock(&global_lock);
  while ((ae = TAILQ_FIRST(&access_entries)) != NULL)
    access_entry_destroy(ae);
  pthread_mutex_unlock(&global_lock);
}
