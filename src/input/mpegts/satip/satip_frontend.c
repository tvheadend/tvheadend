/*
 *  Tvheadend - SAT>IP DVB frontend
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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

#include <fcntl.h>
#include "tvheadend.h"
#include "tvhpoll.h"
#include "streaming.h"
#include "http.h"
#include "satip_private.h"

#if defined(PLATFORM_FREEBSD) || ENABLE_ANDROID
#include <sys/types.h>
#include <sys/socket.h>
#endif

/*
 *
 */
static satip_frontend_t *
satip_frontend_find_by_number( satip_device_t *sd, int num )
{
  satip_frontend_t *lfe;

  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    if (lfe->sf_number == num)
      return lfe;
  return NULL;
}

static char *
satip_frontend_bindaddr( satip_frontend_t *lfe )
{
  char *bindaddr = lfe->sf_tuner_bindaddr;
  if (bindaddr == NULL || bindaddr[0] == '\0')
    bindaddr = lfe->sf_device->sd_bindaddr;
  return bindaddr;
}

static void
satip_frontend_signal_cb( void *aux )
{
  satip_frontend_t      *lfe = aux;
  mpegts_mux_instance_t *mmi = LIST_FIRST(&lfe->mi_mux_active);
  streaming_message_t    sm;
  signal_status_t        sigstat;
  service_t             *svc;

  if (mmi == NULL)
    return;
  if (!lfe->sf_tables) {
    psi_tables_install(mmi->mmi_input, mmi->mmi_mux,
                       ((dvb_mux_t *)mmi->mmi_mux)->lm_tuning.dmc_fe_delsys);
    lfe->sf_tables = 1;
  }
  sigstat.status_text  = signal2str(lfe->sf_status);
  sigstat.snr          = mmi->tii_stats.snr;
  sigstat.signal       = mmi->tii_stats.signal;
  sigstat.ber          = mmi->tii_stats.ber;
  sigstat.unc          = mmi->tii_stats.unc;
  sigstat.signal_scale = mmi->tii_stats.signal_scale;
  sigstat.snr_scale    = mmi->tii_stats.snr_scale;
  sigstat.ec_bit       = mmi->tii_stats.ec_bit;
  sigstat.tc_bit       = mmi->tii_stats.tc_bit;
  sigstat.ec_block     = mmi->tii_stats.ec_block;
  sigstat.tc_block     = mmi->tii_stats.tc_block;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;
  LIST_FOREACH(svc, &mmi->mmi_mux->mm_transports, s_active_link) {
    pthread_mutex_lock(&svc->s_stream_mutex);
    streaming_pad_deliver(&svc->s_streaming_pad, streaming_msg_clone(&sm));
    pthread_mutex_unlock(&svc->s_stream_mutex);
  }
  gtimer_arm_ms(&lfe->sf_monitor_timer, satip_frontend_signal_cb, lfe, 250);
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
satip_frontend_class_save ( idnode_t *in )
{
  satip_device_t *la = ((satip_frontend_t*)in)->sf_device;
  satip_device_save(la);
}

static int
satip_frontend_set_new_type
  ( satip_frontend_t *lfe, const char *type )
{
  free(lfe->sf_type_override);
  lfe->sf_type_override = strdup(type);
  satip_device_destroy_later(lfe->sf_device, 100);
  return 1;
}

static int
satip_frontend_class_override_set( void *obj, const void * p )
{
  satip_frontend_t *lfe = obj;
  const char *s = p;

  if (lfe->sf_type_override == NULL) {
    if (strlen(p) > 0)
      return satip_frontend_set_new_type(lfe, s);
  } else if (strcmp(lfe->sf_type_override, s))
    return satip_frontend_set_new_type(lfe, s);
  return 0;
}

static htsmsg_t *
satip_frontend_class_override_enum( void * p )
{
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "DVB-T");
  htsmsg_add_str(m, NULL, "DVB-C");
  return m;
}

const idclass_t satip_frontend_class =
{
  .ic_super      = &mpegts_input_class,
  .ic_class      = "satip_frontend",
  .ic_caption    = "SAT>IP DVB Frontend",
  .ic_save       = satip_frontend_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_INT,
      .id       = "fe_number",
      .name     = "Frontend Number",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .off      = offsetof(satip_frontend_t, sf_number),
    },
    {
      .type     = PT_INT,
      .id       = "udp_rtp_port",
      .name     = "UDP RTP Port Number (2 ports)",
      .off      = offsetof(satip_frontend_t, sf_udp_rtp_port),
    },
    {
      .type     = PT_INT,
      .id       = "tdelay",
      .name     = "Next tune delay in ms (0-2000)",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_frontend_t, sf_tdelay),
    },
    {
      .type     = PT_BOOL,
      .id       = "play2",
      .name     = "Send full PLAY cmd",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_frontend_t, sf_play2),
    },
    {
      .type     = PT_BOOL,
      .id       = "teardown_delay",
      .name     = "Force teardown delay",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_frontend_t, sf_teardown_delay),
    },
    {
      .type     = PT_STR,
      .id       = "tunerbindaddr",
      .name     = "Tuner bind IP address",
      .opts     = PO_ADVANCED,
      .off      = offsetof(satip_frontend_t, sf_tuner_bindaddr),
    },
    {}
  }
};

const idclass_t satip_frontend_dvbt_class =
{
  .ic_super      = &satip_frontend_class,
  .ic_class      = "satip_frontend_dvbt",
  .ic_caption    = "SAT>IP DVB-T Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "fe_override",
      .name     = "Network Type",
      .set      = satip_frontend_class_override_set,
      .list     = satip_frontend_class_override_enum,
      .off      = offsetof(satip_frontend_t, sf_type_override),
    },
    {}
  }
};

static idnode_set_t *
satip_frontend_dvbs_class_get_childs ( idnode_t *self )
{
  satip_frontend_t   *lfe = (satip_frontend_t*)self;
  idnode_set_t        *is  = idnode_set_create(0);
  satip_satconf_t *sfc;
  TAILQ_FOREACH(sfc, &lfe->sf_satconf, sfc_link)
    idnode_set_add(is, &sfc->sfc_id, NULL);
  return is;
}

static int
satip_frontend_dvbs_class_positions_set ( void *self, const void *val )
{
  satip_frontend_t *lfe = self;
  int n = *(int *)val;

  if (n < 0 || n > 32)
    return 0;
  if (n != lfe->sf_positions) {
    lfe->sf_positions = n;
    satip_satconf_updated_positions(lfe);
    return 1;
  }
  return 0;
}

static int
satip_frontend_dvbs_class_master_set ( void *self, const void *val )
{
  satip_frontend_t *lfe  = self;
  satip_frontend_t *lfe2 = NULL;
  int num = *(int *)val, pos = 0;

  if (num) {
    TAILQ_FOREACH(lfe2, &lfe->sf_device->sd_frontends, sf_link)
      if (lfe2 != lfe && lfe2->sf_type == lfe->sf_type)
        if (++pos == num) {
          num = lfe2->sf_number;
          break;
        }
  }
  if (lfe2 == NULL)
    num = 0;
  else if (lfe2->sf_master)
    num = lfe2->sf_master;
  if (lfe->sf_master != num) {
    lfe->sf_master = num;
    satip_device_destroy_later(lfe->sf_device, 100);
    return 1;
  }
  return 0;
}

