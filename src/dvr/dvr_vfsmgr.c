/*
 *  Digital Video Recorder - file system management
 *  Copyright (C) 2015 Jaroslav Kysela
 *  Copyright (C) 2015 Glenn Christiaensen
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

#include <stdarg.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <libgen.h> /* basename */

#include "tvheadend.h"
#include "dvr.h"
#include "atomic.h"
#include "notify.h"
#include "tvhvfs.h"

#define MIB(v)   ((int64_t)v*((int64_t)1024*1024))
#define TOMIB(v) (v/((int64_t)1024*1024))

struct dvr_vfs_list dvrvfs_list;

static int dvr_disk_space_config_idx;
static int dvr_disk_space_config_size;
static int64_t dvr_disk_space_config_lastdelete;
static int64_t dvr_bfree;
static int64_t dvr_btotal;
static int64_t dvr_bused;
static pthread_mutex_t dvr_disk_space_mutex;
static mtimer_t dvr_disk_space_timer;
static tasklet_t dvr_disk_space_tasklet;

/*
 *
 */
static dvr_vfs_t *
dvr_vfs_find(dvr_vfs_t *old, tvh_fsid_t id)
{
  dvr_vfs_t *dv;

  if (old && old->fsid == id)
    return old;
  LIST_FOREACH(dv, &dvrvfs_list, link)
    if (dv->fsid == id)
      return dv;
  dv = calloc(1, sizeof(*dv));
  dv->fsid = id;
  LIST_INSERT_HEAD(&dvrvfs_list, dv, link);
  return dv;
}

static dvr_vfs_t *
dvr_vfs_find1(dvr_vfs_t *old, htsmsg_t *m)
{
  int64_t v;

  if (!htsmsg_get_s64(m, "fsid", &v))
    return dvr_vfs_find(old, v);
  return NULL;
}

/*
 *
 */
void
dvr_vfs_refresh_entry(dvr_entry_t *de)
{
  htsmsg_field_t *f;
  htsmsg_t *m;
  struct statvfs vst;
  struct stat st;
  dvr_vfs_t *vfs = NULL;
  uint64_t size;
  const char *filename;

  lock_assert(&global_lock);
  if (de->de_files == NULL)
    return;
  HTSMSG_FOREACH(f, de->de_files)
    if ((m = htsmsg_field_get_map(f)) != NULL) {
      vfs = dvr_vfs_find1(vfs, m);
      if (vfs) {
        size = htsmsg_get_s64_or_default(m, "size", 0);
        vfs->used_size = size <= vfs->used_size ? vfs->used_size - size : 0;
      }
      filename = htsmsg_get_str(m, "filename");
      if(filename == NULL ||
         statvfs(filename, &vst) < 0 || stat(filename, &st) < 0) {
        tvherror(LS_DVR, "unable to stat file '%s'", filename);
        goto rem;
      }
      vfs = dvr_vfs_find(vfs, tvh_fsid(vst.f_fsid));
      if (vfs && st.st_size >= 0) {
        htsmsg_set_s64(m, "fsid", tvh_fsid(vst.f_fsid));
        htsmsg_set_s64(m, "size", st.st_size);
        vfs->used_size += st.st_size;
      } else {
rem:
        htsmsg_delete_field(m, "fsid");
        htsmsg_delete_field(m, "size");
      }
    }
}

/*
 *
 */
void
dvr_vfs_remove_entry(dvr_entry_t *de)
{
  htsmsg_field_t *f;
  htsmsg_t *m;
  dvr_vfs_t *vfs = NULL;
  uint64_t size;

  lock_assert(&global_lock);
  HTSMSG_FOREACH(f, de->de_files)
    if ((m = htsmsg_field_get_map(f)) != NULL) {
      vfs = dvr_vfs_find1(vfs, m);
      if (vfs) {
        size = htsmsg_get_s64_or_default(m, "size", 0);
        vfs->used_size = size <= vfs->used_size ? vfs->used_size - size : 0;
      }
      htsmsg_delete_field(m, "fsid");
      htsmsg_delete_field(m, "size");
    }
}

/*
 *
 */
int64_t
dvr_vfs_update_filename(const char *filename, htsmsg_t *fdata)
{
  dvr_vfs_t *vfs;
  struct stat st;
  int64_t size;

  if (filename == NULL || fdata == NULL)
    return -1;
  vfs = dvr_vfs_find1(NULL, fdata);
  if (vfs) {
    size = htsmsg_get_s64_or_default(fdata, "size", 0);
    vfs->used_size = size <= vfs->used_size ? vfs->used_size - size : 0;
    if (stat(filename, &st) >= 0 && st.st_size >= 0) {
      htsmsg_set_s64(fdata, "size", st.st_size);
      vfs->used_size += st.st_size;
      return st.st_size;
    }
  }
  htsmsg_delete_field(fdata, "fsid");
  htsmsg_delete_field(fdata, "size");
  return -1;
}

