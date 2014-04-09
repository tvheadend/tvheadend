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

#define _GNU_SOURCE
#include <sys/socket.h>
#include <fcntl.h>
#include "tvheadend.h"
#include "tvhpoll.h"
#include "streaming.h"
#include "http.h"
#include "satip_private.h"

static int
satip_frontend_tune1
  ( satip_frontend_t *lfe, mpegts_mux_instance_t *mmi );

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static void
satip_frontend_class_save ( idnode_t *in )
{
  satip_device_t *la = ((satip_frontend_t*)in)->sf_device;
  satip_device_save(la);
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
      .type     = PT_BOOL,
      .id       = "fullmux",
      .name     = "Full Mux Rx mode",
      .off      = offsetof(satip_frontend_t, sf_fullmux),
    },
    {
      .type     = PT_INT,
      .id       = "udp_rtp_port",
      .name     = "UDP RTP Port Number (2 ports)",
      .off      = offsetof(satip_frontend_t, sf_udp_rtp_port),
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
    {}
  }
};

static idnode_set_t *
satip_frontend_dvbs_class_get_childs ( idnode_t *self )
{
  satip_frontend_t   *lfe = (satip_frontend_t*)self;
  idnode_set_t        *is  = idnode_set_create();
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
      .name     = "Sattellite Positions",
      .set      = satip_frontend_dvbs_class_positions_set,
      .opts     = PO_NOSAVE,
      .off      = offsetof(satip_frontend_t, sf_positions),
      .def.i    = 4
    },
    {
      .id       = "networks",
      .type     = PT_NONE,
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static int
satip_frontend_is_free ( mpegts_input_t *mi )
{
  satip_device_t *sd = ((satip_frontend_t*)mi)->sf_device;
  satip_frontend_t *lfe;
  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    if (!mpegts_input_is_free((mpegts_input_t*)lfe))
      return 0;
  return 1;
}

static int
satip_frontend_get_weight ( mpegts_input_t *mi )
{
  int weight = 0;
  satip_device_t *sd = ((satip_frontend_t*)mi)->sf_device;
  satip_frontend_t *lfe;
  TAILQ_FOREACH(lfe, &sd->sd_frontends, sf_link)
    weight = MAX(weight, mpegts_input_get_weight((mpegts_input_t*)lfe));
  return weight;
}

static int
satip_frontend_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  int r = mpegts_input_get_priority(mi, mm);
  if (lfe->sf_positions)
    r += satip_satconf_get_priority(lfe, mm);
  return r;
}

static int
satip_frontend_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  int r = 5;
  if (lfe->sf_positions)
    r = 10;
  return r;
}

static int
satip_frontend_is_enabled ( mpegts_input_t *mi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  if (!lfe->mi_enabled) return 0;
  return 1;
}

static void
satip_frontend_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  char buf1[256], buf2[256];

  mi->mi_display_name(mi, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("satip", "%s - stopping %s", buf1, buf2);

  lfe->sf_running   = 0;
  lfe->sf_mmi       = NULL;

  gtimer_disarm(&lfe->sf_monitor_timer);

  /* Stop thread */
  if (lfe->sf_dvr_pipe.wr > 0) {
    tvh_write(lfe->sf_dvr_pipe.wr, "", 1);
    tvhtrace("satip", "%s - waiting for dvr thread", buf1);
    pthread_join(lfe->sf_dvr_thread, NULL);
    tvh_pipe_close(&lfe->sf_dvr_pipe);
    tvhdebug("satip", "%s - stopped dvr thread", buf1);
  }

  udp_close(lfe->sf_rtp);   lfe->sf_rtp        = NULL;
  udp_close(lfe->sf_rtcp);  lfe->sf_rtcp       = NULL;

  free(lfe->sf_pids);       lfe->sf_pids       = NULL;
  free(lfe->sf_pids_tuned); lfe->sf_pids_tuned = NULL;
}

static int
satip_frontend_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  if (lfe->sf_positions > 0)
    lfe->sf_position = satip_satconf_get_position(lfe, mmi->mmi_mux);
  return satip_frontend_tune1((satip_frontend_t*)mi, mmi);
}

