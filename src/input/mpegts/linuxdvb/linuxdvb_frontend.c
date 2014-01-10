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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>

static void
linuxdvb_frontend_monitor ( void *aux );
static void *
linuxdvb_frontend_input_thread ( void *aux );

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
linuxdvb_frontend_class_save ( idnode_t *in )
{
  linuxdvb_adapter_t *la = ((linuxdvb_frontend_t*)in)->lfe_adapter;
  linuxdvb_adapter_save(la);
}

static const void*
linuxdvb_frontend_class_network_get(void *o)
{
  static const char *s;
  linuxdvb_frontend_t *lfe = o;
  if (lfe->mi_network)
    s = idnode_uuid_as_str(&lfe->mi_network->mn_id);
  else
    s = NULL;
  return &s;
}

static int
linuxdvb_frontend_class_network_set(void *o, const void *v)
{
  mpegts_input_t   *mi = o;
  mpegts_network_t *mn = mi->mi_network;
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mn;
  linuxdvb_frontend_t *lfe = o;
  const char *s = v;

  if (lfe->lfe_info.type == FE_QPSK) {
    tvherror("linuxdvb", "cannot set network on DVB-S FE");
    return 0;
  }

  if (mi->mi_network && !strcmp(idnode_uuid_as_str(&mn->mn_id), s ?: ""))
    return 0;

  if (ln && ln->ln_type != lfe->lfe_info.type) {
    tvherror("linuxdvb", "attempt to set network of wrong type");
    return 0;
  }

  mpegts_input_set_network(mi, s ? mpegts_network_find(s) : NULL);
  return 1;
}

static htsmsg_t *
linuxdvb_frontend_class_network_enum(void *o)
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "mpegts/input/network_list");
  htsmsg_add_str(p, "uuid",  idnode_uuid_as_str((idnode_t*)o));
  htsmsg_add_str(m, "event", "mpegts_network");
  htsmsg_add_msg(m, "params", p);

  return m;
}

const idclass_t linuxdvb_frontend_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "linuxdvb_frontend",
  .ic_caption    = "Linux DVB Frontend",
  .ic_save       = linuxdvb_frontend_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "fe_path",
      .name     = "Frontend Path",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_fe_path),
    },
    {
      .type     = PT_STR,
      .id       = "dvr_path",
      .name     = "Input Path",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_dvr_path),
    },
    {
      .type     = PT_STR,
      .id       = "dmx_path",
      .name     = "Demux Path",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_dmx_path),
    },
    {
      .type     = PT_INT,
      .id       = "fe_number",
      .name     = "Frontend Number",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(linuxdvb_frontend_t, lfe_number),
    },
    {
      .type     = PT_BOOL,
      .id       = "fullmux",
      .name     = "Full Mux RX mode",
      .off      = offsetof(linuxdvb_frontend_t, lfe_fullmux),
    },
    {
      .type     = PT_BOOL,
      .id       = "noclosefe",
      .name     = "Keep FE open",
      .off      = offsetof(linuxdvb_frontend_t, lfe_noclosefe),
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbt_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbt",
  .ic_caption    = "Linux DVB-T Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .get      = linuxdvb_frontend_class_network_get,
      .set      = linuxdvb_frontend_class_network_set,
      .list     = linuxdvb_frontend_class_network_enum
    },
    {}
  }
};

static idnode_set_t *
linuxdvb_frontend_dvbs_class_get_childs ( idnode_t *self )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)self;
  idnode_set_t        *is  = idnode_set_create();
  idnode_set_add(is, &lfe->lfe_satconf->ls_id, NULL);
  return is;
}

static int
linuxdvb_frontend_dvbs_class_satconf_set ( void *self, const void *str )
{
  linuxdvb_frontend_t *lfe = self;
  if (lfe->lfe_satconf && !strcmp(str ?: "", lfe->lfe_satconf->ls_type))
    return 0;
  linuxdvb_satconf_delete(lfe->lfe_satconf);
  lfe->lfe_satconf = linuxdvb_satconf_create(lfe, str, NULL, NULL);
  return 1;
}

