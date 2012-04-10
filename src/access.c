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

#include <openssl/sha.h>
#include <openssl/rand.h>

#include "tvheadend.h"
#include "access.h"
#include "dtable.h"
#include "settings.h"

struct access_entry_queue access_entries;
struct access_ticket_queue access_tickets;

const char *superuser_username;
const char *superuser_password;

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
int
access_verify(const char *username, const char *password,
	      struct sockaddr *src, uint32_t mask)
{
  uint32_t bits = 0;
  struct sockaddr_in *si = (struct sockaddr_in *)src;
  uint32_t b = ntohl(si->sin_addr.s_addr);
  access_entry_t *ae;

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

    if((b & ae->ae_netmask) != ae->ae_network)
      continue; /* IP based access mismatches */

    bits |= ae->ae_rights;
  }
  return (mask & bits) == mask ? 0 : -1;
}



/**
 *
 */
uint32_t
access_get_hashed(const char *username, const uint8_t digest[20],
		  const uint8_t *challenge, struct sockaddr *src,
		  int *entrymatch)
{
  struct sockaddr_in *si = (struct sockaddr_in *)src;
  uint32_t b = ntohl(si->sin_addr.s_addr);
  access_entry_t *ae;
  SHA_CTX shactx;
  uint8_t d[20];
  uint32_t r = 0;
  int match = 0;

  if(superuser_username != NULL && superuser_password != NULL) {

    SHA_Init(&shactx);
    SHA_Update(&shactx, (const uint8_t *)superuser_password,
	       strlen(superuser_password));
    SHA_Update(&shactx, challenge, 32);
    SHA_Final(d, &shactx);

    if(!strcmp(superuser_username, username) && !memcmp(d, digest, 20))
      return 0xffffffff;
  }


  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(!ae->ae_enabled)
      continue;

    if((b & ae->ae_netmask) != ae->ae_network)
      continue; /* IP based access mismatches */

    SHA_Init(&shactx);
    SHA_Update(&shactx, (const uint8_t *)ae->ae_password,
	      strlen(ae->ae_password));
    SHA1_Update(&shactx, challenge, 32);
    SHA1_Final(d, &shactx);

    if(strcmp(ae->ae_username, username) || memcmp(d, digest, 20))
      continue;
    match = 1;
    r |= ae->ae_rights;
  }
  if(entrymatch != NULL)
    *entrymatch = match;
  return r;
}



/**
 *
 */
uint32_t
access_get_by_addr(struct sockaddr *src)
{
  struct sockaddr_in *si = (struct sockaddr_in *)src;
  uint32_t b = ntohl(si->sin_addr.s_addr);
  access_entry_t *ae;
  uint32_t r = 0;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(ae->ae_username[0] != '*')
      continue;

    if((b & ae->ae_netmask) != ae->ae_network)
      continue; /* IP based access mismatches */

    r |= ae->ae_rights;
  }
  return r;
}




/**
 *
 */
static void
access_update_flag(access_entry_t *ae, int flag, int bo)
{
  if(bo)
    ae->ae_rights |= flag;
  else
    ae->ae_rights &= ~flag;
}

/**
 *
 */
static void
access_set_prefix(access_entry_t *ae, const char *prefix)
{
  char buf[100];
  int prefixlen;
  char *p;

  if(strlen(prefix) > 90)
    return;
  
  strcpy(buf, prefix);
  p = strchr(buf, '/');
  if(p) {
    *p++ = 0;
    prefixlen = atoi(p);
    if(prefixlen > 32)
      return;
  } else {
    prefixlen = 32;
  }

  ae->ae_ip.s_addr = inet_addr(buf);
  ae->ae_prefixlen = prefixlen;

  ae->ae_netmask   = prefixlen ? 0xffffffff << (32 - prefixlen) : 0;
  ae->ae_network   = ntohl(ae->ae_ip.s_addr) & ae->ae_netmask;
}



/**
 *
 */
static access_entry_t *
access_entry_find(const char *id, int create)
{
  access_entry_t *ae;
  char buf[20];
  static int tally;

  if(id != NULL) {
    TAILQ_FOREACH(ae, &access_entries, ae_link)
      if(!strcmp(ae->ae_id, id))
	return ae;
  }

  if(create == 0)
    return NULL;

  ae = calloc(1, sizeof(access_entry_t));
  if(id == NULL) {
    tally++;
    snprintf(buf, sizeof(buf), "%d", tally);
    id = buf;
  } else {
    tally = MAX(atoi(id), tally);
  }

  ae->ae_id = strdup(id);
  ae->ae_username = strdup("*");
  ae->ae_password = strdup("*");
  ae->ae_comment = strdup("New entry");
  TAILQ_INSERT_TAIL(&access_entries, ae, ae_link);
  return ae;
}



/**
 *
 */
static void
access_entry_destroy(access_entry_t *ae)
{
  free(ae->ae_id);
  free(ae->ae_username);
  free(ae->ae_password);
  TAILQ_REMOVE(&access_entries, ae, ae_link);
  free(ae);
}


/**
 *
 */
