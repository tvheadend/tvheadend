/*
 *  Tvheadend - Linux DVB frontend
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
#include "linuxdvb_private.h"
#include "notify.h"
#include "atomic.h"
#include "tvhpoll.h"
#include "streaming.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h> 

#define NOSIGNAL(x) (((x) & FE_HAS_SIGNAL) == 0)

static void
linuxdvb_frontend_monitor ( void *aux );
static void *
linuxdvb_frontend_input_thread ( void *aux );

/*
 *
 */
static inline int sig_multiply(int value, int multiplier)
{
  return ((value * MAX(1, multiplier)) + 99) / 100;
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
linuxdvb_frontend_class_changed ( idnode_t *in )
{
  linuxdvb_adapter_t *la = ((linuxdvb_frontend_t*)in)->lfe_adapter;
  linuxdvb_adapter_changed(la);
}

const void *
linuxdvb_frontend_class_active_get ( void *obj )
{
  int *active;
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)obj;
  active = (int *)mpegts_input_class_active_get(obj);
  if (!(*active) && lfe->lfe_adapter->la_exclusive)
    *active = lfe->lfe_adapter->la_is_enabled(lfe->lfe_adapter);
  return active;
}

CLASS_DOC(linuxdvb_frontend)
CLASS_DOC(linuxdvb_frontend_dvbs)
CLASS_DOC(linuxdvb_frontend_dvbt) 
CLASS_DOC(linuxdvb_frontend_dvbc)

const idclass_t linuxdvb_frontend_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "linuxdvb_frontend",
  .ic_caption    = N_("Linux DVB frontend"),
  .ic_doc        = tvh_doc_linuxdvb_frontend_class,
  .ic_changed    = linuxdvb_frontend_class_changed,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "fe_path",
      .name     = N_("Frontend path"),
      .desc     = N_("Path to the frontend used by the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_fe_path),
    },
    {
      .type     = PT_STR,
      .id       = "dvr_path",
      .name     = N_("Input path"),
      .desc     = N_("The input path used by the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_dvr_path),
    },
    {
      .type     = PT_STR,
      .id       = "dmx_path",
      .name     = N_("Demux path"),
      .desc     = N_("The demux path used by the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_dmx_path),
    },
    {
      .type     = PT_INT,
      .id       = "fe_number",
      .name     = N_("Frontend number"),
      .desc     = N_("The frontend number given to the device."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_number),
    },
    {
      .type     = PT_STR,
      .id       = "sysfs_device",
      .name     = N_("Device path in sysfs"),
      .desc     = N_("The device path in sysfs filesystem (/sys)."),
      .opts     = PO_RDONLY | PO_NOSAVE | PO_MULTILINE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_sysfs),
    },
    {
      .type     = PT_INT,
      .id       = "pids_max",
      .name     = N_("Maximum PIDs"),
      .desc     = N_("The limit for the PID filter (driver or hardware)."),
      .off      = offsetof(linuxdvb_frontend_t, lfe_pids_max),
      .opts     = PO_ADVANCED,
      .def.i    = 32
    },
    {
      .type     = PT_BOOL,
      .id       = "pids_use_all",
      .name     = N_("Allow all PIDs"),
      .desc     = N_("Allow all PIDs (no filter) when the 'Maximum PIDs' limit is reached."),
      .off      = offsetof(linuxdvb_frontend_t, lfe_pids_use_all),
      .opts     = PO_ADVANCED,
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "powersave",
      .name     = N_("Power save"),
      .desc     = N_("Enable/disable power save mode (if supported by "
                     "the device)."),
      .off      = offsetof(linuxdvb_frontend_t, lfe_powersave),
    },
    {
      .type     = PT_U32,
      .id       = "tune_repeats",
      .name     = N_("# tune repeats"),
      .desc     = N_("Number of repeats for the tune requests (default is "
                     "zero - no repeats). Note: this represents the "
                     "number of repeats, not the number of requests - "
                     "so 0 means 'send once: don't repeat', 1 means "
                     "'send twice: send once, then send one repeat',"
                     " etc."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_tune_repeats),
    },
    {
      .type     = PT_U32,
      .id       = "skip_bytes",
      .name     = N_("Skip initial bytes"),
      .desc     = N_("If set, the first bytes from the MPEG-TS stream "
                     "are discarded. It may be required for some "
                     "drivers or hardware which do not flush the MPEG-TS "
                      "buffers completely after a frequency/parameter change."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_skip_bytes),
    },
    {
      .type     = PT_U32,
      .id       = "ibuf_size",
      .name     = N_("Input buffer (bytes)"),
      .desc     = N_("The number of bytes to buffer. By default, "
                     "linuxdvb's input buffer is 18800 bytes long. The "
                     "accepted range is 18800-1880000 bytes."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_ibuf_size),
    },
    {
      .type     = PT_U32,
      .id       = "status_period",
      .name     = N_("Status period (ms)"),
      .desc     = N_("By default, linuxdvb's status read period is "
                     "1000ms (one second). The accepted range is 250ms "
                     "to 8000ms. Note that for some hardware or drivers "
                     "(like USB), the status operations take too much "
                     "time and CPU. In this case, increase the default "
                     "value. For fast hardware, this value might be "
                     "decreased to make the decision of the re-tune "
                     "algorithm based on the signal status faster."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_status_period),
    },
    {
      .type     = PT_U32,
      .id       = "sig_multiplier",
      .name     = N_("Signal multiplier"),
      .desc     = N_("The signal level reported by the driver is multiplied "
                     "with this value and divided by 100."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_sig_multiplier),
      .def.u32  = 100,
    },
    {
      .type     = PT_U32,
      .id       = "snr_multiplier",
      .name     = N_("SNR multiplier"),
      .desc     = N_("The SNR level reported by the driver is multiplied "
                     "with this value and divided by 100."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_snr_multiplier),
      .def.u32  = 100,
    },
    {
      .type     = PT_BOOL,
      .id       = "old_status",
      .name     = N_("Force old status"),
      .desc     = N_("Always use the old ioctls to read the linuxdvb "
                     "status (signal strength, SNR, error counters). "
                     "Some drivers are not mature enough to provide the "
                     "correct values using the new v5 linuxdvb API."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(linuxdvb_frontend_t, lfe_old_status),
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbt_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbt",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbt_class,
  .ic_caption    = N_("TV Adapters - Linux DVB-T Frontend"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_BOOL,
      .id       = "lna",
      .name     = N_("LNA (low noise amplifier)"),
      .desc     = N_("Enable/disable LNA."),
      .off      = offsetof(linuxdvb_frontend_t, lfe_lna),
    },
    {}
  }
};

static idnode_set_t *
linuxdvb_frontend_dvbs_class_get_childs ( idnode_t *self )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)self;
  idnode_set_t        *is  = idnode_set_create(0);
  idnode_set_add(is, &lfe->lfe_satconf->ls_id, NULL, NULL);
  return is;
}

static int
linuxdvb_frontend_dvbs_class_satconf_set ( void *self, const void *str )
{
  linuxdvb_frontend_t *lfe = self;
  htsmsg_t *conf;
  str = str ?: "";
  if (lfe->lfe_satconf) {
    if (!strcmp(str, lfe->lfe_satconf->ls_type))
      return 0;
    linuxdvb_satconf_delete(lfe->lfe_satconf, 1);
  }
  conf = htsmsg_create_map();
  htsmsg_add_str(conf, "type", str);
  lfe->lfe_satconf = linuxdvb_satconf_create(lfe, conf);
  htsmsg_destroy(conf);
  return 1;
}

static const void *
linuxdvb_frontend_dvbs_class_satconf_get ( void *self )
{
  linuxdvb_frontend_t *lfe = self;
  if (lfe->lfe_satconf)
    prop_ptr = lfe->lfe_satconf->ls_type;
  else
    prop_ptr = NULL;
  return &prop_ptr;
}

static htsmsg_t *
linuxdvb_frontend_dvbs_class_master_enum( void * self, const char *lang )
{
  linuxdvb_frontend_t *lfe = self, *lfe2;
  linuxdvb_adapter_t *la;
  tvh_hardware_t *th;
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *m = htsmsg_create_list();
  if (self == NULL) return m;
  htsmsg_t *e = htsmsg_create_key_val("", N_("This tuner"));
  htsmsg_add_msg(m, NULL, e);
  LIST_FOREACH(th, &tvh_hardware, th_link) {
    if (!idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) continue;
    la = (linuxdvb_adapter_t*)th;
    LIST_FOREACH(lfe2, &la->la_frontends, lfe_link) {
      if (lfe2 != lfe && lfe2->lfe_type == lfe->lfe_type) {
        e = htsmsg_create_key_val(idnode_uuid_as_str(&lfe2->ti_id, ubuf), lfe2->mi_name);
        htsmsg_add_msg(m, NULL, e);
      }
    }
  }
  return m;
}

