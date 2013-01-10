/*
 *  Digital Video Recorder - inotify processing
 *  Copyright (C) 2012 Adam Sutton
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

#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "tvheadend.h"
#include "redblack.h"
#include "dvr/dvr.h"
#include "htsp_server.h"

/* inotify limits */
#define EVENT_SIZE    ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 10 * ( EVENT_SIZE + 16 ) )
#define EVENT_MASK    IN_CREATE    | IN_DELETE     | IN_DELETE_SELF |\
                      IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO
                      
static int                         _inot_fd;
static RB_HEAD(,dvr_inotify_entry) _inot_tree;

typedef struct dvr_inotify_entry
{
  RB_ENTRY(dvr_inotify_entry) link;
  char                        *path;
  int                          fd;
  struct dvr_entry_list        entries;
} dvr_inotify_entry_t;

static void* _dvr_inotify_thread ( void *p );

static int _str_cmp ( void *a, void *b )
{
  return strcmp(((dvr_inotify_entry_t*)a)->path, ((dvr_inotify_entry_t*)b)->path);
}

/**
 * Initialise threads
 */
void dvr_inotify_init ( void )
{
  pthread_t tid;

  _inot_fd = inotify_init();

  pthread_create(&tid, NULL, _dvr_inotify_thread, NULL);
}

/**
 * Add an entry for monitoring
 */
void dvr_inotify_add ( dvr_entry_t *de )
{
  static dvr_inotify_entry_t *skel = NULL;
  dvr_inotify_entry_t *e;
  char *path;
  struct stat st;

  if (!de->de_filename || stat(de->de_filename, &st))
    return;

  path = strdup(de->de_filename);

  if (!skel)
    skel = calloc(1, sizeof(dvr_inotify_entry_t));
  skel->path = dirname(path);
  
  if (stat(skel->path, &st))
    return;
  
  e = RB_INSERT_SORTED(&_inot_tree, skel, link, _str_cmp);
  if (!e) {
    e       = skel;
    skel    = NULL;
    e->path = strdup(e->path);
    e->fd   = inotify_add_watch(_inot_fd, e->path, EVENT_MASK);
    assert(e->fd != -1);
  }

  LIST_INSERT_HEAD(&e->entries, de, de_inotify_link);

  free(path);
}

/*
 * Delete an entry from the monitor
 */
void dvr_inotify_del ( dvr_entry_t *de )
{
  dvr_entry_t *det;
  dvr_inotify_entry_t *e;
  RB_FOREACH(e, &_inot_tree, link) {
    LIST_FOREACH(det, &e->entries, de_inotify_link)
      if (det == de) break;
    if (det) break;
  }

  if (e && det) {
    LIST_REMOVE(det, de_inotify_link);
    if (LIST_FIRST(&e->entries) == NULL) {
      RB_REMOVE(&_inot_tree, e, link);
      inotify_rm_watch(_inot_fd, e->fd);
      free(e->path);
      free(e);
    }
  }
}

/*
 * Find inotify entry
 */
static dvr_inotify_entry_t *
_dvr_inotify_find
  ( int fd )
{
  dvr_inotify_entry_t *e = NULL;
  RB_FOREACH(e, &_inot_tree, link)
    if (e->fd == fd)
      break;
  return e;
}

/*
 * Find DVR entry
 */
static dvr_entry_t *
_dvr_inotify_find2
  ( dvr_inotify_entry_t *die, const char *name )
{
  dvr_entry_t *de = NULL;
  char path[512];
  
  snprintf(path, sizeof(path), "%s/%s", die->path, name);
  
  LIST_FOREACH(de, &die->entries, de_inotify_link)
    if (!strcmp(path, de->de_filename))
      break;
  
  return de;
}

/*
 * File moved
 */
static void
_dvr_inotify_moved
  ( int fd, const char *from, const char *to )
{
  dvr_inotify_entry_t *die;
  dvr_entry_t *de;

  if (!(die = _dvr_inotify_find(fd)))
    return;

  if (!(de = _dvr_inotify_find2(die, from)))
    return;

  if (to) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", die->path, to);
    tvh_str_update(&de->de_filename, path);
    dvr_entry_save(de);
  } else
    dvr_inotify_del(de);
  
  htsp_dvr_entry_update(de);
  dvr_entry_notify(de);
}

/*
 * File deleted
 */
static void
_dvr_inotify_delete
  ( int fd, const char *path )
{
  _dvr_inotify_moved(fd, path, NULL);
}

/*
 * Directory moved
 */
static void
_dvr_inotify_moved_all
  ( int fd, const char *to )
{
  dvr_entry_t *de;
  dvr_inotify_entry_t *die;
  
  if (!(die = _dvr_inotify_find(fd)))
    return;

  while ((de = LIST_FIRST(&die->entries))) {
    htsp_dvr_entry_update(de);
    dvr_entry_notify(de);
    dvr_inotify_del(de);
  }
}

/*
 * Directory deleted
 */
static void
_dvr_inotify_delete_all
  ( int fd )
{
  _dvr_inotify_moved_all(fd, NULL);
}

/*
 * Process events
 */
void* _dvr_inotify_thread ( void *p )
{
  int i, len;
  char buf[EVENT_BUF_LEN];
  const char *from;
  int fromfd;
  int cookie;

  while (1) {

    /* Read events */
    fromfd = 0;
    cookie = 0;
    from   = NULL;
    i      = 0;
    len    = read(_inot_fd, buf, EVENT_BUF_LEN);

    /* Process */
    pthread_mutex_lock(&global_lock);
    while ( i < len ) {
      struct inotify_event *ev = (struct inotify_event*)&buf[i];
      i += EVENT_SIZE + ev->len;

      /* Moved */
      if (ev->mask & IN_MOVED_FROM) {
        from   = ev->name;
        fromfd = ev->wd;
        cookie = ev->cookie;
        continue;

      } else if ((ev->mask & IN_MOVED_TO) && from && ev->cookie == cookie) {
        _dvr_inotify_moved(ev->wd, from, ev->name);
        from = NULL;
      
      /* Removed */
      } else if (ev->mask & IN_DELETE) {
        _dvr_inotify_delete(ev->wd, ev->name);
    
      /* Moved self */
      } else if (ev->mask & IN_MOVE_SELF) {
        _dvr_inotify_moved_all(ev->wd, NULL);
    
      /* Removed self */
      } else if (ev->mask & IN_DELETE_SELF) {
        _dvr_inotify_delete_all(ev->wd);
      }

      if (from) {
        _dvr_inotify_moved(fromfd, from, NULL);
        from   = NULL;
        cookie = 0;
      }
    }
    if (from)
      _dvr_inotify_moved(fromfd, from, NULL);
    pthread_mutex_unlock(&global_lock);
  }

  return NULL;
}