static htsmsg_t *
satip_frontend_dvbs_class_master_enum( void * self )
{
  satip_frontend_t *lfe = self, *lfe2;
  satip_device_t *sd = lfe->sf_device;
  htsmsg_t *m = htsmsg_create_list();
  htsmsg_add_str(m, NULL, "This Tuner");
  TAILQ_FOREACH(lfe2, &sd->sd_frontends, sf_link)
    if (lfe2 != lfe && lfe2->sf_type == lfe->sf_type)
      htsmsg_add_str(m, NULL, lfe2->mi_name);
  return m;
}

const idclass_t satip_frontend_dvbs_class =
{
  .ic_super      = &satip_frontend_class,
  .ic_class      = "satip_frontend_dvbs",
  .ic_caption    = "SAT>IP DVB-S Frontend",
  .ic_get_childs = satip_frontend_dvbs_class_get_childs,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "positions",
      .name     = "Satellite Positions",
      .set      = satip_frontend_dvbs_class_positions_set,
      .opts     = PO_NOSAVE,
      .off      = offsetof(satip_frontend_t, sf_positions),
      .def.i    = 4
    },
    {
      .type     = PT_INT,
      .id       = "fe_master",
      .name     = "Master Tuner",
      .set      = satip_frontend_dvbs_class_master_set,
      .list     = satip_frontend_dvbs_class_master_enum,
      .off      = offsetof(satip_frontend_t, sf_master),
    },
    {
      .id       = "networks",
      .type     = PT_NONE,
    },
    {}
  }
};

const idclass_t satip_frontend_dvbs_slave_class =
{
  .ic_super      = &satip_frontend_class,
  .ic_class      = "satip_frontend_dvbs",
  .ic_caption    = "SAT>IP DVB-S Slave Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "fe_master",
      .name     = "Master Tuner",
      .set      = satip_frontend_dvbs_class_master_set,
      .list     = satip_frontend_dvbs_class_master_enum,
      .off      = offsetof(satip_frontend_t, sf_master),
    },
    {
      .id       = "networks",
      .type     = PT_NONE,
    },
    {}
  }
};

const idclass_t satip_frontend_dvbc_class =
{
  .ic_super      = &satip_frontend_class,
  .ic_class      = "satip_frontend_dvbc",
  .ic_caption    = "SAT>IP DVB-C Frontend",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "fe_override",
      .name     = "Network Type",
      .set      = satip_frontend_class_override_set,
      .list     = satip_frontend_class_override_enum,
      .off      = offsetof(satip_frontend_t, sf_type_override),
    },
    {}
  }
};

const idclass_t satip_frontend_atsc_class =
{
  .ic_super      = &satip_frontend_class,
  .ic_class      = "satip_frontend_atsc",
  .ic_caption    = "SAT>IP ATSC Frontend",
  .ic_properties = (const property_t[]){
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
satip_frontend_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  return mpegts_input_get_weight(mi, mm, flags);
}

static int
satip_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  int r = mpegts_input_get_priority(mi, mm, flags);
  if (lfe->sf_positions)
    r += satip_satconf_get_priority(lfe, mm);
  return r;
}

static int
satip_frontend_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  int r = 5;
  if (lfe->sf_positions || lfe->sf_master)
    r = MIN(60, MAX(r, satip_satconf_get_grace(lfe, mm) ?: 10));
  return r;
}

static int
satip_frontend_match_satcfg ( satip_frontend_t *lfe2, mpegts_mux_t *mm2 )
{
  satip_frontend_t *lfe_master;
  mpegts_mux_t *mm1;
  dvb_mux_conf_t *mc1, *mc2;
  int position, high1, high2;

  if (lfe2->sf_req == NULL || lfe2->sf_req->sf_mmi == NULL)
    return 0;

  lfe_master = lfe2;
  if (lfe2->sf_master)
    lfe_master = satip_frontend_find_by_number(lfe2->sf_device, lfe2->sf_master) ?: lfe2;

  mm1 = lfe2->sf_req->sf_mmi->mmi_mux;
  position = satip_satconf_get_position(lfe2, mm2);
  if (position <= 0 || lfe_master->sf_position != position)
    return 0;
  mc1 = &((dvb_mux_t *)mm1)->lm_tuning;
  mc2 = &((dvb_mux_t *)mm2)->lm_tuning;
  if (mc1->dmc_fe_type != DVB_TYPE_S || mc2->dmc_fe_type != DVB_TYPE_S)
    return 0;
  if (mc1->u.dmc_fe_qpsk.polarisation != mc2->u.dmc_fe_qpsk.polarisation)
    return 0;
  high1 = mc1->dmc_fe_freq > 11700000;
  high2 = mc2->dmc_fe_freq > 11700000;
  if (high1 != high2)
    return 0;
  return 1;
}

static int
satip_frontend_is_enabled ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  satip_frontend_t *lfe2;
  int position;

  lock_assert(&global_lock);

  if (!mpegts_input_is_enabled(mi, mm, flags)) return 0;
  if (lfe->sf_device->sd_dbus_allow <= 0) return 0;
  if (lfe->sf_type != DVB_TYPE_S) return 1;
  /* check if the position is enabled */
  position = satip_satconf_get_position(lfe, mm);
  if (position <= 0)
    return 0;
  /* check if any "blocking" tuner is running */
  TAILQ_FOREACH(lfe2, &lfe->sf_device->sd_frontends, sf_link) {
    if (lfe2 == lfe) continue;
    if (lfe2->sf_type != DVB_TYPE_S) continue;
    if (lfe->sf_master == lfe2->sf_number) {
      if (!lfe2->sf_running)
        return 0; /* master must be running */
      return satip_frontend_match_satcfg(lfe2, mm);
    }
    if (lfe2->sf_master == lfe->sf_number && lfe2->sf_running) {
      if (lfe2->sf_req == NULL || lfe2->sf_req->sf_mmi == NULL)
        return 0;
      return satip_frontend_match_satcfg(lfe2, mm);
    }
  }
  return 1;
}

static void
satip_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  satip_tune_req_t *tr;
  char buf1[256], buf2[256];

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("satip", "%s - stopping %s", buf1, buf2);

  gtimer_disarm(&lfe->sf_monitor_timer);

  /* Stop tune */
  tvh_write(lfe->sf_dvr_pipe.wr, "", 1);

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  tr = lfe->sf_req;
  if (tr && tr != lfe->sf_req_thread) {
    mpegts_pid_done(&tr->sf_pids_tuned);
    mpegts_pid_done(&tr->sf_pids);
    free(tr);
  }
  lfe->sf_running  = 0;
  lfe->sf_req = NULL;
  pthread_mutex_unlock(&lfe->sf_dvr_lock);
}

