/*
 *  DBUS interface
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <fcntl.h>

#include <dbus-1.0/dbus/dbus.h>

#include "tvheadend.h"
#include "tvhpoll.h"
#include "subscriptions.h"
#include "dbus.h"


typedef struct dbus_sig {
  TAILQ_ENTRY(dbus_sig) link;
  char     *obj_name;
  char     *sig_name;
  htsmsg_t *msg;
} dbus_sig_t;

typedef struct dbus_rpc {
  LIST_ENTRY(dbus_rpc) link;
  char *call_name;
  void *opaque;
  int64_t (*rpc_s64)(void *opaque, const char *path, int64_t value);
  char   *(*rpc_str)(void *opaque, const char *path, char *value);
} dbus_rpc_t;

TAILQ_HEAD(dbus_signal_queue, dbus_sig);
LIST_HEAD(dbus_rpc_list, dbus_rpc);
static struct dbus_signal_queue dbus_signals;
static struct dbus_rpc_list dbus_rpcs;
static th_pipe_t dbus_pipe;
static pthread_mutex_t dbus_lock;
static int dbus_running;
static int dbus_session;

/**
 *
 */
void
dbus_emit_signal(const char *obj_name, const char *sig_name, htsmsg_t *msg)
{
  dbus_sig_t *ds;
  int unused __attribute__((unused));
  size_t l;

  if (!atomic_get(&dbus_running)) {
    htsmsg_destroy(msg);
    return;
  }
  ds = calloc(1, sizeof(dbus_sig_t));
  l = strlen(obj_name);
  ds->obj_name = malloc(l + 15);
  strcpy(ds->obj_name, "/org/tvheadend");
  strcpy(ds->obj_name + 14, obj_name);
  ds->sig_name = strdup(sig_name);
  ds->msg = msg;
  pthread_mutex_lock(&dbus_lock);
  TAILQ_INSERT_TAIL(&dbus_signals, ds, link);
  pthread_mutex_unlock(&dbus_lock);
  unused = write(dbus_pipe.wr, "s", 1); /* do not wait here - no tvh_write() */
}

void
dbus_emit_signal_str(const char *obj_name, const char *sig_name, const char *value)
{
  htsmsg_t *l = htsmsg_create_list();
  htsmsg_add_str(l, NULL, value);
  dbus_emit_signal(obj_name, sig_name, l);
}

void
dbus_emit_signal_s64(const char *obj_name, const char *sig_name, int64_t value)
{
  htsmsg_t *l = htsmsg_create_list();
  htsmsg_add_s64(l, NULL, value);
  dbus_emit_signal(obj_name, sig_name, l);
}

/**
 *
 */
static void
dbus_from_htsmsg(htsmsg_t *msg, DBusMessageIter *args)
{
  htsmsg_field_t *f;

  TAILQ_FOREACH(f, &msg->hm_fields, hmf_link) {
    switch(f->hmf_type) {
    case HMF_STR:
      dbus_message_iter_append_basic(args, DBUS_TYPE_STRING, &f->hmf_str);
      break;
    case HMF_S64:
      dbus_message_iter_append_basic(args, DBUS_TYPE_INT64, &f->hmf_s64);
      break;
    case HMF_BOOL:
      dbus_message_iter_append_basic(args, DBUS_TYPE_BOOLEAN, &f->hmf_bool);
      break;
    case HMF_DBL:
      dbus_message_iter_append_basic(args, DBUS_TYPE_DOUBLE, &f->hmf_dbl);
      break;
    default:
      assert(0);
    }
  }
}

/**
 *
 */