const idclass_t linuxdvb_frontend_dvbs_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbs",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbs_class,
  .ic_caption    = N_("TV Adapters - Linux DVB-S Frontend (Master)"),
  .ic_get_childs = linuxdvb_frontend_dvbs_class_get_childs,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "satconf",
      .name     = N_("Satellite config"),
      .desc     = N_("The satellite configuration to use."),
      .opts     = PO_NOSAVE,
      .set      = linuxdvb_frontend_dvbs_class_satconf_set,
      .get      = linuxdvb_frontend_dvbs_class_satconf_get,
      .list     = linuxdvb_satconf_type_list,
      .def.s    = "simple"
    },
    {
      .type     = PT_STR,
      .id       = "fe_master",
      .name     = N_("Master tuner"),
      .desc     = N_("The signal from the standard universal LNB can be "
                     "split using a simple coaxial splitter "
                     "(no multiswitch) to several outputs. In this "
                     "case, the position, the polarization and low-high "
                     "band settings must be equal. If you set another "
                     "tuner as master, then this tuner will act as "
                     "a slave and Tvheadend will assure that this tuner "
                     "will not use incompatible parameters (position, "
                     "polarization, lo-hi)."),
      .list     = linuxdvb_frontend_dvbs_class_master_enum,
      .off      = offsetof(linuxdvb_frontend_t, lfe_master),
    },
    {
      .id       = "networks",
      .type     = PT_NONE,
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbs_slave_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbs_slave",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbs_class,
  .ic_caption    = N_("TV Adapters - Linux DVB-S Slave Frontend"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "fe_master",
      .name     = N_("Master tuner"),
      .desc     = N_("The signal from the standard universal LNB can be "
                     "split using a simple coaxial splitter "
                     "(no multiswitch) to several outputs. In this "
                     "case, the position, the polarization and low-high "
                     "band settings must be equal. If you set another "
                     "tuner as master, then this tuner will act as "
                     "a slave and tvheadend will assure that this tuner "
                     "will not use incompatible parameters (position, "
                     "polarization, lo-hi)."),
      .list     = linuxdvb_frontend_dvbs_class_master_enum,
      .off      = offsetof(linuxdvb_frontend_t, lfe_master),
    },
    {
      .id       = "networks",
      .type     = PT_NONE,
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbc",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbc_class,
  .ic_caption    = N_("TV Adapters - Linux DVB-C Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_atsc_t_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_atsc_t",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbt_class,
  .ic_caption    = N_("TV Adapters - Linux ATSC-T Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_atsc_c_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_atsc_c",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbc_class,
  .ic_caption    = N_("TV Adapters - Linux ATSC-C Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_isdb_t_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_isdb_t",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbt_class,
  .ic_caption    = N_("TV Adapters - Linux ISDB-T Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_isdb_c_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_isdb_c",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbc_class,
  .ic_caption    = N_("TV Adapters - Linux ISDB-C Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_isdb_s_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_isdb_s",
  .ic_doc        = tvh_doc_linuxdvb_frontend_dvbs_class,
  .ic_caption    = N_("TV Adapters - Linux ISDB-S Frontend (Master)"),
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_dab_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dab",
  .ic_caption    = N_("TV Adapters - Linux DAB Frontend"),
  .ic_properties = (const property_t[]){
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
linuxdvb_frontend_close_fd ( linuxdvb_frontend_t *lfe, const char *name )
{
  char buf[256];

  if (lfe->lfe_fe_fd <= 0)
    return;

  if (name == NULL) {
    lfe->mi_display_name((mpegts_input_t *)lfe, buf, sizeof(buf));
    name = buf;
  }

  tvhtrace(LS_LINUXDVB, "%s - closing FE %s (%d)",
           name, lfe->lfe_fe_path, lfe->lfe_fe_fd);
  close(lfe->lfe_fe_fd);
  lfe->lfe_fe_fd = -1;
  if (lfe->lfe_satconf)
    linuxdvb_satconf_reset(lfe->lfe_satconf);
}

static int
linuxdvb_frontend_open_fd ( linuxdvb_frontend_t *lfe, const char *name )
{
  linuxdvb_frontend_t *lfe2;
  const char *extra = "";

  if (lfe->lfe_fe_fd > 0)
    return 0;

  /* Share FD across frontends */
  if (lfe->lfe_adapter->la_exclusive) {
    LIST_FOREACH(lfe2, &lfe->lfe_adapter->la_frontends, lfe_link) {
      if (lfe2->lfe_fe_fd > 0) {
        lfe->lfe_fe_fd = dup(lfe2->lfe_fe_fd);
        extra = " (shared)";
        break;
      }
    }
  }

  if (lfe->lfe_fe_fd <= 0) {
    lfe->lfe_fe_fd = tvh_open(lfe->lfe_fe_path, O_RDWR | O_NONBLOCK, 0);
    extra = "";
  }

  tvhtrace(LS_LINUXDVB, "%s - opening FE %s (%d)%s",
           name, lfe->lfe_fe_path, lfe->lfe_fe_fd, extra);

  return lfe->lfe_fe_fd <= 0;
}

static void
linuxdvb_frontend_enabled_updated ( mpegts_input_t *mi )
{
  char buf[512];
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;

  mi->mi_display_name(mi, buf, sizeof(buf));

  /* Ensure disabled */
  if (!mi->mi_enabled) {
    tvhtrace(LS_LINUXDVB, "%s - disabling tuner", buf);
    linuxdvb_frontend_close_fd(lfe, buf);
    mtimer_disarm(&lfe->lfe_monitor_timer);

  /* Ensure FE opened (if not powersave) */
  } else if (!lfe->lfe_powersave && lfe->lfe_fe_fd <= 0 && lfe->lfe_fe_path) {

    linuxdvb_frontend_open_fd(lfe, buf);

  }
}

static int
linuxdvb_frontend_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  int w = 0;
  linuxdvb_adapter_t *la = ((linuxdvb_frontend_t*)mi)->lfe_adapter;
  linuxdvb_frontend_t *lfe;
  if (la->la_exclusive) {
    LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
      w = MAX(w, mpegts_input_get_weight((mpegts_input_t*)lfe, mm, flags, weight));
  } else {
    w = mpegts_input_get_weight(mi, mm, flags, weight);
  }
  return w;
}

static int
linuxdvb_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  int r = mpegts_input_get_priority(mi, mm, flags);
  if (lfe->lfe_satconf)
    r += linuxdvb_satconf_get_priority(lfe->lfe_satconf, mm);
  return r;
}

static int
linuxdvb_frontend_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  int r = 5;
  if (lfe->lfe_satconf)
    r = linuxdvb_satconf_get_grace(lfe->lfe_satconf, mm);
  return r;
}

static int
linuxdvb_frontend_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi, *lfe2;
  linuxdvb_adapter_t *la;
  tvh_hardware_t *th;
  char ubuf[UUID_HEX_SIZE];
  int w;

  if (lfe->lfe_fe_path == NULL)
    return MI_IS_ENABLED_NEVER;
  w = mpegts_input_is_enabled(mi, mm, flags, weight);
  if (w != MI_IS_ENABLED_OK)
    return w;
  if (access(lfe->lfe_fe_path, R_OK | W_OK))
    return MI_IS_ENABLED_NEVER;
  if (mm == NULL)
    return MI_IS_ENABLED_OK;
  if (lfe->lfe_in_setup)
    return MI_IS_ENABLED_RETRY;
  if (lfe->lfe_type != DVB_TYPE_S)
    goto ok;

  /* check if any "blocking" tuner is running */
  LIST_FOREACH(th, &tvh_hardware, th_link) {
    if (!idnode_is_instance(&th->th_id, &linuxdvb_adapter_class)) continue;
    la = (linuxdvb_adapter_t*)th;
    LIST_FOREACH(lfe2, &la->la_frontends, lfe_link) {
      if (lfe2 == lfe) continue;
      if (lfe2->lfe_type != DVB_TYPE_S) continue;
      if (lfe->lfe_master && !strcmp(lfe->lfe_master, idnode_uuid_as_str(&lfe2->ti_id, ubuf))) {
        if (lfe2->lfe_satconf == NULL)
          return MI_IS_ENABLED_NEVER; /* invalid master */
        if (lfe2->lfe_refcount <= 0)
          return MI_IS_ENABLED_RETRY; /* prefer master */
        if (linuxdvb_satconf_match_mux(lfe2->lfe_satconf, mm))
          goto ok;
        return MI_IS_ENABLED_RETRY;
      }
      if (lfe2->lfe_master &&
          !strcmp(lfe2->lfe_master, idnode_uuid_as_str(&lfe->ti_id, ubuf)) &&
          lfe2->lfe_refcount > 0) {
        if (lfe->lfe_satconf == NULL)
          return MI_IS_ENABLED_NEVER;
        if (linuxdvb_satconf_match_mux(lfe->lfe_satconf, mm))
          goto ok;
        return MI_IS_ENABLED_RETRY;
      }
    }
  }


ok:
  if (lfe->lfe_adapter->la_exclusive) {
    w = -1;
    LIST_FOREACH(lfe2, &lfe->lfe_adapter->la_frontends, lfe_link) {
      if (lfe2 == lfe) continue;
      w = MAX(w, mpegts_input_get_weight((mpegts_input_t *)lfe2, mm, flags, weight));
    }
    if (w >= weight)
      return MI_IS_ENABLED_RETRY;
  }

  return MI_IS_ENABLED_OK;
}

static void
linuxdvb_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  char buf1[256], buf2[256];
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi, *lfe2;

  if (lfe->lfe_master)
    assert(lfe->lfe_type == DVB_TYPE_S);

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug(LS_LINUXDVB, "%s - stopping %s", buf1, buf2);

  /* Stop thread */
  if (lfe->lfe_dvr_pipe.wr > 0) {
    tvh_write(lfe->lfe_dvr_pipe.wr, "", 1);
    tvhtrace(LS_LINUXDVB, "%s - waiting for dvr thread", buf1);
    pthread_join(lfe->lfe_dvr_thread, NULL);
    tvh_pipe_close(&lfe->lfe_dvr_pipe);
    tvhdebug(LS_LINUXDVB, "%s - stopped dvr thread", buf1);
  }

  /* Not locked */
  lfe->lfe_ready   = 0;
  lfe->lfe_locked  = 0;
  lfe->lfe_status  = 0;
  lfe->lfe_status2 = 0;

  /* Ensure it won't happen immediately */
  mtimer_arm_rel(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, sec2mono(2));

  if (lfe->lfe_satconf && lfe->lfe_refcount == 1)
    linuxdvb_satconf_post_stop_mux(lfe->lfe_satconf);

  if (lfe->lfe_master) {
    lfe2 = (linuxdvb_frontend_t *)idnode_find(lfe->lfe_master, &linuxdvb_frontend_class, NULL);
    if (lfe2->lfe_type != lfe->lfe_type)
      lfe2 = NULL;
    if (lfe2) { /* master tuner shutdown procedure */
      assert(lfe2->lfe_refcount >= 0);
      if (--lfe2->lfe_refcount == 0) {
        lfe2->lfe_ready   = 0;
        lfe2->lfe_locked  = 0;
        lfe2->lfe_status  = 0;
        lfe2->lfe_status2 = 0;
        /* Ensure it won't happen immediately */
        mtimer_arm_rel(&lfe2->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, sec2mono(2));
        if (lfe2->lfe_satconf)
          linuxdvb_satconf_post_stop_mux(lfe2->lfe_satconf);
        lfe2->lfe_in_setup = 0;
        lfe2->lfe_freq = 0;
      }
    }
  }

  lfe->lfe_refcount--;
  lfe->lfe_in_setup = 0;
  lfe->lfe_freq = 0;
  mpegts_pid_done(&lfe->lfe_pids);
}

static int
linuxdvb_frontend_warm_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi, *lfe2 = NULL;
  mpegts_mux_instance_t *lmmi = NULL;
  int r;

  r = mpegts_input_warm_mux(mi, mmi);
  if (r)
    return r;

  if (!lfe->lfe_adapter->la_exclusive)
    return 0;

  /* Stop other active frontend (should be only one) */
  LIST_FOREACH(lfe2, &lfe->lfe_adapter->la_frontends, lfe_link) {
    if (lfe2 == lfe) continue;
    pthread_mutex_lock(&lfe2->mi_output_lock);
    lmmi = LIST_FIRST(&lfe2->mi_mux_active);
    pthread_mutex_unlock(&lfe2->mi_output_lock);
    if (lmmi) {
      /* Stop */
      lmmi->mmi_mux->mm_stop(lmmi->mmi_mux, 1, SM_CODE_ABORTED);
    }
    linuxdvb_frontend_close_fd(lfe2, NULL);
    mtimer_disarm(&lfe2->lfe_monitor_timer);
  }
  return 0;
}

static int
linuxdvb_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi, *lfe2;
  int res, f;

  assert(lfe->lfe_in_setup == 0);

  lfe->lfe_refcount++;
  lfe->lfe_in_setup = 1;
  lfe->lfe_ioctls   = 0;

  if (lfe->lfe_master) {
    assert(lfe->lfe_type == DVB_TYPE_S);
    lfe2 = (linuxdvb_frontend_t *)idnode_find(lfe->lfe_master, &linuxdvb_frontend_class, NULL);
    if (lfe2 && lfe2->lfe_type != lfe->lfe_type)
      lfe2 = NULL;
    res = SM_CODE_TUNING_FAILED;
    if (lfe2) {
      f = linuxdvb_satconf_lnb_freq(lfe2->lfe_satconf, mmi);
      if (f <= 0)
        goto end;
      if (lfe2->lfe_refcount++ == 0) {
        lfe2->lfe_in_setup = 1;
        lfe2->lfe_ioctls   = 0;
        res = linuxdvb_satconf_start_mux(lfe2->lfe_satconf, mmi, 0);
        if (res)
          goto end;
      }
      res = linuxdvb_frontend_tune1((linuxdvb_frontend_t*)mi, mmi, f);
    }
    goto end;
  }

  if (lfe->lfe_satconf)
    res = linuxdvb_satconf_start_mux(lfe->lfe_satconf, mmi, lfe->lfe_refcount > 1);
  else
    res = linuxdvb_frontend_tune1((linuxdvb_frontend_t*)mi, mmi, -1);

end:
  if (res) {
    lfe->lfe_in_setup = 0;
    lfe->lfe_refcount--;
  }
  return res;
}

static void
linuxdvb_frontend_update_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  pthread_mutex_lock(&lfe->lfe_dvr_lock);
  mpegts_pid_done(&lfe->lfe_pids);
  RB_FOREACH(mp, &mm->mm_pids, mp_link) {
    if (mp->mp_pid == MPEGTS_FULLMUX_PID)
      lfe->lfe_pids.all = 1;
    else if (mp->mp_pid < MPEGTS_FULLMUX_PID) {
      RB_FOREACH(mps, &mp->mp_subs, mps_link)
        mpegts_pid_add(&lfe->lfe_pids, mp->mp_pid, mps->mps_weight);
    }
  }
  pthread_mutex_unlock(&lfe->lfe_dvr_lock);

  if (lfe->lfe_dvr_pipe.wr > 0)
    tvh_write(lfe->lfe_dvr_pipe.wr, "c", 1);
}

