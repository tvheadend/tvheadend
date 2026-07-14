// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the channel-number comparator. Covers the
 * lexical trap (10 before 2) and the decimal trap (10.9 vs 10.10),
 * since channel numbers are "major.minor" strings on the wire.
 */

import { describe, expect, it } from 'vitest'
import { channelNumberCompare } from '../channelNumberSort'

const sorted = (list: readonly (string | null)[]) => [...list].sort(channelNumberCompare)

describe('channelNumberCompare', () => {
  it('orders whole numbers numerically, not lexically', () => {
    expect(sorted(['1', '10', '101', '11', '2', '20'])).toEqual([
      '1',
      '2',
      '10',
      '11',
      '20',
      '101',
    ])
  })

  it('orders minor parts as integers, not decimals (10.9 before 10.10)', () => {
    expect(sorted(['10.10', '10.2', '10.100', '10', '10.9', '10.1'])).toEqual([
      '10',
      '10.1',
      '10.2',
      '10.9',
      '10.10',
      '10.100',
    ])
  })

  it('treats a bare major as major.0', () => {
    expect(channelNumberCompare('10', '10.1')).toBeLessThan(0)
    expect(channelNumberCompare('10', '10')).toBe(0)
  })

  it('sorts null/undefined first', () => {
    expect(channelNumberCompare(null, '5')).toBeLessThan(0)
    expect(channelNumberCompare('5', null)).toBeGreaterThan(0)
    expect(channelNumberCompare(null, null)).toBe(0)
    expect(channelNumberCompare(undefined, '5')).toBeLessThan(0)
  })

  it('accepts numbers as well as strings', () => {
    expect(channelNumberCompare(2, 10)).toBeLessThan(0)
    expect(channelNumberCompare(10, 2)).toBeGreaterThan(0)
  })
})