static DBusConnection *
dbus_create_session(const char *name)
{
  DBusConnection *conn;
  DBusError       err;
  int             ret;

  dbus_error_init(&err);

  conn = dbus_bus_get_private(dbus_session ? DBUS_BUS_SESSION : DBUS_BUS_SYSTEM, &err);
  if (dbus_error_is_set(&err)) {
    tvherror(LS_DBUS, "Connection error: %s", err.message);
    dbus_error_free(&err);
    return NULL;
  }

  ret = dbus_bus_request_name(conn, name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
  if (dbus_error_is_set(&err)) {
    tvherror(LS_DBUS, "Name error: %s", err.message);
    dbus_error_free(&err);
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    return NULL;
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
    tvherror(LS_DBUS, "Not primary owner");
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    return NULL;
  }
  return conn;
}

/**
 * Send a signal
 */
static int
dbus_send_signal(DBusConnection *conn, const char *obj_name,
                 const char *if_name, const char *sig_name,
                 htsmsg_t *value)
{
  DBusMessage    *msg;
  DBusMessageIter args;

  msg = dbus_message_new_signal(obj_name, if_name, sig_name);
  if (msg == NULL) {
    tvherror(LS_DBUS, "Unable to create signal %s %s %s",
                     obj_name, if_name, sig_name);
    dbus_connection_unref(conn);
    return -1;
  }
  dbus_message_iter_init_append(msg, &args);
  dbus_from_htsmsg(value, &args);
  if (!dbus_connection_send(conn, msg, NULL)) {
    tvherror(LS_DBUS, "Unable to send signal %s %s %s",
                     obj_name, if_name, sig_name);
    dbus_message_unref(msg);
    dbus_connection_unref(conn);
    return -1;
  }
  dbus_connection_flush(conn);
  dbus_message_unref(msg);
  return 0;
}

/**
 * Simple ping (alive) RPC, just return the string
 */
static void
dbus_reply_to_ping(DBusMessage *msg, DBusConnection *conn)
{
  DBusMessageIter args;
  DBusMessage    *reply;
  char           *param;

  if (!dbus_message_iter_init(msg, &args))
    return;
  if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
    return;
  dbus_message_iter_get_basic(&args, &param);
  reply = dbus_message_new_method_return(msg);
  dbus_message_iter_init_append(reply, &args);
  dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param);
  dbus_connection_send(conn, reply, NULL);
  dbus_connection_flush(conn);
  dbus_message_unref(reply);
}

/**
 * Set the subscription postpone delay
 */
static void
dbus_reply_to_rpc(dbus_rpc_t *rpc, DBusMessage *msg, DBusConnection *conn)
{
  DBusMessageIter args;
  DBusMessage    *reply;
  const char     *path;
  int64_t         param_s64;
  char           *param_str;

  path = dbus_message_get_path(msg);
  if (path == NULL)
    return;
  if (strncmp(path, "/org/tvheadend/", 15))
    return;
  path += 14;
  if (!dbus_message_iter_init(msg, &args))
    return;
  if (rpc->rpc_s64) {
    if (DBUS_TYPE_INT64 != dbus_message_iter_get_arg_type(&args))
      return;
    dbus_message_iter_get_basic(&args, &param_s64);
    param_s64 = rpc->rpc_s64(rpc->opaque, path, param_s64);
    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_INT64, &param_s64);
  } else if (rpc->rpc_str) {
    if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
      return;
    dbus_message_iter_get_basic(&args, &param_str);
    param_str = rpc->rpc_str(rpc->opaque, path, param_str);
    reply = dbus_message_new_method_return(msg);
    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &param_str);
    free(param_str);
  } else {
    return;
  }
  dbus_connection_send(conn, reply, NULL);
  dbus_connection_flush(conn);
  dbus_message_unref(reply);
}

/**
 *
 */
void
dbus_register_rpc_s64(const char *call_name, void *opaque,
                      int64_t (*fcn)(void *, const char *, int64_t))
{
  dbus_rpc_t *rpc = calloc(1, sizeof(*rpc));
  rpc->call_name = strdup(call_name);
  rpc->rpc_s64 = fcn;
  rpc->opaque = opaque;
  pthread_mutex_lock(&dbus_lock);
  LIST_INSERT_HEAD(&dbus_rpcs, rpc, link);
  pthread_mutex_unlock(&dbus_lock);
}

/**
 *
 */
void
dbus_register_rpc_str(const char *call_name, void *opaque,
                      char *(*fcn)(void *, const char *, char *))
{
  dbus_rpc_t *rpc = calloc(1, sizeof(*rpc));
  rpc->call_name = strdup(call_name);
  rpc->rpc_str = fcn;
  rpc->opaque = opaque;
  pthread_mutex_lock(&dbus_lock);
  LIST_INSERT_HEAD(&dbus_rpcs, rpc, link);
  pthread_mutex_unlock(&dbus_lock);
}

/**
 *
 */

static void
dbus_connection_safe_close(DBusConnection *conn)
{
  dbus_connection_flush(conn);
  dbus_connection_close(conn);
  dbus_connection_unref(conn);
}

/**
 *
 */
static void
dbus_flush_queue(DBusConnection *conn)
{
  dbus_sig_t *ds;

  while (1) {
    pthread_mutex_lock(&dbus_lock);
    ds = TAILQ_FIRST(&dbus_signals);
    if (ds)
      TAILQ_REMOVE(&dbus_signals, ds, link);
    pthread_mutex_unlock(&dbus_lock);

    if (ds == NULL)
      break;

    if (conn)
      dbus_send_signal(conn,
                       ds->obj_name, "org.tvheadend.notify",
                       ds->sig_name, ds->msg);

    htsmsg_destroy(ds->msg);
    free(ds->sig_name);
    free(ds->obj_name);
    free(ds);
  }
  if (conn)
    dbus_connection_flush(conn);
}