static idnode_set_t *
linuxdvb_frontend_network_list ( mpegts_input_t *mi )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;

  tvhtrace(LS_LINUXDVB, "%s: network list for %s",
           mi->mi_name ?: "", dvb_type2str(lfe->lfe_type));

  return dvb_network_list_by_fe_type(lfe->lfe_type);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static inline int
ioctl_check( linuxdvb_frontend_t *lfe, int bit )
{
  return !(lfe->lfe_ioctls & (1 << bit));
}

static inline void
ioctl_bad( linuxdvb_frontend_t *lfe, int bit )
{
  lfe->lfe_ioctls |= 1 << bit;
}

static void
linuxdvb_frontend_monitor ( void *aux )
{
  uint16_t u16;
  uint32_t u32;
  char buf[256];
  linuxdvb_frontend_t *lfe = aux;
  mpegts_mux_instance_t *mmi = LIST_FIRST(&lfe->mi_mux_active);
  mpegts_mux_t *mm;
  fe_status_t fe_status;
  signal_state_t status;
  signal_status_t sigstat;
  streaming_message_t sm;
  service_t *s;
  int logit = 0, retune, e;
  uint32_t period = MINMAX(lfe->lfe_status_period, 250, 8000);
#if DVB_VER_ATLEAST(5,10)
  struct dtv_property fe_properties[6];
  struct dtv_properties dtv_prop;
  int gotprop;
#endif

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));
  tvhtrace(LS_LINUXDVB, "%s - checking FE status%s", buf, lfe->lfe_ready ? " (ready)" : "");
  
  /* Disabled */
  if (!lfe->mi_enabled && mmi)
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1, SM_CODE_ABORTED);

  /* Close FE */
  if (lfe->lfe_fe_fd > 0 && !lfe->lfe_refcount && lfe->lfe_powersave) {
    linuxdvb_frontend_close_fd(lfe, buf);
    return;
  }

  /* Check accessibility */
  if (lfe->lfe_fe_fd <= 0) {
    if (lfe->lfe_fe_path && access(lfe->lfe_fe_path, R_OK | W_OK)) {
      tvherror(LS_LINUXDVB, "%s - device is not accessible", buf);
      // TODO: disable device
      return;
    }
  }

  /* Stop timer */
  if (!mmi || !lfe->lfe_ready) return;

  /* re-arm */
  mtimer_arm_rel(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, ms2mono(period));

  /* Get current status */
  if (ioctl(lfe->lfe_fe_fd, FE_READ_STATUS, &fe_status) == -1) {
    tvhwarn(LS_LINUXDVB, "%s - FE_READ_STATUS error %s", buf, strerror(errno));
    /* TODO: check error value */
    return;

  } else if (fe_status & FE_HAS_LOCK)
    status = SIGNAL_GOOD;
  else if (fe_status & (FE_HAS_SYNC | FE_HAS_VITERBI | FE_HAS_CARRIER))
    status = SIGNAL_BAD;
  else if (fe_status & FE_HAS_SIGNAL)
    status = SIGNAL_FAINT;
  else
    status = SIGNAL_NONE;

  /* Set default period */
  if (fe_status != lfe->lfe_status) {
    tvhdebug(LS_LINUXDVB, "%s - status %7s (%s%s%s%s%s%s)", buf,
             signal2str(status),
             (fe_status & FE_HAS_SIGNAL) ?  "SIGNAL"  : "",
             (fe_status & FE_HAS_CARRIER) ? " | CARRIER" : "",
             (fe_status & FE_HAS_VITERBI) ? " | VITERBI" : "",
             (fe_status & FE_HAS_SYNC) ?    " | SYNC"    : "",
             (fe_status & FE_HAS_LOCK) ?    " | LOCK"  : "",
             (fe_status & FE_TIMEDOUT) ?    "TIMEOUT" : "");
  } else {
    tvhtrace(LS_LINUXDVB, "%s - status %d (%04X)", buf, status, fe_status);
  }
  retune = NOSIGNAL(fe_status) && NOSIGNAL(lfe->lfe_status) && !NOSIGNAL(lfe->lfe_status2);
  lfe->lfe_status2 = lfe->lfe_status;
  lfe->lfe_status = fe_status;

  /* Retune check - we lost signal or no data were received */
  if (retune || lfe->lfe_nodata) {
    lfe->lfe_nodata = 0;
    if (lfe->lfe_locked && lfe->lfe_freq > 0) {
      tvhwarn(LS_LINUXDVB, "%s - %s", buf, retune ? "retune" : "retune nodata");
      linuxdvb_frontend_tune0(lfe, mmi, lfe->lfe_freq);
      mtimer_arm_rel(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, ms2mono(50));
      lfe->lfe_locked = 1;
    }
  }

  /* Get current mux */
  mm = mmi->mmi_mux;

  /* Waiting for lock */
  if (!lfe->lfe_locked) {

    /* Locked */
    if (status == SIGNAL_GOOD) {
      tvhdebug(LS_LINUXDVB, "%s - locked", buf);
      lfe->lfe_locked = 1;
  
      /* Start input */
      tvh_pipe(O_NONBLOCK, &lfe->lfe_dvr_pipe);
      pthread_mutex_lock(&lfe->lfe_dvr_lock);
      tvhthread_create(&lfe->lfe_dvr_thread, NULL,
                       linuxdvb_frontend_input_thread, lfe, "lnxdvb-front");
      do {
        e = tvh_cond_wait(&lfe->lfe_dvr_cond, &lfe->lfe_dvr_lock);
        if (e == ETIMEDOUT)
          break;
      } while (ERRNO_AGAIN(e));
      pthread_mutex_unlock(&lfe->lfe_dvr_lock);

      /* Table handlers */
      psi_tables_install((mpegts_input_t *)lfe, mm,
                         ((dvb_mux_t *)mm)->lm_tuning.dmc_fe_delsys);

    /* Re-arm (quick) */
    } else {
      mtimer_arm_rel(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor,
                     lfe, ms2mono(50));

      /* Monitor 1 per sec */
      if (mclk() < lfe->lfe_monitor)
        return;
      lfe->lfe_monitor = mclk() + sec2mono(1);
    }
  } else {
    if (mclk() < lfe->lfe_monitor)
      return;
    lfe->lfe_monitor = mclk() + ms2mono(period);
  }


  pthread_mutex_lock(&mmi->tii_stats_mutex);

  /* Statistics - New API */
