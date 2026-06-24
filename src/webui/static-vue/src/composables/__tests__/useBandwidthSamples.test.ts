// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- Test file uses
 * multiple harness Vue components to drive different fixture
 * scenarios; extracting each to a separate file would balloon
 * the test footprint without making the stubs easier to maintain. */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h, ref, type Ref } from 'vue'
import { useBandwidthSamples } from '../useBandwidthSamples'

interface InputRow extends Record<string, unknown> {
  uuid: string
  bps: number
}

interface SubRow extends Record<string, unknown> {
  id: number
  in: number
  out: number
}

function mountHarness<Row extends Record<string, unknown>>(opts: {
  initialRows: Row[]
  keyField: keyof Row
  metrics: readonly string[]
  windowSec: number
}): {
  rows: Ref<Row[]>
  paused: Ref<boolean>
  api: ReturnType<typeof useBandwidthSamples<Row>>
  unmount: () => void
} {
  const rows = ref(opts.initialRows) as Ref<Row[]>
  const paused = ref(false)
  const windowSec = ref(opts.windowSec)
  let api!: ReturnType<typeof useBandwidthSamples<Row>>
  const Harness = defineComponent({
    setup() {
      api = useBandwidthSamples<Row>({
        rows: () => rows.value,
        keyField: opts.keyField,
        metrics: opts.metrics,
        windowSec,
        paused,
      })
      return () => h('div')
    },
  })
  const w = mount(Harness)
  return { rows, paused, api, unmount: () => w.unmount() }
}

beforeEach(() => {
  vi.useFakeTimers()
  vi.setSystemTime(new Date('2026-05-13T10:00:00.000Z'))
})

afterEach(() => {
  vi.useRealTimers()
})

describe('useBandwidthSamples — single-metric (streams)', () => {
  it('captures an initial sample on mount', () => {
    const { api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    const samples = api.samples.value.get('a')?.get('bps')
    expect(samples).toHaveLength(1)
    expect(samples?.[0].v).toBe(1_000_000)
    unmount()
  })

  it('appends a new sample after the dedupe window when the row mutates', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    vi.advanceTimersByTime(1000)
    rows.value = [{ uuid: 'a', bps: 2_000_000 }]
    await Promise.resolve()
    const samples = api.samples.value.get('a')?.get('bps') ?? []
    expect(samples).toHaveLength(2)
    expect(samples[1].v).toBe(2_000_000)
    unmount()
  })

  it('deduplicates mutations inside the 500 ms gate', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    vi.advanceTimersByTime(100)
    rows.value = [{ uuid: 'a', bps: 1_100_000 }]
    await Promise.resolve()
    const samples = api.samples.value.get('a')?.get('bps') ?? []
    expect(samples).toHaveLength(1)
    /* Last value wins — the latest mutation overwrites the
     * existing sample rather than appending a near-duplicate. */
    expect(samples[0].v).toBe(1_100_000)
    unmount()
  })

  it('trims samples beyond the windowSec horizon', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 5,
    })
    /* Push 10 samples 1s apart. */
    for (let i = 1; i <= 10; i++) {
      vi.advanceTimersByTime(1000)
      rows.value = [{ uuid: 'a', bps: 1_000_000 + i }]
      await Promise.resolve()
    }
    const samples = api.samples.value.get('a')?.get('bps') ?? []
    expect(samples.length).toBeLessThanOrEqual(6) /* 5s window plus the current edge */
    /* Oldest retained sample value must be within the window. */
    const now = Date.now()
    for (const s of samples) {
      expect(now - s.t).toBeLessThanOrEqual(5000)
    }
    unmount()
  })

  it('drops a row from the buffers when it leaves the selection', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [
        { uuid: 'a', bps: 1_000_000 },
        { uuid: 'b', bps: 2_000_000 },
      ],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    vi.advanceTimersByTime(1000)
    rows.value = [{ uuid: 'a', bps: 1_500_000 }]
    await Promise.resolve()
    expect(api.samples.value.has('a')).toBe(true)
    expect(api.samples.value.has('b')).toBe(false)
    unmount()
  })

  it('suspends sampling when paused', async () => {
    const { rows, paused, api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    const beforePauseCount = api.samples.value.get('a')?.get('bps')?.length ?? 0
    paused.value = true
    vi.advanceTimersByTime(1000)
    rows.value = [{ uuid: 'a', bps: 2_000_000 }]
    await Promise.resolve()
    /* Buffer unchanged while paused. */
    expect(api.samples.value.get('a')?.get('bps')?.length ?? 0).toBe(beforePauseCount)
    unmount()
  })

  it('clear() wipes every buffer', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [
        { uuid: 'a', bps: 1_000_000 },
        { uuid: 'b', bps: 2_000_000 },
      ],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    vi.advanceTimersByTime(1000)
    rows.value = [
      { uuid: 'a', bps: 1_500_000 },
      { uuid: 'b', bps: 2_500_000 },
    ]
    await Promise.resolve()
    expect(api.samples.value.size).toBe(2)
    api.clear()
    expect(api.samples.value.size).toBe(0)
    unmount()
  })

  it('currentValues reflects the latest sample per row+metric', async () => {
    const { rows, api, unmount } = mountHarness<InputRow>({
      initialRows: [{ uuid: 'a', bps: 1_000_000 }],
      keyField: 'uuid',
      metrics: ['bps'],
      windowSec: 60,
    })
    vi.advanceTimersByTime(1000)
    rows.value = [{ uuid: 'a', bps: 3_500_000 }]
    await Promise.resolve()
    expect(api.currentValues.value.get('a')?.get('bps')).toBe(3_500_000)
    unmount()
  })
})

