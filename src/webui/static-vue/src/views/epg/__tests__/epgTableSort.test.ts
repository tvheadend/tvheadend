// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the EPG Table in-memory sort helpers extracted
 * from TableView.vue. The channelNumber numeric ordering itself
 * lives in utils/channelNumberSort (tested there); here we verify
 * the generic comparator and that applyInMemorySort defers to a
 * supplied column comparator.
 */

import { describe, expect, it } from 'vitest'
import { applyInMemorySort, compareSortValues } from '../epgTableSort'
import { channelNumberCompare } from '@/utils/channelNumberSort'

const numbers = (rows: readonly { channelNumber?: string | null }[]) =>
  rows.map((r) => r.channelNumber)

describe('compareSortValues (generic columns)', () => {
  it('compares numbers numerically', () => {
    expect(compareSortValues(2, 10, 1)).toBeLessThan(0)
  })
  it('compares text with localeCompare', () => {
    expect(compareSortValues('ARD', 'ZDF', 1)).toBeLessThan(0)
  })
  it('would order numeric strings lexically (why channelNumber needs a comparator)', () => {
    expect(compareSortValues('10', '2', 1)).toBeLessThan(0)
  })
  it('respects direction and null-first contract', () => {
    expect(compareSortValues('a', 'b', -1)).toBeGreaterThan(0)
    expect(compareSortValues(null, 'x', 1)).toBeLessThan(0)
  })
})

describe('applyInMemorySort', () => {
  it('uses the supplied comparator for channelNumber (ascending)', () => {
    const rows = [
      { channelNumber: '1' },
      { channelNumber: '10' },
      { channelNumber: '101' },
      { channelNumber: '11' },
      { channelNumber: '2' },
      { channelNumber: '20' },
    ]
    const out = applyInMemorySort(rows, 'channelNumber', 1, channelNumberCompare)
    expect(out.mutated).toBe(true)
    expect(numbers(out.rows)).toEqual(['1', '2', '10', '11', '20', '101'])
  })

  it('orders major.minor via the comparator, descending', () => {
    const rows = [
      { channelNumber: '10.10' },
      { channelNumber: '10.2' },
      { channelNumber: '10.9' },
    ]
    const out = applyInMemorySort(rows, 'channelNumber', -1, channelNumberCompare)
    expect(numbers(out.rows)).toEqual(['10.10', '10.9', '10.2'])
  })

  it('falls back to the generic comparator when none is supplied', () => {
    const rows = [{ channelName: 'ZDF' }, { channelName: 'ARD' }]
    const out = applyInMemorySort(rows, 'channelName', 1)
    expect(out.rows.map((r) => r.channelName)).toEqual(['ARD', 'ZDF'])
  })

  it('skips the copy+sort for the default start-ASC ordering', () => {
    const rows = [{ start: 3 }, { start: 1 }, { start: 2 }]
    const out = applyInMemorySort(rows, 'start', 1)
    expect(out.mutated).toBe(false)
    expect(out.rows).toBe(rows)
  })

  it('is a no-op when no sort key is set', () => {
    const rows = [{ channelNumber: '2' }]
    const out = applyInMemorySort(rows, null, 1)
    expect(out.mutated).toBe(false)
    expect(out.rows).toBe(rows)
  })
})
