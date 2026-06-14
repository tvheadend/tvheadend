// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * fmtDate unit tests. Covers the epoch-to-locale-string formatter
 * IdnodeGrid uses for PT_TIME columns and DVR field defs use for
 * start / stop columns.
 *
 * Desktop behaviour: locale-string output is browser/Node locale
 * dependent, so assertions compare against
 * `new Date(...).toLocaleString()` directly rather than hard-coding
 * format strings — same input through the same path.
 *
 * Phone behaviour: smart-relative format with five branches —
 * today / yesterday / tomorrow / same-year / older-year. Uses
 * vi.setSystemTime to pin "now" for deterministic assertions.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * flag — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the formatter's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({ isPhone: false }))
vi.mock('@/composables/useIsPhone', () => ({
  PHONE_MAX_WIDTH: 768,
  useIsPhone: () => ({ value: phoneFlag.isPhone }),
  isPhoneNow: () => phoneFlag.isPhone,
}))

import { fmtDate, fmtDateOnly, fmtGroupDate } from '../formatTime'
import { useAccessStore } from '@/stores/access'

function setViewport(width: number) {
  phoneFlag.isPhone = width < 768
}

const DESKTOP = 1280
const PHONE = 400

function setMask(mask: string) {
  const access = useAccessStore()
  access.data = { admin: true, dvr: true, date_mask: mask }
}

beforeEach(() => {
  /* fmtDate now consults useAccessStore for the optional
   * `date_mask` config — Pinia must be active. Default access
   * data is null, so no mask, and the locale-default branch is
   * exercised unless a test opts in by setting it. */
  setActivePinia(createPinia())
})

describe('fmtDate — non-positive / non-number sentinels', () => {
  beforeEach(() => setViewport(DESKTOP))

  it('returns "" for epoch 0 (the PT_TIME "not set" sentinel)', () => {
    expect(fmtDate(0)).toBe('')
  })

  it('returns "" for a negative epoch', () => {
    expect(fmtDate(-1)).toBe('')
  })

  it('returns "" for null', () => {
    expect(fmtDate(null)).toBe('')
  })

  it('returns "" for undefined', () => {
    expect(fmtDate(undefined)).toBe('')
  })

  it('returns "" for a string', () => {
    expect(fmtDate('1714780800')).toBe('')
  })

  it('returns "" for an object', () => {
    expect(fmtDate({})).toBe('')
  })

  it('returns "" for NaN', () => {
    /* `new Date(NaN).toLocaleString()` returns 'Invalid Date'; the
     * `> 0` guard naturally rejects NaN since NaN comparisons are
     * always false. */
    expect(fmtDate(Number.NaN)).toBe('')
  })
})

/* Classic's `niceDate` (static/app/tvheadend.js:800-808) default
 * shape: short weekday + locale numeric date + 24-hour clock with
 * seconds, irrespective of browser locale's habitual 12h/24h
 * convention. `fmtDate` mirrors that exactly when no `date_mask` is
 * set; tests compose the expected shape from the same Intl.DateTime
 * options the implementation uses, so locale variance (en-GB vs
 * de-DE vs en-US) doesn't break the assertion. */
const NICE_DATE_OPTS: Intl.DateTimeFormatOptions = {
  weekday: 'short',
  day: '2-digit',
  month: '2-digit',
  year: 'numeric',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hour12: false,
}

describe('fmtDate — desktop viewport (Classic niceDate shape)', () => {
  beforeEach(() => setViewport(DESKTOP))

  it('formats a positive epoch as weekday + numeric date + 24h time', () => {
    const epoch = 1714780800
    expect(fmtDate(epoch)).toBe(
      new Date(epoch * 1000).toLocaleString(undefined, NICE_DATE_OPTS),
    )
  })

  it('preserves year + seconds (no compaction on desktop)', () => {
    /* Use a specific date so the assertion is concrete. The
     * format itself is locale-dependent; just verify the year is
     * present somewhere in the output. */
    const epoch = Math.floor(new Date(2024, 11, 31, 23, 59, 45).getTime() / 1000)
    expect(fmtDate(epoch)).toContain('2024')
  })
})

