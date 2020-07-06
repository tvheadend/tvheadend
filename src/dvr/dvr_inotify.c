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

#include <sys/inotify.h>
#include <sys/stat.h>
#include <signal.h>

#include "tvheadend.h"
#include "redblack.h"
#include "dvr/dvr.h"
#include "htsp_server.h"

/* inotify limits */
#define EVENT_SIZE    (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (5 * EVENT_SIZE + NAME_MAX)
#define EVENT_MASK    IN_DELETE    | IN_DELETE_SELF | \
                      IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO
                      
static int                         _inot_fd;
static RB_HEAD(,dvr_inotify_entry) _inot_tree;

typedef struct dvr_inotify_filename
{
  dvr_entry_t *de;
  LIST_ENTRY(dvr_inotify_filename) link;
} dvr_inotify_filename_t;

typedef struct dvr_inotify_entry
{
  RB_ENTRY(dvr_inotify_entry) link;
  char *path;
  int fd;
  LIST_HEAD(, dvr_inotify_filename) entries;
} dvr_inotify_entry_t;

static SKEL_DECLARE(dvr_inotify_entry_skel, dvr_inotify_entry_t);

static void* _dvr_inotify_thread ( void *p );

static int _str_cmp ( void *a, void *b )
{
  return strcmp(((dvr_inotify_entry_t*)a)->path, ((dvr_inotify_entry_t*)b)->path);
}


/**
 * Initialise threads
 */
pthread_t dvr_inotify_tid;

void dvr_inotify_init ( void )
{
  atomic_set(&_inot_fd, inotify_init1(IN_CLOEXEC));
  if (atomic_get(&_inot_fd) < 0) {
    tvherror(LS_DVR, "failed to initialise inotify (err=%s)", strerror(errno));
    return;
  }

  tvh_thread_create(&dvr_inotify_tid, NULL, _dvr_inotify_thread, NULL, "dvr-inotify");
}

/**
 *
 */
void dvr_inotify_done ( void )
{
  int fd = atomic_exchange(&_inot_fd, -1);
  if (fd >= 0) blacklisted_close(fd);
  tvh_thread_kill(dvr_inotify_tid, SIGTERM);
  pthread_join(dvr_inotify_tid, NULL);
  SKEL_FREE(dvr_inotify_entry_skel);
}

/**
 *
 */
static int dvr_inotify_exists ( dvr_inotify_entry_t *die, dvr_entry_t *de )
{
  dvr_inotify_filename_t *dif;

  LIST_FOREACH(dif, &die->entries, link)
    if (dif->de == de)
      return 1;
  return 0;
}

/**
 * Add an entry for monitoring
 */
static void dvr_inotify_add_one ( dvr_entry_t *de, htsmsg_t *m )
{
  dvr_inotify_filename_t *dif;
  dvr_inotify_entry_t *e;
  const char *filename;
  char path[PATH_MAX];
  int fd = atomic_get(&_inot_fd);

  filename = htsmsg_get_str(m, "filename");
  if (filename == NULL || fd < 0)
    return;

  /* Since filename might be inside a symlinked directory
   * we want to get the true name otherwise when we move
   * a file we will not match correctly since some files
   * may point to /mnt/a/z.ts and some to /mnt/b/z.ts
   */
  if (!realpath(filename, path))
    return;

  SKEL_ALLOC(dvr_inotify_entry_skel);
  dvr_inotify_entry_skel->path = dirname(path);
  
  e = RB_INSERT_SORTED(&_inot_tree, dvr_inotify_entry_skel, link, _str_cmp);
  if (!e) {
    e       = dvr_inotify_entry_skel;
    SKEL_USED(dvr_inotify_entry_skel);
    e->path = strdup(e->path);
    e->fd   = inotify_add_watch(fd, e->path, EVENT_MASK);
  }

  if (!dvr_inotify_exists(e, de)) {

    dif = malloc(sizeof(*dif));
    dif->de = de;

    LIST_INSERT_HEAD(&e->entries, dif, link);

    if (e->fd < 0) {
      tvherror(LS_DVR, "failed to add inotify watch to %s (err=%s)",
               e->path, strerror(errno));
      dvr_inotify_del(de);
    } else {
      tvhdebug(LS_DVR, "adding inotify watch to %s (fd=%d)",
               e->path, e->fd);
    }

  }
}