static int
satip_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  dvb_mux_t *lm = (dvb_mux_t *)mmi->mmi_mux;
  satip_tune_req_t *tr;
  char buf1[256], buf2[256];

  if (lfe->sf_positions > 0) {
    lfe->sf_position = satip_satconf_get_position(lfe, mmi->mmi_mux);
    if (lfe->sf_position <= 0)
      return SM_CODE_TUNING_FAILED;
  }

  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  mpegts_mux_nice_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("satip", "%s - starting %s", buf1, buf2);

  if (!lfe->sf_device->sd_no_univ_lnb &&
      (lm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBS ||
       lm->lm_tuning.dmc_fe_delsys == DVB_SYS_DVBS2)) {
    /* Note: assume universal LNB */
    if (lm->lm_tuning.dmc_fe_freq < 10700000 ||
        lm->lm_tuning.dmc_fe_freq > 12750000) {
      tvhwarn("satip", "DVB-S/S2 frequency %d out of range universal LNB", lm->lm_tuning.dmc_fe_freq);
      return SM_CODE_TUNING_FAILED;
    }
  }

  tr = calloc(1, sizeof(*tr));
  tr->sf_mmi        = mmi;

  mpegts_pid_init(&tr->sf_pids);
  mpegts_pid_init(&tr->sf_pids_tuned);

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  lfe->sf_req       = tr;
  lfe->sf_running   = 1;
  lfe->sf_tables    = 0;
  lfe->sf_atsc_c    = lm->lm_tuning.dmc_fe_modulation != DVB_MOD_VSB_8;
  lfe->sf_status    = SIGNAL_NONE;
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  /* notify thread that we are ready */
  tvh_write(lfe->sf_dvr_pipe.wr, "s", 1);

  gtimer_arm_ms(&lfe->sf_monitor_timer, satip_frontend_signal_cb, lfe, 50);

  return 0;
}

static void
satip_frontend_update_pids
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  satip_tune_req_t *tr;
  mpegts_pid_t *mp;
  mpegts_pid_sub_t *mps;

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if ((tr = lfe->sf_req) != NULL) {
    mpegts_pid_done(&tr->sf_pids);
    RB_FOREACH(mp, &mm->mm_pids, mp_link) {
      if (mp->mp_pid == MPEGTS_FULLMUX_PID) {
        if (lfe->sf_device->sd_fullmux_ok) {
          if (!tr->sf_pids.all)
            tr->sf_pids.all = 1;
        } else {
          mpegts_service_t *s;
          elementary_stream_t *st;
          int w = 0;
          RB_FOREACH(mps, &mp->mp_subs, mps_link)
            w = MAX(w, mps->mps_weight);
          LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
            mpegts_pid_add(&tr->sf_pids, s->s_pmt_pid, w);
            mpegts_pid_add(&tr->sf_pids, s->s_pcr_pid, w);
            TAILQ_FOREACH(st, &s->s_components, es_link)
              mpegts_pid_add(&tr->sf_pids, st->es_pid, w);
          }
        }
      } else if (mp->mp_pid < MPEGTS_FULLMUX_PID) {
        RB_FOREACH(mps, &mp->mp_subs, mps_link)
          mpegts_pid_add(&tr->sf_pids, mp->mp_pid, mps->mps_weight);
      }
    }
  }
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  tvh_write(lfe->sf_dvr_pipe.wr, "c", 1);
}

static idnode_set_t *
satip_frontend_network_list ( mpegts_input_t *mi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  const idclass_t     *idc;

  if (lfe->sf_type == DVB_TYPE_T)
    idc = &dvb_network_dvbt_class;
  else if (lfe->sf_type == DVB_TYPE_S)
    idc = &dvb_network_dvbs_class;
  else if (lfe->sf_type == DVB_TYPE_C)
    idc = &dvb_network_dvbc_class;
  else if (lfe->sf_type == DVB_TYPE_ATSC)
    idc = &dvb_network_atsc_class;
  else
    return NULL;

  return idnode_find_all(idc, NULL);
}

/* **************************************************************************
 * Data processing
 * *************************************************************************/

static void
satip_frontend_decode_rtcp( satip_frontend_t *lfe, const char *name,
                            mpegts_mux_instance_t *mmi,
                            uint8_t *rtcp, size_t len )
{
  signal_state_t status;
  uint16_t l, sl;
  char *s;
  char *argv[4];
  int n;

  /*
   * DVB-S/S2:
   * ver=<major>.<minor>;src=<srcID>;tuner=<feID>,<level>,<lock>,<quality>,\
   * <frequency>,<polarisation>,<system>,<type>,<pilots>,<roll_off>,
   * <symbol_rate>,<fec_inner>;pids=<pid0>,...,<pidn>
   *
   * DVB-T:
   * ver=1.1;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<tmode>,\
   * <mtype>,<gi>,<fec>,<plp>,<t2id>,<sm>;pids=<pid0>,...,<pidn>
   *
   * DVB-C:
   * ver=1.2;tuner=<feID>,<level>,<lock>,<quality>,<freq>,<bw>,<msys>,<mtype>,\
   * <sr>,<c2tft>,<ds>,<plp>,<specinv>;pids=<pid0>,...,<pidn>
   *
   * DVB-C (OctopusNet):
   * ver=0.9;tuner=<feID>,<0>,<lock>,<0>,<freq>,<bw>,<msys>,<mtype>;pids=<pid0>,...<pidn>
   * example:
   * ver=0.9;tuner=1,0,1,0,362.000,6900,dvbc,256qam;pids=0,1,16,17,18
   */

  /* level:
   * Numerical value between 0 and 255
   * An incoming L-band satellite signal of
   * -25dBm corresponds to 224
   * -65dBm corresponds to 32
   *  No signal corresponds to 0
   *
   * lock:
   * lock Set to one of the following values:
   * "0" the frontend is not locked   
   * "1" the frontend is locked
   *
   * quality:
   * Numerical value between 0 and 15
   * Lowest value corresponds to highest error rate
   * The value 15 shall correspond to
   * -a BER lower than 2x10-4 after Viterbi for DVB-S
   * -a PER lower than 10-7 for DVB-S2
   */
  while (len >= 12) {
    if ((rtcp[0] & 0xc0) != 0x80)	        /* protocol version: v2 */
      return;
    l = (((rtcp[2] << 8) | rtcp[3]) + 1) * 4;   /* length of payload */
    if (rtcp[1]  ==  204 && l > 20 &&           /* packet type */
        rtcp[8]  == 'S'  && rtcp[9]  == 'E' &&
        rtcp[10] == 'S'  && rtcp[11] == '1') {
      /* workaround for broken minisatip */
      if (l > len && l - 4 != len)
        return;
      sl = (rtcp[14] << 8) | rtcp[15];
      if (sl > 0 && l - 16 >= sl) {
        rtcp[sl + 16] = '\0';
        s = (char *)rtcp + 16;
        tvhtrace("satip", "Status string: '%s'", s);
        status = SIGNAL_NONE;
        if (strncmp(s, "ver=0.9;tuner=", 14) == 0 ||
            strncmp(s, "ver=1.2;tuner=", 14) == 0) {
          n = http_tokenize(s + 14, argv, 4, ',');
          if (n < 4)
            return;
          if (atoi(argv[0]) != lfe->sf_number)
            return;
          mmi->tii_stats.signal =
            atoi(argv[1]) * 0xffff / lfe->sf_device->sd_sig_scale;
          mmi->tii_stats.signal_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          if (atoi(argv[2]) > 0)
            status = SIGNAL_GOOD;
          mmi->tii_stats.snr = atoi(argv[3]) * 0xffff / 15;
          mmi->tii_stats.snr_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          if (status == SIGNAL_GOOD &&
              mmi->tii_stats.signal == 0 && mmi->tii_stats.snr == 0) {
            /* some values that we're tuned */
            mmi->tii_stats.signal = 50 * 0xffff / 100;
            mmi->tii_stats.snr = 12 * 0xffff / 15;
          }
          goto ok;          
        } else if (strncmp(s, "ver=1.0;", 8) == 0) {
          if ((s = strstr(s + 8, ";tuner=")) == NULL)
            return;
          s += 7;
          n = http_tokenize(s, argv, 4, ',');
          if (n < 4)
            return;
          if (atoi(argv[0]) != lfe->sf_number)
            return;
          mmi->tii_stats.signal =
            atoi(argv[1]) * 0xffff / lfe->sf_device->sd_sig_scale;
          mmi->tii_stats.signal_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          if (atoi(argv[2]) > 0)
            status = SIGNAL_GOOD;
          mmi->tii_stats.snr = atoi(argv[3]) * 0xffff / 15;
          mmi->tii_stats.snr_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          goto ok;          
        } else if (strncmp(s, "ver=1.1;tuner=", 14) == 0) {
          n = http_tokenize(s + 14, argv, 4, ',');
          if (n < 4)
            return;
          if (atoi(argv[0]) != lfe->sf_number)
            return;
          mmi->tii_stats.signal =
            atoi(argv[1]) * 0xffff / lfe->sf_device->sd_sig_scale;
          mmi->tii_stats.signal_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          if (atoi(argv[2]) > 0)
            status = SIGNAL_GOOD;
          mmi->tii_stats.snr = atoi(argv[3]) * 0xffff / 15;
          mmi->tii_stats.snr_scale =
            SIGNAL_STATUS_SCALE_RELATIVE;
          goto ok;
        }
      }
    }
    rtcp += l;
    len -= l;
  }
  return;

ok:
  if (mmi->tii_stats.snr < 2 && status == SIGNAL_GOOD)
    status              = SIGNAL_BAD;
  else if (mmi->tii_stats.snr < 4 && status == SIGNAL_GOOD)
    status              = SIGNAL_FAINT;
  lfe->sf_status        = status;
}