/**
 * Cleanup old recordings for this config until the dvr_cleanup_threshold is reached
 * Only "Keep until space needed" recordings are deleted, starting with the oldest one
 */
static int64_t
dvr_disk_space_cleanup(dvr_config_t *cfg, int include_active)
{
  dvr_entry_t *de, *oldest;
  time_t stoptime;
  int64_t requiredBytes, maximalBytes, availBytes, usedBytes, diskBytes;
  int64_t clearedBytes = 0, fileSize;
  tvh_fsid_t filesystemId;
  struct statvfs diskdata;
  struct tm tm;
  int loops = 0;
  char tbuf[64];
  const char *configName;
  dvr_vfs_t *dvfs;

  if (!cfg || !cfg->dvr_enabled || statvfs(cfg->dvr_storage, &diskdata) == -1)
    return -1;

  dvfs = dvr_vfs_find(NULL, tvh_fsid(diskdata.f_fsid));

  filesystemId  = tvh_fsid(diskdata.f_fsid);
  availBytes    = diskdata.f_frsize * (int64_t)diskdata.f_bavail;
  requiredBytes = MIB(cfg->dvr_cleanup_threshold_free);
  diskBytes     = diskdata.f_frsize * (int64_t)diskdata.f_blocks;
  usedBytes     = dvfs->used_size;
  maximalBytes  = MIB(cfg->dvr_cleanup_threshold_used);
  configName    = cfg != dvr_config_find_by_name(NULL) ? cfg->dvr_config_name : "Default profile";

  /* When deleting a file from the disk, the system needs some time to actually do this */
  /* If calling this function to fast after the previous call, statvfs might be wrong/not updated yet */
  /* So we are risking to delete more files than needed, so allow 10s for the system to handle previous deletes */
  if (dvr_disk_space_config_lastdelete + sec2mono(10) > mclk()) {
    tvhwarn(LS_DVR,"disk space cleanup for config \"%s\" is not allowed now", configName);
    return -1;
  }

  if (diskBytes < requiredBytes) {
    tvhwarn(LS_DVR,"disk space cleanup for config \"%s\", required free space \"%"PRId64" MiB\" is smaller than the total disk size!",
            configName, TOMIB(requiredBytes));
    if (maximalBytes >= usedBytes)
      return -1;
  }

  tvhtrace(LS_DVR, "disk space cleanup for config \"%s\", required/current free space \"%"PRId64"/%"PRId64" MiB\", required/current used space \"%"PRId64"/%"PRId64" MiB\"",
           configName, TOMIB(requiredBytes), TOMIB(availBytes), TOMIB(maximalBytes), TOMIB(usedBytes));

  while (availBytes < requiredBytes || ((maximalBytes < usedBytes) && cfg->dvr_cleanup_threshold_used)) {
    oldest = NULL;
    stoptime = gclk();

    LIST_FOREACH(de, &dvrentries, de_global_link) {
      if (de->de_sched_state != DVR_COMPLETED &&
          de->de_sched_state != DVR_MISSED_TIME)
        continue;

      if (dvr_entry_get_stop_time(de) > stoptime)
        continue;

      if (dvr_entry_get_removal_days(de) != DVR_REM_SPACE) // only remove the allowed ones
        continue;

      if (dvr_get_filename(de) == NULL || dvr_get_filesize(de, DVR_FILESIZE_TOTAL) <= 0)
        continue;

      if(statvfs(dvr_get_filename(de), &diskdata) == -1)
        continue;

      /* Checking for the same config is useless as it's storage path might be changed meanwhile */
      /* Check for the same file system instead */
      if (filesystemId == 0 || tvh_fsid(diskdata.f_fsid) != filesystemId)
        continue;

      oldest = de; // the oldest one until now
      stoptime = dvr_entry_get_stop_time(de);
    }

    if (oldest) {
      fileSize = dvr_get_filesize(oldest, DVR_FILESIZE_TOTAL);
      availBytes += fileSize;
      clearedBytes += fileSize;
      usedBytes -= fileSize;

      localtime_r(&stoptime, &tm);
      if (strftime(tbuf, sizeof(tbuf), "%F %T", &tm) <= 0)
        *tbuf = 0;
      tvhinfo(LS_DVR,"Delete \"until space needed\" recording \"%s\" with stop time \"%s\" and file size \"%"PRId64" MB\"",
              lang_str_get(oldest->de_title, NULL), tbuf, TOMIB(fileSize));

      dvr_disk_space_config_lastdelete = mclk();
      dvr_entry_cancel_remove(oldest, 0); /* Remove stored files and mark as "removed" */
    } else {
      /* Stop active recordings if cleanup is not possible */
      if (loops == 0 && include_active) {
        tvhwarn(LS_DVR, "No \"until space needed\" recordings found for config \"%s\", aborting active recordings now!", configName);
        LIST_FOREACH(de, &dvrentries, de_global_link) {
          if (de->de_sched_state != DVR_RECORDING || !de->de_config || de->de_config != cfg)
            continue;
          dvr_stop_recording(de, SM_CODE_NO_SPACE, 1, 0);
        }
      }
      goto finish;
    }

    loops++;
    if (loops >= 10) {
      tvhwarn(LS_DVR, "Not able to clear the required disk space after deleting %i \"until space needed\" recordings...", loops);
      goto finish;
    }
  }

finish:
  tvhtrace(LS_DVR, "disk space cleanup for config \"%s\", cleared \"%"PRId64" MB\" of disk space, new free disk space \"%"PRId64" MiB\", new used disk space \"%"PRId64" MiB\"",
           configName, TOMIB(clearedBytes), TOMIB(availBytes), TOMIB(usedBytes));

  return clearedBytes;
}

