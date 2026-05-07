/*
 * Shared logic between `EpgTimeline` and `EpgMagazine`.
 *
 * Magazine is the mirror-axis of Timeline (channels-across vs.
 * time-across), so the time-axis math, hour-tick generation, event
 * bucketing, channel-helper formatters, and tooltip composition are
 * identical between the two — only the per-event positioning
 * (left/width vs top/height) and the cursor offset differ.
 *
 * Pure functions only — each consumer keeps its own reactive
 * computeds and feeds inputs in. Keeps this module trivially
 * testable without `vue-test-utils` mounting.
 */

import type { TooltipMode } from './epgViewOptions'
import { flattenKodiText } from './kodiText'

const ONE_HOUR = 3600

/* "Short event" threshold for the `tooltipMode === 'short'` branch.
 * 30 minutes covers news bulletins, station IDs, weather slots — the
 * cases where the title is most likely clipped on the block surface
 * and the tooltip carries the most informational lift. Hardcoded
 * here rather than per-instance because the user's choice is
 * on/off via the radio; the threshold itself doesn't need exposure. */
const SHORT_EVENT_THRESHOLD_MIN = 30

export interface GridEvent {
  channelUuid?: string
  start?: number
  stop?: number
  title?: string
  subtitle?: string
  summary?: string
  description?: string
}

export interface GridChannel {
  uuid: string
  name?: string
  number?: number
}

/* Bucket events by channelUuid for O(channels) row rendering instead
 * of O(channels × events) per render. Generic over the event type
 * so consumers preserve their richer per-view event shapes. */
export function bucketEventsByChannel<T extends GridEvent>(events: readonly T[]): Map<string, T[]> {
  const map = new Map<string, T[]>()
  for (const ev of events) {
    if (!ev.channelUuid) continue
    const list = map.get(ev.channelUuid)
    if (list) list.push(ev)
    else map.set(ev.channelUuid, [ev])
  }
  return map
}

function isShortEvent(ev: GridEvent): boolean {
  if (typeof ev.start !== 'number' || typeof ev.stop !== 'number') return false
  return (ev.stop - ev.start) / 60 < SHORT_EVENT_THRESHOLD_MIN
}

export function fmtTimeRange(start: number | undefined, stop: number | undefined): string {
  if (typeof start !== 'number' || typeof stop !== 'number') return ''
  const s = new Date(start * 1000).toLocaleTimeString(undefined, {
    hour: '2-digit',
    minute: '2-digit',
  })
  const e = new Date(stop * 1000).toLocaleTimeString(undefined, {
    hour: '2-digit',
    minute: '2-digit',
  })
  return `${s} – ${e}`
}

export function iconUrl(icon: string | undefined): string | null {
  if (!icon) return null
  if (icon.startsWith('http://') || icon.startsWith('https://')) return icon
  return '/' + icon.replace(/^\/+/, '')
}

export function channelNumber(ch: GridChannel): string {
  return typeof ch.number === 'number' ? String(ch.number) : ''
}

export function channelName(ch: GridChannel): string {
  return ch.name ?? ch.uuid
}

/* Hour ticks across the trimmed range. Iterates from `start` to
 * `end - 1 hour` so the last label sits at the start of the final
 * hour rather than crowding the right/bottom edge. `offset` is along
 * whichever axis the consumer is laying out (left for Timeline, top
 * for Magazine).
 *
 * `excludeEpochs` skips ticks whose epoch is in the set. Used to
 * suppress the "00:00" tick at every midnight that already carries
 * a day label — the day label takes that slot, so a second tick
 * there would visually clash. Both EPG views compute their day-
 * boundary epoch set and pass it through. Default behaviour (no set
 * passed) is unchanged. */
export function buildHourTicks(
  start: number,
  end: number,
  pxPerMinute: number,
  excludeEpochs?: ReadonlySet<number>
): Array<{ epoch: number; offset: number; label: string }> {
  const out: Array<{ epoch: number; offset: number; label: string }> = []
  for (let t = start; t < end; t += ONE_HOUR) {
    if (excludeEpochs?.has(t)) continue
    const offsetMin = (t - start) / 60
    const h = new Date(t * 1000).getHours()
    out.push({
      epoch: t,
      offset: offsetMin * pxPerMinute,
      label: `${String(h).padStart(2, '0')}:00`,
    })
  }
  return out
}