void dvr_inotify_add ( dvr_entry_t *de )
{
  htsmsg_field_t *f;
  htsmsg_t *m;

  if (atomic_get(&_inot_fd) < 0 || de->de_files == NULL)
    return;

  HTSMSG_FOREACH(f, de->de_files)
    if ((m = htsmsg_field_get_map(f)) != NULL)
      dvr_inotify_add_one(de, m);
}

/*
 * Delete an entry from the monitor
 */
void dvr_inotify_del ( dvr_entry_t *de )
{
  dvr_inotify_filename_t *f = NULL, *f_next;
  dvr_inotify_entry_t *e, *e_next;
  int fd;

  lock_assert(&global_lock);

  for (e = RB_FIRST(&_inot_tree); e; e = e_next) {
    e_next = RB_NEXT(e, link);
    for (f = LIST_FIRST(&e->entries); f; f = f_next) {
      f_next = LIST_NEXT(f, link);
      if (f->de == de) {
        LIST_REMOVE(f, link);
        free(f);
        if (LIST_FIRST(&e->entries) == NULL) {
          RB_REMOVE(&_inot_tree, e, link);
          fd = atomic_get(&_inot_fd);
          if (e->fd >= 0 && fd >= 0)
            inotify_rm_watch(fd, e->fd);
          free(e->path);
          free(e);
        }
      }
    }
  }
}

/*
 * return count of registered entries (for debugging)
 */