describe('fmtDate — phone viewport (smart-relative)', () => {
  beforeEach(() => {
    setViewport(PHONE)
    vi.useFakeTimers()
    /* Fix "now" at 2025-06-15 14:00 local for deterministic
     * relative-day comparisons. */
    vi.setSystemTime(new Date(2025, 5, 15, 14, 0, 0))
  })

  afterEach(() => {
    vi.useRealTimers()
    setViewport(DESKTOP)
  })

  it('returns time-only for an epoch that lands today', () => {
    const epoch = Math.floor(new Date(2025, 5, 15, 9, 30, 0).getTime() / 1000)
    const out = fmtDate(epoch)
    /* Result is locale-dependent (24h vs 12h, separator) but must
     * NOT contain any date / month text. Cheap check: no 4-digit
     * year and no month name. */
    expect(out).not.toMatch(/2025/)
    expect(out).not.toMatch(/Jun/)
    expect(out).toMatch(/\d/)
  })

  it('returns "Yesterday <time>" for an epoch the previous local day', () => {
    const epoch = Math.floor(new Date(2025, 5, 14, 8, 15, 0).getTime() / 1000)
    const out = fmtDate(epoch)
    /* `Intl.RelativeTimeFormat` returns localized labels — assert
     * structure: a capitalized non-numeric label followed by a
     * space then digits. The label itself depends on the runtime
     * locale (English: "Yesterday"). */
    expect(out).toMatch(/^[A-Z][a-z]+ \d/)
    expect(out).not.toMatch(/2025/)
  })

  it('returns "Tomorrow <time>" for an epoch the next local day', () => {
    const epoch = Math.floor(new Date(2025, 5, 16, 16, 30, 0).getTime() / 1000)
    const out = fmtDate(epoch)
    expect(out).toMatch(/^[A-Z][a-z]+ \d/)
    expect(out).not.toMatch(/2025/)
  })

  it('returns month + day + time for same-year (not today/yesterday/tomorrow)', () => {
    /* Two months ago. */
    const epoch = Math.floor(new Date(2025, 3, 10, 11, 45, 0).getTime() / 1000)
    const out = fmtDate(epoch)
    /* Year must NOT be present; month name OR numeric date must
     * appear. */
    expect(out).not.toMatch(/2025/)
    expect(out).toMatch(/\d/)
  })

  it('returns date + 2-digit year (no time) for a different year', () => {
    /* Same calendar day-of-year but 2 years prior. */
    const epoch = Math.floor(new Date(2023, 5, 15, 9, 30, 0).getTime() / 1000)
    const out = fmtDate(epoch)
    /* Older years drop the time; assert no `:` separator. The
     * 2-digit year (`23`) should appear OR a 4-digit year
     * depending on locale; check at least one of those. */
    expect(out).not.toMatch(/:/)
    expect(out).toMatch(/23/)
  })

  it('respects the PT_TIME "not set" sentinel even on phone', () => {
    expect(fmtDate(0)).toBe('')
  })
})

describe('fmtDate — desktop with admin-tuned date_mask', () => {
  beforeEach(() => setViewport(DESKTOP))

  it('renders through toCustomDate when access.data.date_mask is set', () => {
    setMask('%yyyy-%MM-%dd')
    const epoch = Math.floor(new Date(2026, 4, 12, 13, 7, 9).getTime() / 1000)
    expect(fmtDate(epoch)).toBe('2026-05-12')
  })

  it('falls back to Classic niceDate shape when mask is empty string', () => {
    setMask('')
    const epoch = 1714780800
    expect(fmtDate(epoch)).toBe(
      new Date(epoch * 1000).toLocaleString(undefined, NICE_DATE_OPTS),
    )
  })

  it('falls back to Classic niceDate shape when mask is missing entirely', () => {
    /* access.data set but no date_mask field. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true }
    const epoch = 1714780800
    expect(fmtDate(epoch)).toBe(
      new Date(epoch * 1000).toLocaleString(undefined, NICE_DATE_OPTS),
    )
  })

  it('phone viewport ignores the mask (smart-relative preserved)', () => {
    /* The mask is meant for full date+time output that won't fit
     * in a card-pair cell; phone keeps smart-relative regardless.
     * Pin "now" so the "today" branch is predictable. */
    setViewport(PHONE)
    setMask('%yyyy-%MM-%dd')
    const fixedNow = new Date(2026, 4, 12, 18, 0, 0)
    vi.useFakeTimers()
    vi.setSystemTime(fixedNow)
    const epoch = Math.floor(new Date(2026, 4, 12, 13, 7, 9).getTime() / 1000)
    const out = fmtDate(epoch)
    /* Today branch returns HH:MM only, not the mask. */
    expect(out).not.toContain('2026-05-12')
    vi.useRealTimers()
  })
})

/* fmtDateOnly — date-only sibling of fmtDate. Mirrors Classic's
 * `niceDateYearMonth` (static/app/tvheadend.js:815-…) in shape: the
 * caller wants a date with NO time component. Used by the EPG event
 * drawer's First Aired row, where XMLTV stores the broadcast date
 * with 00:00:00 baked in. */
