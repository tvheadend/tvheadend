/*
 *  ISI Probe - Input Stream Identifier scanning for DVB-S2 Multi-Stream
 *
 *  Automatically discovers all ISIs (Input Stream Identifiers) on a DVB-S2
 *  transponder and creates muxes for each one. Only works with drivers that
 *  support the DTV_ISI_LIST and DTV_MATYPE_LIST ioctls (e.g., Neumo stid135).
 *
 *  For each discovered ISI, the MATYPE is checked to determine if it's a
 *  GSE (Generic Stream Encapsulation) or standard TS stream:
 *    - MATYPE bits 7-6 = 11: Transport Stream (MM_TYPE_TS)
 *    - MATYPE bits 7-6 = 00/01: GSE stream (MM_TYPE_GSE)
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
#include "isi_probe.h"
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>

/* Neumo driver ioctls - not in standard kernel headers */
#ifndef DTV_ISI_LIST
#define DTV_ISI_LIST 71
#endif

#ifndef DTV_MATYPE_LIST
#define DTV_MATYPE_LIST 72
#endif

/*
 * ISI probe context - stores frontend FD for driver ioctls
 */
typedef struct isi_probe_ctx {
  int fe_fd;
} isi_probe_ctx_t;

/*
 * Read ISI list from Neumo driver using DTV_ISI_LIST ioctl
 * Returns number of ISIs found, populates isi_list array
 * isi_list must be able to hold 256 entries
 */
static int
isi_read_driver_list(int fe_fd, uint8_t *isi_list, uint8_t *matype_list)
{
  struct dtv_property prop_isi;
  struct dtv_properties cmdseq;
  int count = 0;

  if (fe_fd < 0)
    return 0;

  /* Read ISI bitfield */
  memset(&prop_isi, 0, sizeof(prop_isi));
  prop_isi.cmd = DTV_ISI_LIST;

  cmdseq.num = 1;
  cmdseq.props = &prop_isi;

  if (ioctl(fe_fd, FE_GET_PROPERTY, &cmdseq) != 0) {
    tvhtrace(LS_MPEGTS, "ISI probe: DTV_ISI_LIST ioctl failed: %s", strerror(errno));
    return 0;
  }

  /* Parse ISI bitfield - each bit indicates if that ISI is present
   * Buffer contains up to 32 bytes (256 bits) for ISI 0-255 */
  const uint8_t *bitset = prop_isi.u.buffer.data;
  size_t bitset_len = prop_isi.u.buffer.len;

  if (bitset_len == 0) {
    tvhtrace(LS_MPEGTS, "ISI probe: empty ISI bitset");
    return 0;
  }

  tvhtrace(LS_MPEGTS, "ISI probe: ISI bitset len=%zu", bitset_len);

  for (size_t byte_idx = 0; byte_idx < bitset_len && byte_idx < 32; byte_idx++) {
    uint8_t byte_val = bitset[byte_idx];
    for (int bit = 0; bit < 8; bit++) {
      if (byte_val & (1 << bit)) {
        uint8_t isi = byte_idx * 8 + bit;
        isi_list[count++] = isi;
      }
    }
  }

  if (count > 0) {
    char isi_str[512] = "";
    int pos = 0;
    for (int j = 0; j < count && pos < 500; j++) {
      pos += snprintf(isi_str + pos, sizeof(isi_str) - pos, "%d ", isi_list[j]);
    }
    tvhdebug(LS_MPEGTS, "ISI probe: found %d ISI(s) from driver: %s", count, isi_str);
  } else {
    tvhdebug(LS_MPEGTS, "ISI probe: found 0 ISI(s) from driver");
  }

  /* Now read MATYPE list if we found ISIs */
  if (count > 0 && matype_list) {
    struct dtv_property prop_matype;
    uint16_t matype_buffer[256];

    memset(&prop_matype, 0, sizeof(prop_matype));
    memset(matype_buffer, 0, sizeof(matype_buffer));
    prop_matype.cmd = DTV_MATYPE_LIST;

    /* Neumo uses dtv_matype_list struct */
    struct {
      uint32_t num_entries;
      uint16_t *matypes;
    } matype_req;
    matype_req.num_entries = 256;
    matype_req.matypes = matype_buffer;

    memcpy(&prop_matype.u, &matype_req, sizeof(matype_req));

    cmdseq.props = &prop_matype;

    if (ioctl(fe_fd, FE_GET_PROPERTY, &cmdseq) == 0) {
      /* Parse MATYPE entries: format is (matype << 8) | isi */
      for (int i = 0; i < 256 && matype_buffer[i] != 0; i++) {
        uint16_t entry = matype_buffer[i];
        uint8_t isi = entry & 0xFF;
        uint8_t matype = (entry >> 8) & 0xFF;
        matype_list[isi] = matype;
        tvhtrace(LS_MPEGTS, "ISI probe: ISI %d MATYPE 0x%02X", isi, matype);
      }
    }
  }

  return count;
}

