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
      .id       = "powersave",
      .name     = "Power Save",
      .off      = offsetof(linuxdvb_frontend_t, lfe_powersave),
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
  linuxdvb_satconf_delete(lfe->lfe_satconf, 1);
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
  .ic_caption    = "Linux DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

const idclass_t linuxdvb_frontend_atsc_class =
{
  .ic_super      = &linuxdvb_frontend_class,
  .ic_class      = "linuxdvb_frontend_atsc",
  .ic_caption    = "Linux ATSC Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
linuxdvb_frontend_enabled_updated ( mpegts_input_t *mi )
{
  char buf[512];
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;

  mi->mi_display_name(mi, buf, sizeof(buf));

  /* Ensure disabled */
  if (!mi->mi_enabled) {
    tvhtrace("linuxdvb", "%s - disabling tuner", buf);
    if (lfe->lfe_fe_fd > 0)
      close(lfe->lfe_fe_fd);
    gtimer_disarm(&lfe->lfe_monitor_timer);

  /* Ensure FE opened (if not powersave) */
  } else if (!lfe->lfe_powersave && lfe->lfe_fe_fd <= 0 && lfe->lfe_fe_path) {
    lfe->lfe_fe_fd = tvh_open(lfe->lfe_fe_path, O_RDWR | O_NONBLOCK, 0);
    tvhtrace("linuxdvb", "%s - opening FE %s (%d)",
             buf, lfe->lfe_fe_path, lfe->lfe_fe_fd);
  }
}

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
linuxdvb_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  int r = mpegts_input_get_priority(mi, mm);
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

  if (lfe->lfe_satconf)
    linuxdvb_satconf_post_stop_mux(lfe->lfe_satconf);
}

static int
linuxdvb_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)mi;
  if (lfe->lfe_satconf)
    return linuxdvb_satconf_start_mux(lfe->lfe_satconf, mmi);
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

  /* Not locked */
  if (!lfe->lfe_locked)
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

  if (lfe->lfe_type == DVB_TYPE_T)
    idc = &dvb_network_dvbt_class;
  else if (lfe->lfe_type == DVB_TYPE_C)
    idc = &dvb_network_dvbc_class;
  else if (lfe->lfe_type == DVB_TYPE_S)
    idc = &dvb_network_dvbs_class;
  else if (lfe->lfe_type == DVB_TYPE_ATSC)
    idc = &dvb_network_atsc_class;
  else
    return NULL;

  return idnode_find_all(idc);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static void
linuxdvb_frontend_default_tables 
  ( linuxdvb_frontend_t *lfe, dvb_mux_t *lm )
{
  mpegts_mux_t *mm = (mpegts_mux_t*)lm;

  psi_tables_default(mm);

  /* ATSC */
  if (lfe->lfe_type == DVB_TYPE_ATSC) {
    if (lm->lm_tuning.dmc_fe_modulation == DVB_MOD_VSB_8)
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
  signal_status_t sigstat;
  streaming_message_t sm;
  service_t *s;
#if DVB_VER_ATLEAST(5,10)
  struct dtv_property fe_properties[6];
  struct dtv_properties dtv_prop;
#endif

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));
  tvhtrace("linuxdvb", "%s - checking FE status", buf);
  
  /* Disabled */
  if (!lfe->mi_enabled && mmi)
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1);

  /* Close FE */
  if (lfe->lfe_fe_fd > 0 && !mmi && lfe->lfe_powersave) {
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

  /* Stop timer */
  if (!mmi) return;

  /* re-arm */
  gtimer_arm(&lfe->lfe_monitor_timer, linuxdvb_frontend_monitor, lfe, 1);

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
      linuxdvb_frontend_default_tables(lfe, (dvb_mux_t*)mm);

      /* Locked - ensure everything is open */
      pthread_mutex_lock(&lfe->mi_output_lock);
      RB_FOREACH(mp, &mm->mm_pids, mp_link)
        linuxdvb_frontend_open_pid0(lfe, mp);
      pthread_mutex_unlock(&lfe->mi_output_lock);

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

  /* Send message */
  sigstat.status_text = signal2str(status);
  sigstat.snr         = mmi->mmi_stats.snr;
  sigstat.signal      = mmi->mmi_stats.signal;
  sigstat.ber         = mmi->mmi_stats.ber;
  sigstat.unc         = mmi->mmi_stats.unc;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;
  LIST_FOREACH(s, &lfe->mi_transports, s_active_link) {
    pthread_mutex_lock(&s->s_stream_mutex);
    streaming_pad_deliver(&s->s_streaming_pad, &sm);
    pthread_mutex_unlock(&s->s_stream_mutex);
  }
}