static int
satip_frontend_pid_changed( http_client_t *rtsp,
                            satip_frontend_t *lfe, const char *name )
{
  satip_tune_req_t *tr;
  char *add, *del;
  int i, j, r;
  int max_pids_len = lfe->sf_device->sd_pids_len;
  int max_pids_count = lfe->sf_device->sd_pids_max;
  mpegts_apids_t wpid, padd, pdel;

  pthread_mutex_lock(&lfe->sf_dvr_lock);

  tr = lfe->sf_req_thread;

  if (!lfe->sf_running || !lfe->sf_req || !tr) {
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
    return 0;
  }

  if (tr->sf_pids.all) {

all:
    i = tr->sf_pids_tuned.all;
    mpegts_pid_done(&tr->sf_pids_tuned);
    tr->sf_pids_tuned.all = 1;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
    if (i)
      return 0;

    r = satip_rtsp_play(rtsp, "all", NULL, NULL, max_pids_len);
    r = r == 0 ? 1 : r;

  } else if (!lfe->sf_device->sd_pids_deladd ||
             tr->sf_pids_tuned.all ||
             tr->sf_pids.count == 0) {

    mpegts_pid_weighted(&wpid, &tr->sf_pids, lfe->sf_device->sd_pids_max);
    j = MIN(wpid.count, lfe->sf_device->sd_pids_max);
    add = alloca(1 + j * 5);
    add[0] = '\0';
    for (i = 0; i < j; i++)
      sprintf(add + strlen(add), ",%i", wpid.pids[i].pid);
    mpegts_pid_copy(&tr->sf_pids_tuned, &wpid);
    tr->sf_pids_tuned.all = 0;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
    mpegts_pid_done(&wpid);

    if (!j || add[0] == '\0')
      return 0;

    r = satip_rtsp_play(rtsp, add, NULL, NULL, max_pids_len);
    r = r == 0 ? 1 : r;

  } else {

    mpegts_pid_weighted(&wpid, &tr->sf_pids, lfe->sf_device->sd_pids_max);

    if (wpid.count > max_pids_count) {
      if (lfe->sf_device->sd_fullmux_ok) {
        mpegts_pid_done(&wpid);
        goto all;
      }
    }

    mpegts_pid_compare(&wpid, &tr->sf_pids_tuned, &padd, &pdel);

    add = alloca(1 + padd.count * 5);
    del = alloca(1 + pdel.count * 5);
    add[0] = del[0] = '\0';

    for (i = 0; i < pdel.count; i++) {
      sprintf(del + strlen(del), ",%i", pdel.pids[i].pid);
      mpegts_pid_del(&tr->sf_pids_tuned, pdel.pids[i].pid, pdel.pids[i].weight);
    }

    j = MIN(padd.count, lfe->sf_device->sd_pids_max);
    for (i = 0; i < j; i++)
      sprintf(add + strlen(add), ",%i", padd.pids[i].pid);

    mpegts_pid_copy(&tr->sf_pids_tuned, &wpid);
    tr->sf_pids_tuned.all = 0;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);

    mpegts_pid_done(&wpid);
    mpegts_pid_done(&padd);
    mpegts_pid_done(&pdel);

    if (add[0] == '\0' && del[0] == '\0')
      return 0;

    r = satip_rtsp_play(rtsp, NULL, add, del, max_pids_len);
    r = r == 0 ? 1 : r;
  }

  if (r < 0)
    tvherror("satip", "%s - failed to modify pids: %s", name, strerror(-r));
  return r;
}

static void
satip_frontend_extra_shutdown
  ( satip_frontend_t *lfe, uint8_t *session, long stream_id )
{
  http_client_t *rtsp;
  tvhpoll_t *efd;
  tvhpoll_event_t ev;
  int r, nfds;
  char b[32];

  if (session[0] == '\0' || stream_id <= 0)
    return;

  efd = tvhpoll_create(1);
  rtsp = http_client_connect(lfe, RTSP_VERSION_1_0, "rstp",
                               lfe->sf_device->sd_info.addr,
                               lfe->sf_device->sd_info.rtsp_port,
                               satip_frontend_bindaddr(lfe));
  if (rtsp == NULL)
    goto done;

  rtsp->hc_rtsp_session = strdup((char *)session);

  memset(&ev, 0, sizeof(ev));
  ev.events           = TVHPOLL_IN;
  ev.fd               = rtsp->hc_fd;
  ev.data.ptr         = rtsp;
  tvhpoll_add(efd, &ev, 1);
  rtsp->hc_efd = efd;

  snprintf(b, sizeof(b), "/stream=%li", stream_id);
  r = rtsp_teardown(rtsp, b, NULL);
  if (r < 0) {
    tvhtrace("satip", "bad shutdown for session %s stream id %li", session, stream_id);
  } else {
    while (1) {
      r = http_client_run(rtsp);
      if (r != HTTP_CON_RECEIVING && r != HTTP_CON_SENDING)
        break;
      nfds = tvhpoll_wait(efd, &ev, 1, 100);
      if (nfds == 0)
        break;
      if (nfds < 0) {
        if (ERRNO_AGAIN(errno))
          continue;
        break;
      }
      if(ev.events & (TVHPOLL_ERR | TVHPOLL_HUP))
        break;
    }
  }

  http_client_close(rtsp);
  tvhlog(LOG_INFO, "satip", "extra shutdown done for session %s", session);

done:
  tvhpoll_destroy(efd);
}