/*
 * Check if MATYPE indicates GSE mode
 * MATYPE bits 7-6: 00=GP continuous, 01=GS packetized, 10=reserved, 11=TS
 */
static int
isi_is_gse_matype(uint8_t matype)
{
  int ts_gs = (matype >> 6) & 0x3;
  return (ts_gs == 0 || ts_gs == 1);  /* GSE if not TS mode */
}

/*
 * Find mux by exact match of frequency, polarisation, and stream_id
 * Returns NULL if not found
 */
static dvb_mux_t *
isi_find_mux_exact(dvb_network_t *ln, uint32_t freq, int pol, int stream_id)
{
  mpegts_mux_t *mm;
  dvb_mux_t *dm;

  LIST_FOREACH(mm, &ln->mn_muxes, mm_network_link) {
    dm = (dvb_mux_t *)mm;
    if (dm->lm_tuning.dmc_fe_freq == freq &&
        dm->lm_tuning.u.dmc_fe_qpsk.polarisation == pol &&
        dm->lm_tuning.dmc_fe_stream_id == stream_id) {
      return dm;
    }
  }
  return NULL;
}

/*
 * Create child muxes for discovered ISIs
 */
static int
isi_probe_create_muxes(mpegts_mux_t *mm, const uint8_t *isi_list, int isi_count,
                       const uint8_t *matype_list)
{
  dvb_network_t *ln;
  const dvb_mux_t *outer_dm;
  dvb_mux_t *child_mux;
  dvb_mux_conf_t dmc;
  int i;
  int created = 0;
  uint32_t freq;
  int pol;

  ln = (dvb_network_t *)mm->mm_network;
  outer_dm = (dvb_mux_t *)mm;
  freq = outer_dm->lm_tuning.dmc_fe_freq;
  pol = outer_dm->lm_tuning.u.dmc_fe_qpsk.polarisation;

  tvhdebug(LS_MPEGTS, "ISI probe: outer mux stream_id=%d, freq=%u, pol=%d",
           outer_dm->lm_tuning.dmc_fe_stream_id, freq, pol);

  for (i = 0; i < isi_count; i++) {
    uint8_t isi = isi_list[i];
    uint8_t matype = matype_list[isi];
    int is_gse = isi_is_gse_matype(matype);

    tvhdebug(LS_MPEGTS, "ISI probe: evaluating ISI %d, MATYPE=0x%02X, is_gse=%d",
             isi, matype, is_gse);

    /* Check if mux with this ISI already exists (exact match) */
    child_mux = isi_find_mux_exact(ln, freq, pol, isi);
    if (child_mux) {
      tvhdebug(LS_MPEGTS, "ISI probe: mux for ISI %d already exists (freq=%u, pol=%d)",
               isi, freq, pol);
      child_mux->mm_scan_last_seen = gclk();
      idnode_changed(&child_mux->mm_id);
      continue;
    }

    tvhdebug(LS_MPEGTS, "ISI probe: no existing mux found for ISI %d, creating", isi);

    /* Copy outer mux tuning, set ISI */
    dmc = outer_dm->lm_tuning;
    dmc.dmc_fe_stream_id = isi;

    /* Create new mux for this ISI */
    child_mux = dvb_mux_create0(ln, MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, &dmc, NULL, NULL);
    if (!child_mux)
      continue;

    /* Set type based on MATYPE - GSE or regular TS
     * Note: GSE type may upgrade to DAB-GSE if DAB content is detected later */
    if (is_gse) {
      child_mux->mm_type = MM_TYPE_GSE;
    }
    /* else keep default MM_TYPE_TS */

    tvhinfo(LS_MPEGTS, "ISI probe: created %s mux for ISI %d (MATYPE=0x%02X)",
            is_gse ? "GSE" : "TS", isi, matype);

    child_mux->mm_scan_first = gclk();
    child_mux->mm_scan_last_seen = child_mux->mm_scan_first;
    idnode_changed(&child_mux->mm_id);

    /* Queue for scanning */
    mpegts_network_scan_queue_add((mpegts_mux_t *)child_mux,
                                   SUBSCRIPTION_PRIO_SCAN_INIT,
                                   SUBSCRIPTION_INITSCAN, 10);
    created++;
  }

  return created;
}

