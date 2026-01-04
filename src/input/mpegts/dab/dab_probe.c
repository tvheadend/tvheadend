/*
 *  DAB Probe - Detection of DAB over DVB (EN 301 192)
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
#include <dvbdab/dvbdab_c.h>

/*
 * DAB probe context - simple callback-based scanner
 */
typedef struct dab_probe_ctx {
  mpegts_mux_t      *mm;
  dvbdab_scanner_t  *scanner;
} dab_probe_ctx_t;

/* Scanner timeout in milliseconds */
#define DAB_PROBE_TIMEOUT_MS  5000

/*
 * Secondary scan callback - receives all TS packets
 * Called from mi-table thread context during scan
 */
static void
dab_probe_packet_cb(void *ctx, const uint8_t *pkt, int len)
{
  dab_probe_ctx_t *dctx = ctx;

  if (!dctx || !dctx->scanner)
    return;

  /* Feed data to libdvbdab scanner */
  dvbdab_scanner_feed(dctx->scanner, pkt, len);
}

/*
 * Format IP address to string
 */
static void
format_ip_str(char *buf, size_t len, uint32_t ip)
{
  snprintf(buf, len, "%d.%d.%d.%d",
           (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF, ip & 0xFF);
}

/*
 * Initialize new DAB mux common fields and queue for scanning
 */
static void
init_new_dab_mux(dvb_mux_t *dab_mux, const char *label)
{
  if (label && label[0]) {
    free(dab_mux->mm_provider_network_name);
    dab_mux->mm_provider_network_name = strdup(label);
  }

  dab_mux->mm_scan_first = gclk();
  dab_mux->mm_scan_last_seen = dab_mux->mm_scan_first;
  idnode_changed(&dab_mux->mm_id);

  mpegts_network_scan_queue_add((mpegts_mux_t *)dab_mux,
                                 SUBSCRIPTION_PRIO_SCAN_INIT,
                                 SUBSCRIPTION_INITSCAN, 10);
}

/*
 * Update existing mux timestamp
 */
static void
update_existing_mux(dvb_mux_t *dab_mux)
{
  dab_mux->mm_scan_last_seen = gclk();
  idnode_changed(&dab_mux->mm_id);
}

/*
 * Process a single DAB ensemble from scanner results
 */
static int
process_ensemble(mpegts_mux_t *mm, dvb_network_t *ln,
                 const dvb_mux_t *outer_dm, const dvbdab_ensemble_t *ens)
{
  dvb_mux_conf_t dmc;
  dvb_mux_t *dab_mux;
  const char *type_str;
  char location[64];
  char ip_str[20];
  int is_etina = ens->is_etina;
  int is_tsni = ens->is_tsni;

  /* Determine type and location string */
  if (is_etina) {
    type_str = "ETI-NA";
    snprintf(location, sizeof(location), "ETI-NA PID %d", ens->source_pid);
  } else if (is_tsni) {
    type_str = "DAB-TSNI";
    snprintf(location, sizeof(location), "TSNI PID %d", ens->source_pid);
  } else {
    type_str = "DAB-MPE";
    format_ip_str(ip_str, sizeof(ip_str), ens->source_ip);
    snprintf(location, sizeof(location), "%s:%d", ip_str, ens->source_port);
  }

  tvhinfo(LS_MPEGTS, "mux %s: %s ensemble EID=0x%04X \"%s\" at %s with %d service(s)",
          mm->mm_nicename, type_str, ens->eid, ens->label, location, ens->service_count);

  /* Copy outer mux tuning parameters and set type-specific fields */
  dmc = outer_dm->lm_tuning;
  dmc.dmc_fe_pid = ens->source_pid;
  dmc.dmc_dab_eti_padding = 0;
  dmc.dmc_dab_eti_bit_offset = 0;
  dmc.dmc_dab_eti_inverted = 0;
  dmc.dmc_dab_ip = 0;
  dmc.dmc_dab_port = 0;

  if (is_etina) {
    dmc.dmc_dab_eti_padding = ens->etina_padding;
    dmc.dmc_dab_eti_bit_offset = ens->etina_bit_offset;
    dmc.dmc_dab_eti_inverted = ens->etina_inverted;
    dab_mux = dvb_network_find_mux_dab_eti(ln, &dmc);
  } else if (is_tsni) {
    dab_mux = dvb_network_find_mux_dab_tsni(ln, &dmc);
  } else {
    dmc.dmc_dab_ip = ens->source_ip;
    dmc.dmc_dab_port = ens->source_port;
    dab_mux = dvb_network_find_mux_dab_mpe(ln, &dmc);
  }

  /* Existing mux found - just update timestamp */
  if (dab_mux) {
    update_existing_mux(dab_mux);
    return 1;
  }

  /* Create new mux */
  dab_mux = dvb_mux_create0(ln, MPEGTS_ONID_NONE, ens->eid, &dmc, NULL, NULL);
  if (!dab_mux)
    return 0;

  if (is_etina)
    dab_mux->mm_type = MM_TYPE_DAB_ETI;
  else if (is_tsni)
    dab_mux->mm_type = MM_TYPE_DAB_TSNI;
  else
    dab_mux->mm_type = MM_TYPE_DAB_MPE;

  tvhinfo(LS_MPEGTS, "mux %s: created %s child mux \"%s\" (EID=0x%04X, PID=0x%04X)",
          mm->mm_nicename, type_str, ens->label, ens->eid, dmc.dmc_fe_pid);

  init_new_dab_mux(dab_mux, ens->label);
  return 1;
}

/*
 * Process a single ETI-NA stream from scanner results
 */
static int
process_etina_stream(mpegts_mux_t *mm, dvb_network_t *ln,
                     const dvb_mux_t *outer_dm, const dvbdab_etina_info_t *etina)
{
  dvb_mux_conf_t dmc;
  dvb_mux_t *dab_mux;

  /* Setup DMC for ETI-NA */
  dmc = outer_dm->lm_tuning;
  dmc.dmc_fe_pid = etina->pid;
  dmc.dmc_dab_eti_padding = etina->padding_bytes;
  dmc.dmc_dab_eti_bit_offset = etina->sync_bit_offset;
  dmc.dmc_dab_eti_inverted = etina->inverted ? 1 : 0;
  dmc.dmc_dab_ip = 0;
  dmc.dmc_dab_port = 0;

  dab_mux = dvb_network_find_mux_dab_eti(ln, &dmc);

  /* Existing mux found - just update timestamp */
  if (dab_mux) {
    update_existing_mux(dab_mux);
    return 1;
  }

  /* Create new mux */
  dab_mux = dvb_mux_create0(ln, MPEGTS_ONID_NONE, etina->pid, &dmc, NULL, NULL);
  if (!dab_mux)
    return 0;

  dab_mux->mm_type = MM_TYPE_DAB_ETI;
  free(dab_mux->mm_provider_network_name);
  dab_mux->mm_provider_network_name = strdup("ETI-NA");

  tvhinfo(LS_MPEGTS, "mux %s: created ETI-NA child mux on PID %d",
          mm->mm_nicename, etina->pid);

  dab_mux->mm_scan_first = gclk();
  dab_mux->mm_scan_last_seen = dab_mux->mm_scan_first;
  idnode_changed(&dab_mux->mm_id);
  dab_mux->mm_scan_result = MM_SCAN_OK;
  dab_mux->mm_scan_state = MM_SCAN_STATE_IDLE;

  return 1;
}

/*
 * Process scanner results - create child muxes for discovered ensembles
 */
static int
dab_probe_process_results(mpegts_mux_t *mm, dvbdab_results_t *results)
{
  int i;
  dvb_network_t *ln;
  const dvb_mux_t *outer_dm;
  int found_dab = 0;

  if (!results)
    return 0;

  ln = (dvb_network_t *)mm->mm_network;
  outer_dm = (dvb_mux_t *)mm;

  tvhdebug(LS_MPEGTS, "mux %s: DAB probe found %d ensemble(s), %d ETI-NA stream(s)",
           mm->mm_nicename, results->ensemble_count, results->etina_count);

  /* Process DAB ensembles (MPE, ETI-NA, or TSNI) */
  for (i = 0; i < results->ensemble_count; i++) {
    if (process_ensemble(mm, ln, outer_dm, &results->ensembles[i]))
      found_dab = 1;
  }

  /* Process ETI-NA streams */
  for (i = 0; i < results->etina_count; i++) {
    if (process_etina_stream(mm, ln, outer_dm, &results->etina_streams[i]))
      found_dab = 1;
  }

  return found_dab;
}

/*
 * Start DAB probe - called when mux scan starts
 */
void
mpegts_dab_probe_start(mpegts_mux_t *mm)
{
  dab_probe_ctx_t *ctx;

  /* Skip for GSE muxes - they use GSE-DAB probe instead */
  if (mm->mm_type == MM_TYPE_GSE)
    return;

  /* Already probing? */
  if (mm->mm_dab_probe_ctx)
    return;

  /* Allocate context */
  ctx = calloc(1, sizeof(*ctx));
  if (!ctx)
    return;

  /* Create scanner */
  ctx->scanner = dvbdab_scanner_create();
  if (!ctx->scanner) {
    free(ctx);
    return;
  }
  dvbdab_scanner_set_timeout(ctx->scanner, DAB_PROBE_TIMEOUT_MS);

  ctx->mm = mm;

  /* Store in dedicated field */
  mm->mm_dab_probe_ctx = ctx;

  /* Install secondary callback */
  mm->mm_secondary_ctx = ctx;
  mm->mm_secondary_cb = dab_probe_packet_cb;

  tvhdebug(LS_MPEGTS, "mux %s: DAB probe started", mm->mm_nicename);
}

/*
 * Complete DAB probe - called when mux scan ends
 */
int
mpegts_dab_probe_complete(mpegts_mux_t *mm)
{
  dab_probe_ctx_t *ctx = mm->mm_dab_probe_ctx;
  dvbdab_results_t *results;
  int found = 0;

  if (!ctx)
    return 0;

  /* Remove callback only if still ours */
  if (mm->mm_secondary_ctx == ctx) {
    mm->mm_secondary_cb = NULL;
    mm->mm_secondary_ctx = NULL;
  }
  mm->mm_dab_probe_ctx = NULL;

  /* Get and process results */
  if (ctx->scanner) {
    results = dvbdab_scanner_get_results(ctx->scanner);
    if (results) {
      found = dab_probe_process_results(mm, results);
      dvbdab_results_free(results);
    }
    dvbdab_scanner_destroy(ctx->scanner);
  }

  free(ctx);

  tvhinfo(LS_MPEGTS, "mux %s: DAB probe complete, found=%d", mm->mm_nicename, found);
  return found;
}

/*
 * Check if DAB probe is done (scanner timed out or found everything)
 */
int
mpegts_dab_probe_is_done(mpegts_mux_t *mm)
{
  dab_probe_ctx_t *ctx = mm->mm_dab_probe_ctx;
  if (!ctx || !ctx->scanner)
    return 1;
  return dvbdab_scanner_is_done(ctx->scanner);
}