#if DVB_VER_ATLEAST(5,10)
  memset(&fe_properties, 0, sizeof(fe_properties));

  /* Signal strength */
  fe_properties[0].cmd = DTV_STAT_SIGNAL_STRENGTH;

  /* BER */
  fe_properties[1].cmd = DTV_STAT_PRE_ERROR_BIT_COUNT;
  fe_properties[2].cmd = DTV_STAT_PRE_TOTAL_BIT_COUNT;

  /* SNR */
  fe_properties[3].cmd = DTV_STAT_CNR;

  /* PER / UNC */
  fe_properties[4].cmd = DTV_STAT_ERROR_BLOCK_COUNT;
  fe_properties[5].cmd = DTV_STAT_TOTAL_BLOCK_COUNT;
  dtv_prop.num = 6;
  dtv_prop.props = fe_properties;

  logit = tvhlog_limit(&lfe->lfe_status_log, 3600);

  if(ioctl_check(lfe, 0) && !lfe->lfe_old_status &&
     !ioctl(lfe->lfe_fe_fd, FE_GET_PROPERTY, &dtv_prop)) {
    /* Signal strength */
    gotprop = 0;
    if(ioctl_check(lfe, 1) && fe_properties[0].u.st.len > 0) {
      if(fe_properties[0].u.st.stat[0].scale == FE_SCALE_RELATIVE) {
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
        mmi->tii_stats.signal = sig_multiply(fe_properties[0].u.st.stat[0].uvalue, lfe->lfe_sig_multiplier);
        gotprop = 1;
      }
      else if(fe_properties[0].u.st.stat[0].scale == FE_SCALE_DECIBEL) {
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_DECIBEL;
        mmi->tii_stats.signal = sig_multiply(fe_properties[0].u.st.stat[0].svalue, lfe->lfe_sig_multiplier);
        gotprop = 1;
      }
      else if(fe_properties[0].u.st.stat[0].scale == FE_SCALE_NOT_AVAILABLE) {
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 1);
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled signal scale: %d",
                  fe_properties[0].u.st.stat[0].scale);
      }
    }
    if(!gotprop && ioctl_check(lfe, 2)) {
      /* try old API */
      if (!ioctl(lfe->lfe_fe_fd, FE_READ_SIGNAL_STRENGTH, &u16)) {
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
        mmi->tii_stats.signal = sig_multiply(u16, lfe->lfe_sig_multiplier);
      }
      else {
        ioctl_bad(lfe, 2);
        mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unable to provide signal strength value.");
      }
    }

    /* ERROR_BIT_COUNT */
    gotprop = 0;
    if(ioctl_check(lfe, 3) && fe_properties[1].u.st.len > 0) {
      if(fe_properties[1].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        mmi->tii_stats.ec_bit = fe_properties[1].u.st.stat[0].uvalue;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 3);
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled ERROR_BIT_COUNT scale: %d",
                  fe_properties[1].u.st.stat[0].scale);
      }
    }
    /* TOTAL_BIT_COUNT */
    if(gotprop && (fe_properties[2].u.st.len > 0)) {
      gotprop = 0;
      if(ioctl_check(lfe, 4) && fe_properties[2].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        mmi->tii_stats.tc_bit = fe_properties[2].u.st.stat[0].uvalue;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 4);
        mmi->tii_stats.ec_bit = 0; /* both values or none */
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled TOTAL_BIT_COUNT scale: %d",
                  fe_properties[2].u.st.stat[0].scale);
      }
    }
    if(!gotprop && ioctl_check(lfe, 5)) {
      /* try old API */
      if (!ioctl(lfe->lfe_fe_fd, FE_READ_BER, &u32))
        mmi->tii_stats.ber = u32;
      else {
        ioctl_bad(lfe, 5);
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unable to provide BER value.");
      }
    }
    
    /* SNR */
    gotprop = 0;
    if(ioctl_check(lfe, 6) && fe_properties[3].u.st.len > 0) {
      if(fe_properties[3].u.st.stat[0].scale == FE_SCALE_RELATIVE) {
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
        mmi->tii_stats.snr = sig_multiply(fe_properties[3].u.st.stat[0].uvalue, lfe->lfe_snr_multiplier);
        gotprop = 1;
      }
      else if(fe_properties[3].u.st.stat[0].scale == FE_SCALE_DECIBEL) {
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_DECIBEL;
        mmi->tii_stats.snr = sig_multiply(fe_properties[3].u.st.stat[0].svalue, lfe->lfe_snr_multiplier);
        gotprop = 1;
      }
      else if(fe_properties[3].u.st.stat[0].scale == FE_SCALE_NOT_AVAILABLE) {
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 6);
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled SNR scale: %d",
                  fe_properties[3].u.st.stat[0].scale);
      }
    }
    if(!gotprop && ioctl_check(lfe, 7)) {
      /* try old API */
      if (!ioctl(lfe->lfe_fe_fd, FE_READ_SNR, &u16)) {
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
        mmi->tii_stats.snr = sig_multiply(u16, lfe->lfe_snr_multiplier);
      }
      else {
        ioctl_bad(lfe, 7);
        mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unable to provide SNR value.");
      }
    }

    /* ERROR_BLOCK_COUNT == Uncorrected blocks (UNC) */
    gotprop = 0;
    if(ioctl_check(lfe, 8) && fe_properties[4].u.st.len > 0) {
      if(fe_properties[4].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        atomic_set(&mmi->tii_stats.unc, fe_properties[4].u.st.stat[0].uvalue);
        mmi->tii_stats.ec_block = fe_properties[4].u.st.stat[0].uvalue;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 8);
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled ERROR_BLOCK_COUNT scale: %d",
                  fe_properties[4].u.st.stat[0].scale);
      }
    }

    /* TOTAL_BLOCK_COUNT */
    if(gotprop && (fe_properties[5].u.st.len > 0)) {
      gotprop = 0;
      if(ioctl_check(lfe, 9) && fe_properties[5].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        mmi->tii_stats.tc_block = fe_properties[5].u.st.stat[0].uvalue;
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 9);
        /* both values to none */
        mmi->tii_stats.ec_block = 0;
        atomic_set(&mmi->tii_stats.unc, 0);
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unhandled TOTAL_BLOCK_COUNT scale: %d",
                 fe_properties[5].u.st.stat[0].scale);
      }
    }
    if(!gotprop && ioctl_check(lfe, 10)) {
      /* try old API */
      if (!ioctl(lfe->lfe_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &u32)) {
        atomic_set(&mmi->tii_stats.unc, u32);
        gotprop = 1;
      }
      else {
        ioctl_bad(lfe, 10);
        if (logit)
          tvhwarn(LS_LINUXDVB, "Unable to provide UNC value.");
      }
    }
  /* Older API */
  } else
#endif
  {
    ioctl_bad(lfe, 0);
    if (ioctl_check(lfe, 1) && !ioctl(lfe->lfe_fe_fd, FE_READ_SIGNAL_STRENGTH, &u16)) {
      mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_RELATIVE;
      mmi->tii_stats.signal = sig_multiply(u16, lfe->lfe_sig_multiplier);
    }
    else {
      ioctl_bad(lfe, 1);
      mmi->tii_stats.signal_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
      if (logit)
        tvhwarn(LS_LINUXDVB, "Unable to provide signal strength value.");
    }
    if (ioctl_check(lfe, 2) && !ioctl(lfe->lfe_fe_fd, FE_READ_BER, &u32))
      mmi->tii_stats.ber = u32;
    else {
      ioctl_bad(lfe, 2);
      if (logit)
        tvhwarn(LS_LINUXDVB, "Unable to provide BER value.");
    }
    if (ioctl_check(lfe, 3) && !ioctl(lfe->lfe_fe_fd, FE_READ_SNR, &u16)) {
      mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_RELATIVE;
      mmi->tii_stats.snr = sig_multiply(u16, lfe->lfe_snr_multiplier);
    }
    else {
      ioctl_bad(lfe, 3);
      mmi->tii_stats.snr_scale = SIGNAL_STATUS_SCALE_UNKNOWN;
      if (logit)
        tvhwarn(LS_LINUXDVB, "Unable to provide SNR value.");
    }
    if (ioctl_check(lfe, 4) && !ioctl(lfe->lfe_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &u32))
      atomic_set(&mmi->tii_stats.unc, u32);
    else {
      ioctl_bad(lfe, 4);
      if (logit)
        tvhwarn(LS_LINUXDVB, "Unable to provide UNC value.");
    }
  }

  /* Send message */
  sigstat.status_text  = signal2str(status);
  sigstat.snr          = mmi->tii_stats.snr;
  sigstat.signal       = mmi->tii_stats.signal;
  sigstat.ber          = mmi->tii_stats.ber;
  sigstat.unc          = atomic_get(&mmi->tii_stats.unc);
  sigstat.signal_scale = mmi->tii_stats.signal_scale;
  sigstat.snr_scale    = mmi->tii_stats.snr_scale;
  sigstat.ec_bit       = mmi->tii_stats.ec_bit;
  sigstat.tc_bit       = mmi->tii_stats.tc_bit;
  sigstat.ec_block     = mmi->tii_stats.ec_block;
  sigstat.tc_block     = mmi->tii_stats.tc_block;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;

  pthread_mutex_unlock(&mmi->tii_stats_mutex);

  LIST_FOREACH(s, &mmi->mmi_mux->mm_transports, s_active_link) {
    pthread_mutex_lock(&s->s_stream_mutex);
    streaming_pad_deliver(&s->s_streaming_pad, streaming_msg_clone(&sm));
    pthread_mutex_unlock(&s->s_stream_mutex);
  }
}

typedef struct linuxdvb_pid {
  int fd;
  int pid;
} linuxdvb_pid_t;

static void
linuxdvb_frontend_open_pid0
  ( linuxdvb_frontend_t *lfe, const char *name,
    linuxdvb_pid_t *pids, int pids_size, int pid )
{
  struct dmx_pes_filter_params dmx_param;
  int fd;

  for ( ; pids_size > 0 && pids->fd >= 0; pids_size--, pids++);
  if (pids_size == 0) {
    tvherror(LS_LINUXDVB, "%s - maximum PID count reached, pid %d ignored",
             name, pid);
    return;
  }

  /* Open DMX */
  fd = tvh_open(lfe->lfe_dmx_path, O_RDWR, 0);
  if(fd == -1) {
    tvherror(LS_LINUXDVB, "%s - failed to open dmx for pid %d [e=%s]",
             name, pid, strerror(errno));
    return;
  }

  /* Install filter */
  tvhtrace(LS_LINUXDVB, "%s - open PID %04X (%d) fd %d", name, pid, pid, fd);
  memset(&dmx_param, 0, sizeof(dmx_param));
  dmx_param.pid      = pid;
  dmx_param.input    = DMX_IN_FRONTEND;
  dmx_param.output   = DMX_OUT_TS_TAP;
  dmx_param.pes_type = DMX_PES_OTHER;
  dmx_param.flags    = DMX_IMMEDIATE_START;

  if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
    tvherror(LS_LINUXDVB, "%s - failed to config dmx for pid %d [e=%s]",
             name, pid, strerror(errno));
    close(fd);
    return;
  }

  /* Store */
  pids->fd = fd;
  pids->pid = pid;
}

static void
linuxdvb_frontend_close_pid0
  ( linuxdvb_frontend_t *lfe, const char *name,
    linuxdvb_pid_t *pids, int pids_size, int pid )
{
  for ( ; pids_size > 0 && pids->pid != pid;
          pids_size--, pids++);
  if (pids_size == 0)
    return;
  tvhtrace(LS_LINUXDVB, "%s - close PID %04X (%d) fd %d", name, pid, pid, pids->fd);
  close(pids->fd);
  pids->fd = -1;
  pids->pid = -1;
}

static int
linuxdvb_pid_exists ( linuxdvb_pid_t *pids, int pids_size, int pid )
{
  int i;
  for (i = 0; i < pids_size; i++)
    if (pids[i].fd >= 0 && pids[i].pid == pid)
      return 1;
  return 0;
}