static void *
linuxdvb_frontend_input_thread ( void *aux )
{
  linuxdvb_frontend_t *lfe = aux;
  mpegts_mux_instance_t *mmi;
  int dmx = -1, dvr = -1;
  char buf[256];
  int nfds;
  tvhpoll_event_t ev[2];
  tvhpoll_t *efd;
  sbuf_t sb;

  /* Get MMI */
  pthread_mutex_lock(&lfe->lfe_dvr_lock);
  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));
  mmi = LIST_FIRST(&lfe->mi_mux_active);
  pthread_cond_signal(&lfe->lfe_dvr_cond);
  pthread_mutex_unlock(&lfe->lfe_dvr_lock);
  if (mmi == NULL) return NULL;

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

  /* Allocate memory */
  sbuf_init_fixed(&sb, 18800);

  /* Read */
  while (tvheadend_running) {
    nfds = tvhpoll_wait(efd, ev, 1, -1);
    if (nfds < 1) continue;
    if (ev[0].data.fd != dvr) break;
    
    /* Read */
    if (sbuf_read(&sb, dvr) < 0) {
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
    mpegts_input_recv_packets((mpegts_input_t*)lfe, mmi, &sb, 0, NULL, NULL);
  }

  sbuf_free(&sb);
  tvhpoll_destroy(efd);
  if (dmx != -1) close(dmx);
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

static int
linuxdvb2tvh ( const char *prefix, linuxdvb_tbl_t *tbl, int key, int defval )
{
  while (tbl->t != TABLE_EOD) {
    if (tbl->l == key)
      return tbl->t;
    tbl++;
  }
  tvhtrace("linuxdvb", "%s - linuxdvb2tvh failed %d", prefix, key);
  return defval;
}

static int
tvh2linuxdvb ( const char *prefix, linuxdvb_tbl_t *tbl, int key, int defval )
{
  while (tbl->t != TABLE_EOD) {
    if (tbl->t == key)
      return tbl->l;
    tbl++;
  }
  tvhtrace("linuxdvb", "%s - tvh2linuxdvb failed %d", prefix, key);
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
    { .l = TABLE_EOD }
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
  mpegts_mux_instance_t *cur = LIST_FIRST(&lfe->mi_mux_active);
  dvb_mux_t *lm = (dvb_mux_t*)mmi->mmi_mux;
  dvb_mux_conf_t *dmc;
  struct dvb_frontend_parameters p;

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

  /*
   * copy the universal parameters to the Linux kernel structure
   */
  dmc = &lm->lm_tuning;
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
    p.u.qam.symbol_rate      = dmc->u.dmc_fe_qam.symbol_rate;
    p.u.qam.fec_inner        = TRU(qam.fec_inner, fec_tbl, FEC_AUTO);
    p.u.qam.modulation       = TR(modulation, mod_tbl, QAM_AUTO);
    break;
  case DVB_TYPE_S:
    p.u.qpsk.symbol_rate     = dmc->u.dmc_fe_qpsk.symbol_rate;
    p.u.qpsk.fec_inner       = TRU(qpsk.fec_inner, fec_tbl, FEC_AUTO);
    break;
  case DVB_TYPE_ATSC:
    p.u.vsb.modulation       = TR(modulation, mod_tbl, QAM_AUTO);
    break;
  default:
    tvherror("linuxdvb", "%s - unknown FE type %d", buf1, dmc->dmc_fe_type);
    return SM_CODE_TUNING_FAILED;
  }

  if (freq != (uint32_t)-1)
    p.frequency = freq;

  if (dmc->dmc_fe_type != lfe->lfe_type) {
    tvherror("linuxdvb", "%s - failed to tune [type does not match %i != %i]", buf1, dmc->dmc_fe_type, lfe->lfe_type);
    return SM_CODE_TUNING_FAILED;
  }

  /* S2 tuning */
#if DVB_API_VERSION >= 5
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
    freq = p.frequency;

  
  /* Tune */
#define S2CMD(c, d)\
  cmds[cmdseq.num].cmd      = c;\
  cmds[cmdseq.num++].u.data = d
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

  /* DVB-C */
  } else if (lfe->lfe_type == DVB_TYPE_C) {
    S2CMD(DTV_SYMBOL_RATE,       p.u.qam.symbol_rate);
    S2CMD(DTV_MODULATION,        p.u.qam.modulation);
    S2CMD(DTV_INNER_FEC,         p.u.qam.fec_inner);

  /* DVB-S */
  } else if (lfe->lfe_type == DVB_TYPE_S) {
    S2CMD(DTV_SYMBOL_RATE,       p.u.qpsk.symbol_rate);
    S2CMD(DTV_INNER_FEC,         p.u.qpsk.fec_inner);
    S2CMD(DTV_PILOT,             TR(pilot, pilot_tbl, PILOT_AUTO));
    S2CMD(DTV_MODULATION,        TR(modulation, mod_tbl, QPSK));
    if (lm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBS) {
      S2CMD(DTV_ROLLOFF,         ROLLOFF_35);
    } else {
      S2CMD(DTV_ROLLOFF,         TR(rolloff, rolloff_tbl, ROLLOFF_AUTO));
    }

  /* ATSC */
  } else {
    S2CMD(DTV_MODULATION,        p.u.vsb.modulation);
  }

  /* Tune */
  S2CMD(DTV_TUNE, 0);
