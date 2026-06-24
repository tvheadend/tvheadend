// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { formatBitrate, toBitsPerSecond } from '../formatBitrate'

describe('toBitsPerSecond', () => {
  it('passes through bits-source values unchanged', () => {
    expect(toBitsPerSecond(4_250_000, 'bits')).toBe(4_250_000)
  })

  it('multiplies bytes-source values by 8', () => {
    expect(toBitsPerSecond(1_000_000, 'bytes')).toBe(8_000_000)
  })

  it('floors NaN / negative inputs to 0', () => {
    expect(toBitsPerSecond(Number.NaN, 'bits')).toBe(0)
    expect(toBitsPerSecond(-1, 'bytes')).toBe(0)
  })
})

describe('formatBitrate', () => {
  it('renders zero as "0 kb/s"', () => {
    expect(formatBitrate(0)).toBe('0 kb/s')
  })

  it('renders sub-Mb/s values as integer kb/s', () => {
    expect(formatBitrate(425_000)).toBe('425 kb/s')
    expect(formatBitrate(999_999)).toBe('1000 kb/s')
  })

  it('renders Mb/s values with one decimal', () => {
    expect(formatBitrate(1_500_000)).toBe('1.5 Mb/s')
    expect(formatBitrate(4_250_000)).toBe('4.3 Mb/s')
    expect(formatBitrate(14_200_000)).toBe('14.2 Mb/s')
  })

  it('renders Gb/s values with two decimals', () => {
    expect(formatBitrate(1_250_000_000)).toBe('1.25 Gb/s')
  })

  it('treats NaN and negatives as 0', () => {
    expect(formatBitrate(Number.NaN)).toBe('0 kb/s')
    expect(formatBitrate(-5)).toBe('0 kb/s')
  })
})