static void
linuxdvb_update_pids ( linuxdvb_frontend_t *lfe, const char *name,
                       mpegts_apids_t *tuned, linuxdvb_pid_t *pids,
                       int pids_size )
{
  mpegts_apids_t wpid, padd, pdel;
  int i, max = MAX(14, lfe->lfe_pids_max);
  int all = lfe->lfe_pids.all;

  pthread_mutex_lock(&lfe->lfe_dvr_lock);

  if (!all) {
    mpegts_pid_weighted(&wpid, &lfe->lfe_pids, max);
    if (wpid.count > max && lfe->lfe_pids_use_all) {
      all = 1;
      mpegts_pid_done(&wpid);
    } else {
      mpegts_pid_compare(&wpid, tuned, &padd, &pdel);
      mpegts_pid_done(&wpid);
      for (i = 0; i < pdel.count; i++)
        linuxdvb_frontend_close_pid0(lfe, name, pids, pids_size, pdel.pids[i].pid);
      for (i = 0; i < padd.count; i++)
       linuxdvb_frontend_open_pid0(lfe, name, pids, pids_size, padd.pids[i].pid);
      mpegts_pid_done(&padd);
      mpegts_pid_done(&pdel);
    }
  }

  if (all && !linuxdvb_pid_exists(pids, pids_size, MPEGTS_FULLMUX_PID))
    linuxdvb_frontend_open_pid0(lfe, name, pids, pids_size, MPEGTS_FULLMUX_PID);
  else if (!all && linuxdvb_pid_exists(pids, pids_size, MPEGTS_FULLMUX_PID))
    linuxdvb_frontend_close_pid0(lfe, name, pids, pids_size, MPEGTS_FULLMUX_PID);

  mpegts_pid_done(tuned);
  mpegts_pid_weighted(tuned, &lfe->lfe_pids, max);
  tuned->all = lfe->lfe_pids.all;

  pthread_mutex_unlock(&lfe->lfe_dvr_lock);
}

static void *
linuxdvb_frontend_input_thread ( void *aux )
{
  linuxdvb_frontend_t *lfe = aux;
  mpegts_mux_instance_t *mmi;
  char name[256], b;
  tvhpoll_event_t ev[2];
  tvhpoll_t *efd;
  ssize_t n;
  size_t skip = (MIN(lfe->lfe_skip_bytes, 1024*1024) / 188) * 188;
  size_t counter = 0;
  linuxdvb_pid_t pids[128];
  mpegts_apids_t tuned;
  sbuf_t sb;
  int i, dvr = -1, nfds, nodata = 4;

  mpegts_pid_init(&tuned);
  for (i = 0; i < ARRAY_SIZE(pids); i++) {
    pids[i].fd = -1;
    pids[i].pid = -1;
  }

  /* Get MMI */
  pthread_mutex_lock(&lfe->lfe_dvr_lock);
  lfe->mi_display_name((mpegts_input_t*)lfe, name, sizeof(name));
  mmi = LIST_FIRST(&lfe->mi_mux_active);
  tvh_cond_signal(&lfe->lfe_dvr_cond, 0);
  pthread_mutex_unlock(&lfe->lfe_dvr_lock);
  if (mmi == NULL) return NULL;

  /* Open DVR */
  dvr = tvh_open(lfe->lfe_dvr_path, O_RDONLY | O_NONBLOCK, 0);
  if (dvr < 0) {
    tvherror(LS_LINUXDVB, "%s - failed to open %s", name, lfe->lfe_dvr_path);
    return NULL;
  }

  /* Setup poll */
  efd = tvhpoll_create(2);
  memset(ev, 0, sizeof(ev));
  ev[0].events             = TVHPOLL_IN;
  ev[0].fd = ev[0].data.fd = dvr;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd = ev[1].data.fd = lfe->lfe_dvr_pipe.rd;
  tvhpoll_add(efd, ev, 2);

  /* Allocate memory */
  sbuf_init_fixed(&sb, MINMAX(lfe->lfe_ibuf_size, 18800, 1880000));

  /* Subscribe PIDs */
  linuxdvb_update_pids(lfe, name, &tuned, pids, ARRAY_SIZE(pids));

  /* Read */
  while (tvheadend_is_running()) {
    nfds = tvhpoll_wait(efd, ev, 1, 150);
    if (nfds == 0) { /* timeout */
      if (nodata == 0) {
        tvhwarn(LS_LINUXDVB, "%s - poll TIMEOUT", name);
        nodata = 50;
        lfe->lfe_nodata = 1;
      } else {
        nodata--;
      }
    }
    if (nfds < 1) continue;
    if (ev[0].data.fd == lfe->lfe_dvr_pipe.rd) {
      if (read(lfe->lfe_dvr_pipe.rd, &b, 1) > 0) {
        if (b == 'c')
          linuxdvb_update_pids(lfe, name, &tuned, pids, ARRAY_SIZE(pids));
        else
          break;
      }
      continue;
    }
    if (ev[0].data.fd != dvr) break;

    nodata = 50;
    lfe->lfe_nodata = 0;
    
    /* Read */
    if ((n = sbuf_tsdebug_read(mmi->mmi_mux, &sb, dvr)) < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      if (errno == EOVERFLOW) {
        tvhwarn(LS_LINUXDVB, "%s - read() EOVERFLOW", name);
        continue;
      }
      tvherror(LS_LINUXDVB, "%s - read() error %d (%s)",
               name, errno, strerror(errno));
      break;
    }

    /* Skip the initial bytes */
    if (counter < skip) {
      counter += n;
      if (counter < skip) {
        sbuf_cut(&sb, n);
      } else {
        sbuf_cut(&sb, skip - (counter - n));
      }
    }
    
    /* Process */
    mpegts_input_recv_packets((mpegts_input_t*)lfe, mmi, &sb, 0, NULL);
  }

  sbuf_free(&sb);
  tvhpoll_destroy(efd);
  for (i = 0; i < ARRAY_SIZE(pids); i++)
    if (pids[i].fd >= 0)
      close(pids[i].fd);
  mpegts_pid_done(&tuned);
  close(dvr);
  return NULL;
}

/* **************************************************************************
 * Tuning
 * *************************************************************************/

typedef struct linuxdvb_tbl {
  int t; ///< TVH internal value
  int l; ///< LinuxDVB API value
} linuxdvb_tbl_t;

#define TABLE_EOD -1

#if DVB_API_VERSION >= 5
static int
linuxdvb2tvh ( const char *prefix, linuxdvb_tbl_t *tbl, int key, int defval )
{
  while (tbl->t != TABLE_EOD) {
    if (tbl->l == key)
      return tbl->t;
    tbl++;
  }
  tvhtrace(LS_LINUXDVB, "%s - linuxdvb2tvh failed %d", prefix, key);
  return defval;
}
#endif

static int
tvh2linuxdvb ( const char *prefix, linuxdvb_tbl_t *tbl, int key, int defval )
{
  while (tbl->t != TABLE_EOD) {
    if (tbl->t == key)
      return tbl->l;
    tbl++;
  }
  tvhtrace(LS_LINUXDVB, "%s - tvh2linuxdvb failed %d", prefix, key);
  return defval;
}

#define TOSTR(s) #s
#define TR(s, t, d)  tvh2linuxdvb(TOSTR(s), t, dmc->dmc_fe_##s , d)
#define TRU(s, t, d) tvh2linuxdvb(TOSTR(s), t, dmc->u.dmc_fe_##s , d)

#if DVB_API_VERSION >= 5
static linuxdvb_tbl_t delsys_tbl[] = {
  { .t = DVB_SYS_DVBC_ANNEX_B,        .l = SYS_DVBC_ANNEX_B },
#if DVB_VER_ATLEAST(5,6)
  { .t = DVB_SYS_DVBC_ANNEX_A,        .l = SYS_DVBC_ANNEX_A },
  { .t = DVB_SYS_DVBC_ANNEX_C,        .l = SYS_DVBC_ANNEX_C },
#else
  { .t = DVB_SYS_DVBC_ANNEX_A,        .l = SYS_DVBC_ANNEX_AC },
  { .t = DVB_SYS_DVBC_ANNEX_C,        .l = SYS_DVBC_ANNEX_AC },
#endif
  { .t = DVB_SYS_DVBT,                .l = SYS_DVBT         },
#if DVB_VER_ATLEAST(5,3)
  { .t = DVB_SYS_DVBT2,               .l = SYS_DVBT2        },
#endif
  { .t = DVB_SYS_DVBS,                .l = SYS_DVBS         },
  { .t = DVB_SYS_DVBS2,               .l = SYS_DVBS2        },
  { .t = DVB_SYS_DVBH,                .l = SYS_DVBH         },
#if DVB_VER_ATLEAST(5,1)
  { .t = DVB_SYS_DSS,                 .l = SYS_DSS          },
#endif
  { .t = DVB_SYS_ISDBT,               .l = SYS_ISDBT        },
  { .t = DVB_SYS_ISDBS,               .l = SYS_ISDBS        },
  { .t = DVB_SYS_ISDBC,               .l = SYS_ISDBC        },
  { .t = DVB_SYS_ATSC,                .l = SYS_ATSC         },
  { .t = DVB_SYS_ATSCMH,              .l = SYS_ATSCMH       },
#if DVB_VER_ATLEAST(5,7)
  { .t = DVB_SYS_DTMB,                .l = SYS_DTMB         },
#endif
  { .t = DVB_SYS_CMMB,                .l = SYS_CMMB         },
  { .t = DVB_SYS_DAB,                 .l = SYS_DAB          },
#if DVB_VER_ATLEAST(5,4)
  { .t = DVB_SYS_TURBO,               .l = SYS_TURBO        },
#endif
  { .t = TABLE_EOD }
};
int linuxdvb2tvh_delsys ( int delsys )
{
  return linuxdvb2tvh("delsys", delsys_tbl, delsys, DVB_SYS_NONE);
}
#endif

