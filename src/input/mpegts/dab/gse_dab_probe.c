/*
 *  GSE-DAB Probe - Detection of DAB within GSE streams
 *
 *  Detects DAB ensembles within GSE (Generic Stream Encapsulation) streams
 *  and upgrades MM_TYPE_GSE muxes to MM_TYPE_DAB_GSE.
 *
 *  Uses DMX_SET_FE_STREAM to access BBFrame data and libdvbdab streamer
 *  with DVBDAB_FORMAT_BBF_TS in discovery mode (filter_ip=0).
 *
 *  Copyright (C) 2025
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include "tvheadend.h"
#include "input.h"
#include "input/mpegts/mpegts_dvb.h"
#include "input/mpegts/linuxdvb/linuxdvb_private.h"
#include <dvbdab/dvbdab_c.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>

#ifndef DMX_SET_FE_STREAM
#define DMX_SET_FE_STREAM _IO('o', 55)
#endif

/*
 * GSE-DAB probe context - uses DMX_SET_FE_STREAM for BBFrame access
 */
typedef struct gse_dab_probe_ctx {
  mpegts_mux_t          *mm;
  dvbdab_streamer_t     *streamer;
  int                    dmx_fd;       /* Demux FD with DMX_SET_FE_STREAM */
  int64_t                start_time;
  pthread_t              thread;
  th_pipe_t              pipe;
  int                    running;
} gse_dab_probe_ctx_t;

/* Discovery timeout in milliseconds */
#define GSE_DAB_PROBE_TIMEOUT_MS  8000

/*
 * Process streamer results - upgrade mux type and create child muxes
 */
static int
gse_dab_probe_process_results(mpegts_mux_t *mm, dvbdab_streamer_t *streamer)
{
  dvbdab_ensemble_t *ensembles;
  int count = 0;
  int i;
  dvb_network_t *ln;
  dvb_mux_t *outer_dm;
  dvb_mux_conf_t dmc;
  dvb_mux_t *dab_mux;
  char ip_str[20];
  int found = 0;
  int created = 0;
  int is_already_dab_gse = (mm->mm_type == MM_TYPE_DAB_GSE);

  ensembles = dvbdab_streamer_get_all_ensembles(streamer, &count);
  if (!ensembles || count == 0) {
    tvhdebug(LS_MPEGTS, "mux %s: GSE-DAB probe found no ensembles", mm->mm_nicename);
    return 0;
  }

  ln = (dvb_network_t *)mm->mm_network;
  outer_dm = (dvb_mux_t *)mm;

  tvhinfo(LS_MPEGTS, "mux %s: GSE-DAB probe found %d ensemble(s)", mm->mm_nicename, count);

  for (i = 0; i < count; i++) {
    dvbdab_ensemble_t *ens = &ensembles[i];

    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d",
             (ens->source_ip >> 24) & 0xFF,
             (ens->source_ip >> 16) & 0xFF,
             (ens->source_ip >> 8) & 0xFF,
             ens->source_ip & 0xFF);

    tvhinfo(LS_MPEGTS, "mux %s: GSE-DAB ensemble EID=0x%04X \"%s\" at %s:%d with %d service(s)",
            mm->mm_nicename, ens->eid, ens->label, ip_str, ens->source_port, ens->service_count);

    /* Copy outer mux tuning parameters */
    dmc = outer_dm->lm_tuning;
    dmc.dmc_dab_ip = ens->source_ip;
    dmc.dmc_dab_port = ens->source_port;

    /* Check for existing DAB-GSE mux with same parameters */
    dab_mux = dvb_network_find_mux_dab_gse(ln, &dmc);
    if (dab_mux) {
      tvhdebug(LS_MPEGTS, "mux %s: DAB-GSE mux already exists for %s:%d",
               mm->mm_nicename, ip_str, ens->source_port);
      dab_mux->mm_scan_last_seen = gclk();
      idnode_changed(&dab_mux->mm_id);
      found++;
      continue;
    }

    /* If this is already a DAB-GSE mux, don't upgrade - just create new muxes */
    if (!is_already_dab_gse && created == 0) {
      /* First ensemble on GSE mux: upgrade current mux */
      outer_dm->lm_tuning.dmc_dab_ip = ens->source_ip;
      outer_dm->lm_tuning.dmc_dab_port = ens->source_port;
      mm->mm_type = MM_TYPE_DAB_GSE;
      mm->mm_tsid = ens->eid;  /* Set TSID to ensemble ID */

      if (ens->label[0]) {
        free(mm->mm_provider_network_name);
        mm->mm_provider_network_name = strdup(ens->label);
      }

      tvhinfo(LS_MPEGTS, "mux %s: upgraded to DAB-GSE (EID=0x%04X, %s:%d)",
              mm->mm_nicename, ens->eid, ip_str, ens->source_port);

      mm->mm_scan_last_seen = gclk();
      idnode_changed(&mm->mm_id);
      found++;
      created++;
    } else {
      /* Create new child mux */
      dab_mux = dvb_mux_create0(ln, MPEGTS_ONID_NONE, ens->eid, &dmc, NULL, NULL);
      if (!dab_mux)
        continue;

      dab_mux->mm_type = MM_TYPE_DAB_GSE;

      if (ens->label[0]) {
        free(dab_mux->mm_provider_network_name);
        dab_mux->mm_provider_network_name = strdup(ens->label);
      }

      tvhinfo(LS_MPEGTS, "mux %s: created DAB-GSE child mux \"%s\" (EID=0x%04X, %s:%d)",
              mm->mm_nicename, ens->label, ens->eid, ip_str, ens->source_port);

      dab_mux->mm_scan_first = gclk();
      dab_mux->mm_scan_last_seen = dab_mux->mm_scan_first;
      idnode_changed(&dab_mux->mm_id);

      /* Queue for scanning */
      mpegts_network_scan_queue_add((mpegts_mux_t *)dab_mux,
                                     SUBSCRIPTION_PRIO_SCAN_INIT,
                                     SUBSCRIPTION_INITSCAN, 10);
      found++;
      created++;
    }
  }

  dvbdab_streamer_free_all_ensembles(ensembles, count);

  return found;
}

