/*
 *  Tvheadend - Linux DVB input system
 *
 *  Copyright (C) 2013 Adam Sutton
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
#include "input.h"
#include "linuxdvb_private.h"
#include "queue.h"
#include "fsmonitor.h"
#include "settings.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <openssl/sha.h>

#include <linux/dvb/frontend.h>

#define FE_PATH  "%s/frontend%d"
#define DVR_PATH "%s/dvr%d"
#define DMX_PATH "%s/demux%d"

/* ***************************************************************************
 * DVB Adapter
 * **************************************************************************/

static void
linuxdvb_adapter_class_save ( idnode_t *in )
{
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  linuxdvb_adapter_save(la);
}

static idnode_set_t *
linuxdvb_adapter_class_get_childs ( idnode_t *in )
{
  linuxdvb_frontend_t *lfe;
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  idnode_set_t *is = idnode_set_create();
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
    idnode_set_add(is, &lfe->ti_id, NULL);
  return is;
}

static const char *
linuxdvb_adapter_class_get_title ( idnode_t *in )
{
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  return la->la_name ?: la->la_rootpath;
}

const idclass_t linuxdvb_adapter_class =
{
  .ic_class      = "linuxdvb_adapter",
  .ic_caption    = "LinuxDVB Adapter",
  .ic_save       = linuxdvb_adapter_class_save,
  .ic_get_childs = linuxdvb_adapter_class_get_childs,
  .ic_get_title  = linuxdvb_adapter_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "rootpath",
      .name     = "Device Path",
      .opts     = PO_RDONLY,
      .off      = offsetof(linuxdvb_adapter_t, la_rootpath),
    },
    {}
  }
};

/*
 * Save data
 */
void
linuxdvb_adapter_save ( linuxdvb_adapter_t *la )
{
  htsmsg_t *m, *l;
  linuxdvb_frontend_t *lfe;

  m = htsmsg_create_map();
  idnode_save(&la->th_id, m);

  /* Frontends */
  l = htsmsg_create_map();
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
    linuxdvb_frontend_save(lfe, l);
  htsmsg_add_msg(m, "frontends", l);

  /* Save */
  hts_settings_save(m, "input/linuxdvb/adapters/%s",
                    idnode_uuid_as_str(&la->th_id));
  htsmsg_destroy(m);
}

/*
 * Check if enabled
 */
static int
linuxdvb_adapter_is_enabled ( linuxdvb_adapter_t *la )
{
  linuxdvb_frontend_t *lfe;
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link) {
    if (lfe->mi_is_enabled((mpegts_input_t*)lfe))
      return 1;
  }
  return 0;
}

/*
 * Create
 */
static linuxdvb_adapter_t *
linuxdvb_adapter_create
  ( const char *uuid, htsmsg_t *conf,
    const char *path, int number, struct dvb_frontend_info *dfi )
{
  linuxdvb_adapter_t *la;
  char buf[1024];

  /* Create */
  la = calloc(1, sizeof(linuxdvb_adapter_t));
  if (!tvh_hardware_create0((tvh_hardware_t*)la, &linuxdvb_adapter_class,
                            uuid, conf)) {
    free(la);
    return NULL;
  }

  /* Setup */
  sprintf(buf, "%s [%s]", path, dfi->name);
  free(la->la_rootpath);
  la->la_rootpath   = strdup(path);
  la->la_name       = strdup(buf);
  la->la_dvb_number = number;

  /* Callbacks */
  la->la_is_enabled = linuxdvb_adapter_is_enabled;

  return la;
}

/*
 *
 */
static dvb_fe_type_t
linux_dvb_get_type(int linux_type)
{
  switch (linux_type) {
  case FE_QPSK:
    return DVB_TYPE_S;
  case FE_QAM:
    return DVB_TYPE_C;
  case FE_OFDM:
    return DVB_TYPE_T;
  case FE_ATSC:
    return DVB_TYPE_ATSC;
  default:
    return DVB_TYPE_NONE;
  }
}

/*
 * Add adapter by path
 */
