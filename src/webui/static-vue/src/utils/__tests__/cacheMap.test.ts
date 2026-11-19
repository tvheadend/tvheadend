// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
import { describe, expect, it } from 'vitest'
import {
  CACHE_PALETTE,
  barScale,
  cacheColors,
  cacheMode,
  cacheRuns,
  recordedSlots,
  fillFraction,
  formatBehind,
  formatScale,
  formatSpan,
  locName,
  markerOffset,
  formatCacheSpan,
} from '../cacheMap'

describe('cacheRuns', () => {
  it('merges consecutive equal slots into runs', () => {
    expect(cacheRuns([3, 3, 3, 0, 1, 1])).toEqual([
      { loc: 3, rec: false, start: 0, length: 3 },
      { loc: 0, rec: false, start: 3, length: 1 },
      { loc: 1, rec: false, start: 4, length: 2 },
    ])
  })

  it('splits a run where a recording begins', () => {
    expect(cacheRuns([3, 3, 3, 1], [false, true, true, true])).toEqual([
      { loc: 3, rec: false, start: 0, length: 1 },
      { loc: 3, rec: true, start: 1, length: 2 },
      { loc: 1, rec: true, start: 3, length: 1 },
    ])
  })

  it('gives no run for an empty map', () => {
    expect(cacheRuns([])).toEqual([])
  })
})

describe('recordedSlots', () => {
  /* 4 slots over 0..99 s: 25 s each */
  it('marks the slots a programme meets', () => {
    expect(recordedSlots(4, 1000, 99, [[1050, 2000]])).toEqual([false, false, true, true])
    expect(recordedSlots(4, 1000, 99, [[1010, 1030]])).toEqual([true, true, false, false])
  })

  it('marks nothing without a recording, or past the span', () => {
    expect(recordedSlots(4, 1000, 99, [])).toEqual([false, false, false, false])
    expect(recordedSlots(4, 1000, 99, [[500, 900]])).toEqual([false, false, false, false])
  })
})

describe('markerOffset', () => {
  it('puts live at the right edge and the oldest data at the left', () => {
    expect(markerOffset(0, 600)).toBe(1)
    expect(markerOffset(600, 600)).toBe(0)
    expect(markerOffset(150, 600)).toBe(0.75)
  })

  it('clamps a position outside the span to its edge', () => {
    expect(markerOffset(900, 600)).toBe(0)
    expect(markerOffset(-5, 600)).toBe(1)
  })

  it('puts everything at live when there is no span yet', () => {
    expect(markerOffset(10, 0)).toBe(1)
  })

  it('keeps live at the right edge when the cache fills only part of the bar', () => {
    expect(markerOffset(0, 60, 0.25)).toBe(1)
    expect(markerOffset(30, 60, 0.25)).toBe(0.875)
    expect(markerOffset(60, 60, 0.25)).toBe(0.75)
  })
})

describe('fillFraction', () => {
  it('draws a cache younger than its period over its share of it, against the right edge', () => {
    expect(fillFraction(900, 3600)).toBe(0.25)
    expect(fillFraction(0, 3600)).toBe(0)
  })

  it('fills the bar once the period is reached, or without a period', () => {
    expect(fillFraction(3600, 3600)).toBe(1)
    expect(fillFraction(5000, 3600)).toBe(1)
    expect(fillFraction(900, 0)).toBe(1)
  })
})

describe('formatSpan / formatBehind', () => {
  it('reads in seconds, minutes, then hours and minutes', () => {
    expect(formatSpan(35)).toBe('35 s')
    expect(formatSpan(42 * 60 + 10)).toBe('42 min')
    expect(formatSpan(118 * 60)).toBe('1 h 58 min')
  })

  it('calls a position within a few seconds of live "live"', () => {
    expect(formatBehind(1)).toBe('live')
    expect(formatBehind(12 * 60)).toBe('−12 min')
  })
})

describe('locName', () => {
  it('names each location and treats anything else as a gap', () => {
    expect([0, 1, 2, 3, 9].map(locName)).toEqual(['gap', 'ram', 'staging', 'disk', 'gap'])
  })
})