int dvr_inotify_count ( void )
{
  dvr_inotify_filename_t *f;
  dvr_inotify_entry_t *e;
  int count = 0;
  lock_assert(&global_lock);
  RB_FOREACH(e, &_inot_tree, link)
    LIST_FOREACH(f, &e->entries, link)
      count++;
  return count;
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
 * File moved
 */
static void
_dvr_inotify_moved
  ( int from_fd, const char *from, const char *to, int to_fd )
{
  dvr_inotify_filename_t *dif;
  dvr_inotify_entry_t *die;
  dvr_entry_t *de;
  char path[PATH_MAX];
  const char *filename;
  htsmsg_t *m = NULL;
  htsmsg_field_t *f = NULL;
  char realdir[PATH_MAX];
  char new_path[PATH_MAX+PATH_MAX+1];
  char ubuf[UUID_HEX_SIZE];
  char *file, *dir = NULL;

  if (!(die = _dvr_inotify_find(from_fd)))
    return;

  snprintf(path, sizeof(path), "%s/%s", die->path, from);
  tvhdebug(LS_DVR, "inotify: moved from_fd: %d path: \"%s\" to: \"%s\" to_fd: %d", from_fd, path, to?:"<none>", to_fd);

  de = NULL;
  LIST_FOREACH(dif, &die->entries, link) {
    de = dif->de;
    if (de->de_files == NULL)
      continue;
    HTSMSG_FOREACH(f, de->de_files)
      if ((m = htsmsg_field_get_map(f)) != NULL) {
        filename = htsmsg_get_str(m, "filename");
        if (!filename)
          continue;

        /* Simple case of names match */
        if (!strcmp(path, filename))
          break;

        /* Otherwise get the real path (after symlinks)
         * and compare that.
         *
         * This is made far more complicated since the
         * file has already disappeared so we can't realpath
         * on it, so we need to realpath on the directory
         * it _was_ in, append the filename part and then
         * compare against the realpath of the path we
         * were given by inotify.
         */
        dir = tvh_strdupa(filename);
        dir = dirname(dir);
        if (realpath(dir, realdir)) {
          file = basename(tvh_strdupa(filename));
          snprintf(new_path, sizeof(new_path), "%s/%s", realdir, file);
          if (!strcmp(path, new_path))
            break;
        }
      }
    if (f)
      break;
  }

  if (!dif)
    return;

  if (f && m) {
    /* "to" will be NULL on a delete */
    if (to) {
      /* If we have moved to another directory we are inotify watching
       * then we get an fd for the directory we are moving to which is
       * different to the one we are moving from. So fetch the
       * directory details for that.
       */
      if (to_fd != -1 && to_fd != from_fd) {
        if (!(die = _dvr_inotify_find(to_fd))) {
          tvhdebug(LS_DVR, "Failed to _dvr_inotify_find for fd: %d", to_fd);
          return;
        }
      }
      snprintf(new_path, sizeof(path), "%s/%s", die->path, to);
      tvhdebug(LS_DVR, "inotify: moved from name: \"%s\" to: \"%s\" for \"%s\"", path, new_path, idnode_uuid_as_str(&de->de_id, ubuf));
      htsmsg_set_str(m, "filename", new_path);
      idnode_changed(&de->de_id);
    } else {
      htsmsg_field_destroy(de->de_files, f);
      if (htsmsg_is_empty(de->de_files))
        dvr_inotify_del(de);
    }
  }
  
  dvr_vfs_refresh_entry(de);
  htsp_dvr_entry_update(de);
  idnode_notify_changed(&de->de_id);
}

/*
 * File deleted
 */
static void
_dvr_inotify_delete
  ( int fd, const char *path )
{
  _dvr_inotify_moved(fd, path, NULL, -1);
}

/*
 * Directory moved
 */
static void
_dvr_inotify_moved_all
  ( int fd, const char *to )
{
  dvr_inotify_filename_t *f;
  dvr_inotify_entry_t *die;
  dvr_entry_t *de;

  if (!(die = _dvr_inotify_find(fd)))
    return;

  while ((f = LIST_FIRST(&die->entries))) {
    de = f->de;
    htsp_dvr_entry_update(de);
    idnode_notify_changed(&de->de_id);
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
  int fd, i, len;
  char buf[EVENT_BUF_LEN];
  const char *from;
  int fromfd;
  int cookie;
  struct inotify_event *ev;
  char from_prev[NAME_MAX + 1] = "";
  int fromfd_prev = 0;
  int cookie_prev = 0;

  while (tvheadend_is_running()) {

    /* Read events */
    fromfd = 0;
    cookie = 0;
    from   = NULL;
    i      = 0;
    fd     = atomic_get(&_inot_fd);
    if (fd < 0)
      break;
    len    = read(fd, buf, EVENT_BUF_LEN);
    if (len < 0)
      break;

    /* Process */
    tvh_mutex_lock(&global_lock);
    while (i < len) {
      ev = (struct inotify_event *)&buf[i];
      tvhtrace(LS_DVR_INOTIFY, "i=%d len=%d name=%s", i, len, ev->name);
      i += EVENT_SIZE + ev->len;
      if (i > len || i < 0)
        break;

      /* Moved */
      if (ev->mask & IN_MOVED_FROM) {
        from   = ev->name;
        fromfd = ev->wd;
        cookie = ev->cookie;
        tvhtrace(LS_DVR_INOTIFY, "i=%d len=%d from=%s cookie=%d ", i, len, from, cookie);
        continue;

      } else if (ev->mask & IN_MOVED_TO) {
        tvhtrace(LS_DVR_INOTIFY, "i=%d len=%d to_cookie=%d from_cookie=%d cookie_prev=%d", i, len, ev->cookie, cookie, cookie_prev);
          if (from && ev->cookie == cookie) {
            _dvr_inotify_moved(fromfd, from, ev->name, ev->wd);
            from = NULL;
	    cookie = 0;
          } else if (from_prev[0] != '\0' && ev->cookie == cookie_prev) {
             _dvr_inotify_moved(fromfd_prev, from_prev, ev->name, ev->wd);
             from_prev[0] = '\0';
	     cookie_prev = 0;
	  }

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
    }
    // if still old "from", assume matching "to" is not coming
    if (from_prev[0] != '\0') {
      _dvr_inotify_moved(fromfd_prev, from_prev, NULL, -1);
      from_prev[0] = '\0';
      cookie_prev = 0;
    }
    // if unmatched "from", save in case matching "to" is coming in next read
    if (from) {
      strcpy(from_prev, from);
      fromfd_prev = fromfd;
      cookie_prev = cookie;
      tvhdebug(LS_DVR_INOTIFY, "i=%d len=%d cookie_prev=%d from_prev=%s fd=%d EOR", i, len, cookie_prev, from_prev, fromfd_prev);
    }
    tvh_mutex_unlock(&global_lock);
  }

  return NULL;
}

