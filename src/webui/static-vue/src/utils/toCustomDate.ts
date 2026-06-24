// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Custom-date formatter — verbatim TypeScript port of Classic's
 * `tvheadend.toCustomDate(date, format)` at
 * `src/webui/static/app/tvheadend.js:1458-1497`. Mirrors the same
 * grammar so an admin who tuned a `config.date_mask` for Classic
 * sees identical output on the Vue UI.
 *
 * Grammar (with the % escape):
 *   %y / %yy / %yyy / %yyyy  — year, padded to N digits (right slice)
 *   %M  / %MM                — numeric month
 *   %MMM / %MMMM             — short / long localised month name
 *   %d  / %dd                — numeric day-of-month
 *   %ddd / %dddd             — short / long localised weekday name
 *   %h  / %hh / %H / %HH     — 24-hour clock
 *   %I  / %II                — 12-hour clock
 *   %p  / %P                 — AM/PM / am/pm
 *   %m  / %mm                — minutes
 *   %s  / %ss                — seconds
 *   %S                       — milliseconds (single digit, by design)
 *   %q                       — calendar quarter (1-4)
 *
 * When the mask contains no `%[MmsSyYdhHIpPq]` token, fall back
 * to the same default `toLocaleString` options Classic uses — gives
 * a sensible date+time format for masks that are blank or
 * accidentally invalid.
 *
 * Padding convention matches Classic exactly: each numeric token's
 * value is `String(value).padStart(4, '0').slice(1 - match.length)`.
 * For `%yyyy` (len 5) that's `slice(-4)` → 4 trailing chars;
 * for `%y` (len 2) it's `slice(-1)` → 1 trailing char of the year.
 * This intentionally yields a single-digit "last-of-year" rendering
 * for `%y` — Classic does the same and admins relying on the
 * grammar will already be calibrated to it.
 */

const TOKEN_DETECT = /(%[MmsSyYdhHIpPq]+)/

const FALLBACK_OPTIONS: Intl.DateTimeFormatOptions = {
  month: '2-digit',
  day: '2-digit',
  year: 'numeric',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hour12: false,
}

export function toCustomDate(date: Date, format: string, locale?: string): string {
  if (!format || !TOKEN_DETECT.test(format)) {
    return date.toLocaleString(locale, FALLBACK_OPTIONS)
  }

  /* Localised name tokens come first, longest-prefix-first so the
   * 4-letter form is consumed before the 3-letter form's regex
   * could match a prefix of it. The replacements substitute the
   * tokens with localised month / weekday strings; those strings
   * never contain a `%` so the subsequent numeric-token sweep is
   * unaffected. */
  let out = format
    .replaceAll('%MMMM', date.toLocaleDateString(locale, { month: 'long' }))
    .replaceAll('%MMM', date.toLocaleDateString(locale, { month: 'short' }))
    .replaceAll('%dddd', date.toLocaleDateString(locale, { weekday: 'long' }))
    .replaceAll('%ddd', date.toLocaleDateString(locale, { weekday: 'short' }))

  /* Numeric tokens — order matters only between same-letter
   * groups, since each regex matches just one letter group. The
   * map keeps the same key order as Classic's `o` object literal
   * (line 1460-1472 of tvheadend.js) for parity. */
  const tokens: ReadonlyArray<readonly [RegExp, number | string]> = [
    [/%[yY]+/g, date.getFullYear()],
    [/%M+/g, date.getMonth() + 1],
    [/%d+/g, date.getDate()],
    [/%[hH]+/g, date.getHours()],
    [/%I+/g, date.getHours() % 12 || 12],
    [/%p/g, date.getHours() >= 12 ? 'PM' : 'AM'],
    [/%P/g, date.getHours() >= 12 ? 'pm' : 'am'],
    [/%m+/g, date.getMinutes()],
    [/%s+/g, date.getSeconds()],
    [/%q+/g, Math.floor((date.getMonth() + 3) / 3)],
    [/%S/g, date.getMilliseconds()],
  ]

  for (const [re, value] of tokens) {
    out = out.replace(re, (match) => {
      if (typeof value === 'string') return value
      /* For two-char `%X` tokens (no repetition), keep the value
       * as-is — Classic uses `match.length === 2 ? value : padded`
       * to short-circuit single-char output for things like
       * %p / %P / %S. */
      if (match.length === 2) return String(value)
      return String(value).padStart(4, '0').slice(1 - match.length)
    })
  }

  return out
}