static htsmsg_t *
access_record_build(access_entry_t *ae)
{
  htsmsg_t *e = htsmsg_create_map();
  char buf[100];

  htsmsg_add_u32(e, "enabled",  !!ae->ae_enabled);

  htsmsg_add_str(e, "username", ae->ae_username);
  htsmsg_add_str(e, "password", ae->ae_password);
  htsmsg_add_str(e, "comment",  ae->ae_comment);

  snprintf(buf, sizeof(buf), "%s/%d", inet_ntoa(ae->ae_ip), ae->ae_prefixlen);
  htsmsg_add_str(e, "prefix",   buf);

  htsmsg_add_u32(e, "streaming", ae->ae_rights & ACCESS_STREAMING     ? 1 : 0);
  htsmsg_add_u32(e, "dvr"      , ae->ae_rights & ACCESS_RECORDER      ? 1 : 0);
  htsmsg_add_u32(e, "dvrallcfg", ae->ae_rights & ACCESS_RECORDER_ALL  ? 1 : 0);
  htsmsg_add_u32(e, "webui"    , ae->ae_rights & ACCESS_WEB_INTERFACE ? 1 : 0);
  htsmsg_add_u32(e, "admin"    , ae->ae_rights & ACCESS_ADMIN         ? 1 : 0);


  htsmsg_add_str(e, "id", ae->ae_id);
  
  return e;
}

/**
 *
 */
static htsmsg_t *
access_record_get_all(void *opaque)
{
  htsmsg_t *r = htsmsg_create_list();
  access_entry_t *ae;

  TAILQ_FOREACH(ae, &access_entries, ae_link)
    htsmsg_add_msg(r, NULL, access_record_build(ae));

  return r;
}

/**
 *
 */
static htsmsg_t *
access_record_get(void *opaque, const char *id)
{
  access_entry_t *ae;

  if((ae = access_entry_find(id, 0)) == NULL)
    return NULL;
  return access_record_build(ae);
}


/**
 *
 */
static htsmsg_t *
access_record_create(void *opaque)
{
  return access_record_build(access_entry_find(NULL, 1));
}


/**
 *
 */
static htsmsg_t *
access_record_update(void *opaque, const char *id, htsmsg_t *values, 
		     int maycreate)
{
  access_entry_t *ae;
  const char *s;
  uint32_t u32;

  if((ae = access_entry_find(id, maycreate)) == NULL)
    return NULL;
  
  if((s = htsmsg_get_str(values, "username")) != NULL) {
    free(ae->ae_username);
    ae->ae_username = strdup(s);
  }

  if((s = htsmsg_get_str(values, "comment")) != NULL) {
    free(ae->ae_comment);
    ae->ae_comment = strdup(s);
  }

  if((s = htsmsg_get_str(values, "password")) != NULL) {
    free(ae->ae_password);
    ae->ae_password = strdup(s);
  }

  if((s = htsmsg_get_str(values, "prefix")) != NULL)
    access_set_prefix(ae, s);

  if(!htsmsg_get_u32(values, "enabled", &u32))
    ae->ae_enabled = u32;

  if(!htsmsg_get_u32(values, "streaming", &u32))
    access_update_flag(ae, ACCESS_STREAMING, u32);

  if(!htsmsg_get_u32(values, "dvr", &u32))
    access_update_flag(ae, ACCESS_RECORDER, u32);

  if(!htsmsg_get_u32(values, "dvrallcfg", &u32))
    access_update_flag(ae, ACCESS_RECORDER_ALL, u32);

  if(!htsmsg_get_u32(values, "admin", &u32))
    access_update_flag(ae, ACCESS_ADMIN, u32);

  if(!htsmsg_get_u32(values, "webui", &u32))
    access_update_flag(ae, ACCESS_WEB_INTERFACE, u32);

  return access_record_build(ae);
}


/**
 *
 */
static int
access_record_delete(void *opaque, const char *id)
{
  access_entry_t *ae;

  if((ae = access_entry_find(id, 0)) == NULL)
    return -1;
  access_entry_destroy(ae);
  return 0;
}


/**
 *
 */
static const dtable_class_t access_dtc = {
  .dtc_record_get     = access_record_get,
  .dtc_record_get_all = access_record_get_all,
  .dtc_record_create  = access_record_create,
  .dtc_record_update  = access_record_update,
  .dtc_record_delete  = access_record_delete,
  .dtc_read_access = ACCESS_ADMIN,
  .dtc_write_access = ACCESS_ADMIN,
  .dtc_mutex = &global_lock,
};

/**
 *
 */
void
access_init(int createdefault)
{
  dtable_t *dt;
  htsmsg_t *r, *m;
  access_entry_t *ae;
  const char *s;

  static struct {
    pid_t pid;
    struct timeval tv;
  } randseed;

  randseed.pid = getpid();
  gettimeofday(&randseed.tv, NULL);
  RAND_seed(&randseed, sizeof(randseed));

  TAILQ_INIT(&access_entries);
  TAILQ_INIT(&access_tickets);

  dt = dtable_create(&access_dtc, "accesscontrol", NULL);

  if(dtable_load(dt) == 0 && createdefault) {
    /* No records available */
    ae = access_entry_find(NULL, 1);

    free(ae->ae_comment);
    ae->ae_comment = strdup("Default access entry");

    ae->ae_enabled = 1;
    ae->ae_rights = 0xffffffff;

    r = access_record_build(ae);
    dtable_record_store(dt, ae->ae_id, r);
    htsmsg_destroy(r);

    tvhlog(LOG_WARNING, "accesscontrol",
	   "Created default wide open access controle entry");
  }

  /* Load superuser account */

  if((m = hts_settings_load("superuser")) != NULL) {
    s = htsmsg_get_str(m, "username");
    superuser_username = s ? strdup(s) : NULL;
    s = htsmsg_get_str(m, "password");
    superuser_password =s ? strdup(s) : NULL;
    htsmsg_destroy(m);
  }
}
