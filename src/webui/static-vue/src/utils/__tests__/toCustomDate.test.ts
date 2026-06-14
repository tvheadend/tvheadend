// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * toCustomDate — TypeScript port of Classic's tvheadend.toCustomDate
 * (tvheadend.js:1458-1497). Tests pin each grammar token to the same
 * output Classic produces, so an admin's `config.date_mask` renders
 * identically across the two UIs.
 *
 * Date fixture: 2026-05-12 13:07:09.025 local time. Year 2026,
 * month 5 (May), day 12, hour 13, minute 7, second 9, millis 25.
 * Quarter: floor((5-1 + 3) / 3) = 2 (Apr-Jun). The C-style port
 * uses 0-based months, so the formula reads `(getMonth() + 3) / 3`.
 *
 * Locale is pinned to `en-US` for assertions that depend on
 * localised month / weekday names. CI runners ship with en-US
 * data so the long-form names are deterministic; another locale
 * passes the unit but those assertions are skipped.
 */
import { describe, it, expect } from 'vitest'
import { toCustomDate } from '../toCustomDate'

/* 2026-05-12 13:07:09.025 (constructor uses local time). */
const D = new Date(2026, 4 /* May (0-based) */, 12, 13, 7, 9, 25)
const LOCALE = 'en-US'

describe('toCustomDate — fallback when no tokens present', () => {
  it('returns toLocaleString output with default options when mask has no tokens', () => {
    /* Default options: 2-digit month/day/hour/minute/second,
     * numeric year, 24-hour. Format varies by locale but always
     * includes those parts; assert presence of the year and the
     * second component to confirm we hit the fallback branch. */
    const out = toCustomDate(D, 'no tokens here', LOCALE)
    expect(out).toContain('2026')
    expect(out).toContain('09')
  })

  it('uses fallback for empty string mask', () => {
    const out = toCustomDate(D, '', LOCALE)
    expect(out).toContain('2026')
  })
})

describe('toCustomDate — numeric tokens', () => {
  it('%y returns the year as-is (single-token short-circuit, Classic parity)', () => {
    /* Classic's `match.length === 2 ? value : padded`: %y is two
     * chars so no padding/slicing — full year emitted. */
    expect(toCustomDate(D, '%y', LOCALE)).toBe('2026')
  })

  it('%yy returns the 2-digit year', () => {
    expect(toCustomDate(D, '%yy', LOCALE)).toBe('26')
  })

  it('%yyyy returns the 4-digit year', () => {
    expect(toCustomDate(D, '%yyyy', LOCALE)).toBe('2026')
  })

  it('%M returns the month number as-is', () => {
    expect(toCustomDate(D, '%M', LOCALE)).toBe('5')
  })

  it('%MM returns the zero-padded month', () => {
    expect(toCustomDate(D, '%MM', LOCALE)).toBe('05')
  })

  it('%d returns the day as-is', () => {
    expect(toCustomDate(D, '%d', LOCALE)).toBe('12')
  })

  it('%dd returns the zero-padded day', () => {
    /* Day 12 padStart(4, '0') = '0012', slice(-2) = '12'. */
    expect(toCustomDate(D, '%dd', LOCALE)).toBe('12')
  })

  it('%h / %hh return 24-hour clock values', () => {
    expect(toCustomDate(D, '%h', LOCALE)).toBe('13')
    expect(toCustomDate(D, '%hh', LOCALE)).toBe('13')
  })

  it('%H / %HH are aliases for 24-hour clock', () => {
    expect(toCustomDate(D, '%H', LOCALE)).toBe('13')
    expect(toCustomDate(D, '%HH', LOCALE)).toBe('13')
  })

  it('%I / %II return 12-hour clock values', () => {
    /* Hour 13 → 13 % 12 = 1. */
    expect(toCustomDate(D, '%I', LOCALE)).toBe('1')
    expect(toCustomDate(D, '%II', LOCALE)).toBe('01')
  })

  it('%I returns 12 (not 0) at noon and midnight', () => {
    const noon = new Date(2026, 0, 1, 12, 0, 0)
    expect(toCustomDate(noon, '%I', LOCALE)).toBe('12')
    const midnight = new Date(2026, 0, 1, 0, 0, 0)
    expect(toCustomDate(midnight, '%I', LOCALE)).toBe('12')
  })

  it('%m / %mm return minutes', () => {
    expect(toCustomDate(D, '%m', LOCALE)).toBe('7')
    expect(toCustomDate(D, '%mm', LOCALE)).toBe('07')
  })

  it('%s / %ss return seconds', () => {
    expect(toCustomDate(D, '%s', LOCALE)).toBe('9')
    expect(toCustomDate(D, '%ss', LOCALE)).toBe('09')
  })

  it('%S returns one digit of milliseconds (Classic parity)', () => {
    /* Classic's `%S` has no `+`; match length is always 2 so the
     * short-circuit branch returns the value as-is. For our
     * `25` fixture that's "25". */
    expect(toCustomDate(D, '%S', LOCALE)).toBe('25')
  })

  it('%q returns the calendar quarter', () => {
    /* May (month index 4) → floor((4 + 3) / 3) = 2. */
    expect(toCustomDate(D, '%q', LOCALE)).toBe('2')
    const jan = new Date(2026, 0, 1)
    expect(toCustomDate(jan, '%q', LOCALE)).toBe('1')
    const oct = new Date(2026, 9, 1)
    expect(toCustomDate(oct, '%q', LOCALE)).toBe('4')
  })
})