/*
 * Start ISI probe on a mux
 * Saves the frontend FD for later use in mpegts_isi_probe_complete()
 */
void
mpegts_isi_probe_start(mpegts_mux_t *mm)
{
  isi_probe_ctx_t *ctx;
  const dvb_mux_t *dm;
  const linuxdvb_frontend_t *lfe;
  mpegts_mux_instance_t *mmi;
  const idclass_t *ic;

  /* Already probing? */
  if (mm->mm_isi_probe_ctx)
    return;

  /* Get frontend from active mux instance */
  mmi = LIST_FIRST(&mm->mm_instances);
  if (!mmi || !mmi->mmi_input)
    return;

  /* Check if this is a linuxdvb frontend by walking the class hierarchy */
  lfe = (linuxdvb_frontend_t *)mmi->mmi_input;
  ic = lfe->ti_id.in_class;
  while (ic) {
    if (ic->ic_class && strstr(ic->ic_class, "linuxdvb_frontend"))
      break;
    ic = ic->ic_super;
  }
  if (!ic)
    return;

  /* Check Neumo driver support - return immediately if not available */
  if (!lfe->lfe_neumo_supported)
    return;

  /* Only for DVB-S2 */
  dm = (dvb_mux_t *)mm;
  if (dm->lm_tuning.dmc_fe_delsys != DVB_SYS_DVBS2)
    return;

  /* Allocate context with frontend FD */
  ctx = calloc(1, sizeof(*ctx));
  if (!ctx)
    return;

  ctx->fe_fd = lfe->lfe_fe_fd;
  mm->mm_isi_probe_ctx = ctx;

  tvhdebug(LS_MPEGTS, "mux %s: ISI probe started (fe_fd=%d)", mm->mm_nicename, ctx->fe_fd);
}

/*
 * Complete ISI probe - read ISI list from driver and create muxes
 */
int
mpegts_isi_probe_complete(mpegts_mux_t *mm)
{
  isi_probe_ctx_t *ctx = mm->mm_isi_probe_ctx;
  uint8_t isi_list[256];
  uint8_t matype_list[256];
  int isi_count = 0;
  int found = 0;

  if (!ctx)
    return 0;

  mm->mm_isi_probe_ctx = NULL;

  /* Read ISI list from driver using the frontend FD saved at start */
  memset(isi_list, 0, sizeof(isi_list));
  memset(matype_list, 0, sizeof(matype_list));

  if (ctx->fe_fd >= 0) {
    isi_count = isi_read_driver_list(ctx->fe_fd, isi_list, matype_list);
  }

  /* Create muxes for GSE streams */
  if (isi_count > 0) {
    found = isi_probe_create_muxes(mm, isi_list, isi_count, matype_list);
  }

  free(ctx);

  tvhinfo(LS_MPEGTS, "mux %s: ISI probe complete, created %d mux(es) from %d ISI(s)",
          mm->mm_nicename, found, isi_count);

  return found;
}

/*
 * Check if ISI probe is done
 */
int
mpegts_isi_probe_is_done(mpegts_mux_t *mm __attribute__((unused)))
{
  /* ISI probe is instant (ioctl-based), always done */
  return 1;
}
