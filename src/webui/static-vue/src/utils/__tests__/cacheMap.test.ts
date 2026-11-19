// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
import { describe, expect, it } from 'vitest'
import {
  CACHE_PALETTE,
  barFill,
  cacheColors,
  budgetScale,
  cacheMode,
  cacheRuns,
  recordedSlots,
  fillFraction,
  formatBehind,
  formatSpan,
  locName,
  markerOffset,
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

describe('cacheMode, budgetScale and barFill', () => {
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

  it('scales by the timeshift size, not when it is unlimited or the cache is off', () => {
    expect(budgetScale(on)).toBe(48 * 1048576)
    expect(budgetScale({ ...on, unlimited: true })).toBe(0)
    expect(budgetScale({ ...on, enabled: false })).toBe(0)
  })

  it('fills the bar by whichever limit the cache is nearer', () => {
    /* a minute of a two-hour period, but a third of the budget */
    expect(barFill(60, 16, 7200, 48)).toBeCloseTo(1 / 3)
    /* an hour of the period, a tenth of the budget */
    expect(barFill(3600, 4.8, 7200, 48)).toBeCloseTo(0.5)
    /* bounded by neither: the whole bar */
    expect(barFill(60, 16, 0, 0)).toBe(1)
    expect(barFill(60, 96, 7200, 48)).toBe(1)
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

  it('leaves out yellow and green, kept for the buffer and recordings', () => {
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
    for (const c of CACHE_PALETTE) {
      const h = hue(c)
      expect(h >= 40 && h <= 75, `${c} is yellow`).toBe(false)
      expect(h >= 85 && h <= 160, `${c} is green`).toBe(false)
    }
  })
})