static int
satip_frontend_add_pid( satip_frontend_t *lfe, int pid)
{
  int mid, div;

  if (pid < 0 || pid >= 8191)
    return 0;

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (lfe->sf_pids_count >= lfe->sf_pids_size) {
    lfe->sf_pids_size += 64;
    lfe->sf_pids       = realloc(lfe->sf_pids,
                                 lfe->sf_pids_size * sizeof(uint16_t));
    lfe->sf_pids_tuned = realloc(lfe->sf_pids_tuned,
                                 lfe->sf_pids_size * sizeof(uint16_t));
  }

  if (lfe->sf_pids_count == 0) {
    lfe->sf_pids[lfe->sf_pids_count++] = pid;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);
    return 1;
  }

#if 0
  printf("Insert PID: %i\n", pid);
  if (pid == 0)
    printf("HERE!!!\n");
  { int i; for (i = 0; i < lfe->sf_pids_count; i++)
    printf("Bpid[%i] = %i\n", i, lfe->sf_pids[i]); }
#endif
  /* insert pid to the sorted array */
  mid = div = lfe->sf_pids_count / 2;
  while (1) {
    assert(mid >= 0 && mid < lfe->sf_pids_count);
    if (div > 1)
      div /= 2;
    if (lfe->sf_pids[mid] == pid) {
      pthread_mutex_unlock(&lfe->sf_dvr_lock);
      return 0;
    }
    if (lfe->sf_pids[mid] < pid) {
      if (mid + 1 >= lfe->sf_pids_count) {
        lfe->sf_pids[lfe->sf_pids_count++] = pid;
        break;
      }
      if (lfe->sf_pids[mid + 1] > pid) {
        mid++;
        if (mid < lfe->sf_pids_count)
          memmove(&lfe->sf_pids[mid + 1], &lfe->sf_pids[mid],
                  (lfe->sf_pids_count - mid) * sizeof(uint16_t));
        lfe->sf_pids[mid] = pid;
        lfe->sf_pids_count++;
        break;
      }
      mid += div;
    } else {
      if (mid == 0 || lfe->sf_pids[mid - 1] < pid) {
        memmove(&lfe->sf_pids[mid+1], &lfe->sf_pids[mid],
                (lfe->sf_pids_count - mid) * sizeof(uint16_t));
        lfe->sf_pids[mid] = pid;
        lfe->sf_pids_count++;
        break;
      }
      mid -= div;
    }
  }
#if 0
  { int i; for (i = 0; i < lfe->sf_pids_count; i++)
    printf("Apid[%i] = %i\n", i, lfe->sf_pids[i]); }
#endif
  pthread_mutex_unlock(&lfe->sf_dvr_lock);
  return 1;
}

static mpegts_pid_t *
satip_frontend_open_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  mpegts_pid_t *mp;
  int change = 0;

  if (!(mp = mpegts_input_open_pid(mi, mm, pid, type, owner)))
    return NULL;

  if (type == MPEGTS_FULLMUX_PID) {
    if (lfe->sf_device->sd_fullmux_ok) {
      if (!lfe->sf_pids_any)
        lfe->sf_pids_any = change = 1;
    } else {
      mpegts_service_t *s;
      elementary_stream_t *st;
      LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
        change |= satip_frontend_add_pid(lfe, s->s_pmt_pid);
        change |= satip_frontend_add_pid(lfe, s->s_pcr_pid);
        TAILQ_FOREACH(st, &s->s_components, es_link)
          change |= satip_frontend_add_pid(lfe, st->es_pid);
      }
    }
  } else {
    change |= satip_frontend_add_pid(lfe, mp->mp_pid);
  }

  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (change && !lfe->sf_pids_any_tuned)
    tvh_write(lfe->sf_dvr_pipe.wr, "c", 1);
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  return mp;
}

