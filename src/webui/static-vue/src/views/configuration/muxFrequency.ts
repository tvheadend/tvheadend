// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Frequency cell text for the Muxes grid.
 *
 * The mux subclasses store `frequency` in different units: Hz for
 * terrestrial, cable, ATSC, DTMB and DAB muxes, kHz for DVB-S and
 * ISDB-S (`src/input/mpegts/mpegts_mux_dvb.c`, the "Frequency (Hz)"
 * and "Frequency (kHz)" captions). The grid loads the abstract
 * `mpegts_mux` class, which has no frequency prop, so the unit has
 * to come from the row itself.
 *
 * The raw value is kept (only the unit is added) so the cell matches
 * the number in the Edit drawer and the numeric column filter.
 *
 * DVB-S rows are recognised by their `polarisation` field, which only
 * the DVB-S class has, or by the delivery system.
 */
import type { BaseRow } from '@/types/grid'

const KHZ_DELSYS = new Set(['DVB-S', 'DVB-S2', 'ISDB-S'])

export function muxFrequencyUnit(row: BaseRow): 'Hz' | 'kHz' {
  if (row.polarisation !== undefined) return 'kHz'
  return typeof row.delsys === 'string' && KHZ_DELSYS.has(row.delsys) ? 'kHz' : 'Hz'
}

export function formatMuxFrequency(value: unknown, row: BaseRow): string {
  if (typeof value !== 'number' || !Number.isFinite(value)) return ''
  return `${value} ${muxFrequencyUnit(row)}`
}
