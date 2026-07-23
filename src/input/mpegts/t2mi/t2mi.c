/*
 *  Tvheadend - T2-MI / TS piping decapsulation input
 *
 *  Copyright (C) 2026 Tvheadend Project
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

/*
 * A T2-MI network is a virtual mpegts network whose muxes are carried
 * inside another mux of this tvheadend instance: either as a T2-MI
 * stream (ETSI TS 102 773, baseband frames of a DVB-T2 PLP) or as a
 * plain "TS in TS" data piping stream (ETSI EN 301 192), both delivered
 * on one carrier PID of the source mux.
 *
 * When a T2-MI mux is started, an internal subscription to the source
 * mux (or to the carrier service of the source mux) is created.  Using
 * the carrier service is the recommended mode: the packets then pass
 * through the regular service layer, including the descrambler, which
 * makes scrambled carriers work exactly like any other scrambled
 * service.  The decapsulated inner transport stream is fed back into the
 * standard mpegts input path, so scanning, service discovery, EPG and
 * streaming of the inner network work as for any other mux.
 */

#include "t2mi_private.h"
#include "settings.h"
#include "streaming.h"

#define T2MI_BUF_SIZE (300 * 188)

static void t2mi_mux_unsubscribe(t2mi_mux_t *tm);

static t2mi_input_t *t2mi_input;

/* **************************************************************************
 * Input class
 * *************************************************************************/

extern const idclass_t mpegts_input_class;
const idclass_t t2mi_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "t2mi_input",
  .ic_caption    = N_("T2-MI input"),
  .ic_properties = (const property_t[]){
    {}
  }
};

static int
t2mi_input_is_enabled
  ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)mm;

  int r = mpegts_input_is_enabled(mi, mm, flags, weight);
  if (r != MI_IS_ENABLED_OK)
    return r;
  if (tm->mm_t2mi_src_mux == NULL || tm->mm_t2mi_src_mux[0] == '\0')
    return MI_IS_ENABLED_NEVER;
  return MI_IS_ENABLED_OK;
}

static int
t2mi_input_get_priority ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags )
{
  t2mi_network_t *tn = (t2mi_network_t *)mm->mm_network;
  if ((flags & SUBSCRIPTION_STREAMING) && tn->tn_streaming_priority > 0)
    return tn->tn_streaming_priority;
  return tn->tn_priority;
}

static int
t2mi_input_get_weight ( mpegts_input_t *mi, mpegts_mux_t *mm, int flags, int weight )
{
  /* the input is virtual and never busy: any number of T2-MI muxes can
   * be active at once (e.g. several carriers of one transponder); the
   * real capacity limit is the source mux subscription underneath */
  return 0;
}

static int
t2mi_input_warm_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  /* unlike the default single-tuner implementation, do not stop other
   * active T2-MI muxes when a new one starts */
  return 0;
}

static int
t2mi_input_get_grace ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  t2mi_network_t *tn = (t2mi_network_t *)mm->mm_network;
  return tn->tn_max_timeout ?: 30;
}

static void
t2mi_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "T2-MI");
}

/*
 * Streaming target receiving the source mux / carrier service stream
 */

static void
t2mi_mux_decap_output ( void *aux, const uint8_t *tsb, int len )
{
  t2mi_mux_t *tm = aux;
  sbuf_append(&tm->tm_buffer, tsb, len);
}