static void
satip_frontend_close_pid
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int pid, int type, void *owner )
{
  satip_frontend_t *lfe = (satip_frontend_t*)mi;
  int mid, div;

  /* remove PID */
  pthread_mutex_lock(&lfe->sf_dvr_lock);
  if (lfe->sf_pids) {
    mid = div = lfe->sf_pids_count / 2;
    while (1) {
      if (div > 1)
        div /= 2;
      if (lfe->sf_pids[mid] == pid) {
        if (mid + 1 < lfe->sf_pids_count)
          memmove(&lfe->sf_pids[mid], &lfe->sf_pids[mid+1],
                  (lfe->sf_pids_count - mid - 1) * sizeof(uint16_t));
        lfe->sf_pids_count--;
        break;
      } else if (lfe->sf_pids[mid] < pid) {
        if (mid + 1 > lfe->sf_pids_count)
          break;
        if (lfe->sf_pids[mid + 1] > pid)
          break;
        mid += div;
      } else {
        if (mid == 0)
          break;
        if (lfe->sf_pids[mid - 1] < pid)
          break;
        mid -= div;
      }
    }
  }
  pthread_mutex_unlock(&lfe->sf_dvr_lock);

  mpegts_input_close_pid(mi, mm, pid, type, owner);
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
  else
    return NULL;

  return idnode_find_all(idc);
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
    if (l > len)
      return;
    if (rtcp[1]  ==  204 && l > 20 &&           /* packet type */
        rtcp[8]  == 'S'  && rtcp[9]  == 'E' &&
        rtcp[10] == 'S'  && rtcp[11] == '1') {
      sl = (rtcp[14] << 8) | rtcp[15];
      if (sl > 0 && l - 16 >= sl) {
        rtcp[sl + 16] = '\0';
        s = (char *)rtcp + 16;
        tvhtrace("satip", "Status string: '%s'", s);
        status = SIGNAL_NONE;
        if (strncmp(s, "ver=1.0;", 8) == 0) {
          if ((s = strstr(s + 8, ";tuner=")) == NULL)
            return;
          s += 7;
          n = http_tokenize(s, argv, 4, ',');
          if (n < 4)
            return;
          if (atoi(argv[0]) != lfe->sf_number)
            return;
          mmi->mmi_stats.signal =
            (atoi(argv[1]) * 100) / lfe->sf_device->sd_sig_scale;
          if (atoi(argv[2]) > 0)
            status = SIGNAL_GOOD;
          mmi->mmi_stats.snr = atoi(argv[3]);
          goto ok;          
        } else if (strncmp(s, "ver=1.1;tuner=", 14) == 0) {
          n = http_tokenize(s + 14, argv, 4, ',');
          if (n < 4)
            return;
          if (atoi(argv[0]) != lfe->sf_number)
            return;
          mmi->mmi_stats.signal =
            (atoi(argv[1]) * 100) / lfe->sf_device->sd_sig_scale;
          if (atoi(argv[2]) > 0)
            status = SIGNAL_GOOD;
          mmi->mmi_stats.snr = atoi(argv[3]);
          goto ok;
        }
      }
    }
    rtcp += l;
    len -= l;
  }
  return;

ok:
  if (mmi->mmi_stats.snr < 2 && status == SIGNAL_GOOD)
    status              = SIGNAL_BAD;
  else if (mmi->mmi_stats.snr < 4 && status == SIGNAL_GOOD)
    status              = SIGNAL_FAINT;
  lfe->sf_status        = status;
}

static void
satip_frontend_default_tables 
  ( satip_frontend_t *lfe, mpegts_mux_t *mm )
{
  psi_tables_default(mm);
  psi_tables_dvb(mm);
}

static void
satip_frontend_store_pids(char *buf, uint16_t *pids, int count)
{
  int first = 1;
  char *s = buf;

  *s = '\0';
  while (count--) {
    assert(*pids < 8192);
    if (!first)
      sprintf(s + strlen(s), ",%i", *(pids++));
    else {
      sprintf(s + strlen(s), "%i", *(pids++));
      first = 0;
    }
  }
}