describe('toCustomDate — AM/PM tokens', () => {
  it('%p returns uppercase AM/PM', () => {
    const am = new Date(2026, 0, 1, 8, 0, 0)
    const pm = new Date(2026, 0, 1, 20, 0, 0)
    expect(toCustomDate(am, '%p', LOCALE)).toBe('AM')
    expect(toCustomDate(pm, '%p', LOCALE)).toBe('PM')
  })

  it('%P returns lowercase am/pm', () => {
    const am = new Date(2026, 0, 1, 8, 0, 0)
    const pm = new Date(2026, 0, 1, 20, 0, 0)
    expect(toCustomDate(am, '%P', LOCALE)).toBe('am')
    expect(toCustomDate(pm, '%P', LOCALE)).toBe('pm')
  })
})

describe('toCustomDate — localised name tokens', () => {
  it('%MMMM returns the long localised month name', () => {
    expect(toCustomDate(D, '%MMMM', LOCALE)).toBe('May')
  })

  it('%MMM returns the short localised month name', () => {
    expect(toCustomDate(D, '%MMM', LOCALE)).toBe('May')
  })

  it('%dddd returns the long localised weekday name', () => {
    /* 2026-05-12 is a Tuesday. */
    expect(toCustomDate(D, '%dddd', LOCALE)).toBe('Tuesday')
  })

  it('%ddd returns the short localised weekday name', () => {
    expect(toCustomDate(D, '%ddd', LOCALE)).toBe('Tue')
  })

  it('keeps numeric %M / %d intact when adjacent to name tokens', () => {
    /* `%MMMM-%M` → "May-5" (numeric form replaced AFTER named). */
    expect(toCustomDate(D, '%MMMM-%M', LOCALE)).toBe('May-5')
  })
})

describe('toCustomDate — composite masks', () => {
  it('renders a typical ISO-like mask', () => {
    expect(toCustomDate(D, '%yyyy-%MM-%dd %hh:%mm:%ss', LOCALE)).toBe(
      '2026-05-12 13:07:09',
    )
  })

  it('renders a 12-hour AM/PM mask', () => {
    expect(toCustomDate(D, '%II:%mm %p', LOCALE)).toBe('01:07 PM')
  })

  it('preserves literal text around tokens', () => {
    expect(toCustomDate(D, 'Date: %yyyy-%MM-%dd', LOCALE)).toBe(
      'Date: 2026-05-12',
    )
  })
})
