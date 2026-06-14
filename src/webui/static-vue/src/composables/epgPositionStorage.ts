// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Sticky EPG position — sessionStorage I/O + validation helpers.
 *
 * Persists the (day, time-of-day, top channel) the user was
 * looking at when they navigated away from the EPG, so the next
 * mount can restore them to roughly the same place. Per-tab
 * scope (sessionStorage, not localStorage) because two tabs
 * scrolling around independently shouldn't yank each other.
 *
 * Pure functions; no Vue refs / no reactive coupling. Imported
 * by `useEpgViewState` for read on mount + writer composed with
 * the live `dayStart`. The shape mirrors the existing
 * `pickXxx` validation pattern in `useEpgViewState.ts:121-168`.
 *
 * The freshness check (`isPositionStillFresh`) lives here too
 * so the consumer doesn't reinvent "did the day roll over while
 * I was away" logic each time it reads.
 */

const POSITION_KEY = 'tvh-epg:position'
const LAST_VIEW_KEY = 'tvh-epg:last-view'

const ONE_DAY_SEC = 86400

/* The three EPG sub-views that have their own router child
 * route. Persisted so that re-entering the EPG section lands
 * on whichever sub-view the user was last on, instead of
 * always dropping back to Timeline. */
export type EpgViewName = 'timeline' | 'magazine' | 'table'

const VALID_EPG_VIEWS: readonly EpgViewName[] = ['timeline', 'magazine', 'table']

export interface StickyPosition {
  /* Local-day-start epoch (seconds) the user last had selected
   * via the day picker. */
  dayStart: number
  /* Epoch (seconds) at the leading edge of the visible viewport
   * when last persisted. Restored via the existing scrollToTime
   * helpers in useTimelineScroll / useMagazineScroll. */
  scrollTime: number
  /* UUID of the top-most (Timeline) / left-most (Magazine)
   * visible channel. Empty string is allowed but treated as
   * "no preference" by the consumer (falls back to first
   * channel). */
  topChannelUuid: string
  /* Set by `clampSameDayScrollTimeForward` when a stale same-day
   * scrollTime was pushed forward to nowEpoch. Signals to the
   * initial-scroll composable that the user's effective intent
   * is "show me now", so it should route through the snap+left-
   * align scrollToNow path instead of scrollToTime — otherwise
   * the leftThird/topThird alignment + missing half-hour snap
   * leaves the viewport's leading 1/3 filled with blank cells
   * (events with `stop < now` are filtered server-side, see
   * src/epg.c:2335). Absent on freshly-read positions. */
  wasClamped?: boolean
}

/* Number-typed value or fallback to null; mirrors the
 * pickBoolean / pickEnum precedent in useEpgViewState.ts. */
function pickNumber(v: unknown): number | null {
  return typeof v === 'number' && Number.isFinite(v) ? v : null
}

function pickString(v: unknown): string | null {
  return typeof v === 'string' ? v : null
}

/* Read raw JSON from sessionStorage. Returns null when the key
 * is absent OR access throws (SecurityError / disabled storage
 * / private-browsing quirks). Identical defensive shape to
 * readStoredViewOptionsRaw in useEpgViewState. */
function readRaw(): string | null {
  try {
    return globalThis.sessionStorage?.getItem(POSITION_KEY) ?? null
  } catch {
    return null
  }
}

/* Read + parse + validate. Any malformed entry (corrupt JSON,
 * missing field, wrong-typed field) returns null — caller
 * treats null as "no restore" and falls through to the
 * default (scroll-to-now) path. */
export function readStickyPosition(): StickyPosition | null {
  const raw = readRaw()
  if (raw === null) return null
  let parsed: unknown
  try {
    parsed = JSON.parse(raw)
  } catch {
    return null
  }
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) return null
  const o = parsed as Record<string, unknown>
  const dayStart = pickNumber(o.dayStart)
  const scrollTime = pickNumber(o.scrollTime)
  const topChannelUuid = pickString(o.topChannelUuid)
  if (dayStart === null || scrollTime === null || topChannelUuid === null) return null
  return { dayStart, scrollTime, topChannelUuid }
}

