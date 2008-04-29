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

static int atally;
struct access_entry_queue access_entries;
static void access_load(void);


/**
 *
 */
void
access_init(void)
{
  TAILQ_INIT(&access_entries);
  access_load();
}

/**
 *
 */
static access_entry_t *
access_alloc(void)
{
  access_entry_t *ae;

  ae = calloc(1, sizeof(access_entry_t));
  TAILQ_INSERT_TAIL(&access_entries, ae, ae_link);
  ae->ae_tally = ++atally;
  return ae;
}



/**
 *
 */
access_entry_t *
access_add_network(const char *prefix)
{
  access_entry_t *ae;
  char buf[100];
  int prefixlen;
  char *p;
  struct in_addr ip;
  char title[100];

  if(strlen(prefix) > 90)
    return NULL;
  
  strcpy(buf, prefix);
  p = strchr(buf, '/');
  if(p) {
    *p++ = 0;
    prefixlen = atoi(p);
    if(prefixlen > 32)
      return NULL;
  } else {
    prefixlen = 32;
  }

  ip.s_addr = inet_addr(buf);

  ae = access_alloc();

  ae->ae_ip.s_addr = ip.s_addr;
  ae->ae_prefixlen = prefixlen;
  ae->ae_netmask   = prefixlen ? 0xffffffff << (32 - prefixlen) : 0;
  ae->ae_network   = ntohl(ip.s_addr) & ae->ae_netmask;

  ip.s_addr = htonl(ae->ae_network);

  snprintf(title, sizeof(title), "%s/%d", inet_ntoa(ip), prefixlen);

  ae->ae_title = strdup(title);
  return ae;
}

  

/**
 *
 */
access_entry_t *
access_add_user(const char *username)
{
  access_entry_t *ae;

  ae = access_alloc();
  ae->ae_username = strdup(username);
  ae->ae_title = strdup(username);
  return ae;
}

/**
 *
 */
access_entry_t *
access_add(const char *id)
{
  access_entry_t *ae;

  if(isdigit(id[0]))
    ae = access_add_network(id);
  else
    ae = access_add_user(id);

  if(ae != NULL)
    access_save();

  return ae;
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

  TAILQ_FOREACH(ae, &access_entries, ae_link) {

    if(access_is_prefix(ae)) {
      if((b & ae->ae_netmask) == ae->ae_network)
	bits |= ae->ae_rights;

    } else {

      if(username != NULL && 
	 !strcmp(ae->ae_username, username) &&
	 !strcmp(ae->ae_password, password))
	bits |= ae->ae_rights;
    }
  }

  return (mask & bits) == mask ? 0 : -1;
}


/**
 *
 */
access_entry_t *
access_by_id(int id)
{
  access_entry_t *ae;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {
    if(ae->ae_tally == id)
      return ae;
  }
  return NULL;
}

/**
 *
 */
void
access_delete(access_entry_t *ae)
{
  TAILQ_REMOVE(&access_entries, ae, ae_link);
  free(ae->ae_title);
  free(ae->ae_username);
  free(ae->ae_password);
  free(ae);
  access_save();
}


/**
 *
 */
static void
access_save_right(FILE *fp, access_entry_t *ae, const char *right, int v)
{
  fprintf(fp, "\t%s = %d\n", right, !!(ae->ae_rights & v));
}

/**
 *
 */
static void
access_load_right(struct config_head *h, access_entry_t *ae,
		  const char *right, int v)
{
  int on = atoi(config_get_str_sub(h, right, "0"));
  
  if(on)
    ae->ae_rights |= v;
}

/**
 *
 */
void
access_save(void)
{
  access_entry_t *ae;
  char buf[400];
  FILE *fp;

  snprintf(buf, sizeof(buf), "%s/access.cfg",  settings_dir);
  if((fp = settings_open_for_write(buf)) == NULL)
    return;

  TAILQ_FOREACH(ae, &access_entries, ae_link) {
    if(access_is_prefix(ae)) {
      fprintf(fp, "prefix {\n");
      fprintf(fp, "\tid = %s\n", ae->ae_title);
    } else {
      fprintf(fp, "user {\n");
      fprintf(fp, "\tname = %s\n", ae->ae_username);
      if(ae->ae_password != NULL)
	fprintf(fp, "\tpassword = %s\n", ae->ae_password);
    }

    access_save_right(fp, ae, "streaming",    ACCESS_STREAMING);
    access_save_right(fp, ae, "rec",          ACCESS_RECORDER_VIEW);
    access_save_right(fp, ae, "recedit",      ACCESS_RECORDER_CHANGE);
    access_save_right(fp, ae, "conf",         ACCESS_CONFIGURE);
    access_save_right(fp, ae, "webui",        ACCESS_WEB_INTERFACE);
    access_save_right(fp, ae, "access",       ACCESS_ACCESSCONTROL);

    fprintf(fp, "}\n");
  }
  fclose(fp);
}

/**
 *
 */
static void
access_load(void)
{
  char buf[400];
  access_entry_t *ae;
  const char *name;
  struct config_head cl;
  config_entry_t *ce;

  TAILQ_INIT(&cl);

  snprintf(buf, sizeof(buf), "%s/access.cfg",  settings_dir);
  
  if(config_read_file0(buf, &cl)) {
    ae = access_add_network("0.0.0.0/0");
    ae->ae_rights = ACCESS_FULL;
    access_save();
    return;
  }

  TAILQ_FOREACH(ce, &cl, ce_link) {
    if(ce->ce_type != CFG_SUB)
      continue;

    if(!strcasecmp("user", ce->ce_key)) {

      if((name = config_get_str_sub(&ce->ce_sub, "name", NULL)) == NULL)
	continue;

      ae = access_add_user(name);

      if((name = config_get_str_sub(&ce->ce_sub, "password", NULL)) != NULL)
	ae->ae_password = strdup(name);

    } else if(!strcasecmp("prefix", ce->ce_key)) {

      if((name = config_get_str_sub(&ce->ce_sub, "id", NULL)) == NULL)
	continue;

      ae = access_add_network(name);

    } else {
      continue;
    }

    if(ae == NULL)
      continue;

    access_load_right(&ce->ce_sub, ae, "streaming",    ACCESS_STREAMING);
    access_load_right(&ce->ce_sub, ae, "rec",          ACCESS_RECORDER_VIEW);
    access_load_right(&ce->ce_sub, ae, "recedit",      ACCESS_RECORDER_CHANGE);
    access_load_right(&ce->ce_sub, ae, "conf",         ACCESS_CONFIGURE);
    access_load_right(&ce->ce_sub, ae, "webui",        ACCESS_WEB_INTERFACE);
    access_load_right(&ce->ce_sub, ae, "access",       ACCESS_ACCESSCONTROL);
  }
}