/**
 * Check for each dvr config if the free disk size is below the dvr_cleanup_threshold
 * If so and we are using the dvr config ATM (active recording), we start the cleanup procedure
 */
static void
dvr_disk_space_check()
{
  dvr_config_t *cfg;
  dvr_entry_t *de;
  struct statvfs diskdata;
  int64_t requiredBytes, maximalBytes, availBytes, usedBytes;
  int idx = 0, cleanupDone = 0;
  dvr_vfs_t *dvfs;

  pthread_mutex_lock(&global_lock);

  dvr_disk_space_config_idx++;
  if (dvr_disk_space_config_idx > dvr_disk_space_config_size)
    dvr_disk_space_config_idx = 1;

  LIST_FOREACH(cfg, &dvrconfigs, config_link) {
    idx++;

    if (cfg->dvr_enabled &&
        !cleanupDone &&
        idx >= dvr_disk_space_config_idx &&
        statvfs(cfg->dvr_storage, &diskdata) != -1)
    {
      dvfs = dvr_vfs_find(NULL, tvh_fsid(diskdata.f_fsid));

      availBytes = diskdata.f_frsize * (int64_t)diskdata.f_bavail;
      usedBytes = dvfs->used_size;
      requiredBytes = MIB(cfg->dvr_cleanup_threshold_free);
      maximalBytes = MIB(cfg->dvr_cleanup_threshold_used);

      if (availBytes < requiredBytes || ((maximalBytes < usedBytes) && cfg->dvr_cleanup_threshold_used)) {
        LIST_FOREACH(de, &dvrentries, de_global_link) {

          /* only start cleanup if we are actually writing files right now */
          if (de->de_sched_state != DVR_RECORDING || !de->de_config || de->de_config != cfg)
            continue;

          if (availBytes < requiredBytes) {
            tvhwarn(LS_DVR,"running out of free disk space for dvr config \"%s\", required free space \"%"PRId64" MiB\", current free space \"%"PRId64" MiB\"",
                    cfg != dvr_config_find_by_name(NULL) ? cfg->dvr_config_name : "Default profile",
                    TOMIB(requiredBytes), TOMIB(availBytes));
          } else {
            tvhwarn(LS_DVR,"running out of used disk space for dvr config \"%s\", required used space \"%"PRId64" MiB\", current used space \"%"PRId64" MiB\"",
                    cfg != dvr_config_find_by_name(NULL) ? cfg->dvr_config_name : "Default profile",
                    TOMIB(maximalBytes), TOMIB(usedBytes));
          }

          /* only cleanup one directory at the time as the system needs time to delete the actual files */
          dvr_disk_space_cleanup(de->de_config, 1);
          cleanupDone = 1;
          dvr_disk_space_config_idx = idx;
          break;
        }
        if (!cleanupDone)
          goto checking;
      } else {
checking:
        tvhtrace(LS_DVR, "checking free and used disk space for config \"%s\" : OK",
               cfg != dvr_config_find_by_name(NULL) ? cfg->dvr_config_name : "Default profile");
      }
    }
  }

  if (!cleanupDone)
    dvr_disk_space_config_idx = 0;

  dvr_disk_space_config_size = idx;

  pthread_mutex_unlock(&global_lock);
}

/**
 *
 */