int
linuxdvb_frontend_clear
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi )
{
  char buf1[256];

  /* Open FE */
  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  tvhtrace(LS_LINUXDVB, "%s - frontend clear", buf1);

  if (linuxdvb_frontend_open_fd(lfe, buf1))
    return SM_CODE_TUNING_FAILED;

  lfe->lfe_locked  = 0;
  lfe->lfe_status  = 0;
  lfe->lfe_status2 = 0;

#if DVB_API_VERSION >= 5
  static struct dtv_property clear_p[] = {
    { .cmd = DTV_CLEAR },
  };
  static struct dtv_properties clear_cmdseq = {
    .num = 1,
    .props = clear_p
  };
  if ((ioctl(lfe->lfe_fe_fd, FE_SET_PROPERTY, &clear_cmdseq)) != 0) {
    tvherror(LS_LINUXDVB, "%s - DTV_CLEAR failed [e=%s]", buf1, strerror(errno));
    return -1;
  }

  if (mmi) {
    dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
    dvb_mux_conf_t *dmc = &lm->lm_tuning;

    struct dtv_property delsys_p;
    struct dtv_properties delsys_cmdseq = {
      .num = 1,
      .props = &delsys_p
    };
    delsys_p.cmd = DTV_DELIVERY_SYSTEM;
    delsys_p.u.data = TR(delsys, delsys_tbl, SYS_UNDEFINED);
    if ((ioctl(lfe->lfe_fe_fd, FE_SET_PROPERTY, &delsys_cmdseq)) != 0) {
      tvherror(LS_LINUXDVB, "%s - DTV_DELIVERY_SYSTEM failed [e=%s]", buf1, strerror(errno));
      return -1;
    }
  }
#endif

  return 0;
}

