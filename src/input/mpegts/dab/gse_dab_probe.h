/*
 *  GSE-DAB Probe - Detection of DAB within GSE streams
 *
 *  Automatically detects DAB ensembles in GSE (Generic Stream Encapsulation)
 *  streams and upgrades MM_TYPE_GSE muxes to MM_TYPE_DAB_GSE.
 *
 *  Runs for MM_TYPE_GSE muxes (created by ISI probe) and MM_TYPE_DAB_GSE
 *  muxes (to discover new/missing ensembles on rescan).
 *
 *  Uses DMX_SET_FE_STREAM to access BBFrame data directly.
 *
 *  Copyright (C) 2025
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#ifndef __GSE_DAB_PROBE_H__
#define __GSE_DAB_PROBE_H__

/* Forward declarations */
struct mpegts_mux;

/**
 * Start GSE-DAB probe on a mux
 *
 * Returns immediately (no-op) if:
 * - Mux is not MM_TYPE_GSE or MM_TYPE_DAB_GSE
 * - GSE-DAB probe is already running
 *
 * @param mm  Mux to probe
 */
void mpegts_gse_dab_probe_start(struct mpegts_mux *mm);

/**
 * Complete GSE-DAB probe and process results
 *
 * On DAB detection:
 * - For MM_TYPE_GSE: Upgrades mux type to MM_TYPE_DAB_GSE
 * - Sets dmc_dab_ip, dmc_dab_port, and mm_tsid (ensemble ID)
 * - Creates child muxes for additional ensembles
 *
 * @param mm  Mux being probed
 * @return    Number of DAB ensembles found
 */
int mpegts_gse_dab_probe_complete(struct mpegts_mux *mm);

/**
 * Check if GSE-DAB probe is done
 *
 * @param mm  Mux being probed
 * @return    1 if done (or not running), 0 if still in progress
 */
int mpegts_gse_dab_probe_is_done(struct mpegts_mux *mm);

#endif /* __GSE_DAB_PROBE_H__ */