static void
satip_frontend_shutdown ( http_client_t *rtsp, tvhpoll_t *efd )
{
  char b[32];
  tvhpoll_event_t ev;
  int r, nfds;

  if (rtsp->hc_rtsp_stream_id < 0)
    return;

  snprintf(b, sizeof(b), "/stream=%li", rtsp->hc_rtsp_stream_id);
  r = rtsp_teardown(rtsp, (char *)b, NULL);
  if (r < 0) {
    tvhtrace("satip", "%s - bad teardown", b);
  } else {
    while (1) {
      r = http_client_run(rtsp);
      if (r != HTTP_CON_RECEIVING && r != HTTP_CON_SENDING)
        break;
      nfds = tvhpoll_wait(efd, &ev, 1, 400);
      if (nfds == 0)
        break;
      if (nfds < 0) {
        if (ERRNO_AGAIN(errno))
          continue;
        break;
      }
      if(ev.events & (TVHPOLL_ERR | TVHPOLL_HUP))
        break;
    }
  }
}

static void
satip_frontend_tuning_error ( satip_frontend_t *lfe, satip_tune_req_t *tr )
{
  mpegts_mux_t *mm;
  mpegts_mux_instance_t *mmi;
  char uuid[UUID_HEX_SIZE];

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (lfe->sf_running && lfe->sf_req == tr &&
      (mmi = tr->sf_mmi) != NULL && (mm = mmi->mmi_mux) != NULL) {
    strcpy(uuid, idnode_uuid_as_str(&mm->mm_id));
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
    mpegts_mux_tuning_error(uuid, mmi);
    return;
  }
  pthread_mutex_unlock(&lfe->sf_dvr_lock);
}

static void
satip_frontend_close_rtsp
  ( satip_frontend_t *lfe, tvhpoll_t *efd, http_client_t **rtsp )
{
  tvhpoll_event_t ev;

  memset(&ev, 0, sizeof(ev));
  ev.events   = TVHPOLL_IN;
  ev.fd       = lfe->sf_dvr_pipe.rd;
  ev.data.ptr = NULL;
  tvhpoll_rem(efd, &ev, 1);

  satip_frontend_shutdown(*rtsp, efd);

  memset(&ev, 0, sizeof(ev));
  ev.events   = TVHPOLL_IN;
  ev.fd       = lfe->sf_dvr_pipe.rd;
  ev.data.ptr = NULL;
  tvhpoll_add(efd, &ev, 1);

  http_client_close(*rtsp);
  *rtsp = NULL;
}