static void
satip_frontend_pid_changed( satip_rtsp_connection_t *rtsp,
                            satip_frontend_t *lfe, const char *name )
{
  char *add, *del;
  int i, j, r, count, any = lfe->sf_pids_any;
  int deleted;

  if (!lfe->sf_running)
    return;

  pthread_mutex_lock(&lfe->sf_dvr_lock);

  if (lfe->sf_pids_count > lfe->sf_device->sd_pids_max)
    any = lfe->sf_device->sd_fullmux_ok ? 1 : 0;

  if (any) {

    if (lfe->sf_pids_any_tuned) {
      pthread_mutex_unlock(&lfe->sf_dvr_lock);
      return;
    }
    lfe->sf_pids_any_tuned = 1;
    memcpy(lfe->sf_pids_tuned, lfe->sf_pids,
           lfe->sf_pids_count * sizeof(uint16_t));
    lfe->sf_pids_tcount = lfe->sf_pids_count;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);

    r = satip_rtsp_play(rtsp,  "all", NULL, NULL);

  } else if (!lfe->sf_device->sd_pids_deladd ||
             lfe->sf_pids_any_tuned ||
             lfe->sf_pids_tcount == 0) {

    lfe->sf_pids_any_tuned = 0;
    count = lfe->sf_pids_count;
    if (count > lfe->sf_device->sd_pids_max)
      count = lfe->sf_device->sd_pids_max;
    add = alloca(count * 5);
    /* prioritize higher PIDs (tables are low prio) */
    satip_frontend_store_pids(add,
                              &lfe->sf_pids[lfe->sf_pids_count - count],
                              count);
    memcpy(lfe->sf_pids_tuned, lfe->sf_pids,
           lfe->sf_pids_count * sizeof(uint16_t));
    lfe->sf_pids_tcount = lfe->sf_pids_count;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);

    r = satip_rtsp_play(rtsp, add, NULL, NULL);

  } else {

    add = alloca(lfe->sf_pids_count * 5);
    del = alloca(lfe->sf_pids_count * 5);
    add[0] = del[0] = '\0';

#if 0
    for (i = 0; i < lfe->sf_pids_count; i++)
      printf("pid[%i] = %i\n", i, lfe->sf_pids[i]);
    for (i = 0; i < lfe->sf_pids_tcount; i++)
      printf("tuned[%i] = %i\n", i, lfe->sf_pids_tuned[i]);
#endif

    i = j = deleted = 0;
    while (i < lfe->sf_pids_count && j < lfe->sf_pids_tcount) {
      if (lfe->sf_pids[i] == lfe->sf_pids_tuned[j]) {
        i++; j++;
      } else if (lfe->sf_pids[i] < lfe->sf_pids_tuned[j]) {
        i++;
      } else {
        sprintf(del + strlen(del), ",%i", lfe->sf_pids_tuned[j++]);
        deleted++;
      }
    }
    while (j < lfe->sf_pids_tcount) {
      sprintf(del + strlen(del), ",%i", lfe->sf_pids_tuned[j++]);
      deleted++;
    }

    count = lfe->sf_pids_count + (lfe->sf_pids_tcount - deleted);
    if (count > lfe->sf_device->sd_pids_max)
      count = lfe->sf_device->sd_pids_max;
    /* prioritize higher PIDs (tables are low prio) */
    /* count means "skip count" in following code */
    count = lfe->sf_pids_count - count;
    
    i = j = 0;
    while (i < lfe->sf_pids_count && j < lfe->sf_pids_tcount) {
      if (lfe->sf_pids[i] == lfe->sf_pids_tuned[j]) {
        i++; j++;
      } else if (lfe->sf_pids[i] < lfe->sf_pids_tuned[j]) {
        if (count > 0) {
          count--;
        } else {
          sprintf(add + strlen(add), ",%i", lfe->sf_pids[i]);
        }
        i++;
      } else {
        j++;
      }
    }
    while (i < lfe->sf_pids_count) {
      if (count > 0)
        count--;
      else
        sprintf(add + strlen(add), ",%i", lfe->sf_pids[i++]);
    }

    memcpy(lfe->sf_pids_tuned, lfe->sf_pids,
           lfe->sf_pids_count * sizeof(uint16_t));
    lfe->sf_pids_tcount = lfe->sf_pids_count;
    pthread_mutex_unlock(&lfe->sf_dvr_lock);

    if (add[0] != '\0' || del[0] != '\0')
      r = satip_rtsp_play(rtsp, NULL, add, del);
    else
      r = 0;
  }

  if (r < 0)
    tvherror("satip", "%s - failed to modify pids: %s", name, strerror(-r));
}