static void
t2mi_mux_decap_log ( void *aux, int severity, const char *fmt, ... )
{
  t2mi_mux_t *tm = aux;
  char buf[512];
  va_list args;
  static const int sevmap[4] =
    { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  tvhlog(sevmap[MINMAX(severity, 0, 3)], LS_T2MI, "%s - %s",
         tm->mm_nicename, buf);
}

static void
t2mi_mux_stream_start ( t2mi_mux_t *tm, const streaming_start_t *ss )
{
  const streaming_start_component_t *ssc;
  int i, t2mi_pid = 0, data_pid = 0, data_cnt = 0;

  if (tm->mm_t2mi_src_pid) {
    tm->tm_carrier_pid = tm->mm_t2mi_src_pid;
    return;
  }
  if (tm->tm_carrier_pid > 0)
    return;

  /* resolve the carrier PID from the service components: prefer an
   * explicit T2-MI component, else accept a lone data component */
  for (i = 0; i < ss->ss_num_components; i++) {
    ssc = &ss->ss_components[i];
    if (ssc->es_pid <= 0 || ssc->es_pid >= 8191)
      continue;
    if (ssc->es_type == SCT_T2MI) {
      t2mi_pid = ssc->es_pid;
      break;
    }
    if (ssc->es_type != SCT_CA && ssc->es_type != SCT_CAT &&
        ssc->es_type != SCT_PCR) {
      if (data_cnt++ == 0)
        data_pid = ssc->es_pid;
    }
  }
  if (t2mi_pid == 0 && data_cnt == 1)
    t2mi_pid = data_pid;
  if (t2mi_pid > 0) {
    tm->tm_carrier_pid = t2mi_pid;
    tvhinfo(LS_T2MI, "%s - using carrier PID %d", tm->mm_nicename, t2mi_pid);
  } else {
    tvhwarn(LS_T2MI, "%s - unable to resolve carrier PID, "
            "set it in the mux configuration", tm->mm_nicename);
  }
}

static void
t2mi_mux_stream_cb ( void *opaque, streaming_message_t *sm )
{
  t2mi_mux_t *tm = opaque;
  mpegts_mux_instance_t *mmi;
  pktbuf_t *pb;
  const uint8_t *data;
  size_t len;
  int pid;

  switch (sm->sm_type) {
  case SMT_MPEGTS:
    mmi = tm->mm_active;
    if (mmi == NULL || tm->tm_decap == NULL || tm->tm_carrier_pid <= 0)
      break;
    pb = sm->sm_data;
    data = pktbuf_ptr(pb);
    len = pktbuf_len(pb);
    for ( ; len >= 188; data += 188, len -= 188) {
      if (data[0] != 0x47)
        break;
      pid = ((data[1] & 0x1f) << 8) | data[2];
      if (pid != tm->tm_carrier_pid)
        continue;
      /* skip packets that are still scrambled */
      if (data[3] & 0xc0)
        continue;
      t2mi_decap_input(tm->tm_decap, data);
    }
    if (tm->tm_buffer.sb_ptr > 0)
      mpegts_input_recv_packets(mmi, &tm->tm_buffer, 0, NULL);
#if ENABLE_TRACE
    {
      const t2mi_decap_stats_t *st = t2mi_decap_get_stats(tm->tm_decap);
      if (st->in_packets && (st->in_packets & 0x3fff) < 8)
        tvhtrace(LS_T2MI, "%s - stats: in %"PRIu64" t2mi %"PRIu64
                 " bb %"PRIu64" out %"PRIu64" crc32err %"PRIu64
                 " crc8err %"PRIu64" ccerr %"PRIu64" resync %"PRIu64
                 " dnp %"PRIu64,
                 tm->mm_nicename, st->in_packets, st->t2mi_packets,
                 st->bb_frames, st->out_packets, st->crc32_errors,
                 st->crc8_errors, st->cc_errors, st->resyncs,
                 st->dnp_packets);
    }
#endif
    break;
  case SMT_START:
    tvhdebug(LS_T2MI, "%s - source subscription started", tm->mm_nicename);
    t2mi_mux_stream_start(tm, sm->sm_data);
    break;
  case SMT_NOSTART:
  case SMT_NOSTART_WARN:
    tvhwarn(LS_T2MI, "%s - source subscription cannot start (%s)",
            tm->mm_nicename,
            streaming_code2txt(sm->sm_code));
    break;
  case SMT_STOP:
    tvhdebug(LS_T2MI, "%s - source subscription stopped", tm->mm_nicename);
    break;
  default:
    break;
  }
  streaming_msg_free(sm);
}

static htsmsg_t *
t2mi_mux_stream_info ( void *opaque, htsmsg_t *list )
{
  htsmsg_add_str(list, NULL, "t2mi input");
  return list;
}

static streaming_ops_t t2mi_mux_stream_ops = {
  .st_cb   = t2mi_mux_stream_cb,
  .st_info = t2mi_mux_stream_info
};

/*
 * Source subscription management
 */

static int
t2mi_mux_subscribe ( t2mi_mux_t *tm, mpegts_input_t *mi, int weight )
{
  mpegts_mux_t *src;
  mpegts_service_t *svc;
  mpegts_apids_t pids;
  char buf[256];
  int flags = SUBSCRIPTION_MPEGTS | SUBSCRIPTION_STREAMING;

  lock_assert(&global_lock);

  src = mpegts_mux_find(tm->mm_t2mi_src_mux ?: "");
  if (src == NULL) {
    tvherror(LS_T2MI, "%s - source mux not found (%s)",
             tm->mm_nicename, tm->mm_t2mi_src_mux ?: "not set");
    return SM_CODE_TUNING_FAILED;
  }
  if (src == (mpegts_mux_t *)tm) {
    tvherror(LS_T2MI, "%s - source mux cannot be the mux itself",
             tm->mm_nicename);
    return SM_CODE_TUNING_FAILED;
  }

  tm->tm_carrier_pid = tm->mm_t2mi_src_pid ?: 0;

  streaming_target_init(&tm->tm_st, &t2mi_mux_stream_ops, tm, 0);

  if (tm->mm_t2mi_src_sid > 0) {
    /* carrier service mode - the service layer descrambles for us */
    svc = mpegts_service_find(src, tm->mm_t2mi_src_sid, 0, 0, NULL);
    if (svc == NULL) {
      tvherror(LS_T2MI, "%s - carrier service %d not found on %s",
               tm->mm_nicename, tm->mm_t2mi_src_sid,
               src->mm_nicename ?: "source mux");
      return SM_CODE_TUNING_FAILED;
    }
    profile_chain_init(&tm->tm_prch, NULL, svc, 0);
    tm->tm_prch.prch_st = &tm->tm_st;
    tm->tm_prch.prch_flags = SUBSCRIPTION_MPEGTS;
    tm->tm_sub = subscription_create_from_service
      (&tm->tm_prch, NULL, weight ?: 10, "t2mi", flags,
       NULL, NULL, "t2mi", NULL);
  } else {
    /* raw carrier PID mode - no descrambling possible */
    if (tm->tm_carrier_pid <= 0) {
      tvherror(LS_T2MI, "%s - carrier PID must be set when no carrier "
               "service is configured", tm->mm_nicename);
      return SM_CODE_TUNING_FAILED;
    }
    profile_chain_init(&tm->tm_prch, NULL, src, 0);
    tm->tm_prch.prch_st = &tm->tm_st;
    tm->tm_prch.prch_flags = SUBSCRIPTION_MPEGTS;
    tm->tm_sub = subscription_create_from_mux
      (&tm->tm_prch, NULL, weight ?: 10, "t2mi", flags,
       NULL, NULL, "t2mi", NULL);
    if (tm->tm_sub) {
      mpegts_service_t *ms = (mpegts_service_t *)tm->tm_sub->ths_service;
      mpegts_pid_init(&pids);
      mpegts_pid_add(&pids, tm->tm_carrier_pid, MPS_WEIGHT_RAW);
      if (ms->s_update_pids(ms, &pids)) {
        t2mi_mux_unsubscribe(tm);
        mpegts_pid_done(&pids);
        return SM_CODE_TUNING_FAILED;
      }
      mpegts_pid_done(&pids);
    }
  }

  if (tm->tm_sub == NULL) {
    tvherror(LS_T2MI, "%s - unable to subscribe to source mux %s",
             tm->mm_nicename,
             src->mm_display_name ?
               (src->mm_display_name(src, buf, sizeof(buf)), buf) : "");
    profile_chain_close(&tm->tm_prch);
    return SM_CODE_TUNING_FAILED;
  }
  return 0;
}

static void
t2mi_mux_unsubscribe ( t2mi_mux_t *tm )
{
  lock_assert(&global_lock);

  if (tm->tm_sub) {
    subscription_unsubscribe(tm->tm_sub, UNSUBSCRIBE_FINAL);
    tm->tm_sub = NULL;
  }
  profile_chain_close(&tm->tm_prch);
}

/* Keep the source subscription weight in step with the highest-weight
 * client watching this T2-MI mux, so the source mux (parent tuner) is
 * arbitrated as if the clients subscribed to it directly - a high
 * priority recording of an inner service is not preempted by a lower
 * priority subscription competing for the tuner.  A client that is just
 * being opened is not yet linked on the mux, so its weight is folded in
 * explicitly via the caller. */
static void
t2mi_mux_reweight_source ( t2mi_mux_t *tm, int opening_weight )
{
  mpegts_mux_instance_t *mmi = tm->mm_active;
  int w;

  lock_assert(&global_lock);
  if (tm->tm_sub == NULL || mmi == NULL)
    return;
  tvh_mutex_lock(&mmi->mmi_input->mi_output_lock);
  w = mpegts_mux_instance_weight(mmi);
  tvh_mutex_unlock(&mmi->mmi_input->mi_output_lock);
  if (opening_weight > w)
    w = opening_weight;
  if (w > 0 && tm->tm_sub->ths_weight != w) {
    tvhtrace(LS_T2MI, "%s - source subscription weight -> %d",
             tm->mm_nicename, w);
    subscription_set_weight(tm->tm_sub, w);
  }
}

static void
t2mi_input_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int flags, int first, int weight )
{
  mpegts_input_open_service(mi, s, flags, first, weight);
  t2mi_mux_reweight_source((t2mi_mux_t *)s->s_dvb_mux, weight);
}