static void
linuxdvb_adapter_add ( const char *path )
{
  extern int linuxdvb_adapter_mask;
  int a, i, j, r, fd;
  char fe_path[512], dmx_path[512], dvr_path[512], uuid[UUID_STR_LEN];
  linuxdvb_adapter_t *la = NULL;
  struct dvb_frontend_info dfi;
  SHA_CTX sha1;
  uint8_t uuidbin[20];
  htsmsg_t *conf, *feconf;
  int save = 0;
  dvb_fe_type_t type;
#if DVB_VER_ATLEAST(5,10)
  dvb_fe_type_t fetypes[DVB_TYPE_LAST+1] = { 0 };
  struct dtv_property   cmd = {
    .cmd = DTV_ENUM_DELSYS
  };
  struct dtv_properties cmdseq = {
    .num   = 1,
    .props = &cmd
  };
#endif

  /* Validate the path */
  if (sscanf(path, "/dev/dvb/adapter%d", &a) != 1)
    return;

  if (a >= 0 && a < 32 && (linuxdvb_adapter_mask & (1 << a)) == 0)
    return;

  /* Note: some of the below can take a while, so we relinquish the lock
   *       to stop us blocking everyhing else
   */
  pthread_mutex_unlock(&global_lock);

  /* Process each frontend */
  for (i = 0; i < 32; i++) {
    conf = feconf = NULL;

    snprintf(fe_path, sizeof(fe_path), FE_PATH, path, i);

    /* Wait for access (first FE can take a fe ms to be setup) */
    if (!i) {
      for (j = 0; j < 10; j++) {
        if (!access(fe_path, R_OK | W_OK)) break;
        usleep(100000);
      }
    }
    if (access(fe_path, R_OK | W_OK)) continue;

    /* Get frontend info */
    for (j = 0; j < 10; j++) {
      if ((fd = tvh_open(fe_path, O_RDWR, 0)) > 0) break;
      usleep(100000);
    }
    if (fd == -1) {
      tvhlog(LOG_ERR, "linuxdvb", "unable to open %s", fe_path);
      continue;
    }
#if DVB_VER_ATLEAST(5,10)
    r = ioctl(fd, FE_GET_PROPERTY, &cmdseq);
    if (!r && cmd.u.buffer.len) {
      struct dtv_property fecmd[2] = {
        {
          .cmd    = DTV_DELIVERY_SYSTEM,
          .u.data = cmd.u.buffer.data[0]
        },
        {
          .cmd    = DTV_TUNE
        }
      };
      cmdseq.props = fecmd;
      cmdseq.num   = 2;
      r = ioctl(fd, FE_SET_PROPERTY, &cmdseq);
    } else {
      cmd.u.buffer.len = 0;
    }
#endif
    r = ioctl(fd, FE_GET_INFO, &dfi);
    close(fd);
    if(r) {
      tvhlog(LOG_ERR, "linuxdvb", "unable to query %s", fe_path);
      continue;
    }
    type = linux_dvb_get_type(dfi.type);
    if (type == DVB_TYPE_NONE) {
      tvhlog(LOG_ERR, "linuxdvb", "unable to determine FE type %s - %i", fe_path, dfi.type);
      continue;
    }

    /* DVR/DMX (bit of a guess) */
    snprintf(dmx_path, sizeof(dmx_path), DMX_PATH, path, i);
    if (access(dmx_path, R_OK | W_OK)) {
      snprintf(dmx_path, sizeof(dmx_path), DMX_PATH, path, 0);
      if (access(dmx_path, R_OK | W_OK)) continue;
    }

    snprintf(dvr_path, sizeof(dvr_path), DVR_PATH, path, i);
    if (access(dvr_path, R_OK | W_OK)) {
      snprintf(dvr_path, sizeof(dvr_path), DVR_PATH, path, 0);
      if (access(dvr_path, R_OK | W_OK)) continue;
    }

    /* Create/Find adapter */
    pthread_mutex_lock(&global_lock);
    if (!la) {

      /* Create hash for adapter */
      SHA1_Init(&sha1); 
      SHA1_Update(&sha1, (void*)path,     strlen(path));
      SHA1_Update(&sha1, (void*)dfi.name, strlen(dfi.name));
      SHA1_Final(uuidbin, &sha1);
      idnode_uuid_as_str1(uuidbin, sizeof(uuidbin), uuid);

      /* Load config */
      conf = hts_settings_load("input/linuxdvb/adapters/%s", uuid);
      if (conf)
        feconf = htsmsg_get_map(conf, "frontends");
      else
        save = 1;
  
      /* Create */
      if (!(la = linuxdvb_adapter_create(uuid, conf, path, a, &dfi))) {
        tvhlog(LOG_ERR, "linuxdvb", "failed to create %s", path);
        htsmsg_destroy(conf);
        return; // Note: save to return here as global_lock is held
      }
    }

    /* Create frontend */
    linuxdvb_frontend_create(feconf, la, i, fe_path, dmx_path, dvr_path, type, dfi.name);
#if DVB_VER_ATLEAST(5,10)
    fetypes[type] = 1;
    for (j = 0; j < cmd.u.buffer.len; j++) {

      /* Invalid */
      if ((type = dvb_delsys2type(cmd.u.buffer.data[j])) == DVB_TYPE_NONE)
        continue;

      /* Couldn't find */
      if (fetypes[type])
        continue;

      /* Create */
      linuxdvb_frontend_create(feconf, la, i, fe_path, dmx_path, dvr_path,
                               type, dfi.name);
      fetypes[type] = 1;
    }
#endif
    pthread_mutex_unlock(&global_lock);
    htsmsg_destroy(conf);
  }

  /* Relock before exit */
  pthread_mutex_lock(&global_lock);

  /* Save configuration */
  if (save && la)
    linuxdvb_adapter_save(la);
}