static void *
satip_frontend_input_thread ( void *aux )
{
#define PKTS 64
  satip_frontend_t *lfe = aux;
  mpegts_mux_instance_t *mmi = lfe->sf_mmi;
  satip_rtsp_connection_t *rtsp;
  dvb_mux_t *lm;
  char buf[256];
  uint8_t tsb[PKTS][1356 + 128];
  uint8_t rtcp[2048];
  uint8_t *p;
  sbuf_t sb;
  struct iovec   iov[PKTS];
  struct mmsghdr msg[PKTS];
  int pos, nfds, i, r;
  size_t c;
  int tc;
  tvhpoll_event_t ev[4];
  tvhpoll_event_t evr;
  tvhpoll_t *efd;
  int changing = 0, ms = -1, fatal = 0;
  uint32_t seq = -1, nseq;

  lfe->mi_display_name((mpegts_input_t*)lfe, buf, sizeof(buf));

  if (lfe->sf_rtp == NULL || lfe->sf_rtcp == NULL || mmi == NULL)
    return NULL;

  lm = (dvb_mux_t *)mmi->mmi_mux;

  rtsp = satip_rtsp_connection(lfe->sf_device);
  if (rtsp == NULL)
    return NULL;

  /* Setup poll */
  efd = tvhpoll_create(4);
  memset(ev, 0, sizeof(ev));
  ev[0].events             = TVHPOLL_IN;
  ev[0].fd                 = lfe->sf_rtp->fd;
  ev[0].data.u64           = (uint64_t)lfe->sf_rtp;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd                 = lfe->sf_rtcp->fd;
  ev[1].data.u64           = (uint64_t)lfe->sf_rtcp;
  ev[2].events             = TVHPOLL_IN;
  ev[2].fd                 = rtsp->fd;
  ev[2].data.u64           = (uint64_t)rtsp;
  evr                      = ev[2];
  ev[3].events             = TVHPOLL_IN;
  ev[3].fd                 = lfe->sf_dvr_pipe.rd;
  ev[3].data.u64           = 0;
  tvhpoll_add(efd, ev, 4);

  /* Read */
  memset(&msg, 0, sizeof(msg));
  for (i = 0; i < PKTS; i++) {
    msg[i].msg_hdr.msg_iov    = &iov[i];
    msg[i].msg_hdr.msg_iovlen = 1;
    iov[i].iov_base           = tsb[i];
    iov[i].iov_len            = sizeof(tsb[0]);
  }

  r = satip_rtsp_setup(rtsp,
                       lfe->sf_position, lfe->sf_number,
                       lfe->sf_rtp_port, &lm->lm_tuning,
                       1);
  if (r < 0) {
    tvherror("satip", "%s - failed to tune", buf);
    return NULL;
  }

  sbuf_init_fixed(&sb, 18800);

  while (tvheadend_running && !fatal) {

    if (rtsp->sending) {
      if ((evr.events & TVHPOLL_OUT) == 0) {
        evr.events |= TVHPOLL_OUT;
        tvhpoll_add(efd, &evr, 1);
      }
    } else {
      if (evr.events & TVHPOLL_OUT) {
        evr.events &= ~TVHPOLL_OUT;
        tvhpoll_add(efd, &evr, 1);
      }
    }
    
    nfds = tvhpoll_wait(efd, ev, 1, ms);

    if (nfds > 0 && ev[0].data.u64 == 0) {
      c = read(lfe->sf_dvr_pipe.rd, tsb[0], 1);
      if (c == 1 && tsb[0][0] == 'c') {
        ms = 20;
        changing = 1;
        continue;
      }
      tvhtrace("satip", "%s - input thread received shutdown", buf);
      break;
    }

    if (changing && rtsp->cmd == SATIP_RTSP_CMD_NONE) {
      ms = -1;
      changing = 0;
      satip_frontend_pid_changed(rtsp, lfe, buf);
      continue;
    }

    if (nfds < 1) continue;

    if (ev[0].data.u64 == (uint64_t)rtsp) {
      r = satip_rtsp_run(rtsp);
      if (r < 0) {
        tvhlog(LOG_ERR, "satip", "%s - RTSP error %d (%s) [%i-%i]",
               buf, r, strerror(-r), rtsp->cmd, rtsp->code);
        fatal = 1;
      } else if (r == SATIP_RTSP_READ_DONE) {
        switch (rtsp->cmd) {
        case SATIP_RTSP_CMD_OPTIONS:
          r = satip_rtsp_options_decode(rtsp);
          if (r < 0) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP OPTIONS error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->cmd, rtsp->code);
            fatal = 1;
          }
          break;
        case SATIP_RTSP_CMD_SETUP:
          r = satip_rtsp_setup_decode(rtsp);
          if (r < 0 || rtsp->client_port != lfe->sf_rtp_port) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP SETUP error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->cmd, rtsp->code);
            fatal = 1;
          } else {
            tvhdebug("satip", "%s #%i - new session %s stream id %li",
                        lfe->sf_device->sd_info.addr, lfe->sf_number,
                        rtsp->session, rtsp->stream_id);
            pthread_mutex_lock(&global_lock);
            satip_frontend_default_tables(lfe, mmi->mmi_mux);
            pthread_mutex_unlock(&global_lock);
            satip_frontend_pid_changed(rtsp, lfe, buf);
          }
          break;
        default:
          if (rtsp->code >= 400) {
            tvhlog(LOG_ERR, "satip", "%s - RTSP cmd error %d (%s) [%i-%i]",
                   buf, r, strerror(-r), rtsp->cmd, rtsp->code);
            fatal = 1;
          }
          break;
        }
        rtsp->cmd = SATIP_RTSP_CMD_NONE;
      }
    }

    /* We need to keep the session alive */
    if (rtsp->ping_time + rtsp->timeout / 2 < dispatch_clock &&
        rtsp->cmd == SATIP_RTSP_CMD_NONE)
      satip_rtsp_options(rtsp);

    if (ev[0].data.u64 == (uint64_t)lfe->sf_rtcp) {
      c = recv(lfe->sf_rtcp->fd, rtcp, sizeof(rtcp), MSG_DONTWAIT);
      if (c > 0)
        satip_frontend_decode_rtcp(lfe, buf, mmi, rtcp, c);
      continue;
    }
    
    if (ev[0].data.u64 != (uint64_t)lfe->sf_rtp)
      continue;     

    tc = recvmmsg(lfe->sf_rtp->fd, msg, PKTS, MSG_DONTWAIT, NULL);

    if (tc < 0) {
      if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
        continue;
      if (errno == EOVERFLOW) {
        tvhlog(LOG_WARNING, "satip", "%s - recvmsg() EOVERFLOW", buf);
        continue;
      }
      tvhlog(LOG_ERR, "satip", "%s - recv() error %d (%s)",
             buf, errno, strerror(errno));
      break;
    }

    for (i = 0; i < tc; i++) {
      p = tsb[i];
      c = msg[i].msg_len;

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
      else if (((seq + 1) & 0xffff) != nseq)
        mmi->mmi_stats.unc++;
      seq = nseq;
      /* Process */
      sbuf_append(&sb, p + pos, c - pos);
      mpegts_input_recv_packets((mpegts_input_t*)lfe, mmi,
                                &sb, 0, NULL, NULL);
    }
  }

  sbuf_free(&sb);

  ev[0].events             = TVHPOLL_IN;
  ev[0].fd                 = lfe->sf_rtp->fd;
  ev[0].data.u64           = (uint64_t)lfe->sf_rtp;
  ev[1].events             = TVHPOLL_IN;
  ev[1].fd                 = lfe->sf_rtcp->fd;
  ev[1].data.u64           = (uint64_t)lfe->sf_rtcp;
  ev[2].events             = TVHPOLL_IN;
  ev[2].fd                 = lfe->sf_dvr_pipe.rd;
  ev[2].data.u64           = 0;
  tvhpoll_rem(efd, ev, 3);

  if (rtsp->stream_id) {
    r = satip_rtsp_teardown(rtsp);
    if (r < 0) {
      tvhtrace("satip", "%s - bad teardown", buf);
    } else {
      if (r == SATIP_RTSP_INCOMPLETE) {
        evr.events |= TVHPOLL_OUT;
        tvhpoll_add(efd, &evr, 1);
      }
      r = 0;
      while (r == SATIP_RTSP_INCOMPLETE) {
        if (!rtsp->sending) {
          evr.events &= ~TVHPOLL_OUT;
          tvhpoll_add(efd, &evr, 1);
        }
        nfds = tvhpoll_wait(efd, ev, 1, -1);
        if (nfds < 0)
          break;
        r = satip_rtsp_run(rtsp);
      }
    }
  }
  satip_rtsp_connection_close(rtsp);

  tvhpoll_destroy(efd);
  return NULL;