#undef S2CMD
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
    dvb_fe_type_t type, const char *name )
{
  const idclass_t *idc;
  const char *str, *uuid = NULL, *scuuid = NULL, *sctype = NULL;
  char id[12], lname[256];
  linuxdvb_frontend_t *lfe;
  htsmsg_t *scconf = NULL;

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
    idc = &linuxdvb_frontend_dvbs_class;
  else if (type == DVB_TYPE_C)
    idc = &linuxdvb_frontend_dvbc_class;
  else if (type == DVB_TYPE_T)
    idc = &linuxdvb_frontend_dvbt_class;
  else if (type == DVB_TYPE_ATSC)
    idc = &linuxdvb_frontend_atsc_class;
  else {
    tvherror("linuxdvb", "unknown FE type %d", type);
    return NULL;
  }

  /* Create */
  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  lfe = calloc(1, sizeof(linuxdvb_frontend_t));
  lfe->lfe_number = number;
  lfe->lfe_type = type;
  strncpy(lfe->lfe_name, name, sizeof(lfe->lfe_name));
  lfe->lfe_name[sizeof(lfe->lfe_name)-1] = '\0';
  lfe = (linuxdvb_frontend_t*)mpegts_input_create0((mpegts_input_t*)lfe, idc, uuid, conf);
  if (!lfe) return NULL;

  /* Callbacks */
  lfe->mi_is_free      = linuxdvb_frontend_is_free;
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
  lfe->mi_is_enabled      = linuxdvb_frontend_is_enabled;
  lfe->mi_start_mux       = linuxdvb_frontend_start_mux;
  lfe->mi_stop_mux        = linuxdvb_frontend_stop_mux;
  lfe->mi_network_list    = linuxdvb_frontend_network_list;
  lfe->mi_open_pid        = linuxdvb_frontend_open_pid;
  lfe->mi_enabled_updated = linuxdvb_frontend_enabled_updated;

  /* Adapter link */
  lfe->lfe_adapter = la;
  LIST_INSERT_HEAD(&la->la_frontends, lfe, lfe_link);

  /* DVR lock/cond */
  pthread_mutex_init(&lfe->lfe_dvr_lock, NULL);
  pthread_cond_init(&lfe->lfe_dvr_cond, NULL);
 
  /* Satconf */
  if (conf) {
    if ((scconf = htsmsg_get_map(conf, "satconf"))) {
      sctype = htsmsg_get_str(scconf, "type");
      scuuid = htsmsg_get_str(scconf, "uuid");
    }
  }

  /* Create satconf */
  if (lfe->lfe_type == DVB_TYPE_S && !lfe->lfe_satconf)
    lfe->lfe_satconf = linuxdvb_satconf_create(lfe, sctype, scuuid, scconf);

  /* Double check enabled */
  linuxdvb_frontend_enabled_updated((mpegts_input_t*)lfe);

  return lfe;
}

void
linuxdvb_frontend_save ( linuxdvb_frontend_t *lfe, htsmsg_t *fe )
{
  char id[12];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)lfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->lfe_type));
  if (lfe->lfe_satconf) {
    htsmsg_t *s = htsmsg_create_map();
    linuxdvb_satconf_save(lfe->lfe_satconf, s);
    htsmsg_add_str(s, "uuid", idnode_uuid_as_str(&lfe->lfe_satconf->ls_id));
    htsmsg_add_msg(m, "satconf", s);
  }

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(lfe->lfe_type), lfe->lfe_number);
  htsmsg_add_msg(fe, id, m);
}

void
linuxdvb_frontend_delete ( linuxdvb_frontend_t *lfe )
{
  lock_assert(&global_lock);

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)lfe);

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
    linuxdvb_satconf_delete(lfe->lfe_satconf, 0);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)lfe, 0);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