static const void *
linuxdvb_frontend_dvbs_class_satconf_get ( void *self )
{
  static const char *s;
  linuxdvb_frontend_t *lfe = self;
  if (lfe->lfe_satconf)
    s = lfe->lfe_satconf->ls_type;
  else
    s = NULL;
  return &s;
}

const idclass_t linuxdvb_frontend_dvbs_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbs",
  .ic_caption    = "Linux DVB-S Frontend",
  .ic_get_childs = linuxdvb_frontend_dvbs_class_get_childs,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "satconf",
      .name     = "SatConfig",
      .opts     = PO_NOSAVE,
      .set      = linuxdvb_frontend_dvbs_class_satconf_set,
      .get      = linuxdvb_frontend_dvbs_class_satconf_get,
      .list     = linuxdvb_satconf_type_list,
      .def.s    = "simple"
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_dvbc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_dvbc",
  .ic_caption    = "Linux DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .get      = linuxdvb_frontend_class_network_get,
      .set      = linuxdvb_frontend_class_network_set,
      .list     = linuxdvb_frontend_class_network_enum
    },
    {}
  }
};

const idclass_t linuxdvb_frontend_atsc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_atsc",
  .ic_caption    = "Linux ATSC Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .get      = linuxdvb_frontend_class_network_get,
      .set      = linuxdvb_frontend_class_network_set,
      .list     = linuxdvb_frontend_class_network_enum
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
linuxdvb_frontend_is_free ( mpegts_input_t *mi )
{
  linuxdvb_adapter_t *la = ((linuxdvb_frontend_t*)mi)->lfe_adapter;
  linuxdvb_frontend_t *lfe;
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
    if (!mpegts_input_is_free((mpegts_input_t*)lfe))
      return 0;
  return 1;
}

static int
linuxdvb_frontend_get_weight ( mpegts_input_t *mi )
{
  int weight = 0;
  linuxdvb_adapter_t *la = ((linuxdvb_frontend_t*)mi)->lfe_adapter;
  linuxdvb_frontend_t *lfe;
  LIST_FOREACH(lfe, &la->la_frontends, lfe_link)
    weight = MAX(weight, mpegts_input_get_weight((mpegts_input_t*)lfe));
  return weight;
}

static int
linuxdvb_frontend_is_enabled ( mpegts_input_t *mi )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  if (lfe->lfe_fe_path == NULL) return 0;
  if (!lfe->mi_enabled) return 0;
  if (access(lfe->lfe_fe_path, R_OK | W_OK)) return 0;
  return 1;
}

static void
linuxdvb_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  char buf1[256], buf2[256];
  
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("linuxdvb", "%s - stopping %s", buf1, buf2);

  /* Stop thread */
  if (lfe->lfe_dvr_pipe.wr > 0) {
    tvh_write(lfe->lfe_dvr_pipe.wr, "", 1);
    tvhtrace("linuxdvb", "%s - waiting for dvr thread", buf1);
    pthread_join(lfe->lfe_dvr_thread, NULL);
    tvh_pipe_close(&lfe->lfe_dvr_pipe);
    tvhdebug("linuxdvb", "%s - stopped dvr thread", buf1);
  }

  /* Not locked */
  lfe->lfe_locked = 0;
  lfe->lfe_status = 0;

  /* Ensure it won't happen immediately */
  gtimer_arm(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 2);
}

static int
linuxdvb_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  return linuxdvb_frontend_tune1((linuxdvb_frontend_t*)mi, mmi, -1);
}