#undef PKTS
}

/* **************************************************************************
 * Tuning
 * *************************************************************************/

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
  sigstat.status_text  = signal2str(lfe->sf_status);
  sigstat.snr          = mmi->mmi_stats.snr;
  sigstat.signal       = mmi->mmi_stats.signal;
  sigstat.ber          = mmi->mmi_stats.ber;
  sigstat.unc          = mmi->mmi_stats.unc;
  sm.sm_type = SMT_SIGNAL_STATUS;
  sm.sm_data = &sigstat;
  LIST_FOREACH(svc, &lfe->mi_transports, s_active_link) {
    pthread_mutex_lock(&svc->s_stream_mutex);
    streaming_pad_deliver(&svc->s_streaming_pad, &sm);
    pthread_mutex_unlock(&svc->s_stream_mutex);
  }
  gtimer_arm_ms(&lfe->sf_monitor_timer, satip_frontend_signal_cb, lfe, 250);
}

static int
satip_frontend_tune0
  ( satip_frontend_t *lfe, mpegts_mux_instance_t *mmi )
{
  mpegts_mux_instance_t *cur = LIST_FIRST(&lfe->mi_mux_active);
  udp_connection_t *uc1 = NULL, *uc2 = NULL;
  int res = 0;

  if (cur != NULL) {
    /* Already tuned */
    if (mmi == cur)
      return 0;

    /* Stop current */
    cur->mmi_mux->mm_stop(cur->mmi_mux, 1);
  }
  assert(LIST_FIRST(&lfe->mi_mux_active) == NULL);

  assert(lfe->sf_pids == NULL);
  assert(lfe->sf_pids_tuned == NULL);
  lfe->sf_pids_count      = 0;
  lfe->sf_pids_tcount     = 0;
  lfe->sf_pids_size       = 512;
  lfe->sf_pids            = calloc(lfe->sf_pids_size, sizeof(uint16_t));
  lfe->sf_pids_tuned      = calloc(lfe->sf_pids_size, sizeof(uint16_t));
  lfe->sf_pids_any        = 0;
  lfe->sf_pids_any_tuned  = 0;
  lfe->sf_status          = SIGNAL_NONE;

retry:
  if (lfe->sf_rtp == NULL) {
    lfe->sf_rtp = udp_bind("satip", "satip_rtp",
                           lfe->sf_device->sd_info.myaddr,
                           lfe->sf_udp_rtp_port,
                           NULL, SATIP_BUF_SIZE);
    if (lfe->sf_rtp == NULL || lfe->sf_rtp == UDP_FATAL_ERROR)
      res = SM_CODE_TUNING_FAILED;
    else
      lfe->sf_rtp_port = ntohs(IP_PORT(lfe->sf_rtp->ip));
  }
  if (lfe->sf_rtcp == NULL && !res) {
    lfe->sf_rtcp = udp_bind("satip", "satip_rtcp",
                            lfe->sf_device->sd_info.myaddr,
                            lfe->sf_rtp_port + 1,
                            NULL, 16384);
    if (lfe->sf_rtcp == NULL || lfe->sf_rtcp == UDP_FATAL_ERROR) {
      if (lfe->sf_udp_rtp_port > 0)
        res = SM_CODE_TUNING_FAILED;
      else if (uc1 && uc2)
        res = SM_CODE_TUNING_FAILED;
      /* try to find another free UDP port */
      if (!res) {
        if (uc1 == NULL)
          uc1 = lfe->sf_rtp;
        else
          uc2 = lfe->sf_rtp;
        lfe->sf_rtp = NULL;
        goto retry;
      }
    }
  }
  udp_close(uc1);
  udp_close(uc2);

  if (!res) {
    lfe->sf_mmi = mmi;

    tvh_pipe(O_NONBLOCK, &lfe->sf_dvr_pipe);
    tvhthread_create(&lfe->sf_dvr_thread, NULL,
                     satip_frontend_input_thread, lfe, 0);

    gtimer_arm_ms(&lfe->sf_monitor_timer, satip_frontend_signal_cb, lfe, 250);

    lfe->sf_running = 1;
  }

  return res;
}

