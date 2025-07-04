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
#include <linux/dvb/ca.h>

#define FE_PATH  "%s/frontend%d"
#define CA_PATH  "%s/ca%d"
#define DVR_PATH "%s/dvr%d"
#define DMX_PATH "%s/demux%d"
#define CI_PATH  "%s/ci%d"
#define SEC_PATH "%s/sec%d"

#define MAX_DEV_OPEN_ATTEMPTS 50

/* ***************************************************************************
 * DVB Adapter
 * **************************************************************************/

static htsmsg_t *
linuxdvb_adapter_class_save ( idnode_t *in, char *filename, size_t fsize )
{
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  htsmsg_t *m, *l;
  linuxdvb_frontend_t *lfe;
#if ENABLE_LINUXDVB_CA
  linuxdvb_transport_t *lcat;
#endif
  char ubuf[UUID_HEX_SIZE];

  m = htsmsg_create_map();
  idnode_save(&la->th_id, m);

  if (filename) {
    /* Frontends */
    l = htsmsg_create_map();
    LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
      linuxdvb_frontend_save(lfe, l);
    htsmsg_add_msg(m, "frontends", l);

    /* CAs */
  #if ENABLE_LINUXDVB_CA
    l = htsmsg_create_map();
    LIST_FOREACH(lcat, &la->la_ca_transports, lcat_link)
      linuxdvb_transport_save(lcat, l);
    htsmsg_add_msg(m, "ca_devices", l);
  #endif

    /* Filename */
    snprintf(filename, fsize, "input/linuxdvb/adapters/%s",
             idnode_uuid_as_str(&la->th_id, ubuf));
  }
  return m;
}

static idnode_set_t *
linuxdvb_adapter_class_get_childs ( idnode_t *in )
{
  linuxdvb_frontend_t *lfe;
#if ENABLE_LINUXDVB_CA
  linuxdvb_transport_t *lcat;
  linuxdvb_ca_t *lca;
#endif
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  idnode_set_t *is = idnode_set_create(0);
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
    idnode_set_add(is, &lfe->ti_id, NULL, NULL);
#if ENABLE_LINUXDVB_CA
  LIST_FOREACH(lcat, &la->la_ca_transports, lcat_link)
    LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
      idnode_set_add(is, &lca->lca_id, NULL, NULL);
#endif
  return is;
}

static void
linuxdvb_adapter_class_get_title
  ( idnode_t *in, const char *lang, char *dst, size_t dstsize )
{
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)in;
  snprintf(dst, dstsize, "%s", la->la_name ?: la->la_rootpath);
}

static const void *
linuxdvb_adapter_class_active_get ( void *obj )
{
  static int active;
#if ENABLE_LINUXDVB_CA
  linuxdvb_transport_t *lcat;
  linuxdvb_ca_t *lca;
#endif
  linuxdvb_adapter_t *la = (linuxdvb_adapter_t*)obj;
  active = la->la_is_enabled(la);
#if ENABLE_LINUXDVB_CA
  if (!active) {
    LIST_FOREACH(lcat, &la->la_ca_transports, lcat_link)
      LIST_FOREACH(lca, &lcat->lcat_slots, lca_link)
        if (lca->lca_enabled) {
          active = 1;
          goto caok;
        }
  }
caok:
#endif
  return &active;
}

const idclass_t linuxdvb_adapter_class =
{
  .ic_class      = "linuxdvb_adapter",
  .ic_caption    = N_("LinuxDVB adapter"),
  .ic_event      = "linuxdvb_adapter",
  .ic_save       = linuxdvb_adapter_class_save,
  .ic_get_childs = linuxdvb_adapter_class_get_childs,
  .ic_get_title  = linuxdvb_adapter_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "active",
      .name     = N_("Active"),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_NOUI,
      .get	= linuxdvb_adapter_class_active_get,
    },
    {
      .type     = PT_STR,
      .id       = "rootpath",
      .name     = N_("Device path"),
      .desc     = N_("Path used by the device."),
      .opts     = PO_RDONLY,
      .off      = offsetof(linuxdvb_adapter_t, la_rootpath),
    },
    {}
  }
};

/*
 * Check if enabled
 */
