// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * formatBitrate — display helpers for the Status bandwidth chart.
 *
 * The chart consumes two server-side data shapes:
 *   - Status → Streams: `bps` field, already in BITS per second
 *     (`mpegts_input.c:1922` multiplies the byte counter by 8 before
 *     storing).
 *   - Status → Subscriptions: `in` / `out` fields in BYTES per
 *     second (`subscriptions.c:1094-1097`; computed as the 1-second
 *     byte delta).
 *
 * Two coercions land here so the chart consumer can stay agnostic
 * of which page's data it's plotting:
 *   - `toBitsPerSecond(value, sourceUnits)` normalises both shapes
 *     to bits/sec.
 *   - `formatBitrate(bps)` renders bits/sec for display — kb/s when
 *     small, Mb/s when ≥ 1 Mb/s, Gb/s when ≥ 1 Gb/s. Two significant
 *     decimal digits in the >= unit transition, integer kb/s
 *     otherwise (mirrors the precision of the existing grid columns
 *     that show `kb/s` as integers).
 *
 * "kb/s" not "Kbps" — matches Classic's column captions exactly
 * (`status.js:483` "Bandwidth (kb/s)") so the unit reads the same
 * on the chart axes as on the grid.
 */

export type BitrateUnits = 'bits' | 'bytes'

/**
 * Normalise a raw rate value to bits/sec. Streams' `bps` already
 * is; Subscriptions' `in`/`out` are bytes/sec and need a *8 flip.
 */
export function toBitsPerSecond(value: number, sourceUnits: BitrateUnits): number {
  if (!Number.isFinite(value) || value < 0) return 0
  return sourceUnits === 'bytes' ? value * 8 : value
}

/**
 * Format a bits/sec value for chart axis labels + legend rows.
 *
 * Cut-offs:
 *   < 1 Mb/s        → "X kb/s"  (integer)
 *   < 1 Gb/s        → "X.X Mb/s" (one decimal)
 *   ≥ 1 Gb/s        → "X.XX Gb/s" (two decimals)
 *
 * Empty / NaN / negative input yields "0 kb/s" so the chart never
 * shows blank labels mid-rendering.
 */
export function formatBitrate(bps: number): string {
  if (!Number.isFinite(bps) || bps <= 0) return '0 kb/s'
  if (bps < 1_000_000) return `${Math.round(bps / 1000)} kb/s`
  if (bps < 1_000_000_000) return `${(bps / 1_000_000).toFixed(1)} Mb/s`
  return `${(bps / 1_000_000_000).toFixed(2)} Gb/s`
}
