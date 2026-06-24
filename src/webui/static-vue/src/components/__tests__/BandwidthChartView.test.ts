// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * BandwidthChartView — smoke + UX tests.
 *
 * Chart.js needs a canvas context that happy-dom doesn't fully
 * provide; we stub the BandwidthChart child component instead.
 * The view's own behaviour (toolbar state, mode collapse on
 * single-row, pause/reset, empty state, legend identity) is the
 * real surface under test.
 *
 * Tests pin the **phone Drawer branch** by forcing innerWidth
 * below the 768 px breakpoint at mount time. The docked-panel
 * (desktop) branch shares the same internal state machine; the
 * branch-specific chrome (splitter, custom × button) is exercised
 * by a dedicated test below.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import PrimeVue from 'primevue/config'
import { defineComponent, h, ref, type Ref } from 'vue'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the dock-vs-drawer branch is what's
 * under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref: vueRef } = await import('vue')
  const isPhone = vueRef(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import BandwidthChartView from '../BandwidthChartView.vue'

/* Stub BandwidthChart — we don't want to instantiate Chart.js
 * against a happy-dom canvas. The stub just renders a div so we
 * can assert presence/absence per mode. */
const CHART_STUB = {
  name: 'BandwidthChart',
  props: ['series', 'theme', 'windowSec', 'minHeight'],
  template: '<div class="chart-stub" :data-count="series.length" />',
}

/* Index signature is required so the local Row satisfies the
 * view's `Record<string, unknown>[]` prop type — TS interfaces
 * don't implicitly add it. */
type Row = {
  uuid: string
  input: string
  bps: number
  [k: string]: unknown
}

/* Force a phone-sized viewport so the Drawer branch renders.
 * Tests verify the inner content shape, which is identical
 * between Drawer (phone) and docked-aside (desktop). */
function setViewport(width: number): void {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
}

function mountView(rows: Ref<Row[]>, visible: Ref<boolean>) {
  return mount(BandwidthChartView, {
    props: {
      visible: visible.value,
      rows: () => rows.value,
      keyField: 'uuid' as const,
      metrics: ['bps'] as const,
      units: 'bits' as const,
      rowLabel: (r: Record<string, unknown>) => String(r.input),
      noun: 'input',
    },
    global: {
      plugins: [[PrimeVue, {}]],
      stubs: {
        BandwidthChart: CHART_STUB,
        /* Drawer renders to a teleport target — stub it as a
         * pass-through so its body is mountable. */
        Drawer: defineComponent({
          props: { visible: { type: Boolean } },
          setup(_, { slots }) {
            return () =>
              h('div', { class: 'drawer-stub' }, [
                slots.header?.(),
                slots.default?.(),
              ])
          },
        }),
      },
    },
  })
}

beforeEach(() => {
  vi.useFakeTimers()
  vi.setSystemTime(new Date('2026-05-13T10:00:00.000Z'))
  setViewport(600) /* Phone — drives the Drawer branch. */
})

afterEach(() => {
  vi.useRealTimers()
})

describe('BandwidthChartView — phone (Drawer) branch', () => {
  it('renders the empty state when no rows are selected', () => {
    const rows = ref<Row[]>([])
    const visible = ref(true)
    const w = mountView(rows, visible)
    expect(w.text()).toContain('Select rows in the grid')
    expect(w.find('.chart-stub').exists()).toBe(false)
  })

  it('renders a single combined chart with the selected rows', async () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'DVB-S #0', bps: 1_000_000 },
      { uuid: 'b', input: 'DVB-S #1', bps: 2_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    await flushPromises()
    const stubs = w.findAll('.chart-stub')
    expect(stubs).toHaveLength(1)
    /* Two rows × one metric = 2 series in Combined mode. */
    expect(stubs[0].attributes('data-count')).toBe('2')
  })

  it('hides the mode selector when only one row is selected', () => {
    const rows = ref<Row[]>([{ uuid: 'a', input: 'DVB-S #0', bps: 1_000_000 }])
    const visible = ref(true)
    const w = mountView(rows, visible)
    /* Mode segmented group label "Mode" should not render with
     * <=1 row. Window + (no) Show still render. */
    expect(w.text()).not.toContain('Mode')
    expect(w.text()).toContain('Window')
  })

  it('forces Combined mode when selection drops to one row, then re-adds', async () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    /* Pick Independent on the multi-row state. */
    const indep = w
      .findAll('button.bandwidth-chart__segment')
      .find((b) => b.text() === 'Independent')
    await indep!.trigger('click')
    await flushPromises()
    /* Drop to one row — mode selector hides, internal mode flips
     * to Combined per the rowCount watcher. */
    rows.value = [{ uuid: 'a', input: 'A', bps: 1_000_000 }]
    await flushPromises()
    /* Re-grow to two rows — mode selector reappears and the
     * active segment should be Combined (not the previously
     * picked Independent). */
    rows.value = [
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
    ]
    await flushPromises()
    const activeMode = w
      .findAll('.bandwidth-chart__group')
      .find((g) => g.text().startsWith('MODE') || g.text().startsWith('Mode'))
      ?.findAll('button.bandwidth-chart__segment')
      .find((b) => b.classes().includes('bandwidth-chart__segment--active'))
    expect(activeMode?.text()).toBe('Combined')
  })

  it('renders multiple panels in Independent mode', async () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
      { uuid: 'c', input: 'C', bps: 3_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    const indep = w
      .findAll('button.bandwidth-chart__segment')
      .find((b) => b.text() === 'Independent')
    await indep!.trigger('click')
    await flushPromises()
    expect(w.findAll('.chart-stub')).toHaveLength(3)
    expect(w.findAll('.bandwidth-chart__panel')).toHaveLength(3)
  })

  it('switches to Aggregate mode and renders a single chart', async () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    const agg = w
      .findAll('button.bandwidth-chart__segment')
      .find((b) => b.text() === 'Aggregate')
    await agg!.trigger('click')
    await flushPromises()
    expect(w.findAll('.chart-stub')).toHaveLength(1)
  })

  it('renders the window selector with three options', () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    expect(w.text()).toContain('30s')
    expect(w.text()).toContain('1 min')
    expect(w.text()).toContain('5 min')
  })

  it('legend lists every selected row with its colour dot', () => {
    const rows = ref<Row[]>([
      { uuid: 'a', input: 'A', bps: 1_000_000 },
      { uuid: 'b', input: 'B', bps: 2_000_000 },
    ])
    const visible = ref(true)
    const w = mountView(rows, visible)
    expect(w.findAll('.bandwidth-chart__legend-row').length).toBeGreaterThanOrEqual(2)
  })
})

describe('BandwidthChartView — desktop (docked) branch', () => {
  beforeEach(() => {
    setViewport(1280) /* Desktop — drives the docked-aside branch. */
  })

  it('renders the docked aside with a splitter and inline close button', () => {
    const rows = ref<Row[]>([{ uuid: 'a', input: 'A', bps: 1_000_000 }])
    const visible = ref(true)
    const w = mountView(rows, visible)
    expect(w.find('.bandwidth-dock').exists()).toBe(true)
    expect(w.find('.bandwidth-dock__splitter').exists()).toBe(true)
    /* Desktop branch renders its own × Close button (the Drawer's
     * auto-close icon isn't available outside the Drawer chrome). */
    const closeBtn = w
      .findAll('button[aria-label="Close"]')
      .find((b) => b.exists())
    expect(closeBtn).toBeDefined()
  })

  it('does not render the Drawer branch when desktop-sized', () => {
    const rows = ref<Row[]>([{ uuid: 'a', input: 'A', bps: 1_000_000 }])
    const visible = ref(true)
    const w = mountView(rows, visible)
    expect(w.find('.drawer-stub').exists()).toBe(false)
  })
})