static void *
satip_frontend_input_thread ( void *aux )
{
#define RTP_PKTS      64
#define UDP_PKT_SIZE  1472         /* this is maximum UDP payload (standard ethernet) */
#define RTP_PKT_SIZE  (UDP_PKT_SIZE - 12)             /* minus RTP minimal RTP header */
#define HTTP_CMD_NONE 9874
  satip_frontend_t *lfe = aux, *lfe_master;
  satip_tune_req_t *tr = NULL;
  mpegts_mux_instance_t *mmi;
  http_client_t *rtsp;
  udp_connection_t *rtp = NULL, *rtcp = NULL;
  dvb_mux_t *lm;
  char buf[256];
  struct iovec *iovec;
  uint8_t b[2048], session[32];
  uint8_t *p;
  sbuf_t sb;
  int pos, nfds, i, r, tc, rtp_port, start = 0;
  size_t c;
  tvhpoll_event_t ev[3];
  tvhpoll_t *efd;
  int changing, ms, fatal, running, play2, exit_flag;
  int rtsp_flags, position, reply;
  uint32_t seq, nseq, unc;
  udp_multirecv_t um;
  uint64_t u64, u64_2;
  long stream_id;

  /* If set - the thread will be cancelled */
  exit_flag = 0;
  /* Remember session for extra shutdown (workaround for IDL4K firmware bug) */
  session[0] = '\0';
  stream_id = 0;

  /* Setup poll */
  efd = tvhpoll_create(4);
  rtsp = NULL;

  /* Setup buffers */
  sbuf_init(&sb);
  udp_multirecv_init(&um, 0, 0);

  /*
   * New tune
   */
new_tune:
  sbuf_free(&sb);
  udp_multirecv_free(&um);
  udp_close(rtcp);
  udp_close(rtp);
  rtcp = rtp = NULL;
  lfe_master = NULL;

  memset(ev, 0, sizeof(ev));
  ev[0].events             = TVHPOLL_IN;
  ev[0].fd                 = lfe->sf_dvr_pipe.rd;
  ev[0].data.ptr           = NULL;
  tvhpoll_add(efd, ev, 1);

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));

  while (!start) {

    nfds = tvhpoll_wait(efd, ev, 1, rtsp ? 50 : -1);

    if (!tvheadend_running) { exit_flag = 1; goto done; }
    if (rtsp && nfds == 0) satip_frontend_close_rtsp(lfe, efd, &rtsp);
    if (nfds <= 0) continue;

    if (ev[0].data.ptr == NULL) {
      c = read(lfe->sf_dvr_pipe.rd, b, 1);
      if (c == 1) {
        if (b[0] == 'e') {
          tvhtrace("satip", "%s - input thread received shutdown", buf);
          exit_flag = 1;
          goto done;
        } else if (b[0] == 's') {
          tvhtrace("satip", "%s - start", buf);
          start = 1;
        }
      }
    }

    if (rtsp && ev[0].data.ptr == rtsp) {
      r = http_client_run(rtsp);
      if (r < 0) {
        http_client_close(rtsp);
        rtsp = NULL;
      } else {
        switch (rtsp->hc_cmd) {
        case RTSP_CMD_OPTIONS:
          rtsp_options_decode(rtsp);
          break;
        case RTSP_CMD_SETUP:
          rtsp_setup_decode(rtsp, 1);
          break;
        default:
          break;
        }
      }
    }

  }

  start = 0;

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  lfe->sf_req_thread = tr = lfe->sf_req;
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  if (!lfe->sf_req_thread)
    goto new_tune;

  mmi        = tr->sf_mmi;
  changing   = 0;
  ms         = 500;
  fatal      = 0;
  running    = 1;
  seq        = -1;
  play2      = 1;
  rtsp_flags = 0;

  if (udp_bind_double(&rtp, &rtcp,
                      "satip", "rtp", "rtpc",
                      satip_frontend_bindaddr(lfe), lfe->sf_udp_rtp_port,
                      NULL, SATIP_BUF_SIZE, 16384, 4*1024, 4*1024) < 0) {
    satip_frontend_tuning_error(lfe, tr);
    goto done;
  }

  rtp_port = ntohs(IP_PORT(rtp->ip));

  tvhtrace("satip", "%s - local RTP port %i RTCP port %i",
                    lfe->mi_name,
                    ntohs(IP_PORT(rtp->ip)),
                    ntohs(IP_PORT(rtcp->ip)));

  if (rtp == NULL || rtcp == NULL || mmi == NULL) {
    satip_frontend_tuning_error(lfe, tr);
    goto done;
  }

  lm = (dvb_mux_t *)mmi->mmi_mux;

  lfe_master = lfe;
  if (lfe->sf_master)
    lfe_master = satip_frontend_find_by_number(lfe->sf_device, lfe->sf_master) ?: lfe;

  i = 0;
  if (!rtsp) {
    pthread_mutex_lock(&lfe->sf_device->sd_tune_mutex);
    u64 = lfe_master->sf_last_tune;
    i = lfe_master->sf_tdelay;
    pthread_mutex_unlock(&lfe->sf_device->sd_tune_mutex);
    if (i < 0)
      i = 0;
    if (i > 2000)
      i = 2000;
  }

  tc = 1;
  while (i) {

    u64_2 = (getmonoclock() - u64) / 1000;
    if (u64_2 >= (uint64_t)i)
      break;
    r = (uint64_t)i - u64_2;
      
    if (tc)
      tvhtrace("satip", "%s - last tune diff = %llu (delay = %d)",
               buf, (unsigned long long)u64_2, r);

    tc = 1;
    nfds = tvhpoll_wait(efd, ev, 1, r);

    if (!tvheadend_running) { exit_flag = 1; goto done; }
    if (nfds < 0) continue;
    if (nfds == 0) break;

    if (ev[0].data.ptr == NULL) {
      c = read(lfe->sf_dvr_pipe.rd, b, 1);
      if (c == 1) {
        if (b[0] == 'c') {
          tc = 0;
          ms = 20;
          changing = 1;
          continue;
        } else if (b[0] == 'e') {
          tvhtrace("satip", "%s - input thread received shutdown", buf);
          exit_flag = 1;
          goto done;
        } else if (b[0] == 's') {
          start = 1;
          goto done;
        }
      }
      tvhtrace("satip", "%s - input thread received mux close", buf);
      goto done;
    }

    if (ev[0].data.ptr == rtsp) {
      tc = 0;
      r = http_client_run(rtsp);
      if (r < 0) {
        http_client_close(rtsp);
        rtsp = NULL;
      }
    }

  }

  pthread_mutex_lock(&lfe->sf_device->sd_tune_mutex);
  lfe_master->sf_last_tune = getmonoclock();
  pthread_mutex_unlock(&lfe->sf_device->sd_tune_mutex);

  i = 0;
  if (!rtsp) {
    rtsp = http_client_connect(lfe, RTSP_VERSION_1_0, "rstp",
                               lfe->sf_device->sd_info.addr,
                               lfe->sf_device->sd_info.rtsp_port,
                               satip_frontend_bindaddr(lfe));
    if (rtsp == NULL) {
      satip_frontend_tuning_error(lfe, tr);
      goto done;
    }
    i = 1;
  }

  /* Setup poll */
  memset(ev, 0, sizeof(ev));
  ev[0].events             = TVHPOLL_IN;
  ev[0].fd                 = rtp->fd;
  ev[0].data.ptr           = rtp;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd                 = rtcp->fd;
  ev[1].data.ptr           = rtcp;
  if (i) {
    ev[2].events           = TVHPOLL_IN;
    ev[2].fd               = rtsp->hc_fd;
    ev[2].data.ptr         = rtsp;
  }
  tvhpoll_add(efd, ev, i ? 3 : 2);
  rtsp->hc_efd = efd;

  position = lfe_master->sf_position;
  if (lfe->sf_device->sd_pids0)
    rtsp_flags |= SATIP_SETUP_PIDS0;
  if (lfe->sf_device->sd_pilot_on)
    rtsp_flags |= SATIP_SETUP_PILOT_ON;
  if (lfe->sf_device->sd_pids21)
    rtsp_flags |= SATIP_SETUP_PIDS21;
  r = -12345678;
  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (lfe->sf_req == lfe->sf_req_thread)
    r = satip_rtsp_setup(rtsp,
                         position, lfe->sf_number,
                         rtp_port, &lm->lm_tuning,
                         rtsp_flags);
  pthread_mutex_unlock(&lfe->sf_dvr_lock);
  if (r < 0) {
    if (r != -12345678)
      tvherror("satip", "%s - failed to tune (%i)", buf, r);
    else
      tvhtrace("satip", "%s - mux changed in the middle", buf);
    satip_frontend_tuning_error(lfe, tr);
    goto done;
  }
  reply = 1;

  udp_multirecv_init(&um, RTP_PKTS, RTP_PKT_SIZE);
  sbuf_init_fixed(&sb, RTP_PKTS * RTP_PKT_SIZE);
  
  while ((reply || running) && !fatal) {

    nfds = tvhpoll_wait(efd, ev, 1, ms);

    if (!tvheadend_running) {
      exit_flag = 1;
      running = 0;
    }

    if (nfds > 0 && ev[0].data.ptr == NULL) {
      c = read(lfe->sf_dvr_pipe.rd, b, 1);
      if (c == 1) {
        if (b[0] == 'c') {
          ms = 20;
          changing = 1;
          continue;
        } else if (b[0] == 'e') {
          tvhtrace("satip", "%s - input thread received shutdown", buf);
          exit_flag = 1; running = 0;
          continue;
        } else if (b[0] == 's') {
          start = 1; running = 0;
          continue;
        }
      }
      tvhtrace("satip", "%s - input thread received mux close", buf);
      running = 0;
      continue;
    }

    if (changing && rtsp->hc_cmd == HTTP_CMD_NONE) {
      ms = 500;
      changing = 0;
      if (satip_frontend_pid_changed(rtsp, lfe, buf) > 0)
        reply = 1;
      continue;
    }

    if (nfds < 1) continue;

    if (ev[0].data.ptr == rtsp) {
      r = http_client_run(rtsp);
      if (r < 0) {
        if (rtsp->hc_code == 404 && session[0]) {
          tvhlog(LOG_WARNING, "satip", "%s - RTSP 404 ERROR (retrying)", buf);
          satip_frontend_extra_shutdown(lfe, session, stream_id);
          session[0] = '\0';
          start = 1;
          http_client_close(rtsp);
          rtsp = NULL;
          break;
        }
        tvhlog(LOG_ERR, "satip", "%s - RTSP error %d (%s) [%i-%i]",
               buf, r, strerror(-r), rtsp->hc_cmd, rtsp->hc_code);
        satip_frontend_tuning_error(lfe, tr);
        fatal = 1;
      } else if (r == HTTP_CON_DONE) {
        if (start)
          goto done;
        reply = 0;
        switch (rtsp->hc_cmd) {
        case RTSP_CMD_OPTIONS:
          r = rtsp_options_decode(rtsp);
          if (!running)
            break;
          if (r < 0) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP OPTIONS error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->hc_cmd, rtsp->hc_code);
            satip_frontend_tuning_error(lfe, tr);
            fatal = 1;
          }
          break;
        case RTSP_CMD_SETUP:
          r = rtsp_setup_decode(rtsp, 1);
          if (!running)
            break;
          if (r < 0 || rtsp->hc_rtp_port != rtp_port ||
                       rtsp->hc_rtpc_port != rtp_port + 1) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP SETUP error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->hc_cmd, rtsp->hc_code);
            satip_frontend_tuning_error(lfe, tr);
            fatal = 1;
          } else {
            strncpy((char *)session, rtsp->hc_rtsp_session ?: "", sizeof(session));
            session[sizeof(session)-1] = '\0';
            stream_id = rtsp->hc_rtsp_stream_id;
            tvhdebug("satip", "%s #%i - new session %s stream id %li",
                        rtsp->hc_host, lfe->sf_number,
                        rtsp->hc_rtsp_session, rtsp->hc_rtsp_stream_id);
            if (lfe->sf_play2) {
              r = -12345678;
              pthread_mutex_lock(&lfe->sf_dvr_lock);
              if (lfe->sf_req == lfe->sf_req_thread)
                r = satip_rtsp_setup(rtsp, position, lfe->sf_number,
                                     rtp_port, &lm->lm_tuning,
                                     rtsp_flags | SATIP_SETUP_PLAY);
              pthread_mutex_unlock(&lfe->sf_dvr_lock);
              if (r < 0) {
                tvherror("satip", "%s - failed to tune2 (%i)", buf, r);
                satip_frontend_tuning_error(lfe, tr);
                fatal = 1;
              }
              reply = 1;
              continue;
            } else {
              if (satip_frontend_pid_changed(rtsp, lfe, buf) > 0) {
                reply = 1;
                continue;
              }
            }
          }
          break;
        case RTSP_CMD_PLAY:
          if (!running)
            break;
          if (rtsp->hc_code == 200 && play2) {
            play2 = 0;
            if (satip_frontend_pid_changed(rtsp, lfe, buf) > 0) {
              reply = 1;
              continue;
            }
          }
          /* fall thru */
        default:
          if (rtsp->hc_code >= 400) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP cmd error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->hc_cmd, rtsp->hc_code);
            satip_frontend_tuning_error(lfe, tr);
            fatal = 1;
          }
          break;
        }
        rtsp->hc_cmd = HTTP_CMD_NONE;
      }

      if (!running)
        continue;
    }

    /* We need to keep the session alive */
    if (rtsp->hc_ping_time + rtsp->hc_rtp_timeout / 2 < dispatch_clock &&
        rtsp->hc_cmd == HTTP_CMD_NONE) {
      rtsp_options(rtsp);
      reply = 1;
    }

    if (ev[0].data.ptr == rtcp) {
      c = recv(rtcp->fd, b, sizeof(b), MSG_DONTWAIT);
      if (c > 0) {
        pthread_mutex_lock(&lfe->sf_dvr_lock);
        if (lfe->sf_req == lfe->sf_req_thread)
          satip_frontend_decode_rtcp(lfe, buf, mmi, b, c);
        pthread_mutex_unlock(&lfe->sf_dvr_lock);
      }
      continue;
    }
    
    if (ev[0].data.ptr != rtp)
      continue;     

    tc = udp_multirecv_read(&um, rtp->fd, RTP_PKTS, &iovec);

    if (tc < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      if (errno == EOVERFLOW) {
        tvhlog(LOG_WARNING, "satip", "%s - recvmsg() EOVERFLOW", buf);
        continue;
      }
      tvhlog(LOG_ERR, "satip", "%s - multirecv error %d (%s)",
             buf, errno, strerror(errno));
      break;
    }

    for (i = 0, unc = 0; i < tc; i++) {
      p = iovec[i].iov_base;
      c = iovec[i].iov_len;

      /* Strip RTP header */
      if (c < 12)
        continue;
      if ((p[0] & 0xc0) != 0x80)
        continue;
      if ((p[1] & 0x7f) != 33)
        continue;
      pos = ((p[0] & 0x0f) * 4) + 12;
      if (p[0] & 0x10) {
        if (c < pos + 4)
          continue;
        pos += (((p[pos+2] << 8) | p[pos+3]) + 1) * 4;
      }
      if (c <= pos || ((c - pos) % 188) != 0)
        continue;
      /* Use uncorrectable value to notify RTP delivery issues */
      nseq = (p[2] << 8) | p[3];
      if (seq == -1)
        seq = nseq;
      else if (((seq + 1) & 0xffff) != nseq) {
        unc += ((c - pos) / 188) * (uint32_t)((uint16_t)nseq-(uint16_t)(seq+1));
        tvhtrace("satip", "RTP discontinuity (%i != %i)", seq + 1, nseq);
      }
      seq = nseq;
      /* Process */
      tsdebug_write((mpegts_mux_t *)lm, p + pos, c - pos);
      sbuf_append(&sb, p + pos, c - pos);
    }
    pthread_mutex_lock(&lfe->sf_dvr_lock);
    if (lfe->sf_req == lfe->sf_req_thread) {
      mmi->tii_stats.unc += unc;
      mpegts_input_recv_packets((mpegts_input_t*)lfe, mmi,
                                &sb, NULL, NULL);
    } else
      fatal = 1;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
  }

  /* Do not send the SMT_SIGNAL_STATUS packets - we are out of service */
  gtimer_disarm(&lfe->sf_monitor_timer);

  sbuf_free(&sb);
  udp_multirecv_free(&um);

  ev[0].events             = TVHPOLL_IN;
  ev[0].fd                 = rtp->fd;
  ev[0].data.ptr           = rtp;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd                 = rtcp->fd;
  ev[1].data.ptr           = rtcp;
  ev[2].events             = TVHPOLL_IN;
  ev[2].fd                 = lfe->sf_dvr_pipe.rd;
  ev[2].data.ptr           = NULL;
  tvhpoll_rem(efd, ev, 3);

  if (exit_flag) {
    satip_frontend_shutdown(rtsp, efd);
    http_client_close(rtsp);
    rtsp = NULL;
  }