/**
 * Listen for remote requests
 */
static void *
dbus_server_thread(void *aux)
{
  DBusMessage    *msg;
  DBusConnection *conn, *notify;
  tvhpoll_t      *poll;
  tvhpoll_event_t ev;
  dbus_rpc_t     *rpc;
  int             n;
  uint8_t         c;

  conn = dbus_create_session("org.tvheadend.server");
  if (conn == NULL) {
    atomic_set(&dbus_running, 0);
    return NULL;
  }

  notify = dbus_create_session("org.tvheadend.notify");
  if (notify == NULL) {
    atomic_set(&dbus_running, 0);
    dbus_connection_safe_close(conn);
    return NULL;
  }

  poll = tvhpoll_create(2);
  memset(&ev, 0, sizeof(ev));
  ev.fd       = dbus_pipe.rd;
  ev.events   = TVHPOLL_IN;
  ev.data.ptr = &dbus_pipe;
  tvhpoll_add(poll, &ev, 1);
  memset(&ev, 0, sizeof(ev));
  if (!dbus_connection_get_unix_fd(conn, &ev.fd)) {
    atomic_set(&dbus_running, 0);
    tvhpoll_destroy(poll);
    dbus_connection_safe_close(notify);
    dbus_connection_safe_close(conn);
    return NULL;
  }
  ev.events   = TVHPOLL_IN;
  ev.data.ptr = conn;
  tvhpoll_add(poll, &ev, 1);

  while (atomic_get(&dbus_running)) {

    n = tvhpoll_wait(poll, &ev, 1, -1);
    if (n < 0) {
      if (atomic_get(&dbus_running) && !ERRNO_AGAIN(errno))
        tvherror(LS_DBUS, "tvhpoll_wait() error");
    } else if (n == 0) {
      continue;
    }

    if (ev.data.ptr == &dbus_pipe) {
      if (read(dbus_pipe.rd, &c, 1) == 1) {
        if (c == 's')
          dbus_flush_queue(notify);
        else
          break; /* end-of-task */
      }
      continue;
    }

    while (1) {
      dbus_connection_read_write(conn, 0);
      msg = dbus_connection_pop_message(conn);
      if (msg == NULL)
        break;

      if (dbus_message_is_method_call(msg, "org.tvheadend", "ping")) {
        dbus_reply_to_ping(msg, conn);
        continue;
      }

      pthread_mutex_lock(&dbus_lock);
      LIST_FOREACH(rpc, &dbus_rpcs, link)
        if (dbus_message_is_method_call(msg, "org.tvheadend", rpc->call_name))
          break;
      pthread_mutex_unlock(&dbus_lock);

      if (rpc)
        dbus_reply_to_rpc(rpc, msg, conn);

      dbus_message_unref(msg);
    }
  }

  dbus_connection_safe_close(conn);
  dbus_flush_queue(notify);
  dbus_connection_safe_close(notify);
  tvhpoll_destroy(poll);
  return NULL;
}

/**
 *
 */
pthread_t dbus_tid;

void
dbus_server_init(int enabled, int session)
{
  dbus_session = session;
  pthread_mutex_init(&dbus_lock, NULL);
  TAILQ_INIT(&dbus_signals);
  LIST_INIT(&dbus_rpcs);
  if (enabled) {
    tvh_pipe(O_NONBLOCK, &dbus_pipe);
    dbus_threads_init_default();
    atomic_set(&dbus_running, 1);
    dbus_emit_signal_str("/main", "start", tvheadend_version);
  }
}

void
dbus_server_start(void)
{
  if (dbus_pipe.wr > 0)
    tvhthread_create(&dbus_tid, NULL, dbus_server_thread, NULL, "dbus");
}

void
dbus_server_done(void)
{
  dbus_rpc_t *rpc;

  dbus_emit_signal_str("/main", "stop", "bye");
  atomic_set(&dbus_running, 0);
  if (dbus_pipe.wr > 0) {
    tvh_write(dbus_pipe.wr, "", 1);
    pthread_kill(dbus_tid, SIGTERM);
    pthread_join(dbus_tid, NULL);
  }
  dbus_flush_queue(NULL);
  while ((rpc = LIST_FIRST(&dbus_rpcs)) != NULL) {
    LIST_REMOVE(rpc, link);
    free(rpc->call_name);
    free(rpc);
  }
}