describe('cacheMode, barScale and formatScale', () => {
  const on = {
    enabled: true,
    ram_only: false,
    unlimited: false,
    size_used: 0,
    size_max: 48 * 1048576,
    ram_used: 0,
    ram_max: 16 * 1048576,
    staging_used: 0,
    staging_max: 32 * 1048576,
    keepalive: 15,
    period: 7200,
  }

  it('tells the modes apart', () => {
    expect(cacheMode(on)).toBe('both')
    expect(cacheMode({ ...on, ram_only: true })).toBe('ram-only')
    expect(cacheMode({ ...on, ram_max: 0 })).toBe('disk-only')
    expect(cacheMode({ ...on, enabled: false })).toBe('off')
    expect(cacheMode(null)).toBe('off')
  })

  it('draws every bar against the first of 15, 30, 60, 120 min the longest cache fills to 90 %', () => {
    const min = 60
    expect(barScale([5 * min, 8 * min, 4 * min], 7200)).toBe(15 * min)
    /* on to the next before the edge: 14 min is past nine tenths of 15 */
    expect(barScale([14 * min], 7200)).toBe(30 * min)
    expect(barScale([16 * min, 2 * min], 7200)).toBe(30 * min)
    expect(barScale([45 * min], 7200)).toBe(60 * min)
    expect(barScale([55 * min], 7200)).toBe(120 * min)
    /* no caches yet: the smallest */
    expect(barScale([], 7200)).toBe(15 * min)
  })

  it('goes no further than the period the caches grow to, and on an hour at a time past 2 h', () => {
    const min = 60
    /* a 90 min period: 60, then 90 rather than 120 */
    expect(barScale([70 * min], 90 * min)).toBe(90 * min)
    /* a 10 min period: the period itself */
    expect(barScale([3 * min], 10 * min)).toBe(10 * min)
    /* past 2 h, an hour more at a time */
    expect(barScale([150 * min], 0)).toBe(180 * min)
    expect(barScale([170 * min], 0)).toBe(240 * min)
    expect(barScale([500 * min], 0)).toBe(600 * min)
    expect(barScale([150 * min], 240 * min)).toBe(180 * min)
    expect(barScale([230 * min], 240 * min)).toBe(240 * min)
  })

  it('says the scale in minutes, or in whole hours', () => {
    expect(formatScale(900)).toBe('15 min')
    expect(formatScale(3600)).toBe('1 h')
    expect(formatScale(7200)).toBe('2 h')
    expect(formatScale(5400)).toBe('1 h 30 min')
  })
})

describe('cacheColors', () => {
  const e = (uuid: string, channel: string) => ({ uuid, channel }) as never

  it('gives each cache a colour in the order of the names, the same in every view', () => {
    const a = cacheColors([e('2', 'TVA'), e('1', 'CBC')])
    const b = cacheColors([e('1', 'CBC'), e('2', 'TVA')])
    expect(a.get('1')).toBe(CACHE_PALETTE[0])
    expect(a.get('2')).toBe(CACHE_PALETTE[1])
    expect([...a]).toEqual([...b].reverse().reverse())
  })

  it('keeps to tones from mauve to pink, dark and light in turn', () => {
    const hue = (c: string) => {
      const [r, g, b] = [1, 3, 5].map((i) => Number.parseInt(c.slice(i, i + 2), 16) / 255)
      const max = Math.max(r, g, b)
      const d = max - Math.min(r, g, b)
      if (d === 0) return -1
      let h = (r - g) / d + 4
      if (max === r) h = ((g - b) / d) % 6
      else if (max === g) h = (b - r) / d + 2
      return (h * 60 + 360) % 360
    }
    /* blue, green and yellow say what is cached, recorded and buffered */
    for (const c of CACHE_PALETTE) {
      const h = hue(c)
      expect(h >= 260 && h <= 340, `${c} is not between mauve and pink`).toBe(true)
    }
    const light = (c: string) =>
      [1, 3, 5].map((i) => Number.parseInt(c.slice(i, i + 2), 16)).reduce((a, b) => a + b, 0)
    for (let i = 1; i < CACHE_PALETTE.length; i++)
      expect(
        light(CACHE_PALETTE[i]) > light(CACHE_PALETTE[i - 1]) === (i % 2 === 1),
        `${CACHE_PALETTE[i]} does not stand apart from ${CACHE_PALETTE[i - 1]}`
      ).toBe(true)
  })
})

describe('formatCacheSpan', () => {
  it('says when a cache has reached the maximum period', () => {
    expect(formatCacheSpan(3000, 7200)).toBe('50 min')
    expect(formatCacheSpan(7170, 7200)).toBe('2 h 0 min · max')
    expect(formatCacheSpan(9000, 0)).toBe('2 h 30 min')
  })
})