int
linuxdvb_frontend_tune0
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq )
{
  static linuxdvb_tbl_t inv_tbl[] = {
    { .t = DVB_INVERSION_AUTO,          .l = INVERSION_AUTO  },
    { .t = DVB_INVERSION_OFF,           .l = INVERSION_OFF   },
    { .t = DVB_INVERSION_ON,            .l = INVERSION_ON    },
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t bw_tbl[] = {
    { .t = DVB_BANDWIDTH_AUTO,          .l = BANDWIDTH_AUTO      },
#if DVB_VER_ATLEAST(5,3)
    { .t = DVB_BANDWIDTH_1_712_MHZ,     .l = BANDWIDTH_1_712_MHZ },
    { .t = DVB_BANDWIDTH_5_MHZ,         .l = BANDWIDTH_5_MHZ     },
#endif
    { .t = DVB_BANDWIDTH_6_MHZ,         .l = BANDWIDTH_6_MHZ     },
    { .t = DVB_BANDWIDTH_7_MHZ,         .l = BANDWIDTH_7_MHZ     },
    { .t = DVB_BANDWIDTH_8_MHZ,         .l = BANDWIDTH_8_MHZ     },
#if DVB_VER_ATLEAST(5,3)
    { .t = DVB_BANDWIDTH_10_MHZ,        .l = BANDWIDTH_10_MHZ    },
#endif
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t fec_tbl[] = {
    { .t = DVB_FEC_NONE,                .l = FEC_NONE  },
    { .t = DVB_FEC_AUTO,                .l = FEC_AUTO  },
    { .t = DVB_FEC_1_2,                 .l = FEC_1_2   },
    { .t = DVB_FEC_2_3,                 .l = FEC_2_3   },
#if DVB_VER_ATLEAST(5,7)
    { .t = DVB_FEC_2_5,                 .l = FEC_2_5   },
#endif
    { .t = DVB_FEC_3_4,                 .l = FEC_3_4   },
#if DVB_VER_ATLEAST(5,0)
    { .t = DVB_FEC_3_5,                 .l = FEC_3_5   },
#endif
    { .t = DVB_FEC_4_5,                 .l = FEC_4_5   },
    { .t = DVB_FEC_5_6,                 .l = FEC_5_6   },
    { .t = DVB_FEC_6_7,                 .l = FEC_6_7   },
    { .t = DVB_FEC_7_8,                 .l = FEC_7_8   },
    { .t = DVB_FEC_8_9,                 .l = FEC_8_9   },
#if DVB_VER_ATLEAST(5,0)
    { .t = DVB_FEC_9_10,                .l = FEC_9_10  },
#endif
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t mod_tbl[] = {
    { .t = DVB_MOD_AUTO,                .l = QAM_AUTO },
    { .t = DVB_MOD_QPSK,                .l = QPSK     },
    { .t = DVB_MOD_QAM_16,              .l = QAM_16   },
    { .t = DVB_MOD_QAM_32,              .l = QAM_32   },
    { .t = DVB_MOD_QAM_64,              .l = QAM_64   },
    { .t = DVB_MOD_QAM_128,             .l = QAM_128  },
    { .t = DVB_MOD_QAM_256,             .l = QAM_256  },
    { .t = DVB_MOD_QAM_AUTO,            .l = QAM_AUTO },
    { .t = DVB_MOD_VSB_8,               .l = VSB_8    },
    { .t = DVB_MOD_VSB_16,              .l = VSB_16   },
#if DVB_VER_ATLEAST(5,1)
    { .t = DVB_MOD_PSK_8,               .l = PSK_8,   },
    { .t = DVB_MOD_APSK_16,             .l = APSK_16  },
    { .t = DVB_MOD_APSK_32,             .l = APSK_32  },
#endif
#if DVB_VER_ATLEAST(5,0)
    { .t = DVB_MOD_DQPSK,               .l = DQPSK    },
#endif
#if DVB_VER_ATLEAST(5,7)
    { .t = DVB_MOD_QAM_4_NR,            .l = QAM_4_NR },
#endif
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t trans_tbl[] = {
    { .t = DVB_TRANSMISSION_MODE_AUTO,  .l = TRANSMISSION_MODE_AUTO  },
#if DVB_VER_ATLEAST(5,3)
    { .t = DVB_TRANSMISSION_MODE_1K,    .l = TRANSMISSION_MODE_1K    },
#endif
    { .t = DVB_TRANSMISSION_MODE_2K,    .l = TRANSMISSION_MODE_2K    },
#if DVB_VER_ATLEAST(5,1)
    { .t = DVB_TRANSMISSION_MODE_4K,    .l = TRANSMISSION_MODE_4K    },
#endif
    { .t = DVB_TRANSMISSION_MODE_8K,    .l = TRANSMISSION_MODE_8K    },
#if DVB_VER_ATLEAST(5,3)
    { .t = DVB_TRANSMISSION_MODE_16K,   .l = TRANSMISSION_MODE_16K   },
    { .t = DVB_TRANSMISSION_MODE_32K,   .l = TRANSMISSION_MODE_32K   },
#endif
#if DVB_VER_ATLEAST(5,7)
    { .t = DVB_TRANSMISSION_MODE_C3780, .l = TRANSMISSION_MODE_C3780 },
    { .t = DVB_TRANSMISSION_MODE_C1,    .l = TRANSMISSION_MODE_C1    },
#endif
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t guard_tbl[] = {
    { .t = DVB_GUARD_INTERVAL_AUTO,     .l = GUARD_INTERVAL_AUTO   },
    { .t = DVB_GUARD_INTERVAL_1_4,      .l = GUARD_INTERVAL_1_4    },
    { .t = DVB_GUARD_INTERVAL_1_8,      .l = GUARD_INTERVAL_1_8    },
    { .t = DVB_GUARD_INTERVAL_1_16,     .l = GUARD_INTERVAL_1_16   },
    { .t = DVB_GUARD_INTERVAL_1_32,     .l = GUARD_INTERVAL_1_32   },
#if DVB_VER_ATLEAST(5,3)
    { .t = DVB_GUARD_INTERVAL_1_128,    .l = GUARD_INTERVAL_1_128  },
    { .t = DVB_GUARD_INTERVAL_19_128,   .l = GUARD_INTERVAL_19_128 },
    { .t = DVB_GUARD_INTERVAL_19_256,   .l = GUARD_INTERVAL_19_256 },
#endif
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t h_tbl[] = {
    { .t = DVB_HIERARCHY_NONE,          .l = HIERARCHY_NONE },
    { .t = DVB_HIERARCHY_AUTO,          .l = HIERARCHY_AUTO },
    { .t = DVB_HIERARCHY_1,             .l = HIERARCHY_1    },
    { .t = DVB_HIERARCHY_2,             .l = HIERARCHY_2    },
    { .t = DVB_HIERARCHY_4,             .l = HIERARCHY_4    },
    { .t = TABLE_EOD }
  };
#if DVB_API_VERSION >= 5
  static linuxdvb_tbl_t pilot_tbl[] = {
    { .t = DVB_PILOT_AUTO,              .l = PILOT_AUTO },
    { .t = DVB_PILOT_ON,                .l = PILOT_ON   },
    { .t = DVB_PILOT_OFF,               .l = PILOT_OFF  },
    { .t = TABLE_EOD }
  };
  static linuxdvb_tbl_t rolloff_tbl[] = {
    { .t = DVB_HIERARCHY_AUTO,          .l = ROLLOFF_AUTO },
    { .t = DVB_ROLLOFF_20,              .l = ROLLOFF_20   },
    { .t = DVB_ROLLOFF_25,              .l = ROLLOFF_25   },
    { .t = DVB_ROLLOFF_35,              .l = ROLLOFF_35   },
    { .t = TABLE_EOD },
  };
#endif
  int r;
  struct dvb_frontend_event ev;
  char buf1[256];
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc;
  struct dvb_frontend_parameters p;

  r = linuxdvb_frontend_clear(lfe, NULL);
  if (r) return r;

  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));

  /*
   * copy the universal parameters to the Linux kernel structure
   */

  dmc = &lm->lm_tuning;
  if (tvhtrace_enabled()) {
    char buf2[256];
    dvb_mux_conf_str(&lm->lm_tuning, buf2, sizeof(buf2));
    tvhtrace(LS_LINUXDVB, "tuner %s tuning to %s (freq %i)", buf1, buf2, freq);
  }
  memset(&p, 0, sizeof(p));
  p.frequency                = dmc->dmc_fe_freq;
  p.inversion                = TR(inversion, inv_tbl, INVERSION_AUTO);
  switch (dmc->dmc_fe_type) {
  case DVB_TYPE_T:
#define _OFDM(xyz) p.u.ofdm.xyz
    _OFDM(bandwidth)         = TRU(ofdm.bandwidth, bw_tbl, BANDWIDTH_AUTO);
    _OFDM(code_rate_HP)      = TRU(ofdm.code_rate_HP, fec_tbl, FEC_AUTO);
    _OFDM(code_rate_LP)      = TRU(ofdm.code_rate_LP, fec_tbl, FEC_AUTO);
    _OFDM(constellation)     = TR(modulation, mod_tbl, QAM_AUTO);
    _OFDM(transmission_mode) = TRU(ofdm.transmission_mode, trans_tbl, TRANSMISSION_MODE_AUTO);
    _OFDM(guard_interval)    = TRU(ofdm.guard_interval, guard_tbl, GUARD_INTERVAL_AUTO);
    _OFDM(hierarchy_information)
                             = TRU(ofdm.hierarchy_information, h_tbl, HIERARCHY_AUTO);
#undef _OFDM
    break;
  case DVB_TYPE_C:
  case DVB_TYPE_ATSC_C:
  case DVB_TYPE_ISDB_C:
    p.u.qam.symbol_rate      = dmc->u.dmc_fe_qam.symbol_rate;
    p.u.qam.fec_inner        = TRU(qam.fec_inner, fec_tbl, FEC_AUTO);
    p.u.qam.modulation       = TR(modulation, mod_tbl, QAM_AUTO);
    break;
  case DVB_TYPE_S:
    p.u.qpsk.symbol_rate     = dmc->u.dmc_fe_qpsk.symbol_rate;
    p.u.qpsk.fec_inner       = TRU(qpsk.fec_inner, fec_tbl, FEC_AUTO);
    break;
  case DVB_TYPE_ATSC_T:
    p.u.vsb.modulation       = TR(modulation, mod_tbl, QAM_AUTO);
    break;
#if DVB_API_VERSION >= 5
  case DVB_TYPE_ISDB_T:
  case DVB_TYPE_DAB:
    break;
#endif
  default:
    tvherror(LS_LINUXDVB, "%s - unknown FE type %d", buf1, dmc->dmc_fe_type);
    return SM_CODE_TUNING_FAILED;
  }

  if (freq != (uint32_t)-1)
    p.frequency = lfe->lfe_freq = freq;

  if (dmc->dmc_fe_type != lfe->lfe_type) {
    tvherror(LS_LINUXDVB, "%s - failed to tune [type does not match %i != %i]", buf1, dmc->dmc_fe_type, lfe->lfe_type);
    return SM_CODE_TUNING_FAILED;
  }

  /* S2 tuning */
#if DVB_API_VERSION >= 5
  struct dtv_property cmds[32];
  struct dtv_properties cmdseq;
  

  if (freq == (uint32_t)-1)
    freq = p.frequency;

  memset(&cmdseq, 0, sizeof(cmdseq));
  cmdseq.props = cmds;
  memset(&cmds, 0, sizeof(cmds));
  
  /* Tune */
#define S2CMD(c, d) do { \
  cmds[cmdseq.num].cmd      = c; \
  cmds[cmdseq.num++].u.data = d; \
} while (0)

  memset(&cmds, 0, sizeof(cmds));
  S2CMD(DTV_DELIVERY_SYSTEM, TR(delsys, delsys_tbl, SYS_UNDEFINED));
  S2CMD(DTV_FREQUENCY,       freq);
  S2CMD(DTV_INVERSION,       p.inversion);

  /* DVB-T */
  if (lfe->lfe_type == DVB_TYPE_T) {
    S2CMD(DTV_BANDWIDTH_HZ,      dvb_bandwidth(dmc->u.dmc_fe_ofdm.bandwidth));
#if DVB_VER_ATLEAST(5,1)
    S2CMD(DTV_CODE_RATE_HP,      p.u.ofdm.code_rate_HP);
    S2CMD(DTV_CODE_RATE_LP,      p.u.ofdm.code_rate_LP);
#endif
    S2CMD(DTV_MODULATION,        p.u.ofdm.constellation);
#if DVB_VER_ATLEAST(5,1)
    S2CMD(DTV_TRANSMISSION_MODE, p.u.ofdm.transmission_mode);
    S2CMD(DTV_GUARD_INTERVAL,    p.u.ofdm.guard_interval);
    S2CMD(DTV_HIERARCHY,         p.u.ofdm.hierarchy_information);
#endif
    if (lm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBT2) {
#if DVB_VER_ATLEAST(5,9)
      S2CMD(DTV_STREAM_ID, dmc->dmc_fe_stream_id);
#elif DVB_VER_ATLEAST(5,3)
      S2CMD(DTV_DVBT2_PLP_ID, dmc->dmc_fe_stream_id);
#endif
    }
#if DVB_VER_ATLEAST(5,9)
    if (lfe->lfe_lna)
      S2CMD(DTV_LNA, 1);
#endif

  /* DVB-C */
  } else if (lfe->lfe_type == DVB_TYPE_C ||
             lfe->lfe_type == DVB_TYPE_ATSC_C ||
             lfe->lfe_type == DVB_TYPE_ISDB_C) {
    S2CMD(DTV_SYMBOL_RATE,       p.u.qam.symbol_rate);
    S2CMD(DTV_MODULATION,        p.u.qam.modulation);
    S2CMD(DTV_INNER_FEC,         p.u.qam.fec_inner);

  /* DVB-S */
  } else if (lfe->lfe_type == DVB_TYPE_S) {
    S2CMD(DTV_SYMBOL_RATE,       p.u.qpsk.symbol_rate);
    S2CMD(DTV_INNER_FEC,         p.u.qpsk.fec_inner);
    S2CMD(DTV_MODULATION,        TR(modulation, mod_tbl, QPSK));
    if (lm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBS) {
      S2CMD(DTV_ROLLOFF,         ROLLOFF_35);
    } else {
      S2CMD(DTV_PILOT,           TR(pilot, pilot_tbl, PILOT_AUTO));
      S2CMD(DTV_ROLLOFF,         TR(rolloff, rolloff_tbl, ROLLOFF_AUTO));
      r = dmc->dmc_fe_stream_id != DVB_NO_STREAM_ID_FILTER ? (dmc->dmc_fe_stream_id & 0xFF) |
          ((dmc->dmc_fe_pls_code & 0x3FFFF)<<8) | ((dmc->dmc_fe_pls_mode & 0x3)<<26) :
          DVB_NO_STREAM_ID_FILTER;
#if DVB_VER_ATLEAST(5,9)
      S2CMD(DTV_STREAM_ID,       r);
#elif DVB_VER_ATLEAST(5,3)
      S2CMD(DTV_DVBT2_PLP_ID,    r);
#endif
    }

  /* ATSC-T */
  } else if (lfe->lfe_type == DVB_TYPE_ATSC_T) {
    S2CMD(DTV_MODULATION,        p.u.vsb.modulation);

#if DVB_VER_ATLEAST(5,0)
  /* ISDB-T */
  } else if (lfe->lfe_type == DVB_TYPE_ISDB_T) {
    int i, j;
    S2CMD(DTV_BANDWIDTH_HZ,      dvb_bandwidth(dmc->u.dmc_fe_isdbt.bandwidth));
#if DVB_VER_ATLEAST(5,1)
    S2CMD(DTV_GUARD_INTERVAL, TRU(isdbt.guard_interval, guard_tbl, GUARD_INTERVAL_AUTO));
    S2CMD(DTV_TRANSMISSION_MODE, TRANSMISSION_MODE_AUTO);
    S2CMD(DTV_ISDBT_LAYER_ENABLED,     7);
    S2CMD(DTV_ISDBT_PARTIAL_RECEPTION, 1);
    S2CMD(DTV_ISDBT_SOUND_BROADCASTING,0);
    S2CMD(DTV_ISDBT_SB_SUBCHANNEL_ID,  0);
    S2CMD(DTV_ISDBT_SB_SEGMENT_IDX,    0);
    S2CMD(DTV_ISDBT_SB_SEGMENT_COUNT,  0);
    for (i = j = 0; i < 3; i++, j += DTV_ISDBT_LAYERB_FEC - DTV_ISDBT_LAYERA_FEC) {
      S2CMD(DTV_ISDBT_LAYERA_FEC               + j, TRU(isdbt.layers[i].fec, fec_tbl, FEC_AUTO));
      S2CMD(DTV_ISDBT_LAYERA_MODULATION        + j, TRU(isdbt.layers[i].modulation, mod_tbl, QAM_AUTO));
      S2CMD(DTV_ISDBT_LAYERA_SEGMENT_COUNT     + j, dmc->u.dmc_fe_isdbt.layers[i].segment_count);
      S2CMD(DTV_ISDBT_LAYERA_TIME_INTERLEAVING + j, dmc->u.dmc_fe_isdbt.layers[i].time_interleaving);
    }
#endif

  /* ISDB-S */
  } else if (lfe->lfe_type == DVB_TYPE_ISDB_S) {
    r = dmc->dmc_fe_stream_id != DVB_NO_STREAM_ID_FILTER ? (dmc->dmc_fe_stream_id & 0xFF) |
        ((dmc->dmc_fe_pls_code & 0x3FFFF)<<8) | ((dmc->dmc_fe_pls_mode & 0x3)<<26) :
        DVB_NO_STREAM_ID_FILTER;
#if DVB_VER_ATLEAST(5,9)
    S2CMD(DTV_STREAM_ID,       r);
#elif DVB_VER_ATLEAST(5,3)
    S2CMD(DTV_DVBT2_PLP_ID,    r);
#endif

  /* DAB */
  } else if (lfe->lfe_type == DVB_TYPE_DAB) {
#endif /* 5.0+ */
  }

  /* Tune */
  S2CMD(DTV_TUNE, 0);
#undef S2CMD
#endif

  /* discard stale events */
  while (1) {
    if (ioctl(lfe->lfe_fe_fd, FE_GET_EVENT, &ev) < 0)
      break;
  }

  /* S2 tuning */
#if DVB_API_VERSION >= 5
  if (tvhtrace_enabled()) {
    int i;
    for (i = 0; i < cmdseq.num; i++)
      tvhtrace(LS_LINUXDVB, "S2CMD %02u => %u", cmds[i].cmd, cmds[i].u.data);
  }
  r = ioctl(lfe->lfe_fe_fd, FE_SET_PROPERTY, &cmdseq);

  /* v3 tuning */
#else
  r = ioctl(lfe->lfe_fe_fd, FE_SET_FRONTEND, p);
#endif

  /* Failed */
  if (r != 0) {
    tvherror(LS_LINUXDVB, "%s - failed to tune [e=%s]", buf1, strerror(errno));
    if (errno == EINVAL)
      mmi->mmi_tune_failed = 1;
    return SM_CODE_TUNING_FAILED;
  }

  return r;
}

int
linuxdvb_frontend_tune1
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq )
{
  int r = 0, i, rep;
  char buf1[256], buf2[256];

  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug(LS_LINUXDVB, "%s - starting %s", buf1, buf2);

  /* Tune */
  tvhtrace(LS_LINUXDVB, "%s - tuning", buf1);
  rep = lfe->lfe_tune_repeats > 0 ? lfe->lfe_tune_repeats : 0;
  for (i = 0; i <= rep; i++) {
    if (i > 0)
      tvh_safe_usleep(15000);
    r = linuxdvb_frontend_tune0(lfe, mmi, freq);
    if (r)
      break;
  }

  /* Start monitor */
  if (!r) {
    time(&lfe->lfe_monitor);
    lfe->lfe_monitor += 4;
    mtimer_arm_rel(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, ms2mono(50));
    lfe->lfe_ready = 1;
  }

  lfe->lfe_in_setup = 0;
  
  return r;
}


/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

static mpegts_network_t *
linuxdvb_frontend_wizard_network ( linuxdvb_frontend_t *lfe )
{
  linuxdvb_satconf_ele_t *ele = NULL;

  if (lfe->lfe_satconf) {
    ele = TAILQ_FIRST(&lfe->lfe_satconf->ls_elements);
    if (ele && ele->lse_networks && ele->lse_networks->is_count > 0)
      return (mpegts_network_t *)ele->lse_networks->is_array[0];
  }
  return (mpegts_network_t *)LIST_FIRST(&lfe->mi_networks);
}

static htsmsg_t *
linuxdvb_frontend_wizard_get( tvh_input_t *ti, const char *lang )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)ti;
  mpegts_network_t *mn;
  const idclass_t *idc = NULL;

  mn = linuxdvb_frontend_wizard_network(lfe);
  if ((lfe->lfe_master == 0 && mn == NULL) || (mn && mn->mn_wizard))
    idc = dvb_network_class_by_fe_type(lfe->lfe_type);
  return mpegts_network_wizard_get((mpegts_input_t *)lfe, idc, mn, lang);
}