static void
t2mi_input_close_service ( mpegts_input_t *mi, mpegts_service_t *s )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)s->s_dvb_mux;
  mpegts_input_close_service(mi, s);
  t2mi_mux_reweight_source(tm, 0);
}

static int
t2mi_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi, int weight )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)mmi->mmi_mux;
  int ret;

  if (tm->tm_running)
    return 0;

  sbuf_reset_and_alloc(&tm->tm_buffer, T2MI_BUF_SIZE);
  tm->tm_decap = t2mi_decap_create(tm->mm_t2mi_format,
                                   tm->mm_t2mi_plp < 0 ?
                                     T2MI_DECAP_PLP_AUTO : tm->mm_t2mi_plp,
                                   t2mi_mux_decap_output, tm,
                                   t2mi_mux_decap_log, tm);
  if (tm->tm_decap == NULL)
    return SM_CODE_TUNING_FAILED;

  /* accept pid open calls before the subscription starts delivering */
  tm->mm_active = mmi;

  ret = t2mi_mux_subscribe(tm, mi, weight);
  if (ret) {
    tm->mm_active = NULL;
    t2mi_decap_destroy(tm->tm_decap);
    tm->tm_decap = NULL;
    sbuf_free(&tm->tm_buffer);
    return ret;
  }

  tm->tm_running = 1;

  psi_tables_install(mi, (mpegts_mux_t *)tm, DVB_SYS_DVBT);

  return 0;
}

static void
t2mi_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)mmi->mmi_mux;
  const t2mi_decap_stats_t *st;

  t2mi_mux_unsubscribe(tm);

  if (tm->tm_decap) {
    st = t2mi_decap_get_stats(tm->tm_decap);
    tvhinfo(LS_T2MI, "%s - stopped (in %"PRIu64" out %"PRIu64" packets, "
            "%"PRIu64" T2-MI packets, %"PRIu64" BB frames, "
            "%"PRIu64" CRC errors)",
            tm->mm_nicename, st->in_packets, st->out_packets,
            st->t2mi_packets, st->bb_frames,
            st->crc32_errors + st->crc8_errors);
    t2mi_decap_destroy(tm->tm_decap);
    tm->tm_decap = NULL;
  }
  sbuf_free(&tm->tm_buffer);
  tm->tm_running = 0;
}