done:
  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (tr && tr != lfe->sf_req) {
    mpegts_pid_done(&tr->sf_pids);
    mpegts_pid_done(&tr->sf_pids_tuned);
    free(tr);
  }
  lfe->sf_req_thread = tr = NULL;
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  udp_close(rtcp);
  udp_close(rtp);
  rtcp = rtp = NULL;

  if (lfe->sf_teardown_delay && lfe_master) {
    pthread_mutex_lock(&lfe->sf_device->sd_tune_mutex);
    lfe->sf_last_tune = lfe_master->sf_last_tune = getmonoclock();
    pthread_mutex_unlock(&lfe->sf_device->sd_tune_mutex);
  }

  if (!exit_flag)
    goto new_tune;

  if (rtsp)
    http_client_close(rtsp);

  tvhpoll_destroy(efd);
  return NULL;
#undef PKTS
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

static void
satip_frontend_hacks( satip_frontend_t *lfe, int *def_positions )
{
  satip_device_t *sd = lfe->sf_device;

  lfe->sf_tdelay = 50; /* should not hurt anything */
  if (strstr(sd->sd_info.location, ":8888/octonet.xml")) {
    if (lfe->sf_type == DVB_TYPE_S)
      lfe->sf_play2 = 1;
    lfe->sf_tdelay = 250;
    lfe->sf_teardown_delay = 1;
  } else if (!strcmp(sd->sd_info.modelname, "IPLNB")) {
    *def_positions = 1;
  } else if (strstr(sd->sd_info.manufacturer, "AVM Berlin") &&
              strstr(sd->sd_info.modelname, "FRITZ!")) {
    lfe->sf_play2 = 1;
  }
}