static void
linuxdvb_frontend_wizard_set( tvh_input_t *ti, htsmsg_t *conf, const char *lang )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)ti;
  const char *ntype = htsmsg_get_str(conf, "mpegts_network_type");
  mpegts_network_t *mn;
  htsmsg_t *nlist;

  if (LIST_FIRST(&lfe->mi_mux_active))
    return;
  mpegts_network_wizard_create(ntype, &nlist, lang);
  mn = linuxdvb_frontend_wizard_network(lfe);
  if (ntype && (mn == NULL || mn->mn_wizard)) {
    if (lfe->lfe_satconf) {
      htsmsg_t *conf = htsmsg_create_map();
      htsmsg_t *elems = htsmsg_create_list();
      htsmsg_t *elem = htsmsg_create_map();
      htsmsg_add_str(conf, "type", "4port");
      htsmsg_add_bool(elem, "enable", 1);
      htsmsg_add_msg(elem, "networks", nlist);
      htsmsg_add_msg(elems, NULL, elem);
      htsmsg_add_msg(conf, "elements", elems);
      if (lfe->lfe_satconf) {
        linuxdvb_satconf_delete(lfe->lfe_satconf, 0);
        lfe->lfe_satconf = NULL;
      }
      lfe->lfe_satconf = linuxdvb_satconf_create(lfe, conf);
      htsmsg_destroy(conf);
    } else {
      mpegts_input_set_networks((mpegts_input_t *)lfe, nlist);
      htsmsg_destroy(nlist);
    }
    if (linuxdvb_frontend_wizard_network(lfe))
      mpegts_input_set_enabled((mpegts_input_t *)lfe, 1);
    linuxdvb_adapter_changed(lfe->lfe_adapter);
  } else {
    htsmsg_destroy(nlist);
  }
}

linuxdvb_frontend_t *
linuxdvb_frontend_create
  ( htsmsg_t *conf, linuxdvb_adapter_t *la, int number,
    const char *fe_path, const char *dmx_path, const char *dvr_path,
    dvb_fe_type_t type, const char *name )
{
  const idclass_t *idc;
  const char *str, *uuid = NULL, *muuid = NULL;
  char id[16], lname[256], buf[256];
  linuxdvb_frontend_t *lfe;
  htsmsg_t *scconf;
  ssize_t r;

  /* Tuner slave */
  snprintf(id, sizeof(id), "master for #%d", number);
  if (conf && type == DVB_TYPE_S) {
    muuid = htsmsg_get_str(conf, id);
    if (muuid && uuid && !strcmp(muuid, uuid))
      muuid = NULL;
  }

  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(type), number);
  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  /* Fudge configuration for old network entry */
  if (conf) {
    if (!htsmsg_get_list(conf, "networks") &&
        (str = htsmsg_get_str(conf, "network"))) {
      htsmsg_t *l = htsmsg_create_list();
      htsmsg_add_str(l, NULL, str);
      htsmsg_add_msg(conf, "networks", l);
    }
  }

  /* Class */
  if (type == DVB_TYPE_S)
    idc = muuid ? &linuxdvb_frontend_dvbs_slave_class :
                  &linuxdvb_frontend_dvbs_class;
  else if (type == DVB_TYPE_C)
    idc = &linuxdvb_frontend_dvbc_class;
  else if (type == DVB_TYPE_T)
    idc = &linuxdvb_frontend_dvbt_class;
  else if (type == DVB_TYPE_ATSC_T)
    idc = &linuxdvb_frontend_atsc_t_class;
  else if (type == DVB_TYPE_ATSC_C)
    idc = &linuxdvb_frontend_atsc_c_class;
  else if (type == DVB_TYPE_ISDB_T)
    idc = &linuxdvb_frontend_isdb_t_class;
  else if (type == DVB_TYPE_ISDB_C)
    idc = &linuxdvb_frontend_isdb_c_class;
  else if (type == DVB_TYPE_ISDB_S)
    idc = &linuxdvb_frontend_isdb_s_class;
  else if (type == DVB_TYPE_DAB)
    idc = &linuxdvb_frontend_dab_class;
  else {
    tvherror(LS_LINUXDVB, "unknown FE type %d", type);
    return NULL;
  }

  /* Create */
  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  lfe = calloc(1, sizeof(linuxdvb_frontend_t));
  lfe->lfe_number = number;
  lfe->lfe_type   = type;
  lfe->lfe_master = muuid ? strdup(muuid) : NULL;
  strncpy(lfe->lfe_name, name, sizeof(lfe->lfe_name));
  lfe->lfe_name[sizeof(lfe->lfe_name)-1] = '\0';
  lfe->lfe_ibuf_size = 188000;
  lfe->lfe_status_period = 1000;
  lfe->lfe_pids_max = 32;
  lfe->lfe_pids_use_all = 1;
  lfe->lfe_sig_multiplier = 100;
  lfe->lfe_snr_multiplier = 100;
  lfe = (linuxdvb_frontend_t*)mpegts_input_create0((mpegts_input_t*)lfe, idc, uuid, conf);
  if (!lfe) return NULL;

  /* Sysfs path */
  snprintf(lname, sizeof(lname), "/sys/class/dvb/dvb%d.frontend%d", la->la_dvb_number, number);
  r = readlink(lname, buf, sizeof(buf));
  if (r > 0) {
    if (r == sizeof(buf)) r--;
    buf[r] = '\0';
    if (strncmp(str = buf, "../../", 6) == 0) str += 6;
    lfe->lfe_sysfs = strdup(str);
  }

  /* Callbacks */
  lfe->mi_get_weight   = linuxdvb_frontend_get_weight;
  lfe->mi_get_priority = linuxdvb_frontend_get_priority;
  lfe->mi_get_grace    = linuxdvb_frontend_get_grace;

  /* Default name */
  if (!lfe->mi_name) {
    snprintf(lname, sizeof(lname), "%s : %s", name, id);
    lfe->mi_name = strdup(lname);
  }

  /* Set paths */
  lfe->lfe_fe_path  = strdup(fe_path);
  lfe->lfe_dmx_path = strdup(dmx_path);
  lfe->lfe_dvr_path = strdup(dvr_path);

  /* Input callbacks */
  lfe->ti_wizard_get      = linuxdvb_frontend_wizard_get;
  lfe->ti_wizard_set      = linuxdvb_frontend_wizard_set;
  lfe->mi_is_enabled      = linuxdvb_frontend_is_enabled;
  lfe->mi_warm_mux        = linuxdvb_frontend_warm_mux;
  lfe->mi_start_mux       = linuxdvb_frontend_start_mux;
  lfe->mi_stop_mux        = linuxdvb_frontend_stop_mux;
  lfe->mi_network_list    = linuxdvb_frontend_network_list;
  lfe->mi_update_pids     = linuxdvb_frontend_update_pids;
  lfe->mi_enabled_updated = linuxdvb_frontend_enabled_updated;
  lfe->mi_empty_status    = mpegts_input_empty_status;

  /* Adapter link */
  lfe->lfe_adapter = la;
  LIST_INSERT_HEAD(&la->la_frontends, lfe, lfe_link);

  /* DVR lock/cond */
  pthread_mutex_init(&lfe->lfe_dvr_lock, NULL);
  tvh_cond_init(&lfe->lfe_dvr_cond);
  mpegts_pid_init(&lfe->lfe_pids);
 
  /* Create satconf */
  if (lfe->lfe_type == DVB_TYPE_S && !lfe->lfe_satconf && !muuid) {
    scconf = conf ? htsmsg_get_map(conf, "satconf") : NULL;
    lfe->lfe_satconf = linuxdvb_satconf_create(lfe, scconf);
  }

  /* Double check enabled */
  linuxdvb_frontend_enabled_updated((mpegts_input_t*)lfe);

  return lfe;
}

void
linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *fe )
{
  char id[16], ubuf[UUID_HEX_SIZE];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)lfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->lfe_type));
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&lfe->ti_id, ubuf));
  if (lfe->lfe_satconf && !lfe->lfe_master) {
    htsmsg_t *s = htsmsg_create_map();
    linuxdvb_satconf_save(lfe->lfe_satconf, s);
    htsmsg_add_str(s, "uuid", idnode_uuid_as_str(&lfe->lfe_satconf->ls_id, ubuf));
    htsmsg_add_msg(m, "satconf", s);
  }
  htsmsg_delete_field(m, "fe_master");

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(lfe->lfe_type), lfe->lfe_number);
  htsmsg_add_msg(fe, id, m);

  if (lfe->lfe_master) {
    snprintf(id, sizeof(id), "master for #%d", lfe->lfe_number);
    htsmsg_add_str(fe, id, lfe->lfe_master);
  }
}

void
linuxdvb_frontend_delete ( linuxdvb_frontend_t *lfe )
{
  lock_assert(&global_lock);

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)lfe);

  /* Stop monitor */
  mtimer_disarm(&lfe->lfe_monitor_timer);

  /* Close FDs */
  if (lfe->lfe_fe_fd > 0)
    close(lfe->lfe_fe_fd);

  /* Remove from adapter */
  LIST_REMOVE(lfe, lfe_link);

  /* Free memory */
  free(lfe->lfe_sysfs);
  free(lfe->lfe_fe_path);
  free(lfe->lfe_dmx_path);
  free(lfe->lfe_dvr_path);
  free(lfe->lfe_master);

  /* Delete satconf */
  if (lfe->lfe_satconf)
    linuxdvb_satconf_delete(lfe->lfe_satconf, 0);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)lfe, 0);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
