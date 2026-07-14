// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Comparator for tvheadend channel numbers, which the server
 * emits as a "major.minor" STRING ("1", "10", "10.1") — see
 * api_epg_add_channel in src/api/api_epg.c. Neither
 * String.localeCompare (orders "10" before "2") nor a decimal
 * parse (as decimals 10.9 > 10.10, but as channel numbers 10.9
 * precedes 10.10 — the minor is a sub-index up to
 * CHANNEL_SPLIT-1, not a fraction) sorts these correctly.
 *
 * Shared between the EPG Table's in-memory sort and DataGrid's
 * grouped within-cluster secondary sort via ColumnDef.sortComparator.
 */

/* Split "major.minor" into two integer parts. Missing/non-numeric
 * majors sink to the end (Infinity); a missing minor is 0 (bare
 * "10" == "10.0"). */
function channelNumberParts(v: unknown): [number, number] {
  const s = String(v)
  const dot = s.indexOf('.')
  const maj = Number(dot < 0 ? s : s.slice(0, dot))
  const min = dot < 0 ? 0 : Number(s.slice(dot + 1))
  return [
    Number.isFinite(maj) ? maj : Number.POSITIVE_INFINITY,
    Number.isFinite(min) ? min : 0,
  ]
}

/* Direction-agnostic ascending comparator: negative when a sorts
 * before b. Callers apply the sort direction. null/undefined sort
 * first (matching the grid's generic comparators). */
export function channelNumberCompare(a: unknown, b: unknown): number {
  if (a == null && b == null) return 0
  if (a == null) return -1
  if (b == null) return 1
  const [amaj, amin] = channelNumberParts(a)
  const [bmaj, bmin] = channelNumberParts(b)
  return amaj !== bmaj ? amaj - bmaj : amin - bmin
}