static t2mi_input_t *
t2mi_input_create ( void )
{
  t2mi_input_t *mi = calloc(1, sizeof(*mi));
  mpegts_network_t *mn;

  mpegts_input_create0((mpegts_input_t *)mi, &t2mi_input_class, NULL, NULL);
  mi->mi_start_mux      = t2mi_input_start_mux;
  mi->mi_stop_mux       = t2mi_input_stop_mux;
  mi->mi_is_enabled     = t2mi_input_is_enabled;
  mi->mi_get_priority   = t2mi_input_get_priority;
  mi->mi_get_weight     = t2mi_input_get_weight;
  mi->mi_warm_mux       = t2mi_input_warm_mux;
  mi->mi_open_service   = t2mi_input_open_service;
  mi->mi_close_service  = t2mi_input_close_service;
  mi->mi_get_grace      = t2mi_input_get_grace;
  mi->mi_display_name   = t2mi_input_display_name;
  mi->mi_enabled        = 1;

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
    if (idnode_is_instance(&mn->mn_id, &t2mi_network_class))
      mpegts_input_add_network((mpegts_input_t *)mi, mn);

  return mi;
}

/* **************************************************************************
 * Mux class
 * *************************************************************************/

/* enumerate all muxes that can act as a T2-MI / piping source
 * (any mux of this instance except muxes of a T2-MI network) */
static htsmsg_t *
t2mi_src_mux_enum ( void *o, const char *lang )
{
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  htsmsg_t *l = htsmsg_create_list(), *e;
  char ubuf[UUID_HEX_SIZE], buf[384];

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (idnode_is_instance(&mn->mn_id, &t2mi_network_class))
      continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      e = htsmsg_create_map();
      htsmsg_add_str(e, "key", idnode_uuid_as_str(&mm->mm_id, ubuf));
      htsmsg_add_str(e, "val", idnode_get_title(&mm->mm_id, lang,
                                                buf, sizeof(buf)));
      htsmsg_add_msg(l, NULL, e);
    }
  }
  return l;
}

static htsmsg_t *
t2mi_mux_class_format_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Auto-detect"),               T2MI_DECAP_FORMAT_AUTO },
    { N_("T2-MI (TS 102 773)"),        T2MI_DECAP_FORMAT_T2MI },
    { N_("Plain TS data piping"),      T2MI_DECAP_FORMAT_PIPE },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t t2mi_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "t2mi_mux",
  .ic_caption    = N_("T2-MI Multiplex"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "t2mi_src_mux",
      .name     = N_("Source mux"),
      .desc     = N_("The mux carrying this stream (e.g. the DVB-S2 "
                     "transponder)."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_src_mux),
      .list     = t2mi_src_mux_enum,
    },
    {
      .type     = PT_U32,
      .id       = "t2mi_src_sid",
      .name     = N_("Carrier service ID"),
      .desc     = N_("Service ID of the carrier service inside the source "
                     "mux. Using the carrier service enables descrambling "
                     "of scrambled carriers. Set to 0 to read the carrier "
                     "PID directly without a service (clear carriers "
                     "only)."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_src_sid),
      .def.u32  = 0,
    },
    {
      .type     = PT_U32,
      .id       = "t2mi_src_pid",
      .name     = N_("Carrier PID"),
      .desc     = N_("PID inside the source mux carrying the T2-MI or "
                     "piped stream (often 4096). 0 = detect from the "
                     "carrier service components."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_src_pid),
      .def.u32  = 0,
    },
    {
      .type     = PT_INT,
      .id       = "t2mi_format",
      .name     = N_("Carrier format"),
      .desc     = N_("Format of the carrier stream: T2-MI baseband "
                     "frames or plain TS data piping."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_format),
      .list     = t2mi_mux_class_format_list,
      .def.i    = T2MI_DECAP_FORMAT_AUTO,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "t2mi_plp",
      .name     = N_("PLP ID"),
      .desc     = N_("Physical Layer Pipe to extract from a T2-MI "
                     "stream. -1 = first data PLP."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_plp),
      .def.i    = -1,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "t2mi_auto",
      .name     = N_("Automatically created"),
      .desc     = N_("This mux was created automatically from a discovered "
                     "carrier and is removed when its source mux is removed."),
      .off      = offsetof(t2mi_mux_t, mm_t2mi_auto),
      .opts     = PO_RDONLY | PO_HIDDEN,
    },
    {}
  }
};

static void
t2mi_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)mm;
  mpegts_mux_t *src;
  char sbuf[128];

  src = tm->mm_t2mi_src_mux ? mpegts_mux_find(tm->mm_t2mi_src_mux) : NULL;
  if (src && src->mm_display_name) {
    src->mm_display_name(src, sbuf, sizeof(sbuf));
  } else {
    strlcpy(sbuf, "?", sizeof(sbuf));
  }
  if (tm->mm_t2mi_src_sid)
    snprintf(buf, len, "%s/T2MI-SID-%u", sbuf, tm->mm_t2mi_src_sid);
  else
    snprintf(buf, len, "%s/T2MI-PID-%u", sbuf, tm->mm_t2mi_src_pid);
}

static htsmsg_t *
t2mi_mux_config_save ( mpegts_mux_t *mm, char *filename, size_t fsize )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  if (filename == NULL) {
    mpegts_mux_save(mm, c, 1);
  } else {
    mpegts_mux_save(mm, c, 0);
    snprintf(filename, fsize, "input/t2mi/networks/%s/muxes/%s",
             idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
             idnode_uuid_as_str(&mm->mm_id, ubuf2));
  }
  return c;
}

static void
t2mi_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  if (delconf)
    hts_settings_remove("input/t2mi/networks/%s/muxes/%s",
                        idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
                        idnode_uuid_as_str(&mm->mm_id, ubuf2));

  mpegts_mux_delete(mm, delconf);
}

