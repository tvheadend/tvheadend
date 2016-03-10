/*
 *  Tvheadend - File/Directory monitoring
 *
 *  Copyright (C) 2014 Adam Sutton
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

#include "tvheadend.h"
#include "fsmonitor.h"
#include "redblack.h"
#include "queue.h"

#if ENABLE_INOTIFY

#include <signal.h>
#include <sys/inotify.h>
#include <sys/stat.h>

/* Global list of monitorer paths (and inotify FD) */
RB_HEAD(,fsmonitor_path) fsmonitor_paths;
int                      fsmonitor_fd;

/* RB tree sorting of paths */
static int
fmp_cmp ( fsmonitor_path_t *a, fsmonitor_path_t *b )
{
  return strcmp(a->fmp_path, b->fmp_path);
}

/*
 * Inotify thread for handling events
 */
static void *
fsmonitor_thread ( void* p )
{
  int c, i;
  uint8_t buf[sizeof(struct inotify_event) * 10];
  char path[1024];
  struct inotify_event *ev;
  fsmonitor_path_t *fmp;
  fsmonitor_link_t *fml;
  fsmonitor_t *fsm;

  while (atomic_get(&tvheadend_running)) {

    /* Wait for event */
    c = read(fsmonitor_fd, buf, sizeof(buf));
    if (fsmonitor_fd < 0)
      break;

    /* Process */
    pthread_mutex_lock(&global_lock);
    i = 0;
    while ( i < c ) {
      ev = (struct inotify_event*)&buf[i];
      i += sizeof(struct inotify_event) + ev->len;
      if (i > c)
        break;
      tvhtrace("fsmonitor", "event fd %d name %s mask %08X",
               ev->wd, ev->len ? ev->name : NULL, ev->mask);

      /* Find */
      // TODO: make this more efficient (especially if number of
      //       watched paths gets big)
      RB_FOREACH(fmp, &fsmonitor_paths, fmp_link)
        if (fmp->fmp_fd == ev->wd)
          break;
      if (!fmp) continue;

      /* Full path */
      snprintf(path, sizeof(path), "%s/%s", fmp->fmp_path, ev->name);

      /* Process listeners */
      LIST_FOREACH(fml, &fmp->fmp_monitors, fml_plink) {
        fsm = fml->fml_monitor;
        if (ev->mask & IN_CREATE && fsm->fsm_create)
          fsm->fsm_create(fsm, path);
        else if (ev->mask & IN_DELETE && fsm->fsm_delete)
          fsm->fsm_delete(fsm, path);
      }
    }
    pthread_mutex_unlock(&global_lock);
  }
  return NULL;
}

/*
 * Start the fsmonitor subsystem
 */
pthread_t fsmonitor_tid;

void
fsmonitor_init ( void )
{
  /* Intialise inotify */
  fsmonitor_fd = inotify_init1(IN_CLOEXEC);
  tvhthread_create(&fsmonitor_tid, NULL, fsmonitor_thread, NULL, "fsmonitor");
}

/*
 * Stop the fsmonitor subsystem
 */
void
fsmonitor_done ( void )
{
  shutdown(fsmonitor_fd, SHUT_RDWR);
  pthread_kill(fsmonitor_tid, SIGTERM);
  pthread_join(fsmonitor_tid, NULL);
  close(fsmonitor_fd);
  fsmonitor_fd = -1;
}

/*
 * Add a new path
 */
int
fsmonitor_add ( const char *path, fsmonitor_t *fsm )
{
  int mask;
  fsmonitor_path_t *skel;
  fsmonitor_path_t *fmp;
  fsmonitor_link_t *fml;

  lock_assert(&global_lock);

  skel = calloc(1, sizeof(fsmonitor_path_t));
  skel->fmp_path = (char*)path;

  /* Build mask */
  mask = IN_CREATE | IN_DELETE;

  /* Find */
  fmp = RB_INSERT_SORTED(&fsmonitor_paths, skel, fmp_link, fmp_cmp);
  if (!fmp) {
    fmp = skel;
    fmp->fmp_fd = inotify_add_watch(fsmonitor_fd, path, mask);

    /* Failed */
    if (fmp->fmp_fd <= 0) {
      RB_REMOVE(&fsmonitor_paths, fmp, fmp_link);
      free(fmp);
      tvhdebug("fsmonitor", "failed to add %s (exists?)", path);
      printf("ERROR: failed to add %s\n", path);
      return -1;
    }

    /* Setup */
    fmp->fmp_path = strdup(path);
    tvhdebug("fsmonitor", "watch %s", fmp->fmp_path);
  } else {
    free(skel);
  }

  /* Check doesn't exist */
  // TODO: could make this more efficient
  LIST_FOREACH(fml, &fmp->fmp_monitors, fml_plink)
    if (fml->fml_monitor == fsm)
      return 0;

  /* Add */
  fml = calloc(1, sizeof(fsmonitor_link_t));
  fml->fml_path    = fmp;
  fml->fml_monitor = fsm;
  LIST_INSERT_HEAD(&fmp->fmp_monitors, fml, fml_plink);
  LIST_INSERT_HEAD(&fsm->fsm_paths,    fml, fml_mlink);
  return 0;
}

/*
 * Remove an existing path
 */
void
fsmonitor_del ( const char *path, fsmonitor_t *fsm )
{
  static fsmonitor_path_t skel;
  fsmonitor_path_t *fmp;
  fsmonitor_link_t *fml;

  lock_assert(&global_lock);

  skel.fmp_path = (char*)path;

  /* Find path */
  fmp = RB_FIND(&fsmonitor_paths, &skel, fmp_link, fmp_cmp);
  if (fmp) {

    /* Find link */
    LIST_FOREACH(fml, &fmp->fmp_monitors, fml_plink)
      if (fml->fml_monitor == fsm)
        break;

    /* Remove link */
    if (fml) {
      LIST_REMOVE(fml, fml_plink);
      LIST_REMOVE(fml, fml_mlink);
      free(fml);
    }

    /* Remove path */
    if (LIST_EMPTY(&fmp->fmp_monitors)) {
      tvhdebug("fsmonitor", "unwatch %s", fmp->fmp_path);
      RB_REMOVE(&fsmonitor_paths, fmp, fmp_link);
      inotify_rm_watch(fsmonitor_fd, fmp->fmp_fd);
      free(fmp->fmp_path);
      free(fmp);
    }
  }
}

#else /* ENABLE_INOTIFY */

void
fsmonitor_init ( void )
{
}

void
fsmonitor_done ( void )
{
}

int
fsmonitor_add ( const char *path, fsmonitor_t *fsm )
{
  return 0; // TODO: is this the right value?
}

void
fsmonitor_del ( const char *path, fsmonitor_t *fsm )
{
}

#endif