static void
dvr_get_disk_space_update(const char *path, int locked)
{
  struct statvfs diskdata;
  dvr_vfs_t *dvfs;

  if(statvfs(path, &diskdata) == -1)
    return;

  if (!locked)
    pthread_mutex_lock(&global_lock);
  dvfs = dvr_vfs_find(NULL, tvh_fsid(diskdata.f_fsid));
  if (!locked)
    pthread_mutex_unlock(&global_lock);

  pthread_mutex_lock(&dvr_disk_space_mutex);
  dvr_bfree = diskdata.f_frsize * (int64_t)diskdata.f_bavail;
  dvr_btotal = diskdata.f_frsize * (int64_t)diskdata.f_blocks;
  dvr_bused = dvfs ? dvfs->used_size : 0;
  pthread_mutex_unlock(&dvr_disk_space_mutex);
}

/**
 *
 */
static void
dvr_get_disk_space_tcb(void *opaque, int dearmed)
{
  if (!dearmed) {
    htsmsg_t *m = htsmsg_create_map();

    /* update disk space from default dvr config */
    dvr_get_disk_space_update((char *)opaque, 0);
    htsmsg_add_s64(m, "freediskspace", dvr_bfree);
    htsmsg_add_s64(m, "useddiskspace", dvr_bused);
    htsmsg_add_s64(m, "totaldiskspace", dvr_btotal);
    notify_by_msg("diskspaceUpdate", m, 0);

    /* check free disk space for each dvr config and start cleanup if needed */
    dvr_disk_space_check();
  }

  free(opaque);
}

static void
dvr_get_disk_space_cb(void *aux)
{
  dvr_config_t *cfg;
  char *path;

  lock_assert(&global_lock);

  cfg = dvr_config_find_by_name_default(NULL);
  if (cfg->dvr_storage && cfg->dvr_storage[0]) {
    path = strdup(cfg->dvr_storage);
    tasklet_arm(&dvr_disk_space_tasklet, dvr_get_disk_space_tcb, path);
  }
  mtimer_arm_rel(&dvr_disk_space_timer, dvr_get_disk_space_cb, NULL, sec2mono(15));
}

/**
 * Returns the available disk space for a new recording.
 * If '0' (= below configured minimum), a new recording should not be started.
 */
int64_t
dvr_vfs_rec_start_check(dvr_config_t *cfg)
{
  struct statvfs diskdata;
  dvr_vfs_t *dvfs;
  int64_t availBytes, requiredBytes, usedBytes, maximalBytes, cleanedBytes;

  lock_assert(&global_lock);
  if (!cfg || !cfg->dvr_enabled || statvfs(cfg->dvr_storage, &diskdata) == -1)
    return 0;
  availBytes    = diskdata.f_frsize * (int64_t)diskdata.f_bavail;
  requiredBytes = MIB(cfg->dvr_cleanup_threshold_free);
  maximalBytes  = MIB(cfg->dvr_cleanup_threshold_used);
  dvfs          = dvr_vfs_find(NULL, tvh_fsid(diskdata.f_fsid));
  usedBytes     = dvfs->used_size;

  if (availBytes < requiredBytes || ((maximalBytes < usedBytes) && cfg->dvr_cleanup_threshold_used)) {
    /* Not enough space to start recording, check if cleanup helps */
    cleanedBytes = dvr_disk_space_cleanup(cfg, 0);
    availBytes += cleanedBytes;
    usedBytes -= cleanedBytes;
    if (availBytes < requiredBytes || ((maximalBytes < usedBytes) && cfg->dvr_cleanup_threshold_used))
      return 0;
  }
  return availBytes;
}

/**
 *
 */
void
dvr_disk_space_boot(void)
{
  LIST_INIT(&dvrvfs_list);
}

/**
 *
 */
void
dvr_disk_space_init(void)
{
  dvr_config_t *cfg = dvr_config_find_by_name_default(NULL);
  pthread_mutex_init(&dvr_disk_space_mutex, NULL);
  dvr_get_disk_space_update(cfg->dvr_storage, 1);
  mtimer_arm_rel(&dvr_disk_space_timer, dvr_get_disk_space_cb, NULL, sec2mono(5));
}

/**
 *
 */
void
dvr_disk_space_done(void)
{
  dvr_vfs_t *vfs;

  tasklet_disarm(&dvr_disk_space_tasklet);
  mtimer_disarm(&dvr_disk_space_timer);
  while ((vfs = LIST_FIRST(&dvrvfs_list)) != NULL) {
    LIST_REMOVE(vfs, link);
    free(vfs);
  }
}

/**
 *
 */
int
dvr_get_disk_space(int64_t *bfree, int64_t *bused, int64_t *btotal)
{
  int res = 0;

  pthread_mutex_lock(&dvr_disk_space_mutex);
  if (dvr_bfree || dvr_btotal) {
    *bfree = dvr_bfree;
    *bused = dvr_bused;
    *btotal = dvr_btotal;
  } else {
    res = -EINVAL;
  }
  pthread_mutex_unlock(&dvr_disk_space_mutex);
  return res;
}
