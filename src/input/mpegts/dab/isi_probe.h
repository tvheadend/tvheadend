/*
 *  ISI Probe - Input Stream Identifier scanning for DVB-S2 Multi-Stream
 *
 *  Automatically discovers ISIs on DVB-S2 transponders using Neumo driver
 *  ioctls (DTV_ISI_LIST, DTV_MATYPE_LIST) and creates muxes for each one.
 *
 *  Copyright (C) 2025
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#ifndef __ISI_PROBE_H__
#define __ISI_PROBE_H__

/* Forward declarations */
struct mpegts_mux;

/**
 * Start ISI probe on a mux
 *
 * Returns immediately (no-op) if:
 * - Neumo driver support is not available
 * - Mux is not DVB-S2
 * - ISI probe is already running
 *
 * @param mm  Mux to probe
 */
void mpegts_isi_probe_start(struct mpegts_mux *mm);

/**
 * Complete ISI probe and process results
 *
 * Reads ISI list from driver and creates child muxes for each discovered ISI.
 * Sets mm_type to MM_TYPE_GSE for GSE streams (based on MATYPE).
 *
 * @param mm  Mux being probed
 * @return    Number of child muxes created
 */
int mpegts_isi_probe_complete(struct mpegts_mux *mm);

/**
 * Check if ISI probe is done
 *
 * @param mm  Mux being probed
 * @return    1 if done (or not running), 0 if still in progress
 */
int mpegts_isi_probe_is_done(struct mpegts_mux *mm);

#endif /* __ISI_PROBE_H__ */