/* Silent-fail write. Storage exceptions (quota exceeded,
 * disabled storage) drop the write rather than break the EPG
 * mount path — a missing sticky position is a non-fatal
 * degradation. */
export function writeStickyPosition(p: StickyPosition): void {
  try {
    globalThis.sessionStorage?.setItem(POSITION_KEY, JSON.stringify(p))
  } catch {
    /* swallow — no UX-visible recovery */
  }
}

export function clearStickyPosition(): void {
  try {
    globalThis.sessionStorage?.removeItem(POSITION_KEY)
  } catch {
    /* swallow */
  }
}

/* ---- Last-view memory ----
 *
 * Separate sessionStorage key from `StickyPosition` because it
 * applies to all three sub-views (Timeline / Magazine / Table)
 * — Table doesn't have a time axis so it has no scrollTime /
 * topChannelUuid to record, but it should still be remembered
 * as "the view the user was on." Written unconditionally on
 * sub-view mount; a non-null value implies "user visited this
 * view at least once this tab session." */

export function readLastView(): EpgViewName | null {
  let raw: string | null = null
  try {
    raw = globalThis.sessionStorage?.getItem(LAST_VIEW_KEY) ?? null
  } catch {
    return null
  }
  if (raw === null) return null
  return (VALID_EPG_VIEWS as readonly string[]).includes(raw) ? (raw as EpgViewName) : null
}

export function writeLastView(view: EpgViewName): void {
  try {
    globalThis.sessionStorage?.setItem(LAST_VIEW_KEY, view)
  } catch {
    /* swallow — same silent-fail contract as writeStickyPosition */
  }
}

/* True when the persisted day is today or in the future
 * relative to `nowEpoch` (seconds). `startOfDay` is injected
 * so the consumer uses the same local-midnight helper the
 * composable already has — avoids duplicating that logic
 * here and lets tests inject a deterministic version.
 *
 * Past-date positions reset to the live "now" cursor; this is
 * the predicate that drives the fallback. */
export function isPositionStillFresh(
  p: StickyPosition,
  nowEpoch: number,
  startOfDay: (epoch: number) => number,
): boolean {
  return p.dayStart >= startOfDay(nowEpoch)
}

/* When the saved day is TODAY and the saved scroll-time is in the
 * past, return a position with `scrollTime` clamped forward to
 * `nowEpoch` AND `wasClamped: true` so the restore-scroll path
 * knows to take the snap+left-align scrollToNow branch. Otherwise
 * return the position unchanged.
 *
 * Why this exists: the EPG server filter drops events whose
 * `stop < now` (`src/epg.c:2335`), so a stale morning scroll-time
 * restored later in the same day would leave the leftmost cells
 * empty and push the now-cursor off-screen to the right. Forward-
 * scrolled positions (tomorrow's primetime, deliberate planning)
 * stay intact because the same-day check filters them out. Top-
 * channel restoration is orthogonal and untouched.
 *
 * The `wasClamped` flag is necessary because clamping alone fixes
 * the off-screen now-cursor but leaves the leftThird/topThird
 * alignment in place — the viewport's leading 1/3 still maps to
 * times *before* nowEpoch, which the server filter also drops,
 * leaving an awkward blank wedge. Signalling the clamp lets the
 * call site swap to `scrollToNow` (which uses align='left'+half-
 * hour snap) for the no-wedge presentation.
 *
 * Pure function so the call site (useEpgViewState's onMounted)
 * stays a one-liner and the cases can be enumerated in unit
 * tests against this helper rather than the whole composable. */
export function clampSameDayScrollTimeForward(
  p: StickyPosition,
  nowEpoch: number,
  startOfDay: (epoch: number) => number,
): StickyPosition {
  const sameDay = p.dayStart === startOfDay(nowEpoch)
  if (!sameDay) return p
  if (p.scrollTime >= nowEpoch) return p
  return { ...p, scrollTime: nowEpoch, wasClamped: true }
}

/* Re-export internal constants for tests + callers that need to
 * key off them (e.g. cross-tab broadcasting hooks if added
 * later). */
export const _internals = {
  POSITION_KEY,
  LAST_VIEW_KEY,
  ONE_DAY_SEC,
}
