// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * logFilter — predicate tests. Covers text/regex/subsystem/
 * severity dimensions individually + the AND combination.
 */

import { describe, expect, it } from 'vitest'
import {
  EMPTY_LOG_FILTER,
  isFilterActive,
  matchesFilter,
  safeRegex,
  type LogFilter,
} from '../logFilter'
import type { LogLine } from '@/stores/log'

function line(over: Partial<LogLine> = {}): LogLine {
  return {
    id: 1,
    ts: '12:00:00',
    subsys: 'mpegts',
    body: 'tuned in',
    severity: 'info',
    raw: '12:00:00 mpegts: tuned in',
    ...over,
  }
}

function filter(over: Partial<LogFilter> = {}): LogFilter {
  return {
    ...EMPTY_LOG_FILTER,
    subsystems: new Set(EMPTY_LOG_FILTER.subsystems),
    severities: new Set(EMPTY_LOG_FILTER.severities),
    ...over,
  }
}

describe('isFilterActive', () => {
  it('returns false on the empty filter', () => {
    expect(isFilterActive(EMPTY_LOG_FILTER)).toBe(false)
  })
  it('returns true when any dimension is set', () => {
    expect(isFilterActive(filter({ text: 'x' }))).toBe(true)
    expect(isFilterActive(filter({ subsystems: new Set(['a']) }))).toBe(true)
    expect(isFilterActive(filter({ severities: new Set(['error']) }))).toBe(true)
  })
})

describe('matchesFilter — text', () => {
  it('matches case-insensitive substring on body', () => {
    expect(matchesFilter(line({ body: 'Tuned IN' }), filter({ text: 'tuned in' }))).toBe(true)
    expect(matchesFilter(line({ body: 'Tuned IN' }), filter({ text: 'OFF' }))).toBe(false)
  })
  it('regex mode matches on a valid pattern (case-insensitive)', () => {
    expect(
      matchesFilter(line({ body: 'signal LOST' }), filter({ text: String.raw`^signal\s+lost$`, textIsRegex: true })),
    ).toBe(true)
  })
  it('regex mode returns no-match on invalid regex (does not throw)', () => {
    expect(() =>
      matchesFilter(line({ body: 'anything' }), filter({ text: '[unclosed', textIsRegex: true })),
    ).not.toThrow()
    expect(
      matchesFilter(line({ body: 'anything' }), filter({ text: '[unclosed', textIsRegex: true })),
    ).toBe(false)
  })
  it('empty text passes every line', () => {
    expect(matchesFilter(line(), filter({ text: '' }))).toBe(true)
  })
})

describe('matchesFilter — subsystem', () => {
  it('empty set passes every subsystem', () => {
    expect(matchesFilter(line({ subsys: 'mpegts' }), EMPTY_LOG_FILTER)).toBe(true)
  })
  it('non-empty set passes only the listed subsystems', () => {
    const f = filter({ subsystems: new Set(['mpegts', 'epg']) })
    expect(matchesFilter(line({ subsys: 'mpegts' }), f)).toBe(true)
    expect(matchesFilter(line({ subsys: 'epg' }), f)).toBe(true)
    expect(matchesFilter(line({ subsys: 'http' }), f)).toBe(false)
  })
})

describe('matchesFilter — severity', () => {
  it('empty set passes every severity', () => {
    expect(matchesFilter(line({ severity: 'error' }), EMPTY_LOG_FILTER)).toBe(true)
  })
  it('non-empty set passes only the listed severities', () => {
    const f = filter({ severities: new Set(['error', 'warning']) })
    expect(matchesFilter(line({ severity: 'error' }), f)).toBe(true)
    expect(matchesFilter(line({ severity: 'warning' }), f)).toBe(true)
    expect(matchesFilter(line({ severity: 'info' }), f)).toBe(false)
    expect(matchesFilter(line({ severity: 'debug' }), f)).toBe(false)
  })
})

describe('matchesFilter — AND combination', () => {
  it('requires all dimensions to pass', () => {
    const f = filter({
      text: 'tuned',
      subsystems: new Set(['mpegts']),
      severities: new Set(['info']),
    })
    expect(matchesFilter(line({ body: 'tuned in', subsys: 'mpegts', severity: 'info' }), f)).toBe(true)
    /* Right subsystem, wrong severity. */
    expect(matchesFilter(line({ body: 'tuned in', subsys: 'mpegts', severity: 'error' }), f)).toBe(false)
    /* Right severity, wrong subsystem. */
    expect(matchesFilter(line({ body: 'tuned in', subsys: 'http', severity: 'info' }), f)).toBe(false)
    /* Right subsystem + severity, body mismatch. */
    expect(matchesFilter(line({ body: 'signal lost', subsys: 'mpegts', severity: 'info' }), f)).toBe(false)
  })
})

describe('safeRegex', () => {
  it('returns a RegExp on a valid pattern', () => {
    expect(safeRegex('^signal')).toBeInstanceOf(RegExp)
  })
  it('returns null on an invalid pattern', () => {
    expect(safeRegex('[unclosed')).toBeNull()
  })
})
