/*
 *  tvheadend, Notification framework
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

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "tvheadend.h"
#include "notify.h"
#include "webui/webui.h"

static pthread_cond_t         notify_cond;
static pthread_mutex_t        notify_mutex;
static htsmsg_t              *notify_queue;
static pthread_t              notify_tid;
static void*                  notify_thread(void* p);

void
notify_by_msg(const char *class, htsmsg_t *m, int rewrite)
{
  htsmsg_add_str(m, "notificationClass", class);
  comet_mailbox_add_message(m, 0, rewrite);
  htsmsg_destroy(m);
}


void
notify_reload(const char *class)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_add_u32(m, "reload", 1);
  notify_by_msg(class, m, 0);
}

void
notify_delayed(const char *id, const char *event, const char *action)
{
  htsmsg_t *m = NULL, *e = NULL;
  htsmsg_field_t *f;

  if (!tvheadend_running)
    return;

  pthread_mutex_lock(&notify_mutex);
  if (notify_queue == NULL) {
    notify_queue = htsmsg_create_map();
  } else {
    m = htsmsg_get_map(notify_queue, event);
  }
  if (m == NULL) {
    m = htsmsg_add_msg(notify_queue, event, htsmsg_create_map());
  } else {
    e = htsmsg_get_list(m, action);
  }
  if (e == NULL)
    e = htsmsg_add_msg(m, action, htsmsg_create_list());
  HTSMSG_FOREACH(f, e)
    if (strcmp(htsmsg_field_get_str(f) ?: "", id) == 0)
      goto skip;
  htsmsg_add_str(e, NULL, id);
  pthread_cond_signal(&notify_cond);
skip:
  pthread_mutex_unlock(&notify_mutex);
}

void *
notify_thread ( void *p )
{
  htsmsg_t *q = NULL;
  htsmsg_field_t *f;

  pthread_mutex_lock(&notify_mutex);

  while (tvheadend_running) {

    /* Get queue */
    if (!notify_queue) {
      pthread_cond_wait(&notify_cond, &notify_mutex);
      continue;
    }
    q            = notify_queue;
    notify_queue = NULL;
    pthread_mutex_unlock(&notify_mutex);

    /* Process */
    pthread_mutex_lock(&global_lock);

    HTSMSG_FOREACH(f, q)
      notify_by_msg(f->hmf_name, htsmsg_detach_submsg(f), 0);

    /* Finished */
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(q);

    /* Wait */
    usleep(500000);
    pthread_mutex_lock(&notify_mutex);
  }
  pthread_mutex_unlock(&notify_mutex);

  return NULL;
}

/*
 *
 */

void notify_init( void )
{
  notify_queue = NULL;
  pthread_mutex_init(&notify_mutex, NULL);
  pthread_cond_init(&notify_cond, NULL);
  tvhthread_create(&notify_tid, NULL, notify_thread, NULL, "notify");
}

void notify_done( void )
{
  pthread_cond_signal(&notify_cond);
  pthread_join(notify_tid, NULL);
  pthread_mutex_lock(&notify_mutex);
  htsmsg_destroy(notify_queue);
  notify_queue = NULL;
  pthread_mutex_unlock(&notify_mutex);
}