satip_frontend_t *
satip_frontend_create
  ( htsmsg_t *conf, satip_device_t *sd, dvb_fe_type_t type, int v2, int num )
{
  const idclass_t *idc;
  const char *uuid = NULL, *override = NULL;
  char id[16], lname[256], nname[60];
  satip_frontend_t *lfe;
  uint32_t master = 0;
  int i, def_positions = sd->sd_info.srcs;

  /* Override type */
  snprintf(id, sizeof(id), "override #%d", num);
  if (conf && type != DVB_TYPE_S) {
    override = htsmsg_get_str(conf, id);
    if (override) {
      i = dvb_str2type(override);
      if ((i == DVB_TYPE_T || i == DVB_TYPE_C || i == DVB_TYPE_S) && i != type)
        type = i;
      else
        override = NULL;
    }
  }
  /* Tuner slave */
  snprintf(id, sizeof(id), "master for #%d", num);
  if (conf && type == DVB_TYPE_S) {
    if (htsmsg_get_u32(conf, id, &master))
      master = 0;
    if (master == num)
      master = 0;
  }
  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(type), num);
  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  /* Class */
  if (type == DVB_TYPE_S)
    idc = master ? &satip_frontend_dvbs_slave_class :
                   &satip_frontend_dvbs_class;
  else if (type == DVB_TYPE_T)
    idc = &satip_frontend_dvbt_class;
  else if (type == DVB_TYPE_C)
    idc = &satip_frontend_dvbc_class;
  else if (type == DVB_TYPE_ATSC)
    idc = &satip_frontend_atsc_class;
  else {
    tvherror("satip", "unknown FE type %d", type);
    return NULL;
  }

  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  lfe = calloc(1, sizeof(satip_frontend_t));
  lfe->sf_device   = sd;
  lfe->sf_number   = num;
  lfe->sf_type     = type;
  lfe->sf_type_v2  = v2;
  lfe->sf_master   = master;
  lfe->sf_type_override = override ? strdup(override) : NULL;
  satip_frontend_hacks(lfe, &def_positions);
  TAILQ_INIT(&lfe->sf_satconf);
  pthread_mutex_init(&lfe->sf_dvr_lock, NULL);
  lfe = (satip_frontend_t*)mpegts_input_create0((mpegts_input_t*)lfe, idc, uuid, conf);
  if (!lfe) return NULL;

  /* Defaults */
  lfe->sf_position     = -1;

  /* Callbacks */
  lfe->mi_get_weight   = satip_frontend_get_weight;
  lfe->mi_get_priority = satip_frontend_get_priority;
  lfe->mi_get_grace    = satip_frontend_get_grace;

  /* Default name */
  if (!lfe->mi_name ||
      (strncmp(lfe->mi_name, "SAT>IP ", 7) == 0 &&
       strstr(lfe->mi_name, " Tuner ") &&
       strstr(lfe->mi_name, " #"))) {
    snprintf(lname, sizeof(lname), "SAT>IP %s Tuner #%i (%s)",
             dvb_type2str(type), num,
             satip_device_nicename(sd, nname, sizeof(nname)));
    free(lfe->mi_name);
    lfe->mi_name = strdup(lname);
  }

  /* Input callbacks */
  lfe->mi_is_enabled     = satip_frontend_is_enabled;
  lfe->mi_start_mux      = satip_frontend_start_mux;
  lfe->mi_stop_mux       = satip_frontend_stop_mux;
  lfe->mi_network_list   = satip_frontend_network_list;
  lfe->mi_update_pids    = satip_frontend_update_pids;

  /* Adapter link */
  lfe->sf_device = sd;
  TAILQ_INSERT_TAIL(&sd->sd_frontends, lfe, sf_link);

  /* Create satconf */
  if (lfe->sf_type == DVB_TYPE_S && master == 0)
    satip_satconf_create(lfe, conf, def_positions);

  /* Slave networks update */
  if (master) {
    satip_frontend_t *lfe2 = satip_frontend_find_by_number(sd, master);
    if (lfe2) {
      htsmsg_t *l = (htsmsg_t *)mpegts_input_class_network_get(lfe2);
      if (l) {
        mpegts_input_class_network_set(lfe, l);
        htsmsg_destroy(l);
      }
    }
  }

  tvh_pipe(O_NONBLOCK, &lfe->sf_dvr_pipe);
  tvhthread_create(&lfe->sf_dvr_thread, NULL,
                   satip_frontend_input_thread, lfe);

  return lfe;
}

void
satip_frontend_save ( satip_frontend_t *lfe, htsmsg_t *fe )
{
  char id[16];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)lfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->sf_type));
  htsmsg_add_str(m, "uuid", idnode_uuid_as_str(&lfe->ti_id));
  if (lfe->ti_id.in_class == &satip_frontend_dvbs_class) {
    satip_satconf_save(lfe, m);
    htsmsg_delete_field(m, "networks");
  }
  htsmsg_delete_field(m, "fe_override");
  htsmsg_delete_field(m, "fe_master");

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(lfe->sf_type), lfe->sf_number);
  htsmsg_add_msg(fe, id, m);
  if (lfe->sf_type_override) {
    snprintf(id, sizeof(id), "override #%d", lfe->sf_number);
    htsmsg_add_str(fe, id, lfe->sf_type_override);
  }
  if (lfe->sf_master) {
    snprintf(id, sizeof(id), "master for #%d", lfe->sf_number);
    htsmsg_add_u32(fe, id, lfe->sf_master);
  }
}

void
satip_frontend_delete ( satip_frontend_t *lfe )
{
  char buf1[256];

  lock_assert(&global_lock);

  lfe->mi_display_name((mpegts_input_t *)lfe, buf1, sizeof(buf1));

  /* Ensure we're stopped */
  mpegts_input_stop_all((mpegts_input_t*)lfe);

  /* Stop thread */
  if (lfe->sf_dvr_pipe.wr > 0) {
    tvh_write(lfe->sf_dvr_pipe.wr, "e", 1);
    tvhtrace("satip", "%s - waiting for control thread", buf1);
    pthread_join(lfe->sf_dvr_thread, NULL);
    tvh_pipe_close(&lfe->sf_dvr_pipe);
    tvhdebug("satip", "%s - stopped control thread", buf1);
  }

  /* Stop timer */
  gtimer_disarm(&lfe->sf_monitor_timer);

  /* Remove from adapter */
  TAILQ_REMOVE(&lfe->sf_device->sd_frontends, lfe, sf_link);

  /* Delete satconf */
  satip_satconf_destroy(lfe);

  free(lfe->sf_type_override);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)lfe, 0);
}