/*
 * GSE-DAB probe thread - reads BBFrame data via DMX_SET_FE_STREAM
 */
static void *
gse_dab_probe_thread(void *aux)
{
  gse_dab_probe_ctx_t *ctx = aux;
  struct pollfd pfd[2];
  uint8_t buf[32768];
  ssize_t n;
  int nfds;
  int64_t elapsed;

  tvhdebug(LS_MPEGTS, "mux %s: GSE-DAB probe thread started", ctx->mm->mm_nicename);

  memset(pfd, 0, sizeof(pfd));
  pfd[0].fd = ctx->dmx_fd;
  pfd[0].events = POLLIN;
  pfd[1].fd = ctx->pipe.rd;
  pfd[1].events = POLLIN;

  while (ctx->running && tvheadend_is_running()) {
    /* Check timeout */
    elapsed = gclk() - ctx->start_time;
    if (elapsed >= sec2mono(GSE_DAB_PROBE_TIMEOUT_MS / 1000)) {
      tvhdebug(LS_MPEGTS, "mux %s: GSE-DAB probe timeout", ctx->mm->mm_nicename);
      break;
    }

    /* Check if we have enough results */
    if (ctx->streamer && dvbdab_streamer_is_basic_ready(ctx->streamer)) {
      tvhdebug(LS_MPEGTS, "mux %s: GSE-DAB probe got basic results", ctx->mm->mm_nicename);
      break;
    }

    nfds = poll(pfd, 2, 200);
    if (nfds < 0) {
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    if (nfds == 0)
      continue;

    /* Check control pipe */
    if (pfd[1].revents & POLLIN)
      break;

    /* Read BBFrame data from demux */
    if (pfd[0].revents & POLLIN) {
      n = read(ctx->dmx_fd, buf, sizeof(buf));
      if (n <= 0)
        continue;

      /* Feed to libdvbdab streamer */
      if (ctx->streamer)
        dvbdab_streamer_feed(ctx->streamer, buf, n);
    }
  }

  tvhdebug(LS_MPEGTS, "mux %s: GSE-DAB probe thread stopped", ctx->mm->mm_nicename);
  return NULL;
}

/*
 * Start GSE-DAB probe - called when GSE mux scan starts
 * Also runs on DAB-GSE muxes to discover new/missing ensembles
 */
void
mpegts_gse_dab_probe_start(mpegts_mux_t *mm)
{
  gse_dab_probe_ctx_t *ctx;
  dvbdab_streamer_config_t config;
  linuxdvb_frontend_t *lfe;
  struct dmx_pes_filter_params pes_filter;
  int dmx_fd;

  /* Run for MM_TYPE_GSE and MM_TYPE_DAB_GSE muxes */
  if (mm->mm_type != MM_TYPE_GSE && mm->mm_type != MM_TYPE_DAB_GSE)
    return;

  /* Already probing? */
  if (mm->mm_gse_dab_probe_ctx)
    return;

  /* Get frontend for demux path */
  mpegts_mux_instance_t *mmi = LIST_FIRST(&mm->mm_instances);
  if (!mmi || !mmi->mmi_input)
    return;
  lfe = (linuxdvb_frontend_t *)mmi->mmi_input;

  /* Open demux for DMX_SET_FE_STREAM */
  dmx_fd = tvh_open(lfe->lfe_dmx_path, O_RDONLY | O_NONBLOCK, 0);
  if (dmx_fd < 0) {
    tvherror(LS_MPEGTS, "mux %s: GSE-DAB probe failed to open demux: %s",
             mm->mm_nicename, strerror(errno));
    return;
  }

  /* Set buffer size */
  ioctl(dmx_fd, DMX_SET_BUFFER_SIZE, 4 * 1024 * 1024);

  /* Enable frontend stream mode for BBFrame access */
  if (ioctl(dmx_fd, DMX_SET_FE_STREAM) < 0) {
    tvherror(LS_MPEGTS, "mux %s: GSE-DAB probe DMX_SET_FE_STREAM failed: %s",
             mm->mm_nicename, strerror(errno));
    close(dmx_fd);
    return;
  }

  /* Set PES filter */
  memset(&pes_filter, 0, sizeof(pes_filter));
  pes_filter.pid = 0x2000;
  pes_filter.input = DMX_IN_FRONTEND;
  pes_filter.output = DMX_OUT_TSDEMUX_TAP;
  pes_filter.pes_type = DMX_PES_OTHER;

  if (ioctl(dmx_fd, DMX_SET_PES_FILTER, &pes_filter) < 0) {
    tvherror(LS_MPEGTS, "mux %s: GSE-DAB probe DMX_SET_PES_FILTER failed: %s",
             mm->mm_nicename, strerror(errno));
    close(dmx_fd);
    return;
  }

  /* Start demux */
  if (ioctl(dmx_fd, DMX_START) < 0) {
    tvherror(LS_MPEGTS, "mux %s: GSE-DAB probe DMX_START failed: %s",
             mm->mm_nicename, strerror(errno));
    close(dmx_fd);
    return;
  }

  /* Allocate context */
  ctx = calloc(1, sizeof(*ctx));
  if (!ctx) {
    close(dmx_fd);
    return;
  }

  /* Create streamer in discovery mode (filter_ip=0) with BBF_TS format */
  memset(&config, 0, sizeof(config));
  config.format = DVBDAB_FORMAT_BBF_TS;  /* BBFrame-in-PseudoTS from DMX_SET_FE_STREAM */
  config.pid = 0;
  config.filter_ip = 0;    /* Discovery mode: find all ensembles */
  config.filter_port = 0;

  ctx->streamer = dvbdab_streamer_create(&config);
  if (!ctx->streamer) {
    tvherror(LS_MPEGTS, "mux %s: GSE-DAB probe failed to create streamer",
             mm->mm_nicename);
    close(dmx_fd);
    free(ctx);
    return;
  }

  ctx->mm = mm;
  ctx->dmx_fd = dmx_fd;
  ctx->start_time = gclk();
  ctx->running = 1;

  /* Create control pipe */
  if (tvh_pipe(O_NONBLOCK, &ctx->pipe) < 0) {
    dvbdab_streamer_destroy(ctx->streamer);
    close(dmx_fd);
    free(ctx);
    return;
  }

  /* Start probe thread */
  if (tvh_thread_create(&ctx->thread, NULL, gse_dab_probe_thread, ctx, "gse-dab-probe") != 0) {
    tvh_pipe_close(&ctx->pipe);
    dvbdab_streamer_destroy(ctx->streamer);
    close(dmx_fd);
    free(ctx);
    return;
  }

  mm->mm_gse_dab_probe_ctx = ctx;

  tvhinfo(LS_MPEGTS, "mux %s: GSE-DAB probe started (DMX_SET_FE_STREAM)", mm->mm_nicename);
}

/*
 * Complete GSE-DAB probe - called when mux scan ends
 */
int
mpegts_gse_dab_probe_complete(mpegts_mux_t *mm)
{
  gse_dab_probe_ctx_t *ctx = mm->mm_gse_dab_probe_ctx;
  int found = 0;

  if (!ctx)
    return 0;

  mm->mm_gse_dab_probe_ctx = NULL;

  /* Stop probe thread */
  if (ctx->running) {
    ctx->running = 0;
    tvh_write(ctx->pipe.wr, "q", 1);
    pthread_join(ctx->thread, NULL);
    tvh_pipe_close(&ctx->pipe);
  }

  /* Process results */
  if (ctx->streamer) {
    found = gse_dab_probe_process_results(mm, ctx->streamer);
    dvbdab_streamer_destroy(ctx->streamer);
  }

  /* Close demux */
  if (ctx->dmx_fd >= 0) {
    ioctl(ctx->dmx_fd, DMX_STOP);
    close(ctx->dmx_fd);
  }

  free(ctx);

  tvhinfo(LS_MPEGTS, "mux %s: GSE-DAB probe complete, found %d ensemble(s)",
          mm->mm_nicename, found);

  return found;
}

/*
 * Check if GSE-DAB probe is done
 */
int
mpegts_gse_dab_probe_is_done(mpegts_mux_t *mm)
{
  gse_dab_probe_ctx_t *ctx = mm->mm_gse_dab_probe_ctx;
  int64_t elapsed;

  if (!ctx)
    return 1;

  /* Thread finished? */
  if (!ctx->running)
    return 1;

  /* Check if streamer has basic results */
  if (ctx->streamer && dvbdab_streamer_is_basic_ready(ctx->streamer))
    return 1;

  /* Check timeout */
  elapsed = gclk() - ctx->start_time;
  if (elapsed >= sec2mono(GSE_DAB_PROBE_TIMEOUT_MS / 1000))
    return 1;

  return 0;
}
