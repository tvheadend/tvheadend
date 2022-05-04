/*
 *  AVAHI service publisher
 *  Copyright (C) 2009 Andreas Öman
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

/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
  USA.
***/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

#include "tvheadend.h"
#include "avahi.h"
#include "config.h"

static AvahiEntryGroup *group = NULL;
static char *name = NULL, *name2 = NULL;
static AvahiSimplePoll *avahi_asp = NULL;
static const AvahiPoll *avahi_poll = NULL;
static int avahi_do_restart = 0;

static void create_services(AvahiClient *c);

static inline int
avahi_required(void)
{
  return tvheadend_webui_port > 0 || tvheadend_htsp_port > 0;
}

static void
entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, 
		     void *userdata)
{
  assert(g == group || group == NULL);
  group = g;

  /* Called whenever the entry group state changes */

  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED :
    /* The entry group has been established successfully */
    tvhinfo(LS_AVAHI, "Service '%s' successfully established.", name);
    break;

  case AVAHI_ENTRY_GROUP_COLLISION : {
    char *n;

    /* A service name collision with a remote service
     * happened. Let's pick a new name */
    n = avahi_alternative_service_name(name);
    if (name != name2) avahi_free(name);
    name = n;
    
    tvherror(LS_AVAHI, "Service name collision, renaming service to '%s'", name);

    /* And recreate the services */
    create_services(avahi_entry_group_get_client(g));
    break;
  }

  case AVAHI_ENTRY_GROUP_FAILURE :
     tvherror(LS_AVAHI,
	      "Entry group failure: %s", 
	      avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));
    break;

  case AVAHI_ENTRY_GROUP_UNCOMMITED:
  case AVAHI_ENTRY_GROUP_REGISTERING:
    ;
  }
}


/**
 *
 */
static void 
create_services(AvahiClient *c) 
{
  char *n;
  char *path = NULL;
  int ret;
  assert(c);

  /* If this is the first time we're called, let's create a new
   * entry group if necessary */

  if (!group)
    if (!(group = avahi_entry_group_new(c, entry_group_callback, NULL))) {
      tvherror(LS_AVAHI,
	       "avahi_entry_group_new() failed: %s",
	        avahi_strerror(avahi_client_errno(c)));
      goto fail;
    }

  /* If the group is empty (either because it was just created, or
   * because it was reset previously, add our entries.  */

  if (avahi_entry_group_is_empty(group)) {
     tvhdebug(LS_AVAHI, "Adding service '%s'", name);

    /* Add the service for HTSP */
    if (tvheadend_htsp_port > 0) {
      if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC,
                                               AVAHI_PROTO_UNSPEC, 0, name,
                                               "_htsp._tcp", NULL, NULL,
                                               tvheadend_htsp_port,
                                               NULL)) < 0) {

        if (ret == AVAHI_ERR_COLLISION)
          goto collision;

        tvherror(LS_AVAHI,
                 "Failed to add _htsp._tcp service: %s",
                 avahi_strerror(ret));
        goto fail;
      }
    }

    /* Add the service for HTTP */
    if (tvheadend_webui_port > 0) {
      if (tvheadend_webroot) {
        path = malloc(strlen(tvheadend_webroot) + 6);
        sprintf(path, "path=%s", tvheadend_webroot);
      } else {
        path = strdup("path=/");
      }

      if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC,
                                               AVAHI_PROTO_UNSPEC, 0, name,
                                               "_http._tcp", NULL, NULL, tvheadend_webui_port,
                                               path,
                                               NULL)) < 0) {

        if (ret == AVAHI_ERR_COLLISION)
          goto collision;

        tvherror(LS_AVAHI,
                 "Failed to add _http._tcp service: %s",
                 avahi_strerror(ret));
        goto fail;
      }
    }

    /* Tell the server to register the service */
    if ((ret = avahi_entry_group_commit(group)) < 0) {
      tvherror(LS_AVAHI,
	       "Failed to commit entry group: %s",
               avahi_strerror(ret));
      goto fail;
    }
  }

  free(path);
  return;

 collision:

  /* A service name collision with a local service happened. Let's
   * pick a new name */
  n = avahi_alternative_service_name(name);
  if (name != name2) avahi_free(name);
  name = n;

  tvherror(LS_AVAHI, "Service name collision, renaming service to '%s'", name);

  avahi_entry_group_reset(group);

  create_services(c);
  return;

 fail:
  free(path);
}


/**
 *
 */
static void
client_callback(AvahiClient *c, AvahiClientState state, void *userdata)
{
  assert(c);

  /* Called whenever the client or server state changes */

  switch (state) {
  case AVAHI_CLIENT_S_RUNNING:

    /* The server has startup successfully and registered its host
     * name on the network, so it's time to create our services */
    create_services(c);
    break;

  case AVAHI_CLIENT_FAILURE:
    tvherror(LS_AVAHI, "Client failure: %s", avahi_strerror(avahi_client_errno(c)));
    break;

  case AVAHI_CLIENT_S_COLLISION:

    /* Let's drop our registered services. When the server is back
     * in AVAHI_SERVER_RUNNING state we will register them
     * again with the new host name. */

  case AVAHI_CLIENT_S_REGISTERING:

    /* The server records are now being established. This
     * might be caused by a host name change. We need to wait
     * for our own records to register until the host name is
     * properly esatblished. */

    if(group)
      avahi_entry_group_reset(group);

    break;

  case AVAHI_CLIENT_CONNECTING:
    ;
  }
}


/**
 *
 */
static void *
avahi_thread(void *aux)
{
  AvahiClient *ac;

  do {
    if (avahi_poll)
        avahi_simple_poll_free((AvahiSimplePoll *)avahi_poll);

    avahi_asp = avahi_simple_poll_new();
    avahi_poll = avahi_simple_poll_get(avahi_asp);

    if(avahi_do_restart) {
      tvhinfo(LS_AVAHI, "Service restarted.");
      avahi_do_restart = 0;
      group = NULL;
    }

    name = name2 = avahi_strdup(config_get_server_name());

    ac = avahi_client_new(avahi_poll, AVAHI_CLIENT_NO_FAIL, client_callback, NULL, NULL);

    if (ac) {
      while((avahi_do_restart == 0) &&
            (avahi_simple_poll_iterate(avahi_asp, -1) == 0));

      avahi_client_free(ac);
    }

    name = NULL;
    avahi_free(name2);
    name2 = NULL;

  } while (tvheadend_is_running() && avahi_do_restart);

  return NULL;
}

/**
 *
 */
pthread_t avahi_tid;

void
avahi_init(void)
{
  if (!avahi_required())
    return;
  tvh_thread_create(&avahi_tid, NULL, avahi_thread, NULL, "avahi");
}

void
avahi_done(void)
{
  if (!avahi_required())
    return;
  avahi_simple_poll_quit(avahi_asp);
  tvh_thread_kill(avahi_tid, SIGTERM);
  pthread_join(avahi_tid, NULL);
  avahi_simple_poll_free((AvahiSimplePoll *)avahi_poll);
}

void
avahi_restart(void)
{
  if (!avahi_required())
    return;
  avahi_do_restart = 1;
  avahi_simple_poll_quit(avahi_asp);
}
