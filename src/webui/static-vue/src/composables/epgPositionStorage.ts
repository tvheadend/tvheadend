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

const ONE_DAY_SEC = 86400

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

/* Re-export internal constants for tests + callers that need to
 * key off them (e.g. cross-tab broadcasting hooks if added
 * later). */
export const _internals = {
  POSITION_KEY,
  ONE_DAY_SEC,
}
