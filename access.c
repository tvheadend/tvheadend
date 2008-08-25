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

#include "tvhead.h"
#include "access.h"
#include "dtable.h"

static int atally;
struct access_entry_queue access_entries;




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

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(ae->ae_username[0] != '*') {
      /* acl entry requires username to match */
      printf("Need user access\n");
      if(username == NULL)
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

  if(id != NULL) {
    TAILQ_FOREACH(ae, &access_entries, ae_link)
      if(!strcmp(ae->ae_id, id))
	return ae;
  }

  if(create == 0)
    return NULL;

  ae = calloc(1, sizeof(access_entry_t));
  if(id == NULL) {
    atally++;
    snprintf(buf, sizeof(buf), "%d", atally);
    id = buf;
  } else {
    atally = atoi(id);
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
  htsmsg_t *e = htsmsg_create();
  char buf[100];

  htsmsg_add_u32(e, "enabled",  !!ae->ae_enabled);

  htsmsg_add_str(e, "username", ae->ae_username);
  htsmsg_add_str(e, "password", ae->ae_password);
  htsmsg_add_str(e, "comment",  ae->ae_comment);

  snprintf(buf, sizeof(buf), "%s/%d", inet_ntoa(ae->ae_ip), ae->ae_prefixlen);
  htsmsg_add_str(e, "prefix",   buf);

  htsmsg_add_u32(e, "streaming", ae->ae_rights & ACCESS_STREAMING     ? 1 : 0);
  htsmsg_add_u32(e, "pvr"      , ae->ae_rights & ACCESS_RECORDER      ? 1 : 0);
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
  htsmsg_t *r = htsmsg_create_array();
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

  if(!htsmsg_get_u32(values, "pvr", &u32))
    access_update_flag(ae, ACCESS_RECORDER, u32);

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
};

/**
 *
 */
void
access_init(void)
{
  dtable_t *dt;
  htsmsg_t *r;
  access_entry_t *ae;

  TAILQ_INIT(&access_entries);

  dt = dtable_create(&access_dtc, "accesscontrol", NULL);

  if(dtable_load(dt) == 0) {
    /* No records available */
    ae = access_entry_find(NULL, 1);

    free(ae->ae_comment);
    ae->ae_comment = strdup("Default access entry");

    ae->ae_enabled = 1;
    ae->ae_rights = 0xffffffff;

    r = access_record_build(ae);
    dtable_record_store(dt, ae->ae_id, r);
    htsmsg_destroy(r);

    fprintf(stderr, "Notice: Created default access controle entry\n");

  }
}