static int
linuxdvb_adapter_is_enabled ( linuxdvb_adapter_t *la )
{
  linuxdvb_frontend_t *lfe;
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link) {
    if (lfe->mi_is_enabled((mpegts_input_t*)lfe, NULL, 0, -1) != MI_IS_ENABLED_NEVER)
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
    const char *path, int number, const char *name )
{
  linuxdvb_adapter_t *la;
  char buf[1024];

  /* Create */
  la = calloc(1, sizeof(linuxdvb_adapter_t));
  if (!tvh_hardware_create0((tvh_hardware_t*)la, &linuxdvb_adapter_class,
                            uuid, conf)) {
    /* Note: la variable is freed in tvh_hardware_create0() */
    return NULL;
  }

  /* Setup */
  sprintf(buf, "%s [%s]", path, name);
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
static linuxdvb_adapter_t *
linuxdvb_adapter_new(const char *path, int a, const char *name,
                     const char *display_name, htsmsg_t **conf, int *save)
{
  linuxdvb_adapter_t *la;
  SHA_CTX sha1;
  uint8_t uuidbin[20];
  char uhex[UUID_HEX_SIZE];

  /* Create hash for adapter */
  SHA1_Init(&sha1);
  SHA1_Update(&sha1, (void*)path,     strlen(path));
  SHA1_Update(&sha1, (void*)name,     strlen(name));
  SHA1_Final(uuidbin, &sha1);

  bin2hex(uhex, sizeof(uhex), uuidbin, sizeof(uuidbin));

  /* Load config */
  *conf = hts_settings_load("input/linuxdvb/adapters/%s", uhex);
  if (*conf == NULL)
    *save = 1;

  /* Create */
  if (!(la = linuxdvb_adapter_create(uhex, *conf, path, a, display_name))) {
    htsmsg_destroy(*conf);
    *conf = NULL;
    return NULL;
  }

  tvhinfo(LS_LINUXDVB, "adapter added %s", path);
  return la;
}

/*
 *
 */
static int force_dvbs;

static dvb_fe_type_t
linuxdvb_get_type(int linux_type)
{
  /* useful for debugging with DVB-T */
  if (force_dvbs)
    return DVB_TYPE_S;

  switch (linux_type) {
  case FE_QPSK:
    return DVB_TYPE_S;
  case FE_QAM:
    return DVB_TYPE_C;
  case FE_OFDM:
    return DVB_TYPE_T;
  case FE_ATSC:
    return DVB_TYPE_ATSC_T;
  default:
    return DVB_TYPE_NONE;
  }
}

/*
 *
 */
#if DVB_VER_ATLEAST(5,5)
static void
linuxdvb_get_systems(int fd, struct dtv_property *_cmd)
{
  struct dtv_property cmd = {
    .cmd = DTV_ENUM_DELSYS
  };
  struct dtv_properties cmdseq = {
    .num   = 1,
    .props = &cmd
  };
  int r;

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
  *_cmd = cmd;
}
#endif

#if ENABLE_DDCI
/* ret:  0 .. DDCI found and usable
 *      -1 .. DDCI found but not usable
 *      -2 .. DDCI not found
 */
static int
linuxdvb_check_ddci ( const char *ci_path )
{
  int j, fd, ret = -2;

  tvhtrace(LS_DDCI, "checking for DDCI %s", ci_path);

  /* check existence */
  if (!access(ci_path, R_OK | W_OK)) {
    for (j = 0; j < MAX_DEV_OPEN_ATTEMPTS; j++) {
      if ((fd = tvh_open(ci_path, O_WRONLY, 0)) >= 0) break;
      tvh_safe_usleep(100000);
    }
    if (fd >= 0) {
      close(fd);
      tvhinfo(LS_DDCI, "DDCI found %s", ci_path);
      ret = 0;
    }
    else {
      ret = -1;
      tvherror(LS_DDCI, "unable to open %s", ci_path);
    }
  }
  return ret;
}
#endif /* ENABLE_DDCI */

/*
 * Add adapter by path
 */
static void
linuxdvb_adapter_add ( const char *path )
{
  extern int linuxdvb_adapter_mask;
  int a, i, j, r, fd;
  char fe_path[512], dmx_path[512], dvr_path[512], name[132];
#if ENABLE_LINUXDVB_CA
  linuxdvb_transport_t *lcat;
  char ca_path[512];
  htsmsg_t *caconf = NULL;
  const char *ci_found = NULL;
#if ENABLE_DDCI
  linuxdvb_adapter_t *la_fe = NULL;
  char ci_path[512];
#endif
#endif
  linuxdvb_adapter_t *la = NULL;
  struct dvb_frontend_info dfi;
  ca_caps_t cac;
  htsmsg_t *conf = NULL, *feconf = NULL;
  int save = 0;
  dvb_fe_type_t type;
#if DVB_VER_ATLEAST(5,5)
  int delsys, fenum, type5;
  dvb_fe_type_t fetypes[DVB_TYPE_LAST+1];
  struct dtv_property cmd;
#endif

  tvhtrace(LS_LINUXDVB, "scanning adapter %s", path);

  /* Validate the path */
  if (sscanf(path, "/dev/dvb/adapter%d", &a) != 1)
    return;

  if (a >= 0 && a < 32 && (linuxdvb_adapter_mask & (1 << a)) == 0)
    return;

  /* Note: some of the below can take a while, so we relinquish the lock
   *       to stop us blocking everyhing else
   */
  tvh_mutex_unlock(&global_lock);

  /* Process each frontend */
  for (i = 0; i < 32; i++) {
    snprintf(fe_path, sizeof(fe_path), FE_PATH, path, i);

    /* Wait for access (first FE can take a fe ms to be setup) */
    if (!i) {
      for (j = 0; j < MAX_DEV_OPEN_ATTEMPTS; j++) {
        if (!access(fe_path, R_OK | W_OK)) break;
        tvh_safe_usleep(100000);
      }
    }
    if (access(fe_path, R_OK | W_OK)) continue;

    /* Get frontend info */
    for (j = 0; j < MAX_DEV_OPEN_ATTEMPTS; j++) {
      if ((fd = tvh_open(fe_path, O_RDWR, 0)) >= 0) break;
      tvh_safe_usleep(100000);
    }
    if (fd < 0) {
      tvherror(LS_LINUXDVB, "unable to open %s", fe_path);
      continue;
    }
#if DVB_VER_ATLEAST(5,5)
    linuxdvb_get_systems(fd, &cmd);
#endif
    r = ioctl(fd, FE_GET_INFO, &dfi);
    close(fd);
    if(r) {
      tvherror(LS_LINUXDVB, "unable to query %s", fe_path);
      continue;
    }
    snprintf(name, sizeof(name), "%s #%d", dfi.name, a);
    type = linuxdvb_get_type(dfi.type);
    if (type == DVB_TYPE_NONE) {
      tvherror(LS_LINUXDVB, "unable to determine FE type %s - %i", fe_path, dfi.type);
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
    tvh_mutex_lock(&global_lock);
    if (!la) {
      la = linuxdvb_adapter_new(path, a, dfi.name, name, &conf, &save);
      if (la == NULL) {
        tvherror(LS_LINUXDVB, "failed to create %s", path);
        return; // Note: save to return here as global_lock is held
      }
      if (conf)
        feconf = htsmsg_get_map(conf, "frontends");
    }

    /* Create frontend */
#if DVB_VER_ATLEAST(5,5)
    memset(fetypes, 0, sizeof(fetypes));
    for (j = fenum = 0; j < cmd.u.buffer.len; j++) {
      delsys = cmd.u.buffer.data[j];

      if ((delsys = linuxdvb2tvh_delsys(delsys)) == DVB_SYS_NONE)
        continue;

      if (force_dvbs)
        delsys = DVB_SYS_DVBS;

      /* Invalid */
      if ((type5 = dvb_delsys2type(NULL, delsys)) == DVB_TYPE_NONE)
        continue;

      /* Couldn't find */
      if (fetypes[type5])
        continue;

      /* Create */
      linuxdvb_frontend_create(feconf, la, i, fe_path, dmx_path, dvr_path,
                               type5, name);
      fetypes[type5] = 1;
      fenum++;
    }
    if (fenum == 0)
      linuxdvb_frontend_create(feconf, la, i, fe_path, dmx_path, dvr_path,
                               type, name);
#else
    linuxdvb_frontend_create(feconf, la, i, fe_path, dmx_path, dvr_path, type, name);
#endif
    tvh_mutex_unlock(&global_lock);
  }

  /* Process each CA device */
#if ENABLE_LINUXDVB_CA
  /* A normal DVB card with hard wired CI interface creates the caX device in
   * the same adapter directory than the frontendX device.
   * The Digital Device CI interfaces do the same, when the driver is started
   * with adapter_alloc=3. This parameter is used together with the redirect
   * feature of the DD CI to inform user mode applications, that the caX device
   * is hard wired to the DVB tuner. This means the special DDCI feature must
   * not be activated, when the caX and the frontedX device are present in the
   * same adapter directory.
   *
   * The normal use case for the DD CI is a stand alone CI interface, which can
   * be used by any tuner in the system, which is not limited to Digital Devices
   * hardware.
   * This is the default mode of the driver or started with adapter_alloc=0
   * parameter. In this mode the caX device is created in a dedicated adapter
   * directory.
   *
   * In both modes also a secX or ciX device is create additionally. In the
   * first mode (adapter_alloc=3 and redirect) this shall be ignored. In the
   * second mode it needs to be associated with the caX device and later on
   * used to send/receive the crypted/decrypted TS stream to/from the CAM.
   *
   */

#if ENABLE_DDCI
  /* remember, if la exists already, which means DD CI is hard wired to the
   * tuner
   */
  la_fe = la;
#endif

  for (i = 0; i < 32; i++) {
    snprintf(ca_path, sizeof(ca_path), CA_PATH, path, i);
    if (access(ca_path, R_OK | W_OK)) continue;

    /* Get ca info */
    for (j = 0; j < MAX_DEV_OPEN_ATTEMPTS; j++) {
      if ((fd = tvh_open(ca_path, O_RDWR, 0)) >= 0) break;
      tvh_safe_usleep(100000);
    }
    if (fd < 0) {
      tvherror(LS_LINUXDVB, "unable to open %s", ca_path);
      continue;
    }
    memset(&cac, 0, sizeof(cac));
    r = ioctl(fd, CA_RESET, NULL);
    if (r == 0)
      r = ioctl(fd, CA_GET_CAP, &cac);
    close(fd);
    if(r) {
      tvherror(LS_LINUXDVB, "unable to query %s", ca_path);
      continue;
    }
    if(cac.slot_num == 0) {
      tvherror(LS_LINUXDVB, "CAM %s has no slots", ca_path);
      continue;
    }
    if((cac.slot_type & (CA_CI_LINK|CA_CI)) == 0) {
      tvherror(LS_LINUXDVB, "unsupported CAM type %08x", cac.slot_type);
      continue;
    }

#if ENABLE_DDCI
    /* check for DD CI only, if no frontend was found (stand alone mode) */
    if (!la_fe) {
      int ddci_ret;

      /* DD driver uses ciX */
      snprintf(ci_path, sizeof(ci_path), CI_PATH, path, i);
      ddci_ret = linuxdvb_check_ddci ( ci_path );
      if (ddci_ret == -2 ) {
        /* Mainline driver uses secX */
        snprintf(ci_path, sizeof(ci_path), SEC_PATH, path, i);
        ddci_ret = linuxdvb_check_ddci ( ci_path );
      }

      /* The DD CI device have not been found, or was not usable, so we
       * ignore the whole caX device also, because we are in DD CI stand alone
       * mode and this requires a working ciX/secX device.
       * It would be possible to check for -1 so that it get ignored only in
       * case of an open error.
       */
      if (ddci_ret) {
        tvherror(LS_LINUXDVB, "ignoring DDCI %s", ca_path);
        continue;
      }
      ci_found = ci_path;
    }
#endif /* ENABLE_DDCI */

    tvh_mutex_lock(&global_lock);

    if (!la) {
      snprintf(name, sizeof(name), "CAM #%d", i);
      la = linuxdvb_adapter_new(path, a, ca_path, name, &conf, &save);
      if (la == NULL) {
        tvherror(LS_LINUXDVB, "failed to create %s", path);
        continue;
      }
    }

    if (conf)
      caconf = htsmsg_get_map(conf, "ca_devices");

    lcat = linuxdvb_transport_create(la, i, cac.slot_num, ca_path, ci_found);
    if (lcat) {
      for (j = 0; j < cac.slot_num; j++)
        linuxdvb_ca_create(caconf, lcat, j);
    }
    tvh_mutex_unlock(&global_lock);
  }
#endif /* ENABLE_LINUXDVB_CA */

  /* Cleanup */
  if (conf)
    htsmsg_destroy(conf);

  /* Relock before exit */
  tvh_mutex_lock(&global_lock);

  /* Adapter couldn't be opened; there's nothing to work with  */
  if (!la)
    return;

  /* Save configuration */
  if (save && la)
    linuxdvb_adapter_changed(la);
}

static void
linuxdvb_adapter_del ( const char *path )
{
  int a;
  linuxdvb_frontend_t *lfe, *lfe_next;
#if ENABLE_LINUXDVB_CA
  linuxdvb_transport_t *lcat, *lcat_next;
#endif
  linuxdvb_adapter_t *la = NULL;
  tvh_hardware_t *th;

  if (sscanf(path, "/dev/dvb/adapter%d", &a) == 1) {
    LIST_FOREACH(th, &tvh_hardware, th_link)
      if (idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) {
        la = (linuxdvb_adapter_t*)th;
        if (!strcmp(path, la->la_rootpath))
          break;
      }
    if (!th) return;

    idnode_save_check(&la->th_id, 0);
  
    /* Delete the frontends */
    for (lfe = LIST_FIRST(&la->la_frontends); lfe != NULL; lfe = lfe_next) {
      lfe_next = LIST_NEXT(lfe, lfe_link);
      linuxdvb_frontend_destroy(lfe);
    }
    
#if ENABLE_LINUXDVB_CA
    /* Delete the CA devices */
    for (lcat = LIST_FIRST(&la->la_ca_transports); lcat != NULL; lcat = lcat_next) {
      lcat_next = LIST_NEXT(lcat, lcat_link);
      linuxdvb_transport_destroy(lcat);
    }
#endif

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
  tvhinfo(LS_LINUXDVB, "adapter removed %s", path);
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
  force_dvbs = getenv("TVHEADEND_DEBUG_FORCE_DVBS") != NULL;

  idclass_register(&linuxdvb_adapter_class);

  idclass_register(&linuxdvb_frontend_dvbt_class);
  idclass_register(&linuxdvb_frontend_dvbs_class);
  idclass_register(&linuxdvb_frontend_dvbs_slave_class);
  idclass_register(&linuxdvb_frontend_dvbc_class);
  idclass_register(&linuxdvb_frontend_atsc_t_class);
  idclass_register(&linuxdvb_frontend_atsc_c_class);
  idclass_register(&linuxdvb_frontend_isdb_t_class);
  idclass_register(&linuxdvb_frontend_isdb_c_class);
  idclass_register(&linuxdvb_frontend_isdb_s_class);
  idclass_register(&linuxdvb_frontend_dab_class);

  idclass_register(&linuxdvb_lnb_class);
  idclass_register(&linuxdvb_rotor_class);
  idclass_register(&linuxdvb_rotor_gotox_class);
  idclass_register(&linuxdvb_rotor_usals_class);
  idclass_register(&linuxdvb_en50494_class);
  idclass_register(&linuxdvb_switch_class);
  idclass_register(&linuxdvb_diseqc_class);

  idclass_register(&linuxdvb_satconf_class);
  idclass_register(&linuxdvb_satconf_lnbonly_class);
  idclass_register(&linuxdvb_satconf_2port_class);
  idclass_register(&linuxdvb_satconf_4port_class);
  idclass_register(&linuxdvb_satconf_en50494_class);
  idclass_register(&linuxdvb_satconf_advanced_class);
  idclass_register(&linuxdvb_satconf_ele_class);

#if ENABLE_LINUXDVB_CA
  idclass_register(&linuxdvb_ca_class);
  linuxdvb_ca_init();
#endif

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

  tvh_mutex_lock(&global_lock);
  fsmonitor_del("/dev/dvb", &devdvbmon);
  fsmonitor_del("/dev", &devmon);
  for (th = LIST_FIRST(&tvh_hardware); th != NULL; th = n) {
    n = LIST_NEXT(th, th_link);
    if (idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) {
      la = (linuxdvb_adapter_t*)th;
      linuxdvb_adapter_del(la->la_rootpath);
    }
  }
#if ENABLE_LINUXDVB_CA
  linuxdvb_ca_done();
#endif
  tvh_mutex_unlock(&global_lock);
}