static void
linuxdvb_frontend_open_pid0
  ( linuxdvb_frontend_t *lfe, mpegts_pid_t *mp )
{
  char name[256];
  struct dmx_pes_filter_params dmx_param;
  int fd;

  /* Already opened */
  if (mp->mp_fd != -1)
    return;

  /* Not locked OR full mux mode */
  if (!lfe->lfe_locked || lfe->lfe_fullmux)
    return;

  lfe->mi_display_name((mpegts_input_t*)lfe, name, sizeof(name));

  /* Open DMX */
  fd = tvh_open(lfe->lfe_dmx_path, O_RDWR, 0);
  if(fd == -1) {
    tvherror("linuxdvb", "%s - failed to open dmx for pid %d [e=%s]",
             name, mp->mp_pid, strerror(errno));
    return;
  }

  /* Install filter */
  tvhtrace("linuxdvb", "%s - open PID %04X (%d) fd %d",
           name, mp->mp_pid, mp->mp_pid, fd);
  memset(&dmx_param, 0, sizeof(dmx_param));
  dmx_param.pid      = mp->mp_pid;
  dmx_param.input    = DMX_IN_FRONTEND;
  dmx_param.output   = DMX_OUT_TS_TAP;
  dmx_param.pes_type = DMX_PES_OTHER;
  dmx_param.flags    = DMX_IMMEDIATE_START;

  if(ioctl(fd, DMX_SET_PES_FILTER, &dmx_param)) {
    tvherror("linuxdvb", "%s - failed to config dmx for pid %d [e=%s]",
             name, mp->mp_pid, strerror(errno));
    close(fd);
    return;
  }

  /* Store */
  mp->mp_fd = fd;

  return;
}

static mpegts_pid_t *
linuxdvb_frontend_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  mpegts_pid_t *mp;
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;

  if (!(mp = mpegts_input_open_pid(mi, mm, pid, type, owner)))
    return NULL;

  linuxdvb_frontend_open_pid0(lfe, mp);

  return mp;
}

static idnode_set_t *
linuxdvb_frontend_network_list ( mpegts_input_t *mi )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  const idclass_t     *idc;
  extern const idclass_t linuxdvb_network_dvbt_class;
  extern const idclass_t linuxdvb_network_dvbc_class;
  extern const idclass_t linuxdvb_network_dvbs_class;
  extern const idclass_t linuxdvb_network_atsc_class;

  if (lfe->lfe_info.type == FE_OFDM)
    idc = &linuxdvb_network_dvbt_class;
  else if (lfe->lfe_info.type == FE_QAM)
    idc = &linuxdvb_network_dvbc_class;
  else if (lfe->lfe_info.type == FE_QPSK)
    idc = &linuxdvb_network_dvbs_class;
  else if (lfe->lfe_info.type == FE_ATSC)
    idc = &linuxdvb_network_atsc_class;
  else
    return NULL;

  return idnode_find_all(idc);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static void
linuxdvb_frontend_default_tables 
  ( linuxdvb_frontend_t *lfe, linuxdvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)lm;

  psi_tables_default(mm);

  /* ATSC */
  if (lfe->lfe_info.type == FE_ATSC) {
    if (lm->lm_tuning.dmc_fe_params.u.vsb.modulation == VSB_8)
      psi_tables_atsc_t(mm);
    else
      psi_tables_atsc_c(mm);

  /* DVB */
  } else {
    psi_tables_dvb(mm);
  }
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
  mpegts_pid_t *mp;
  fe_status_t fe_status;
  signal_state_t status;
#if DVB_VER_ATLEAST(5,10)
  struct dtv_property fe_properties[6];
  struct dtv_properties dtv_prop;