static void
t2mi_mux_free ( mpegts_mux_t *mm )
{
  t2mi_mux_t *tm = (t2mi_mux_t *)mm;
  free(tm->mm_t2mi_src_mux);
  mpegts_mux_free(mm);
}

static t2mi_mux_t *
t2mi_mux_create0 ( t2mi_network_t *tn, const char *uuid, htsmsg_t *conf )
{
  htsmsg_t *c, *c2, *e;
  htsmsg_field_t *f;
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  t2mi_mux_t *tm =
    mpegts_mux_create(t2mi_mux, uuid, (mpegts_network_t *)tn,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf);
  if (tm == NULL)
    return NULL;

  tm->mm_display_name  = t2mi_mux_display_name;
  tm->mm_config_save   = t2mi_mux_config_save;
  tm->mm_delete        = t2mi_mux_delete;
  tm->mm_free          = t2mi_mux_free;

  sbuf_init(&tm->tm_buffer);

  /* services */
  c2 = NULL;
  c = htsmsg_get_map(conf, "services");
  if (c == NULL)
    c = c2 = hts_settings_load_r(1, "input/t2mi/networks/%s/muxes/%s/services",
                                 idnode_uuid_as_str(&tn->mn_id, ubuf1),
                                 idnode_uuid_as_str(&tm->mm_id, ubuf2));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      mpegts_service_create1(htsmsg_field_name(f), (mpegts_mux_t *)tm,
                             0, 0, e);
    }
  }
  htsmsg_destroy(c2);

  return tm;
}

/* **************************************************************************
 * Network class
 *
 * A T2-MI network references a set of source muxes and discovers the
 * T2-MI / piping carriers among their services, creating one inner mux
 * per carrier.  Muxes can also be added by hand; discovery and manual
 * muxes coexist in the same network.
 * *************************************************************************/

static void t2mi_network_discover ( t2mi_network_t *tn );

static void
t2mi_network_delete ( mpegts_network_t *mn, int delconf )
{
  t2mi_network_t *tn = (t2mi_network_t *)mn;

  mtimer_disarm(&tn->tn_disc_timer);
  /* drop the source mux mappings before the network memory goes away;
   * silent at shutdown (delconf == 0) */
  idnode_list_destroy(&tn->tn_src_muxes, delconf ? tn : NULL);
  mpegts_network_delete(mn, delconf);
}

static void
t2mi_network_class_delete ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t *)in;
  char ubuf[UUID_HEX_SIZE];

  hts_settings_remove("input/t2mi/networks/%s",
                      idnode_uuid_as_str(&mn->mn_id, ubuf));
  t2mi_network_delete(mn, 1);
}

/*
 * Discovery timer / triggering
 */

static void
t2mi_network_timer_cb ( void *aux )
{
  t2mi_network_t *tn = aux;
  t2mi_network_discover(tn);
  if (tn->tn_scan_period)
    mtimer_arm_rel(&tn->tn_disc_timer, t2mi_network_timer_cb, tn,
                   sec2mono(tn->tn_scan_period * 60));
}

static void
t2mi_network_trigger ( t2mi_network_t *tn )
{
  mtimer_arm_rel(&tn->tn_disc_timer, t2mi_network_timer_cb, tn, sec2mono(2));
}

/* a PMT on a source mux produced a T2-MI carrier component - reconcile
 * the networks that selected the mux (called from dvb_psi_pmt.c) */
void
t2mi_carrier_seen ( struct mpegts_mux *mm )
{
  idnode_list_mapping_t *ilm;

  lock_assert(&global_lock);
  LIST_FOREACH(ilm, &mm->mm_t2mi_networks, ilm_in2_link)
    t2mi_network_trigger((t2mi_network_t *)ilm->ilm_in1);
}

/* a source mux is being deleted: at a real deletion (delconf) cascade to
 * the inner muxes it produced, otherwise (shutdown) just unlink silently */
void
t2mi_source_mux_deleting ( struct mpegts_mux *mm, int delconf )
{
  mpegts_network_t *mn;
  mpegts_mux_t *tmm, *tmm_next;
  t2mi_mux_t *tm;
  char ubuf[UUID_HEX_SIZE];

  lock_assert(&global_lock);

  if (delconf) {
    idnode_uuid_as_str(&mm->mm_id, ubuf);
    LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
      if (!idnode_is_instance(&mn->mn_id, &t2mi_network_class))
        continue;
      for (tmm = LIST_FIRST(&mn->mn_muxes); tmm; tmm = tmm_next) {
        tmm_next = LIST_NEXT(tmm, mm_network_link);
        tm = (t2mi_mux_t *)tmm;
        if (tm->mm_t2mi_auto && tm->mm_t2mi_src_mux &&
            !strcmp(tm->mm_t2mi_src_mux, ubuf)) {
          tvhinfo(LS_T2MI, "source mux removed, removing %s", tmm->mm_nicename);
          tmm->mm_delete(tmm, 1);
        }
      }
    }
  }
  idnode_list_destroy(&mm->mm_t2mi_networks, delconf ? mm : NULL);
}

/*
 * Carrier discovery
 */

/* A genuine T2-MI / piping carrier service has exactly one data stream
 * (the carrier PID) and no audio or video.  Requiring that avoids false
 * positives from network/SI pseudo-services whose private streams were
 * flagged by the private-stream mapping (e.g. SID 65534 on Abertis). */