static void
linuxdvb_adapter_del ( const char *path )
{
  int a;
  linuxdvb_frontend_t *lfe, *next;
  linuxdvb_adapter_t *la;
  tvh_hardware_t *th;

  if (sscanf(path, "/dev/dvb/adapter%d", &a) == 1) {
    LIST_FOREACH(th, &tvh_hardware, th_link)
      if (idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) {
        la = (linuxdvb_adapter_t*)th;
        if (!strcmp(path, la->la_rootpath))
          break;
      }
    if (!th) return;
  
    /* Delete the frontends */
    for (lfe = LIST_FIRST(&la->la_frontends); lfe != NULL; lfe = next) {
      next = LIST_NEXT(lfe, lfe_link);
      linuxdvb_frontend_delete(lfe);
    }

    /* Free memory */
    free(la->la_rootpath);
    free(la->la_name);
    
    /* Delete */
    tvh_hardware_delete((tvh_hardware_t*)la);

    free(la);
  }
}

/* **************************************************************************
 * Adapter Management
 * *************************************************************************/

/*
 * Scan for adapters
 */
static void
linuxdvb_adapter_scan ( void )
{
  DIR *dir;
  struct dirent buf, *de;
  char path[1024];

  if ((dir = opendir("/dev/dvb"))) {
    while (!readdir_r(dir, &buf, &de) && de) {
      if (de->d_name[0] == '.') continue;
      snprintf(path, sizeof(path), "/dev/dvb/%s", de->d_name);
      linuxdvb_adapter_add(path);
    }
    closedir(dir);
  }
}

/*
 * /dev/dvb monitor
 */
static void
devdvb_create ( fsmonitor_t *fsm, const char *path )
{
  linuxdvb_adapter_add(path);
}

static void
devdvb_delete ( fsmonitor_t *fsm, const char *path )
{
  linuxdvb_adapter_del(path);
}

static fsmonitor_t devdvbmon = {
  .fsm_create = devdvb_create,
  .fsm_delete = devdvb_delete
};

/*
 * /dev monitor
 */

static void
dev_create ( fsmonitor_t *fsm, const char *path )
{
  if (!strcmp(path, "/dev/dvb")) {
    fsmonitor_add("/dev/dvb", &devdvbmon);
    linuxdvb_adapter_scan();
  }
}

static void
dev_delete ( fsmonitor_t *fsm, const char *path )
{
  if (!strcmp(path, "/dev/dvb"))
    fsmonitor_del("/dev/dvb", &devdvbmon);
}

static fsmonitor_t devmon = { 
  .fsm_create = dev_create,
  .fsm_delete = dev_delete
};

/*
 * Initialise
 */

void
linuxdvb_adapter_init ( void )
{
  /* Install monitor on /dev */
  (void)fsmonitor_add("/dev", &devmon);

  /* Install monitor on /dev/dvb */
  if (!access("/dev/dvb", R_OK)) {
    (void)fsmonitor_add("/dev/dvb", &devdvbmon);

    /* Scan for adapters */
    linuxdvb_adapter_scan();
  }
}

void
linuxdvb_adapter_done ( void )
{
  linuxdvb_adapter_t *la;
  tvh_hardware_t *th, *n;

  pthread_mutex_lock(&global_lock);
  fsmonitor_del("/dev/dvb", &devdvbmon);
  fsmonitor_del("/dev", &devmon);
  for (th = LIST_FIRST(&tvh_hardware); th != NULL; th = n) {
    n = LIST_NEXT(th, th_link);
    if (idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) {
      la = (linuxdvb_adapter_t*)th;
      linuxdvb_adapter_del(la->la_rootpath);
    }
  }
  pthread_mutex_unlock(&global_lock);
}