#endif

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));
  tvhtrace("linuxdvb", "%s - checking FE status", buf);

  /* Close FE */
  if (lfe->lfe_fe_fd > 0 && !mmi && !lfe->lfe_noclosefe) {
    tvhtrace("linuxdvb", "%s - closing frontend", buf);
    close(lfe->lfe_fe_fd);
    lfe->lfe_fe_fd = -1;
  }

  /* Check accessibility */
  if (lfe->lfe_fe_fd <= 0) {
    if (lfe->lfe_fe_path && access(lfe->lfe_fe_path, R_OK | W_OK)) {
      tvherror("linuxdvb", "%s - device is not accessible", buf);
      // TODO: disable device
      return;
    }
  }

  gtimer_arm(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 1);
  if (!mmi) return;

  /* Get current status */
  if (ioctl(lfe->lfe_fe_fd, FE_READ_STATUS, &fe_status) == -1) {
    tvhwarn("linuxdvb", "%s - FE_READ_STATUS error %s", buf, strerror(errno));
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
    tvhdebug("linuxdvb", "%s - status %7s (%s%s%s%s%s%s)", buf,
             signal2str(status),
             (fe_status & FE_HAS_SIGNAL) ?  "SIGNAL"  : "",
             (fe_status & FE_HAS_CARRIER) ? " | CARRIER" : "",
             (fe_status & FE_HAS_VITERBI) ? " | VITERBI" : "",
             (fe_status & FE_HAS_SYNC) ?    " | SYNC"    : "",
             (fe_status & FE_HAS_LOCK) ?    " | SIGNAL"  : "",
             (fe_status & FE_TIMEDOUT) ?    "TIMEOUT" : "");
  } else {
    tvhtrace("linuxdvb", "%s - status %d (%04X)", buf, status, fe_status);
  }
  lfe->lfe_status = fe_status;

  /* Get current mux */
  mm = mmi->mmi_mux;

  /* Waiting for lock */
  if (!lfe->lfe_locked) {

    /* Locked */
    if (status == SIGNAL_GOOD) {
      tvhdebug("linuxdvb", "%s - locked", buf);
      lfe->lfe_locked = 1;
  
      /* Start input */
      tvh_pipe(O_NONBLOCK, &lfe->lfe_dvr_pipe);
      pthread_mutex_lock(&lfe->lfe_dvr_lock);
      tvhthread_create(&lfe->lfe_dvr_thread, NULL,
                       linuxdvb_frontend_input_thread, lfe, 0);
      pthread_cond_wait(&lfe->lfe_dvr_cond, &lfe->lfe_dvr_lock);
      pthread_mutex_unlock(&lfe->lfe_dvr_lock);

      /* Table handlers */
      linuxdvb_frontend_default_tables(lfe, (linuxdvb_mux_t*)mm);

      /* Locked - ensure everything is open */
      pthread_mutex_lock(&lfe->mi_delivery_mutex);
      RB_FOREACH(mp, &mm->mm_pids, mp_link)
        linuxdvb_frontend_open_pid0(lfe, mp);
      pthread_mutex_unlock(&lfe->mi_delivery_mutex);

    /* Re-arm (quick) */
    } else {
      gtimer_arm_ms(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor,
                    lfe, 50);

      /* Monitor 1 per sec */
      if (dispatch_clock < lfe->lfe_monitor)
        return;
      lfe->lfe_monitor = dispatch_clock + 1;
    }
  }

  /* Statistics - New API */
#if DVB_VER_ATLEAST(5,10)
  fe_properties[0].cmd = DTV_STAT_SIGNAL_STRENGTH;

  /* BER */
  fe_properties[1].cmd = DTV_STAT_PRE_ERROR_BIT_COUNT;
  fe_properties[2].cmd = DTV_STAT_PRE_TOTAL_BIT_COUNT;
  fe_properties[3].cmd = DTV_STAT_CNR;

  /* PER */
  fe_properties[4].cmd = DTV_STAT_ERROR_BLOCK_COUNT;
  fe_properties[5].cmd = DTV_STAT_TOTAL_BLOCK_COUNT;
  dtv_prop.num = 6;
  dtv_prop.props = fe_properties;

  if(!ioctl(lfe->lfe_fe_fd, FE_GET_PROPERTY, &dtv_prop)) {
    if(fe_properties[0].u.st.len > 0) {
      if(fe_properties[0].u.st.stat[0].scale == FE_SCALE_RELATIVE)
        mmi->mmi_stats.signal = (fe_properties[0].u.st.stat[0].uvalue * 100) / 0xffff;
      /* TODO: handle other scales */
    }

    /* Calculate BER from PRE_ERROR and TOTAL_BIT_COUNT */
    if(fe_properties[1].u.st.len > 0) {
      if(fe_properties[1].u.st.stat[0].scale == FE_SCALE_COUNTER)
        u16 = fe_properties[1].u.st.stat[0].uvalue;
    }
    if(fe_properties[2].u.st.len > 0) {
      if(fe_properties[2].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        if(fe_properties[2].u.st.stat[0].uvalue > 0 )
          mmi->mmi_stats.ber = u16 / fe_properties[2].u.st.stat[0].uvalue;
        else
          mmi->mmi_stats.ber = 0;
      }
    }
    
    /* SNR */
    if(fe_properties[3].u.st.len > 0) {
      /* note that decibel scale means 1 = 0.0001 dB units here */
      if(fe_properties[3].u.st.stat[0].scale == FE_SCALE_DECIBEL)
        mmi->mmi_stats.snr = fe_properties[3].u.st.stat[0].svalue * 0.0001;
      /* TODO: handle other scales */
    }

    /* Calculate PER from PRE_ERROR and TOTAL_BIT_COUNT */
    if(fe_properties[4].u.st.len > 0) {
      if(fe_properties[4].u.st.stat[0].scale == FE_SCALE_COUNTER)
        u16 = fe_properties[4].u.st.stat[0].uvalue;
    }
    if(fe_properties[5].u.st.len > 0) {
      if(fe_properties[5].u.st.stat[0].scale == FE_SCALE_COUNTER) {
        if(fe_properties[5].u.st.stat[0].uvalue > 0 )
          mmi->mmi_stats.unc = u16 / fe_properties[5].u.st.stat[0].uvalue;
        else
          mmi->mmi_stats.unc = 0;
      }
    }
  
  /* Older API */
  } else