static int
t2mi_service_is_carrier ( mpegts_service_t *s )
{
  elementary_stream_t *es;
  int t2mi = 0, av = 0;

  tvh_mutex_lock(&s->s_stream_mutex);
  TAILQ_FOREACH(es, &s->s_components.set_all, es_link) {
    if (es->es_type == SCT_T2MI)
      t2mi++;
    else if (SCT_ISVIDEO(es->es_type) || SCT_ISAUDIO(es->es_type))
      av++;
  }
  tvh_mutex_unlock(&s->s_stream_mutex);

  if (t2mi > 0 && (t2mi != 1 || av)) {
    tvhtrace(LS_T2MI, "service %s (%u) not a carrier "
             "(%d T2-MI streams, %d A/V streams)",
             s->s_nicename ?: "", service_id16(s), t2mi, av);
    return 0;
  }
  return t2mi == 1;
}

/* Re-parse the PMTs of an already-tuned source mux so the just-enabled
 * private-stream mapping takes effect.  The mux is held open, so a queued
 * rescan would not run until it goes idle; instead drop the stale
 * NULL-opaque one-shot PMT tables left by the scan (they would make
 * mpegts_table_add a no-op) and install fresh ones. */
static void
t2mi_reparse_source_pmts ( mpegts_mux_t *mm )
{
  mpegts_table_t *mt, *stale[64];
  mpegts_service_t *s;
  int n = 0, i;

  tvh_mutex_lock(&mm->mm_tables_lock);
  LIST_FOREACH(mt, &mm->mm_tables, mt_link)
    if (mt->mt_callback == dvb_pmt_callback && mt->mt_opaque == NULL &&
        !mt->mt_destroyed && n < ARRAY_SIZE(stale)) {
      mpegts_table_grab(mt);
      stale[n++] = mt;
    }
  tvh_mutex_unlock(&mm->mm_tables_lock);

  for (i = 0; i < n; i++) {
    mpegts_table_destroy(stale[i]);
    mpegts_table_release(stale[i]);
  }

  LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
    uint16_t pid = s->s_components.set_pmt_pid;
    if (pid > 0 && pid < 8191)
      mpegts_table_add(mm, DVB_PMT_BASE, DVB_PMT_MASK, dvb_pmt_callback,
                       NULL, "pmt", LS_TBL_BASE,
                       MT_CRC | MT_QUICKREQ | MT_ONESHOT,
                       pid, MPS_WEIGHT_PMT_SCAN);
  }
}

/* is there already an inner mux for this (source mux, carrier SID)? */
static t2mi_mux_t *
t2mi_network_find_mux ( t2mi_network_t *tn, const char *src_uuid, uint16_t sid )
{
  mpegts_mux_t *mm;
  t2mi_mux_t *tm;

  LIST_FOREACH(mm, &tn->mn_muxes, mm_network_link) {
    tm = (t2mi_mux_t *)mm;
    if (tm->mm_t2mi_src_sid == sid && tm->mm_t2mi_src_mux &&
        !strcmp(tm->mm_t2mi_src_mux, src_uuid))
      return tm;
  }
  return NULL;
}

static t2mi_mux_t *
t2mi_network_add_mux ( t2mi_network_t *tn, const char *src_uuid, uint16_t sid )
{
  htsmsg_t *conf = htsmsg_create_map();
  t2mi_mux_t *tm;

  htsmsg_add_str(conf, "t2mi_src_mux", src_uuid);
  htsmsg_add_u32(conf, "t2mi_src_sid", sid);
  htsmsg_add_s32(conf, "t2mi_format", T2MI_DECAP_FORMAT_AUTO);
  htsmsg_add_s32(conf, "t2mi_plp", T2MI_DECAP_PLP_AUTO);
  htsmsg_add_bool(conf, "t2mi_auto", 1);
  htsmsg_add_bool(conf, "enabled", 1);
  tm = t2mi_mux_create0(tn, NULL, conf);
  htsmsg_destroy(conf);
  if (tm)
    mpegts_mux_post_create((mpegts_mux_t *)tm);
  return tm;
}

static void
t2mi_network_discover ( t2mi_network_t *tn )
{
  idnode_list_mapping_t *ilm;
  mpegts_mux_t *mm;
  mpegts_service_t *s;
  t2mi_mux_t *tm;
  char src_ubuf[UUID_HEX_SIZE];
  int created = 0;
  uint16_t sid;

  lock_assert(&global_lock);

  /* mark all discovered muxes unseen so we can spot the vanished ones */
  LIST_FOREACH(mm, &tn->mn_muxes, mm_network_link)
    ((t2mi_mux_t *)mm)->tm_seen = 0;

  LIST_FOREACH(ilm, &tn->tn_src_muxes, ilm_in1_link) {
    mm = (mpegts_mux_t *)ilm->ilm_in2;

    /* for feeds without a T2MI descriptor, flag the private streams as
     * carriers and reparse; the carriers then re-trigger discovery */
    if (tn->tn_src_private && !mm->mm_t2mi_carriers) {
      mm->mm_t2mi_carriers = 1;
      idnode_changed(&mm->mm_id);
      if (mm->mm_active)
        t2mi_reparse_source_pmts(mm);
      else
        mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
    }

    idnode_uuid_as_str(&mm->mm_id, src_ubuf);
    LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link) {
      sid = service_id16(s);
      if (sid == 0 || !t2mi_service_is_carrier(s))
        continue;
      if ((tm = t2mi_network_find_mux(tn, src_ubuf, sid)) != NULL) {
        tm->tm_seen = 1;
        continue;
      }
      /* honor the network discovery setting for new carriers */
      if (tn->mn_autodiscovery == MN_DISCOVERY_DISABLE)
        continue;
      if ((tm = t2mi_network_add_mux(tn, src_ubuf, sid)) != NULL) {
        tm->tm_seen = 1;
        created++;
        tvhinfo(LS_T2MI, "discovered carrier on %s SID %u", src_ubuf, sid);
      }
    }
  }

  /* Discovered muxes whose carrier is not currently present are KEPT
   * (their scan simply fails, as for a dark DVB transponder) so that mux
   * and service UUIDs - and any channel mappings - survive feed changes.
   * They are removed only with their source mux (t2mi_source_mux_deleting)
   * or when the whole network is deleted. */

  if (created)
    tvhinfo(LS_T2MI, "%s: %d carrier(s) added",
            tn->mn_network_name ?: "", created);
}