static int
satip_frontend_tune1
  ( satip_frontend_t *lfe, mpegts_mux_instance_t *mmi )
{
  char buf1[256], buf2[256];

  lfe->mi_display_name((mpegts_input_t*)lfe, buf1, sizeof(buf1));
  mmi->mmi_mux->mm_display_name(mmi->mmi_mux, buf2, sizeof(buf2));
  tvhdebug("satip", "%s - starting %s", buf1, buf2);

  /* Tune */
  tvhtrace("satip", "%s - tuning", buf1);
  return satip_frontend_tune0(lfe, mmi);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

satip_frontend_t *
satip_frontend_create
  ( htsmsg_t *conf, satip_device_t *sd, dvb_fe_type_t type, int t2, int num )
{
  const idclass_t *idc;
  const char *uuid = NULL;
  char id[12], lname[256];
  satip_frontend_t *lfe;

  /* Internal config ID */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(type), num);
  if (conf)
    conf = htsmsg_get_map(conf, id);
  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");

  /* Class */
  if (type == DVB_TYPE_S)
    idc = &satip_frontend_dvbs_class;
  else if (type == DVB_TYPE_T)
    idc = &satip_frontend_dvbt_class;
  else {
    tvherror("satip", "unknown FE type %d", type);
    return NULL;
  }

  // Note: there is a bit of a chicken/egg issue below, without the
  //       correct "fe_type" we cannot set the network (which is done
  //       in mpegts_input_create()). So we must set early.
  lfe = calloc(1, sizeof(satip_frontend_t));
  lfe->sf_number   = num;
  lfe->sf_type     = type;
  lfe->sf_type_t2  = t2;
  TAILQ_INIT(&lfe->sf_satconf);
  pthread_mutex_init(&lfe->sf_dvr_lock, NULL);
  lfe = (satip_frontend_t*)mpegts_input_create0((mpegts_input_t*)lfe, idc, uuid, conf);
  if (!lfe) return NULL;

  /* Defaults */
  lfe->sf_position     = -1;

  /* Callbacks */
  lfe->mi_is_free      = satip_frontend_is_free;
  lfe->mi_get_weight   = satip_frontend_get_weight;
  lfe->mi_get_priority = satip_frontend_get_priority;
  lfe->mi_get_grace    = satip_frontend_get_grace;

  /* Default name */
  if (!lfe->mi_name) {
    snprintf(lname, sizeof(lname), "SAT>IP %s Tuner %s #%i",
             dvb_type2str(type), sd->sd_info.addr, num);
    lfe->mi_name = strdup(lname);
  }

  /* Input callbacks */
  lfe->mi_is_enabled     = satip_frontend_is_enabled;
  lfe->mi_start_mux      = satip_frontend_start_mux;
  lfe->mi_stop_mux       = satip_frontend_stop_mux;
  lfe->mi_network_list   = satip_frontend_network_list;
  lfe->mi_open_pid       = satip_frontend_open_pid;
  lfe->mi_close_pid      = satip_frontend_close_pid;

  /* Adapter link */
  lfe->sf_device = sd;
  TAILQ_INSERT_TAIL(&sd->sd_frontends, lfe, sf_link);

  /* Create satconf */
  if (lfe->sf_type == DVB_TYPE_S)
    satip_satconf_create(lfe, conf);

  return lfe;
}