const DATE_ONLY_OPTS: Intl.DateTimeFormatOptions = {
  weekday: 'short',
  day: '2-digit',
  month: '2-digit',
  year: 'numeric',
}

describe('fmtDateOnly — sentinel handling', () => {
  beforeEach(() => setViewport(DESKTOP))

  it('returns empty string for non-numeric input', () => {
    expect(fmtDateOnly('not-a-number')).toBe('')
    expect(fmtDateOnly(null)).toBe('')
    expect(fmtDateOnly(undefined)).toBe('')
  })

  it('returns empty string for the PT_TIME zero sentinel', () => {
    expect(fmtDateOnly(0)).toBe('')
  })

  it('returns empty string for negative epochs', () => {
    expect(fmtDateOnly(-1)).toBe('')
  })

  it('returns empty string for NaN', () => {
    expect(fmtDateOnly(Number.NaN)).toBe('')
  })
})

describe('fmtDateOnly — desktop viewport', () => {
  beforeEach(() => setViewport(DESKTOP))

  it('formats a positive epoch as date only (no time component)', () => {
    const epoch = 1714780800
    const out = fmtDateOnly(epoch)
    expect(out).toBe(
      new Date(epoch * 1000).toLocaleDateString(undefined, DATE_ONLY_OPTS),
    )
    /* Explicit assertion that no clock-time digits sneak through. */
    expect(out).not.toMatch(/\d{2}:\d{2}/)
  })

  it('routes through toCustomDate when mask is date-only', () => {
    setMask('%yyyy-%MM-%dd')
    const epoch = Math.floor(new Date(2026, 4, 12, 13, 7, 9).getTime() / 1000)
    expect(fmtDateOnly(epoch)).toBe('2026-05-12')
  })

  it('ignores the mask when it carries time tokens', () => {
    /* User wanted date+time format, but caller asked for date only.
     * Falling through to the locale-default date-only branch keeps
     * the value from carrying a misleading 00:00 tail. */
    setMask('%yyyy-%MM-%dd %HH:%mm')
    const epoch = 1714780800
    const out = fmtDateOnly(epoch)
    expect(out).toBe(
      new Date(epoch * 1000).toLocaleDateString(undefined, DATE_ONLY_OPTS),
    )
    expect(out).not.toMatch(/\d{2}:\d{2}/)
  })
})

describe('fmtGroupDate — stable ISO group-key projection', () => {
  it('returns YYYY-MM-DD for a positive Unix-seconds timestamp', () => {
    /* 2026-04-12 09:30 UTC; the projector reads the LOCAL calendar
     * date, so use a Date roundtrip to stay locale-stable. */
    const epoch = Math.floor(new Date(2026, 3, 12, 9, 30, 0).getTime() / 1000)
    expect(fmtGroupDate(epoch)).toBe('2026-04-12')
  })

  it('returns YYYY-MM-DD for a numeric string', () => {
    const epoch = Math.floor(new Date(2026, 0, 1, 0, 0, 0).getTime() / 1000)
    expect(fmtGroupDate(String(epoch))).toBe('2026-01-01')
  })

  it('pads single-digit month and day with leading zero', () => {
    const epoch = Math.floor(new Date(2026, 0, 5, 12, 0, 0).getTime() / 1000)
    expect(fmtGroupDate(epoch)).toBe('2026-01-05')
  })

  it('groups two timestamps on the same local day to the same key', () => {
    const a = Math.floor(new Date(2026, 3, 12, 1, 0, 0).getTime() / 1000)
    const b = Math.floor(new Date(2026, 3, 12, 22, 59, 0).getTime() / 1000)
    expect(fmtGroupDate(a)).toBe(fmtGroupDate(b))
  })

  it('produces lexicographically sortable keys (= chronologically)', () => {
    const a = Math.floor(new Date(2026, 0, 31, 0, 0, 0).getTime() / 1000)
    const b = Math.floor(new Date(2026, 1, 1, 0, 0, 0).getTime() / 1000)
    expect(fmtGroupDate(a) < fmtGroupDate(b)).toBe(true)
  })

  it('returns empty string for sentinels (0 / negative / NaN / non-numeric)', () => {
    expect(fmtGroupDate(0)).toBe('')
    expect(fmtGroupDate(-1)).toBe('')
    expect(fmtGroupDate(Number.NaN)).toBe('')
    expect(fmtGroupDate(undefined)).toBe('')
    expect(fmtGroupDate(null)).toBe('')
    expect(fmtGroupDate('not a number')).toBe('')
  })
})