/*
 * Class
 */

static void
t2mi_network_scan ( mpegts_network_t *mn )
{
  t2mi_network_t *tn = (t2mi_network_t *)mn;
  idnode_list_mapping_t *ilm;

  /* rescan the source muxes so carriers are (re)discovered, then reconcile */
  LIST_FOREACH(ilm, &tn->tn_src_muxes, ilm_in1_link)
    mpegts_mux_scan_state_set((mpegts_mux_t *)ilm->ilm_in2,
                              MM_SCAN_STATE_PEND);
  t2mi_network_trigger(tn);
}

static void
t2mi_network_class_changed ( idnode_t *in )
{
  t2mi_network_trigger((t2mi_network_t *)in);
}

static int
t2mi_network_src_mux_link ( idnode_t *in1, idnode_t *in2, void *origin )
{
  t2mi_network_t *tn = (t2mi_network_t *)in1;
  mpegts_mux_t *mm = (mpegts_mux_t *)in2;

  /* a T2-MI mux cannot be the source of another T2-MI mux */
  if (idnode_is_instance(&mm->mm_network->mn_id, &t2mi_network_class))
    return 0;
  return idnode_list_link(in1, &tn->tn_src_muxes,
                          in2, &mm->mm_t2mi_networks, origin, 1) ? 1 : 0;
}

static int
t2mi_network_class_src_muxes_set ( void *o, const void *v )
{
  t2mi_network_t *tn = o;
  return idnode_list_set1(&tn->mn_id, &tn->tn_src_muxes,
                          &mpegts_mux_class, (htsmsg_t *)v,
                          t2mi_network_src_mux_link);
}

static const void *
t2mi_network_class_src_muxes_get ( void *o )
{
  return idnode_list_get1(&((t2mi_network_t *)o)->tn_src_muxes);
}

static char *
t2mi_network_class_src_muxes_rend ( void *o, const char *lang )
{
  return idnode_list_get_csv1(&((t2mi_network_t *)o)->tn_src_muxes, lang);
}

PROP_DOC(priority)
PROP_DOC(streaming_priority)

extern const idclass_t mpegts_network_class;
const idclass_t t2mi_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "t2mi_network",
  .ic_caption    = N_("T2-MI Network"),
  .ic_delete     = t2mi_network_class_delete,
  .ic_changed    = t2mi_network_class_changed,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .islist   = 1,
      .id       = "src_muxes",
      .name     = N_("Source muxes"),
      .desc     = N_("The muxes carrying the T2-MI / piped streams (e.g. "
                     "the DVB-S transponders). Their services are checked "
                     "for carriers and a T2-MI mux is created for each "
                     "carrier found (unless network discovery is disabled). "
                     "A mux whose carrier disappears is kept - its scan "
                     "fails - and resumes when the carrier returns; muxes "
                     "are only removed when their source mux is removed "
                     "from Tvheadend."),
      .set      = t2mi_network_class_src_muxes_set,
      .get      = t2mi_network_class_src_muxes_get,
      .list     = t2mi_src_mux_enum,
      .rend     = t2mi_network_class_src_muxes_rend,
    },
    {
      .type     = PT_BOOL,
      .id       = "src_private",
      .name     = N_("Detect private-stream carriers"),
      .desc     = N_("Treat private data streams on the source muxes as "
                     "T2-MI / piping carriers (enables 'Map private streams "
                     "as T2-MI carriers' on them). Needed for feeds without "
                     "a T2MI descriptor, such as Abertis/Cellnex."),
      .off      = offsetof(t2mi_network_t, tn_src_private),
      .def.i    = 1,
    },
    {
      .type     = PT_U32,
      .id       = "scan_period",
      .name     = N_("Rediscovery period (minutes)"),
      .desc     = N_("How often to re-scan the source muxes for new or "
                     "removed carriers."),
      .off      = offsetof(t2mi_network_t, tn_scan_period),
      .def.i    = 60,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .desc     = N_("The network's priority. The network with the "
                     "highest priority value will be used out of "
                     "preference if available. See Help for details."),
      .off      = offsetof(t2mi_network_t, tn_priority),
      .doc      = prop_doc_priority,
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming priority"),
      .desc     = N_("When streaming a service (via http or htsp) "
                     "Tvheadend will use the network with the highest "
                     "streaming priority set here. See Help for details."),
      .doc      = prop_doc_streaming_priority,
      .off      = offsetof(t2mi_network_t, tn_streaming_priority),
      .def.i    = 1,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "max_timeout",
      .name     = N_("Maximum timeout (seconds)"),
      .desc     = N_("Maximum time to wait (in seconds) for a stream "
                     "before a timeout. The source mux may need to tune "
                     "and descramble first."),
      .off      = offsetof(t2mi_network_t, tn_max_timeout),
      .def.i    = 30,
      .opts     = PO_ADVANCED
    },
    {}
  }
};