void
satip_frontend_save ( satip_frontend_t *lfe, htsmsg_t *fe )
{
  char id[12];
  htsmsg_t *m = htsmsg_create_map();

  /* Save frontend */
  mpegts_input_save((mpegts_input_t*)lfe, m);
  htsmsg_add_str(m, "type", dvb_type2str(lfe->sf_type));
  satip_satconf_save(lfe, m);
  if (lfe->ti_id.in_class == &satip_frontend_dvbs_class)
    htsmsg_delete_field(m, "networks");

  /* Add to list */
  snprintf(id, sizeof(id), "%s #%d", dvb_type2str(lfe->sf_type), lfe->sf_number);
  htsmsg_add_msg(fe, id, m);
}

void
satip_frontend_delete ( satip_frontend_t *lfe )
{
  mpegts_mux_instance_t *mmi;

  lock_assert(&global_lock);

  /* Ensure we're stopped */
  if ((mmi = LIST_FIRST(&lfe->mi_mux_active)))
    mmi->mmi_mux->mm_stop(mmi->mmi_mux, 1);

  gtimer_disarm(&lfe->sf_monitor_timer);

  /* Remove from adapter */
  TAILQ_REMOVE(&lfe->sf_device->sd_frontends, lfe, sf_link);

  /* Delete satconf */
  satip_satconf_destroy(lfe);

  /* Finish */
  mpegts_input_delete((mpegts_input_t*)lfe, 0);
}