/* Most-recent local-time midnight at or before `epochSec`. Used by
 * scroll listeners that need to snap a viewport's centre time to
 * the day-bucket it belongs to (e.g., for emitting a day-cursor
 * update or computing day-aligned fetch ranges). DST-aware via
 * `Date.setHours(0, 0, 0, 0)`. */
export function startOfLocalDayEpoch(epochSec: number): number {
  const d = new Date(epochSec * 1000)
  d.setHours(0, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
}

/* Hover-tooltip content for an event block.
 *
 * Block surface space is tight (a 30-min programme is 120 px wide
 * at 4 px/min — ~10-15 chars of title visible; very short events
 * like 5-min news bulletins get only ~20 px and clip the title
 * almost entirely). The tooltip is the right place to show:
 *   - first line: time range AND title (with ` — ` separator) so
 *     short-event titles that don't fit on the block surface
 *     remain readable on hover.
 *   - subtitle / extra-text on its own line.
 *   - description (never on the block) below that.
 *
 * Layered gates:
 *   1. Global `quicktips` is the master kill-switch (config-level
 *      setting; consumer reads it from access store).
 *   2. Local `mode`: `off` ⇒ never; `short` ⇒ only short events
 *      whose title is likely clipped on the block; `always` ⇒
 *      original behaviour.
 *   3. Even when both gates allow it, an event with no informative
 *      content beyond its position skips the tooltip (we don't show
 *      a popover that just repeats the time range).
 *
 * Returned `class` hooks the unscoped CSS rule that bumps Aura's
 * default `max-width` from 12.5 rem to 480 px (drawer width).
 * `fitContent: false` removes PrimeVue's default `width: fit-content`
 * so multi-line descriptions wrap at the wider max-width instead of
 * sizing to the longest unbreakable line. */
/* Append the non-compact detail lines (extra, blank gap, description)
 * to `parts`. Skips description when it duplicates extra (happens when
 * an event has only `description` set — extraText falls all the way
 * through to it). Extracted from buildTooltip to keep that function's
 * cognitive complexity below the project threshold. */
function appendDetailLines(
  parts: string[],
  extra: string | undefined,
  desc: string | undefined
): void {
  if (extra) parts.push(extra)
  if (desc && desc !== extra) {
    if (parts.length > 0) parts.push('')
    parts.push(desc)
  }
}

export function buildTooltip(opts: {
  ev: GridEvent
  quicktips: boolean
  mode: TooltipMode
  extraText: (ev: GridEvent) => string | undefined
  cssClass: string
  /* Phone-mode tooltip: time + title only, no extra text, no
   * description. Tooltips on phone-width viewports (which still
   * fire on hover-capable devices like a small desktop window)
   * easily exceed the available horizontal space when they carry
   * a multi-line description; the compact form keeps them small
   * enough to fit beside an event near the screen edge. The
   * description is reachable by opening the drawer. */
  compact?: boolean
  /* When true, run user-supplied text fields through the kodi
   * flattener so codes like `[B]…[/B]` / `[CR]` / `[UPPERCASE]…[/…]`
   * don't leak into the tooltip as literal markers. Mirrors the
   * drawer's `KodiText :enabled` gate so the two views stay
   * consistent: with the access flag off, tooltips show codes
   * literally just like the drawer does. */
  labelFormatting: boolean
}): { value: string; class: string; fitContent: false } | null {
  const { ev, quicktips, mode, extraText, cssClass, compact, labelFormatting } = opts
  if (!quicktips) return null
  if (mode === 'off') return null
  if (mode === 'short' && !isShortEvent(ev)) return null
  const fmt = (s: string | undefined): string | undefined =>
    s && labelFormatting ? flattenKodiText(s) : s
  const title = fmt(ev.title?.trim())
  const extra = fmt(extraText(ev))
  const desc = fmt(ev.description?.trim())
  if (!title && !extra && !desc) return null
  const parts: string[] = []
  const range = fmtTimeRange(ev.start, ev.stop)
  /* First line: time range + title joined by an em dash. Either may
   * be empty (no start/stop ⇒ no range; missing title ⇒ no title);
   * `filter(Boolean).join(' — ')` collapses to whichever is set. */
  const firstLine = [range, title].filter(Boolean).join(' — ')
  if (firstLine) parts.push(firstLine)
  if (!compact) appendDetailLines(parts, extra, desc)
  return {
    value: parts.join('\n'),
    class: cssClass,
    fitContent: false,
  }
}
