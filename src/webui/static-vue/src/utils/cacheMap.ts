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

/* One colour per cache, the same wherever it is drawn: tones from mauve to
 * pink, dark and light in turn so that neighbours stand apart.  Blue,
 * green and yellow are left out: they say what is cached, what a recording
 * is made of, and what waits for the disk. */
export const CACHE_PALETTE = [
  '#6d2f9c',
  '#d6a4f0',
  '#a8327f',
  '#f4a6d2',
  '#8f4fd6',
  '#e65ea8',
  '#4f2373',
  '#ebc6f2',
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

/* The span every cache bar is drawn against: the first of 15, 30, 60 and
 * 120 minutes -- then an hour more at a time -- that the longest cache
 * fills no more than nine tenths of, so that the bars move on to the next
 * before one reaches the edge, and no longer than the period the caches
 * grow to (0: none).  One scale for every bar, so that the bars compare
 * and a young cache is no sliver. */
export function barScale(spans: readonly number[], period = 0): number {
  const longest = Math.max(0, ...spans.filter((s) => Number.isFinite(s)))
  let tier = 15 * 60
  while (tier * 0.9 < longest && (period <= 0 || tier < period))
    tier = tier < 120 * 60 ? tier * 2 : tier + 60 * 60
  return period > 0 ? Math.min(tier, period) : tier
}

/* The bars' span as said beside them: "15 min", "2 h". */
export function formatScale(seconds: number): string {
  const min = Math.round(seconds / 60)
  return min >= 60 && min % 60 === 0 ? t('{0} h', min / 60) : formatSpan(seconds)
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

/* A cache's span, saying when it has reached the longest a cache keeps --
 * the timeshift "Maximum period", `period` seconds, 0 for none: it grows no
 * further, its oldest data going as new data comes. */
export function formatCacheSpan(seconds: number, period?: number): string {
  if (period && period > 0 && seconds >= period - 60) return t('{0} · max', formatSpan(period))
  return formatSpan(seconds)
}

/* How far behind live a viewer or a recording is: "live" within a
 * couple of seconds of it, else "−12 min". */
export function formatBehind(seconds: number): string {
  if (seconds < 3) return t('live')
  return '−' + formatSpan(seconds)
}

/* Who is in a cache, one entry each, as Status > Subscriptions words it:
 * each recording, then each viewer numbered, whether it is paused (null
 * for a recording or a live stream), how far behind live, and who the
 * viewer is -- its client and address, what pointing at its marker says
 * --, then each stream played live past the cache, numbered after them
 * (Status > Subscriptions says how each is served). */
export interface CacheOccupant {
  key: string
  label: string
  paused: boolean | null
  rest: string
  who?: string
}

export function cacheOccupants(e: CacheEntry): CacheOccupant[] {
  const out: CacheOccupant[] = (e.recordings ?? []).map((behind, i) => ({
    key: `r${i}`,
    label: `${t('Recording')}:`,
    paused: null,
    rest: ` ${formatBehind(behind)}`,
  }))
  ;(e.viewers ?? []).forEach((behind, i) => {
    const paused = e.paused?.[i] === true
    out.push({
      key: `v${i}`,
      label: `${t('Viewer {0}', i + 1)}:`,
      paused,
      rest: ` ${paused ? t('(paused)') + ' ' : ''}${formatBehind(behind)}`,
      who: e.clients?.[i],
    })
  })
  const n = e.viewers?.length ?? 0
  ;(e.live ?? []).forEach((who, i) => {
    out.push({
      key: `l${i}`,
      label: `${t('Viewer {0}', n + i + 1)}:`,
      paused: null,
      rest: ` ${formatBehind(0)}`,
      who,
    })
  })
  return out
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
