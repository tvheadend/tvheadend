// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Render a Unix epoch (seconds) as a locale date+time string.
 *
 * Returns '' for non-positive epochs because the server's `PT_TIME`
 * properties use 0 as a "not set" sentinel — `last_seen` of a service
 * that's never been seen, `stop` of a recording that hasn't ended,
 * etc. Without this guard, naive `new Date(0)` would render as
 * "1/1/1970, 1:00:00 AM" which is meaningless to the user.
 *
 * Mirrors ExtJS's idnode time renderer (see
 * `src/webui/static/app/idnode.js:368-401` in the legacy UI).
 *
 * Phone-mode (shared `useIsPhone` breakpoint) switches to a
 * smart-relative compact form so dates fit comfortably inside
 * the 50%-width card-pair cells. Same hierarchy iMessage / Slack
 * use:
 *   - Today      → "11:59 PM"           (time only)
 *   - Yesterday  → "Yesterday 09:15"    (locale-aware label)
 *   - Tomorrow   → "Tomorrow 14:00"     (DVR Upcoming case)
 *   - This year  → "Dec 31, 11:59 PM"   (month + day + time)
 *   - Older      → "Dec 31, 24"         (date + 2-digit year, no time)
 *
 * Desktop keeps the full `toLocaleString()` for archival lookups
 * (year + seconds preserved). When the admin has tuned
 * `config.date_mask` (Config → General → Base → "Custom date
 * Format") and the access store carries a non-empty value, the
 * desktop path runs the mask through `toCustomDate` instead —
 * same grammar Classic uses (`tvheadend.js:1458-1497`). Phone
 * intentionally stays on smart-relative; full custom formats
 * don't fit in card-pair cells.
 */
import { getActivePinia } from 'pinia'
import { useAccessStore } from '@/stores/access'
import { isPhoneNow } from '@/composables/useIsPhone'
import { toCustomDate } from './toCustomDate'

function sameLocalDay(a: Date, b: Date): boolean {
  return (
    a.getFullYear() === b.getFullYear() &&
    a.getMonth() === b.getMonth() &&
    a.getDate() === b.getDate()
  )
}

function timeOnly(d: Date): string {
  return d.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' })
}

function smartRelative(d: Date, now: Date): string {
  if (sameLocalDay(d, now)) {
    return timeOnly(d)
  }

  /* Yesterday / Tomorrow — `Intl.RelativeTimeFormat` with
   * `numeric: 'auto'` returns the localized "yesterday" /
   * "tomorrow" label (German "gestern" / "morgen", etc.) when
   * the offset is exactly -1 / +1 day. The label arrives
   * lowercase from the API; capitalize for sentence-start
   * placement in a card. */
  const yesterday = new Date(now)
  yesterday.setDate(now.getDate() - 1)
  const tomorrow = new Date(now)
  tomorrow.setDate(now.getDate() + 1)

  if (sameLocalDay(d, yesterday) || sameLocalDay(d, tomorrow)) {
    const offset = sameLocalDay(d, tomorrow) ? 1 : -1
    const rel = new Intl.RelativeTimeFormat(undefined, { numeric: 'auto' }).format(
      offset,
      'day',
    )
    const capitalized = rel.charAt(0).toUpperCase() + rel.slice(1)
    return `${capitalized} ${timeOnly(d)}`
  }

  /* Same calendar year — month + day + time, no year. */
  if (d.getFullYear() === now.getFullYear()) {
    return d.toLocaleString(undefined, {
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
    })
  }

  /* Different year — month + day + 2-digit year, no time. The
   * card stays scannable for archival rows; tap to open the
   * editor / drawer for the full timestamp. */
  return d.toLocaleDateString(undefined, {
    month: 'short',
    day: 'numeric',
    year: '2-digit',
  })
}

/* Pull the admin's `config.date_mask` from the access store if it
 * exists and a Pinia instance is active. Defensive against
 * fmtDate being called from non-Pinia contexts (unit tests that
 * don't bootstrap a store, SSR pre-render windows): a missing
 * Pinia is silently treated as "no mask" so the caller falls
 * through to the locale-default branch. */
function getDateMask(): string {
  if (!getActivePinia()) return ''
  return useAccessStore().data?.date_mask ?? ''
}

export const fmtDate = (v: unknown): string => {
  /* Guard mirrors the original sentinel logic — `v > 0` rejects
   * negatives, zero (the PT_TIME "not set" marker), AND NaN
   * (since NaN comparisons are always false). */
  if (typeof v !== 'number' || v <= 0 || Number.isNaN(v)) return ''
  const d = new Date(v * 1000)
  if (isPhoneNow()) {
    return smartRelative(d, new Date())
  }
  const mask = getDateMask()
  if (mask) return toCustomDate(d, mask)
  /* Mirror Classic's `tvheadend.niceDate` (static/app/tvheadend.js:
   * 800-808) default shape: short weekday + locale numeric date +
   * 24-hour clock with seconds, regardless of browser locale's
   * habitual 12h/24h convention. Keeps every admin surface
   * (DVR Start / Stop, EPG drawer Start Time / End Time, EPG
   * Table Start, idnode form PT_TIME fields) on a single,
   * unambiguous timestamp shape. Users wanting a different shape
   * set Config → General → Base → "Custom date Format" and the
   * mask branch above wins. */
  return d.toLocaleString(undefined, {
    weekday: 'short',
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  })
}

/*
 * Date-only counterpart to `fmtDate`. Used for fields where the time
 * portion is meaningless or routinely absent — XMLTV's `first_aired`
 * is the canonical case: original-broadcast dates are stored as the
 * date with 00:00:00 baked in, so showing the time component just
 * renders noise.
 *
 * date_mask awareness: if the admin's mask carries no time tokens
 * (`%HH`, `%hh`, `%mm`, `%ss`, `%aa`), the mask renders verbatim —
 * users who want a custom date-only shape get it. If the mask DOES
 * carry time tokens, we fall through to the locale-default date-
 * only path so the rendered value doesn't carry a misleading 00:00
 * tail. Phone smart-relative trims to the date-only branch the same
 * way `fmtDate` does.
 */
const TIME_TOKEN_RE = /%(?:HH|hh|mm|ss|aa)/

/*
 * Stable group-key projector for date-based row grouping.
 *
 * Returns a locale-independent ISO `YYYY-MM-DD` string so two rows
 * recorded on the same calendar day cluster together regardless of
 * the user's locale, date_mask, or viewport. The cluster header
 * itself runs `fmtDateOnly` over the same timestamp at render time,
 * so the user sees a locale-formatted date even though the grouping
 * key under the hood is ISO.
 *
 * Why not reuse `fmtDateOnly` directly as the key? It produces
 * different strings on different viewports (smart-relative on phone)
 * and under different `date_mask` settings, so two rows on the same
 * day could land in different clusters depending on render-time
 * state. ISO is stable.
 *
 * Accepts numbers (Unix seconds) or numeric strings. Falls back to
 * empty string for invalid inputs — PrimeVue treats empty-string
 * keys as a single "no group" cluster, which is the least-surprising
 * behaviour for missing data.
 */
export const fmtGroupDate = (v: unknown): string => {
  let n: number
  if (typeof v === 'number') n = v
  else if (typeof v === 'string') n = Number(v)
  else n = Number.NaN
  if (!Number.isFinite(n) || n <= 0) return ''
  const d = new Date(n * 1000)
  const y = d.getFullYear()
  const m = String(d.getMonth() + 1).padStart(2, '0')
  const day = String(d.getDate()).padStart(2, '0')
  return `${y}-${m}-${day}`
}

export const fmtDateOnly = (v: unknown): string => {
  if (typeof v !== 'number' || v <= 0 || Number.isNaN(v)) return ''
  const d = new Date(v * 1000)
  if (isPhoneNow()) {
    /* Reuse smart-relative's date-only branches by handing it a
     * `now` reference. The hour/minute branches won't fire for a
     * date stripped to midnight because the time-of-day matters
     * only inside the same-local-day case, which a fresh `now` of
     * mid-day-today rules out for any first_aired in the past. */
    return smartRelative(d, new Date())
  }
  const mask = getDateMask()
  if (mask && !TIME_TOKEN_RE.test(mask)) return toCustomDate(d, mask)
  return d.toLocaleDateString(undefined, {
    weekday: 'short',
    day: '2-digit',
    month: '2-digit',
    year: 'numeric',
  })
}
