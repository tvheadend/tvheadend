// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DST-correct local-day arithmetic.
 *
 * Naive `± N * 86 400` day math drifts by an hour across DST
 * transitions (a local day is 23 or 25 hours twice a year). These
 * helpers go through the Date API's wall-clock setters, which the
 * engine resolves against the host time zone, so "midnight" and
 * "same time tomorrow" stay aligned to the local calendar.
 */

const ONE_DAY_MS = 86_400_000

/* Local midnight (ms) of the day containing `ms`. */
export function startOfLocalDayMs(ms: number): number {
  const d = new Date(ms)
  d.setHours(0, 0, 0, 0)
  return d.getTime()
}

/* Local midnight (Unix seconds) of the day containing `epochSeconds`. */
export function startOfLocalDayEpoch(epochSeconds: number): number {
  return Math.floor(startOfLocalDayMs(epochSeconds * 1000) / 1000)
}

/* Same wall-clock time `days` local days later (negative = earlier).
 * `Date.setDate` carries the hour/minute across DST transitions, so
 * feeding a local midnight always yields a local midnight. */
export function addLocalDaysEpoch(epochSeconds: number, days: number): number {
  const d = new Date(epochSeconds * 1000)
  d.setDate(d.getDate() + days)
  return Math.floor(d.getTime() / 1000)
}

/* Whole local days between two LOCAL-MIDNIGHT timestamps (ms).
 * Rounding absorbs the ±1h a DST transition adds or removes from
 * the raw difference — exact only for true local midnights. */
export function localDayDiff(midnightMsA: number, midnightMsB: number): number {
  return Math.round((midnightMsA - midnightMsB) / ONE_DAY_MS)
}
