// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * cacheMap — pure helpers behind the cache bar (CacheBar.vue): runs
 * of equal slots in a cache's map, where a position behind live
 * falls on the bar, and the durations shown beside it.
 */
import { t } from '@/composables/useI18n'
import { CacheLoc, type CacheBudget, type CacheEntry, type CacheLocCode } from '@/types/cache'

export interface CacheRun {
  loc: CacheLocCode
  /* Part of a recording's programme. */
  rec: boolean
  /* First slot and number of slots of the run. */
  start: number
  length: number
}

/* Consecutive equal slots merged, so the bar draws one segment per
 * run instead of one per slot; `rec` marks the slots a recording is
 * made of, which make runs of their own. */
export function cacheRuns(map: readonly number[], rec: readonly boolean[] = []): CacheRun[] {
  const runs: CacheRun[] = []
  for (let i = 0; i < map.length; i++) {
    const loc = (map[i] ?? CacheLoc.gap) as CacheLocCode
    const r = rec[i] === true
    const last = runs[runs.length - 1]
    if (last?.loc === loc && last.rec === r) last.length++
    else runs.push({ loc, rec: r, start: i, length: 1 })
  }
  return runs
}

/* Which of a map's `n` slots fall within a recording's programme.
 * Slot i holds the wall-clock seconds from oldest + i·(span+1)/n on,
 * as the server spreads them (svcbuf_status_slot()), and a slot that
 * meets any [start, stop] range belongs to that recording. */
export function recordedSlots(
  n: number,
  oldest: number,
  span: number,
  ranges: readonly (readonly number[])[] = []
): boolean[] {
  const out = new Array<boolean>(Math.max(0, n)).fill(false)
  if (!Number.isFinite(n) || n <= 0 || ranges.length === 0) return out
  const width = (span + 1) / n
  for (let i = 0; i < n; i++) {
    const a = oldest + i * width
    const b = a + width
    out[i] = ranges.some(([start = 0, stop = 0]) => start < b && stop >= a)
  }
  return out
}

/* How much of the bar a cache fills. The right edge is now (live) and
 * the left is the past: a cache younger than the period it grows to
 * is drawn against the right edge over its share of that period, the
 * past it does not hold yet left empty, so a new cache grows from the
 * right towards the left; once its span reaches the period (or with
 * no period), the cache is the whole bar. */
export function fillFraction(span: number, period = 0): number {
  if (!Number.isFinite(period) || period <= 0 || !Number.isFinite(span) || span >= period) return 1
  return Math.max(0, span / period)
}

/* One colour per cache, the same wherever it is drawn: distinct hues, then
 * lighter ones, like the bands of a weather radar map.  Yellow and green
 * are left out: they mean the buffer and recordings. */
export const CACHE_PALETTE = [
  '#4f8ef7',
  '#a66cf0',
  '#2bb3b1',
  '#e0619e',
  '#9ec2fb',
  '#d0b3f7',
  '#8fdcd9',
  '#f2a9cb',
]

export function cacheName(e: Pick<CacheEntry, 'uuid' | 'channel' | 'service'>): string {
  return e.channel || e.service || e.uuid
}

/* The colour of each cache, by uuid: in the order of their names, so that
 * every view gives a cache the same one. */
export function cacheColors(entries: readonly CacheEntry[]): Map<string, string> {
  const sorted = [...entries].sort((a, b) => cacheName(a).localeCompare(cacheName(b)))
  return new Map(sorted.map((e, i) => [e.uuid, CACHE_PALETTE[i % CACHE_PALETTE.length]]))
}

/* Where the caches live, as the budget says: RAM then disk, RAM only, or
 * disk only (no RAM budget); 'off' when the shared cache is turned off. */
export type CacheMode = 'both' | 'ram-only' | 'disk-only' | 'off'

export function cacheMode(b?: CacheBudget | null): CacheMode {
  if (!b?.enabled) return 'off'
  if (b.ram_only) return 'ram-only'
  return b.ram_max > 0 ? 'both' : 'disk-only'
}

/* The size a cache may grow to, to scale bars by what they hold: the
 * timeshift size -- the RAM size under "RAM only" -- or 0 when it is
 * unlimited. */
export function budgetScale(b?: CacheBudget | null): number {
  if (!b?.enabled || b.unlimited || !Number.isFinite(b.size_max) || b.size_max <= 0) return 0
  return b.size_max
}

/* How much of the bar a cache fills: its share of the period it grows
 * to or of the budget it draws on, whichever it is nearer; the whole bar
 * when neither bounds it. */
export function barFill(span: number, size: number, period = 0, scale = 0): number {
  const byTime =
    Number.isFinite(period) && period > 0 && Number.isFinite(span)
      ? Math.min(1, Math.max(0, span / period))
      : -1
  const bySize =
    Number.isFinite(scale) && scale > 0 && Number.isFinite(size)
      ? Math.min(1, Math.max(0, size / scale))
      : -1
  if (byTime < 0 && bySize < 0) return 1
  return Math.max(0, byTime, bySize)
}

/* Where a point `behind` seconds before the newest data falls on a
 * bar whose right edge is live and whose cache fills its rightmost
 * `fill`, from 1 (live) down to 1 - fill (the oldest data held). A
 * position outside the span is clamped to its edge. */
export function markerOffset(behind: number, span: number, fill = 1): number {
  if (!Number.isFinite(span) || span <= 0) return 1
  return 1 - fill * Math.min(1, Math.max(0, behind / span))
}

/* "1 h 58 min", "42 min", "35 s". */
export function formatSpan(seconds: number): string {
  const s = Math.max(0, Math.round(seconds))
  if (s < 60) return t('{0} s', s)
  const min = Math.floor(s / 60)
  if (min < 60) return t('{0} min', min)
  return t('{0} h {1} min', Math.floor(min / 60), min % 60)
}

/* How far behind live a viewer or a recording is: "live" within a
 * couple of seconds of it, else "−12 min". */
export function formatBehind(seconds: number): string {
  if (seconds < 3) return t('live')
  return '−' + formatSpan(seconds)
}

/* The name of a map slot's location, for class names and labels. */
export function locName(loc: number): 'gap' | 'ram' | 'staging' | 'disk' {
  switch (loc) {
    case CacheLoc.ram:
      return 'ram'
    case CacheLoc.staging:
      return 'staging'
    case CacheLoc.disk:
      return 'disk'
    default:
      return 'gap'
  }
}