static mpegts_service_t *
t2mi_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return mpegts_service_create1(NULL, mm, sid, pmt_pid, NULL);
}

static const idclass_t *
t2mi_network_mux_class ( mpegts_network_t *mn )
{
  return &t2mi_mux_class;
}

static mpegts_mux_t *
t2mi_network_create_mux2 ( mpegts_network_t *mn, htsmsg_t *conf )
{
  t2mi_mux_t *tm = t2mi_mux_create0((t2mi_network_t *)mn, NULL, conf);
  return mpegts_mux_post_create((mpegts_mux_t *)tm);
}

static htsmsg_t *
t2mi_network_config_save ( mpegts_network_t *mn, char *filename, size_t fsize )
{
  htsmsg_t *c = htsmsg_create_map();
  char ubuf[UUID_HEX_SIZE];
  idnode_save(&mn->mn_id, c);
  if (filename)
    snprintf(filename, fsize, "input/t2mi/networks/%s/config",
             idnode_uuid_as_str(&mn->mn_id, ubuf));
  return c;
}

/* migrate a pre-mux-selection config (source network by UUID) to the
 * equivalent selection of all that network's muxes */
static void
t2mi_network_migrate ( t2mi_network_t *tn, htsmsg_t *conf )
{
  const char *legacy;
  mpegts_network_t *src;
  mpegts_mux_t *mm;

  if (conf == NULL || LIST_FIRST(&tn->tn_src_muxes) != NULL)
    return;
  legacy = htsmsg_get_str(conf, "src_network");
  if (legacy == NULL || (src = mpegts_network_find(legacy)) == NULL ||
      src == (mpegts_network_t *)tn)
    return;
  LIST_FOREACH(mm, &src->mn_muxes, mm_network_link)
    t2mi_network_src_mux_link(&tn->mn_id, &mm->mm_id, NULL);
  idnode_changed(&tn->mn_id);
  tvhinfo(LS_T2MI, "%s: migrated source network %s to a mux selection",
          tn->mn_network_name ?: "", legacy);
}

static t2mi_network_t *
t2mi_network_create0 ( const char *uuid, htsmsg_t *conf )
{
  t2mi_network_t *tn = calloc(1, sizeof(*tn));
  t2mi_mux_t *tm;
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  char ubuf[UUID_HEX_SIZE];

  tn->tn_priority           = 1;
  tn->tn_streaming_priority = 1;
  tn->tn_max_timeout        = 30;
  tn->tn_src_private        = 1;
  tn->tn_scan_period        = 60;
  tn->mn_autodiscovery      = MN_DISCOVERY_NEW;
  if (!mpegts_network_create0((mpegts_network_t *)tn, &t2mi_network_class,
                              uuid, NULL, conf)) {
    free(tn);
    return NULL;
  }
  tn->mn_create_service = t2mi_network_create_service;
  tn->mn_mux_class      = t2mi_network_mux_class;
  tn->mn_mux_create2    = t2mi_network_create_mux2;
  tn->mn_config_save    = t2mi_network_config_save;
  tn->mn_delete         = t2mi_network_delete;
  tn->mn_scan           = t2mi_network_scan;

  if (!conf)
    tn->mn_skipinitscan = 1;

  t2mi_network_migrate(tn, conf);

  /* Load muxes */
  if ((c = hts_settings_load_r(1, "input/t2mi/networks/%s/muxes",
                               idnode_uuid_as_str(&tn->mn_id, ubuf)))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      tm = t2mi_mux_create0(tn, htsmsg_field_name(f), e);
      mpegts_mux_post_create((mpegts_mux_t *)tm);
    }
    htsmsg_destroy(c);
  }

  t2mi_network_trigger(tn);
  return tn;
}

static mpegts_network_t *
t2mi_network_builder ( const idclass_t *idc, htsmsg_t *conf )
{
  mpegts_network_t *mn = (mpegts_network_t *)t2mi_network_create0(NULL, conf);
  if (mn && t2mi_input)
    mpegts_input_add_network((mpegts_input_t *)t2mi_input, mn);
  return mn;
}

/* **************************************************************************
 * Initialise
 * *************************************************************************/

void t2mi_init ( void )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Register class + builder */
  idclass_register(&t2mi_mux_class);
  mpegts_network_register_builder(&t2mi_network_class, t2mi_network_builder);

#if !ENABLE_TVHCSA
  tvhwarn(LS_T2MI, "built without libdvbcsa (--disable-tvhcsa): scrambled "
          "T2-MI carriers cannot be descrambled");
#endif

  /* Load settings */
  if ((c = hts_settings_load_r(1, "input/t2mi/networks"))) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      t2mi_network_create0(htsmsg_field_name(f), e);
    }
    htsmsg_destroy(c);
  }

  /* Input */
  t2mi_input = t2mi_input_create();
}

void t2mi_done ( void )
{
  tvh_mutex_lock(&global_lock);
  if (t2mi_input) {
    mpegts_input_stop_all((mpegts_input_t *)t2mi_input);
    mpegts_input_delete((mpegts_input_t *)t2mi_input, 0);
    t2mi_input = NULL;
  }
  mpegts_network_unregister_builder(&t2mi_network_class);
  mpegts_network_class_delete(&t2mi_network_class, 0);
  tvh_mutex_unlock(&global_lock);
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