describe('useBandwidthSamples — multi-metric (subscriptions)', () => {
  it('captures `in` AND `out` per row', () => {
    const { api, unmount } = mountHarness<SubRow>({
      initialRows: [{ id: 12, in: 800_000, out: 750_000 }],
      keyField: 'id',
      metrics: ['in', 'out'],
      windowSec: 60,
    })
    const row = api.samples.value.get(12)
    expect(row?.get('in')?.[0].v).toBe(800_000)
    expect(row?.get('out')?.[0].v).toBe(750_000)
    unmount()
  })

  it('aggregate sums each metric across all rows per-second-bucket', () => {
    const { api, unmount } = mountHarness<SubRow>({
      initialRows: [
        { id: 12, in: 800_000, out: 750_000 },
        { id: 13, in: 500_000, out: 480_000 },
      ],
      keyField: 'id',
      metrics: ['in', 'out'],
      windowSec: 60,
    })
    const aggIn = api.aggregate.value.get('in')
    const aggOut = api.aggregate.value.get('out')
    expect(aggIn).toHaveLength(1)
    expect(aggIn?.[0].v).toBe(1_300_000)
    expect(aggOut?.[0].v).toBe(1_230_000)
    unmount()
  })
})

describe('useBandwidthSamples — windowSec changes', () => {
  it('shrinking the window drops out-of-horizon samples immediately', async () => {
    /* Build up 10 samples at 1s intervals with a 60 s window. */
    const rows = ref<InputRow[]>([{ uuid: 'a', bps: 1_000_000 }]) as Ref<InputRow[]>
    const paused = ref(false)
    const windowSec = ref(60)
    let api!: ReturnType<typeof useBandwidthSamples<InputRow>>
    const Harness = defineComponent({
      setup() {
        api = useBandwidthSamples<InputRow>({
          rows: () => rows.value,
          keyField: 'uuid',
          metrics: ['bps'],
          windowSec,
          paused,
        })
        return () => h('div')
      },
    })
    const w = mount(Harness)
    for (let i = 1; i <= 10; i++) {
      vi.advanceTimersByTime(1000)
      rows.value = [{ uuid: 'a', bps: 1_000_000 + i }]
      await Promise.resolve()
    }
    expect(api.samples.value.get('a')?.get('bps')?.length).toBeGreaterThan(5)
    /* Shrink window to 3s. */
    windowSec.value = 3
    await Promise.resolve()
    const trimmed = api.samples.value.get('a')?.get('bps') ?? []
    expect(trimmed.length).toBeLessThanOrEqual(4)
    w.unmount()
  })
})