#endif
  {
    if (!ioctl(lfe->lfe_fe_fd, FE_READ_SIGNAL_STRENGTH, &u16))
      mmi->mmi_stats.signal = u16;
    if (!ioctl(lfe->lfe_fe_fd, FE_READ_BER, &u32))
      mmi->mmi_stats.ber = u32;
    if (!ioctl(lfe->lfe_fe_fd, FE_READ_SNR, &u16))
      mmi->mmi_stats.snr = u16;
    if (!ioctl(lfe->lfe_fe_fd, FE_READ_UNCORRECTED_BLOCKS, &u32))
      mmi->mmi_stats.unc = u32;
  }
}

static void *
linuxdvb_frontend_input_thread ( void *aux )
{
  linuxdvb_frontend_t *lfe = aux;
  mpegts_mux_instance_t *mmi;
  int dmx = -1, dvr = -1;
  char buf[256];
  uint8_t tsb[18800];
  int pos = 0, nfds;
  ssize_t c;
  tvhpoll_event_t ev[2];
  struct dmx_pes_filter_params dmx_param;
  int fullmux;
  tvhpoll_t *efd;

  /* Get MMI */
  pthread_mutex_lock(&lfe->lfe_dvr_lock);
  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));
  mmi = LIST_FIRST(&lfe->mi_mux_active);
  fullmux = lfe->lfe_fullmux;
  pthread_cond_signal(&lfe->lfe_dvr_cond);
  pthread_mutex_unlock(&lfe->lfe_dvr_lock);
  if (mmi == NULL) return NULL;

  /* Open DMX */
  if (fullmux) {
    dmx = tvh_open(lfe->lfe_dmx_path, O_RDWR, 0);
    if (dmx < 0) {
      tvherror("linuxdvb", "%s - failed to open %s", buf, lfe->lfe_dmx_path);
      return NULL;
    }
    memset(&dmx_param, 0, sizeof(dmx_param));
    dmx_param.pid      = 0x2000;
    dmx_param.input    = DMX_IN_FRONTEND;
    dmx_param.output   = DMX_OUT_TS_TAP;
    dmx_param.pes_type = DMX_PES_OTHER;
    dmx_param.flags    = DMX_IMMEDIATE_START;
    if(ioctl(dmx, DMX_SET_PES_FILTER, &dmx_param) == -1) {
      tvherror("linuxdvb", "%s - open raw filter failed [e=%s]",
               buf, strerror(errno));
      close(dmx);
      return NULL;
    }
  }

  /* Open DVR */
  dvr = tvh_open(lfe->lfe_dvr_path, O_RDONLY | O_NONBLOCK, 0);
  if (dvr < 0) {
    close(dmx);
    tvherror("linuxdvb", "%s - failed to open %s", buf, lfe->lfe_dvr_path);
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

  /* Read */
  while (1) {
    nfds = tvhpoll_wait(efd, ev, 1, 10);
    if (nfds < 1) continue;
    if (ev[0].data.fd != dvr) break;
    
    /* Read */
    c = read(dvr, tsb+pos, sizeof(tsb)-pos);
    if (c < 0) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;
      if (errno == EOVERFLOW) {
        tvhlog(LOG_WARNING, "linuxdvb", "%s - read() EOVERFLOW", buf);
        continue;
      }
      tvhlog(LOG_ERR, "linuxdvb", "%s - read() error %d (%s)",
             buf, errno, strerror(errno));
      break;
    }
    
    /* Process */
    pos = mpegts_input_recv_packets((mpegts_input_t*)lfe, mmi, tsb, c+pos,
                                    NULL, NULL, buf);
  }

  tvhpoll_destroy(efd);
  if (dmx != -1) close(dmx);
  close(dvr);
  return NULL;
}

/* **************************************************************************
 * Tuning
 * *************************************************************************/

int
linuxdvb_frontend_tune0
  ( linuxdvb_frontend_t *lfe, mpegts_mux_instance_t *mmi, uint32_t freq )
{
  int r;
  struct dvb_frontend_event ev;
  char buf1[256];
  mpegts_mux_instance_t *cur = LIST_FIRST(&lfe->mi_mux_active);
  linuxdvb_mux_t *lm = (linuxdvb_mux_t*)mmi->mmi_mux;

  // Not sure if this is right place?
  /* Currently active */
  if (cur != NULL) {

    /* Already tuned */
    if (mmi == cur)
      return 0;

    /* Stop current */
    cur->mmi_mux->mm_stop(cur->mmi_mux, 1);
  }
  assert(LIST_FIRST(&lfe->mi_mux_active) == NULL);

  /* Open FE */
  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  if (lfe->lfe_fe_fd <= 0) {
    lfe->lfe_fe_fd = tvh_open(lfe->lfe_fe_path, O_RDWR | O_NONBLOCK, 0);
    tvhtrace("linuxdvb", "%s - opening FE %s (%d)", buf1, lfe->lfe_fe_path, lfe->lfe_fe_fd);
    if (lfe->lfe_fe_fd <= 0) {
      return SM_CODE_TUNING_FAILED;
    }
  }
  lfe->lfe_locked = 0;
  lfe->lfe_status = 0;

  /* S2 tuning */
#if DVB_API_VERSION >= 5
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  struct dtv_property cmds[20];
  struct dtv_properties cmdseq = { .num = 0, .props = cmds };
  
  /* Clear Q */
  static struct dtv_property clear_p[] = {
    { .cmd = DTV_CLEAR },
  };
  static struct dtv_properties clear_cmdseq = {
    .num = 1,
    .props = clear_p
  };
  if ((ioctl(lfe->lfe_fe_fd, FE_SET_PROPERTY, &clear_cmdseq)) != 0)
    return -1;

  if (freq == (uint32_t)-1)
    freq = p->frequency;
  
  /* Tune */
#define S2CMD(c, d)\
  cmds[cmdseq.num].cmd      = c;\
  cmds[cmdseq.num++].u.data = d
  memset(&cmds, 0, sizeof(cmds));
  S2CMD(DTV_DELIVERY_SYSTEM, lm->lm_tuning.dmc_fe_delsys);
  S2CMD(DTV_FREQUENCY,       freq);
  S2CMD(DTV_INVERSION,       p->inversion);

  /* DVB-T */
  if (lfe->lfe_info.type == FE_OFDM) {
    S2CMD(DTV_BANDWIDTH_HZ,      dvb_bandwidth(p->u.ofdm.bandwidth));
#if DVB_VER_ATLEAST(5,1)
    S2CMD(DTV_CODE_RATE_HP,      p->u.ofdm.code_rate_HP);
    S2CMD(DTV_CODE_RATE_LP,      p->u.ofdm.code_rate_LP);
#endif
    S2CMD(DTV_MODULATION,        p->u.ofdm.constellation);
#if DVB_VER_ATLEAST(5,1)
    S2CMD(DTV_TRANSMISSION_MODE, p->u.ofdm.transmission_mode);
    S2CMD(DTV_GUARD_INTERVAL,    p->u.ofdm.guard_interval);
    S2CMD(DTV_HIERARCHY,         p->u.ofdm.hierarchy_information);
#endif

  /* DVB-C */
  } else if (lfe->lfe_info.type == FE_QAM) {
    S2CMD(DTV_SYMBOL_RATE,       p->u.qam.symbol_rate);
    S2CMD(DTV_MODULATION,        p->u.qam.modulation);
    S2CMD(DTV_INNER_FEC,         p->u.qam.fec_inner);

  /* DVB-S */
  } else if (lfe->lfe_info.type == FE_QPSK) {
    S2CMD(DTV_SYMBOL_RATE,       p->u.qpsk.symbol_rate);
    S2CMD(DTV_INNER_FEC,         p->u.qpsk.fec_inner);
    S2CMD(DTV_PILOT,             dmc->dmc_fe_pilot);
    if (lm->lm_tuning.dmc_fe_delsys == SYS_DVBS) {
      S2CMD(DTV_MODULATION,        QPSK);
      S2CMD(DTV_ROLLOFF,           ROLLOFF_35);
    } else {
      S2CMD(DTV_MODULATION,        dmc->dmc_fe_modulation);
      S2CMD(DTV_ROLLOFF,           dmc->dmc_fe_rolloff);
    }

  /* ATSC */
  } else {
    S2CMD(DTV_MODULATION,        p->u.vsb.modulation);
  }

  /* Tune */
  S2CMD(DTV_TUNE, 0);
#undef S2CMD
#else
  dvb_mux_conf_t dmc = lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc.dmc_fe_params;
  if (freq != (uint32_t)-1)
    p->frequency = freq;
#endif

  /* discard stale events */
  while (1) {
    if (ioctl(lfe->lfe_fe_fd, FE_GET_EVENT, &ev) == -1)
      break;
  }

  /* S2 tuning */
#if DVB_API_VERSION >= 5
#if ENABLE_TRACE
  int i;
  for (i = 0; i < cmdseq.num; i++)
    tvhtrace("linuxdvb", "S2CMD %02u => %u", cmds[i].cmd, cmds[i].u.data);
#endif
  r = ioctl(lfe->lfe_fe_fd, FE_SET_PROPERTY, &cmdseq);

  /* v3 tuning */
#else
  r = ioctl(lfe->lfe_fe_fd, FE_SET_FRONTEND, p);
#endif

  /* Failed */
  if (r != 0) {
    tvherror("linuxdvb", "%s - failed to tune [e=%s]", buf1, strerror(errno));
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
  int r;
  char buf1[256], buf2[256];

  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("linuxdvb", "%s - starting %s", buf1, buf2);

  /* Tune */
  tvhtrace("linuxdvb", "%s - tuning", buf1);
  r = linuxdvb_frontend_tune0(lfe, mmi, freq);

  /* Start monitor */
  if (!r) {
    time(&lfe->lfe_monitor);
    lfe->lfe_monitor += 4;
    gtimer_arm_ms(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 50);
  }
  
  return r;
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

linuxdvb_frontend_t *
linuxdvb_frontend_create
  ( htsmsg_t *conf, linuxdvb_adapter_t *la, int number,
    const char *fe_path, const char *dmx_path, const char *dvr_path,
    struct dvb_frontend_info *dfi )
{
  const idclass_t *idc;
  const char *uuid = NULL, *scuuid = NULL, *sctype = NULL;
  char id[12], name[256];
  linuxdvb_frontend_t *lfe;
  htsmsg_t *scconf = NULL;
  pthread_t tid;

  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(dfi->type), number);
  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  /* Class */
  if (dfi->type == FE_QPSK)
    idc = &linuxdvb_frontend_dvbs_class;
  else if (dfi->type == FE_QAM)
    idc = &linuxdvb_frontend_dvbc_class;
  else if (dfi->type == FE_OFDM)
    idc = &linuxdvb_frontend_dvbt_class;
  else if (dfi->type == FE_ATSC)
    idc = &linuxdvb_frontend_atsc_class;
  else {
    tvherror("linuxdvb", "unknown FE type %d", dfi->type);
    return NULL;
  }

  /* Create */
  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  lfe = calloc(1, sizeof(linuxdvb_frontend_t));
  lfe->lfe_number = number;
  memcpy(&lfe->lfe_info, dfi, sizeof(struct dvb_frontend_info));
  lfe = (linuxdvb_frontend_t*)mpegts_input_create0((mpegts_input_t*)lfe, idc, uuid, conf);
  if (!lfe) return NULL;

  /* Callbacks */
  lfe->mi_is_free    = linuxdvb_frontend_is_free;
  lfe->mi_get_weight = linuxdvb_frontend_get_weight;

  /* Default name */
  if (!lfe->mi_name) {
    snprintf(name, sizeof(name), "%s : %s", dfi->name, id);
    lfe->mi_name = strdup(name);
  }

  /* Set paths */
  lfe->lfe_fe_path  = strdup(fe_path);
  lfe->lfe_dmx_path = strdup(dmx_path);
  lfe->lfe_dvr_path = strdup(dvr_path);

  /* Input callbacks */
  lfe->mi_is_enabled     = linuxdvb_frontend_is_enabled;
  lfe->mi_start_mux      = linuxdvb_frontend_start_mux;
  lfe->mi_stop_mux       = linuxdvb_frontend_stop_mux;
  lfe->mi_network_list   = linuxdvb_frontend_network_list;
  lfe->mi_open_pid       = linuxdvb_frontend_open_pid;

  /* Adapter link */
  lfe->lfe_adapter = la;
  LIST_INSERT_HEAD(&la->la_frontends, lfe, lfe_link);

  /* DVR lock/cond */
  pthread_mutex_init(&lfe->lfe_dvr_lock, NULL);
  pthread_cond_init(&lfe->lfe_dvr_cond, NULL);
 
  /* Start table thread */
  tvhthread_create(&tid, NULL, mpegts_input_table_thread, lfe, 1);

  /* Satconf */
  if (conf) {
    if ((scconf = htsmsg_get_map(conf, "satconf"))) {
      sctype = htsmsg_get_str(scconf, "type");
      scuuid = htsmsg_get_str(scconf, "uuid");
    }
  }

  /* Create satconf */
  if (dfi->type == FE_QPSK && !lfe->lfe_satconf)
    lfe->lfe_satconf = linuxdvb_satconf_create(lfe, sctype, scuuid, scconf);

  return lfe;
}

void
linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *fe )
{
  char id[12];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)lfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->lfe_info.type));
  if (lfe->lfe_satconf) {
    htsmsg_t *s = htsmsg_create_map();
    linuxdvb_satconf_save(lfe->lfe_satconf, s);
    htsmsg_add_str(s, "uuid", idnode_uuid_as_str(&lfe->lfe_satconf->ls_id));
    htsmsg_add_msg(m, "satconf", s);
  }

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(lfe->lfe_info.type), lfe->lfe_number);
  htsmsg_add_msg(fe, id, m);
}

void
linuxdvb_frontend_delete ( linuxdvb_frontend_t *lfe )
{
  mpegts_mux_instance_t *mmi;

  lock_assert(&global_lock);

  /* Ensure we're stopped */
  if ((mmi = LIST_FIRST(&lfe->mi_mux_active)))
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1);

  /* Stop monitor */
  gtimer_disarm(&lfe->lfe_monitor_timer);

  /* Close FDs */
  if (lfe->lfe_fe_fd > 0)
    close(lfe->lfe_fe_fd);

  /* Remove from adapter */
  LIST_REMOVE(lfe, lfe_link);

  /* Free memory */
  free(lfe->lfe_fe_path);
  free(lfe->lfe_dmx_path);
  free(lfe->lfe_dvr_path);

  /* Delete satconf */
  if (lfe->lfe_satconf)
    linuxdvb_satconf_delete(lfe->lfe_satconf);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)lfe);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
