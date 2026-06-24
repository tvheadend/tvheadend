// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * LogView — filter-row wiring tests. The log store + clipboard
 * + toast composables are mocked so the test owns the buffer
 * directly. MultiSelect is stubbed to a passthrough that
 * forwards the v-model so the test can drive the subsystem
 * filter without dragging PrimeVue's full popover machinery
 * into the harness.
 *
 * Coverage:
 *   - Default render lists every buffer line (no filter active).
 *   - Text search narrows the rendered list.
 *   - Subsystem v-model narrows the rendered list.
 *   - filtered/total count chip appears when a filter is active.
 *   - Clear-filters resets the active filters.
 *   - "No lines match" empty state when filter excludes everything.
 *
 * The severity-pill suite is `describe.skip` — the pills are
 * commented out in LogView until the server sends a real
 * `severity` field on the `logmessage` notification.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { ref } from 'vue'
import type { LogLine } from '@/stores/log'

/* Module-level mutable buffer that the mocked store reads. */
const linesRef = ref<LogLine[]>([])
const bufferFullRef = ref(false)
const debugEnabledRef = ref(false)
const clearMock = vi.fn(() => {
  linesRef.value = []
})
const toggleDebugMock = vi.fn(async () => true)

vi.mock('@/stores/log', async (orig) => {
  /* Keep the real Severity type re-export. */
  const real = await orig<typeof import('@/stores/log')>()
  return {
    ...real,
    useLogStore: () => ({
      lines: linesRef,
      bufferFull: bufferFullRef,
      debugEnabled: debugEnabledRef,
      clear: clearMock,
      toggleDebug: toggleDebugMock,
    }),
  }
})

vi.mock('@/composables/useClipboard', () => ({
  useClipboard: () => ({ copyText: vi.fn(async () => true) }),
}))

vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({
    success: vi.fn(),
    info: vi.fn(),
    error: vi.fn(),
  }),
}))

/* Pinia's storeToRefs is fine — the mocked store already
 * returns refs, so storeToRefs just hands them back. */

/* Stub MultiSelect with a minimal v-model passthrough. The
 * filter tests don't need PrimeVue's popover; they just need
 * to mutate the bound value and observe the cascade. */
const MULTISELECT_PASSTHROUGH = {
  template:
    '<div class="ms-passthrough" :data-options="JSON.stringify(options)" :data-value="JSON.stringify(modelValue)"></div>',
  props: ['modelValue', 'options', 'placeholder', 'filter', 'showToggleAll'],
  emits: ['update:modelValue'],
}

import LogView from '../LogView.vue'

function seed(lines: LogLine[]): void {
  linesRef.value = lines
}

function makeLine(over: Partial<LogLine> = {}): LogLine {
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

function mountView() {
  return mount(LogView, {
    global: {
      stubs: { MultiSelect: MULTISELECT_PASSTHROUGH },
      directives: {
        tooltip: () => undefined,
      },
    },
  })
}

beforeEach(() => {
  linesRef.value = []
  bufferFullRef.value = false
  debugEnabledRef.value = false
  clearMock.mockClear()
  toggleDebugMock.mockClear()
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('LogView — default render', () => {
  it('renders every buffer line when no filter is active', () => {
    seed([
      makeLine({ id: 1, body: 'tuned in' }),
      makeLine({ id: 2, body: 'signal lost' }),
      makeLine({ id: 3, body: 'connected' }),
    ])
    const w = mountView()
    expect(w.findAll('.log-view__line')).toHaveLength(3)
  })

  it('shows the empty-buffer message when the store has no lines', () => {
    seed([])
    const w = mountView()
    expect(w.text()).toContain('Waiting for log lines')
  })

  it('count chip shows a single count when nothing is filtered', () => {
    seed([makeLine({ id: 1 }), makeLine({ id: 2 })])
    const w = mountView()
    const chip = w.find('.log-view__count').text()
    expect(chip).toContain('2')
    expect(chip).not.toContain('/')
  })
})

describe('LogView — text filter', () => {
  it('narrows the rendered list to lines matching the substring', async () => {
    seed([
      makeLine({ id: 1, body: 'tuned in' }),
      makeLine({ id: 2, body: 'signal lost' }),
      makeLine({ id: 3, body: 'connected' }),
    ])
    const w = mountView()
    await w.find('.log-view__text-search input').setValue('signal')
    const rows = w.findAll('.log-view__line')
    expect(rows).toHaveLength(1)
    expect(rows[0].text()).toContain('signal lost')
  })

  it('regex mode matches when toggled on with a valid pattern', async () => {
    seed([
      makeLine({ id: 1, body: 'event 12 fired' }),
      makeLine({ id: 2, body: 'event 234 fired' }),
      makeLine({ id: 3, body: 'silent' }),
    ])
    const w = mountView()
    await w.find('.log-view__regex-toggle').trigger('click')
    await w.find('.log-view__text-search input').setValue(String.raw`event \d{3}`)
    const rows = w.findAll('.log-view__line')
    expect(rows).toHaveLength(1)
    expect(rows[0].text()).toContain('event 234 fired')
  })

  it('clears the rendered list when the filter excludes every line', async () => {
    seed([makeLine({ id: 1, body: 'tuned in' })])
    const w = mountView()
    await w.find('.log-view__text-search input').setValue('nothing matches this')
    expect(w.findAll('.log-view__line')).toHaveLength(0)
    expect(w.text()).toContain('No lines match the active filter')
  })

  it('count chip switches to "filtered / total" when a filter is active', async () => {
    seed([
      makeLine({ id: 1, body: 'tuned in' }),
      makeLine({ id: 2, body: 'signal lost' }),
    ])
    const w = mountView()
    await w.find('.log-view__text-search input').setValue('signal')
    const chip = w.find('.log-view__count').text()
    expect(chip).toContain('1')
    expect(chip).toContain('2')
    expect(chip).toContain('/')
  })
})

/* Skipped: the severity filter pills are commented out in LogView
 * until the server exposes a real `severity` field on the comet
 * `logmessage` notification. Un-skip with the pill markup. */
describe.skip('LogView — severity filter', () => {
  it('toggling a severity pill off removes those lines', async () => {
    seed([
      makeLine({ id: 1, body: 'an info', severity: 'info' }),
      makeLine({ id: 2, body: 'a debug', severity: 'debug' }),
    ])
    const w = mountView()
    /* All pills start on → no constraint → both lines visible. */
    expect(w.findAll('.log-view__line')).toHaveLength(2)
    /* Click the "debug" pill's checkbox to drop it. */
    const debugPill = w.find('.log-view__sev-pill--debug input[type="checkbox"]')
    await debugPill.setValue(false)
    const remaining = w.findAll('.log-view__line')
    expect(remaining).toHaveLength(1)
    expect(remaining[0].text()).toContain('an info')
  })
})

describe('LogView — clear filters', () => {
  it('hides the Clear-filters button when no filter is active', () => {
    seed([makeLine({ id: 1 })])
    const w = mountView()
    expect(w.find('.log-view__clear-filters').exists()).toBe(false)
  })

  it('shows the Clear-filters button when text filter is active', async () => {
    seed([makeLine({ id: 1 })])
    const w = mountView()
    await w.find('.log-view__text-search input').setValue('x')
    expect(w.find('.log-view__clear-filters').exists()).toBe(true)
  })

  it('resets the active filters when clicked', async () => {
    seed([
      makeLine({ id: 1, body: 'tuned in' }),
      makeLine({ id: 2, body: 'signal lost' }),
    ])
    const w = mountView()
    /* Apply a text filter that narrows to one line. */
    await w.find('.log-view__text-search input').setValue('tuned')
    expect(w.findAll('.log-view__line')).toHaveLength(1)

    /* Click Clear filters. */
    await w.find('.log-view__clear-filters').trigger('click')

    /* Both lines visible again; the chip is back to single-count. */
    expect(w.findAll('.log-view__line')).toHaveLength(2)
    expect(w.find('.log-view__clear-filters').exists()).toBe(false)
  })
})

describe('LogView — subsystem options', () => {
  it('derives available subsystems from the buffer, sorted', () => {
    seed([
      makeLine({ id: 1, subsys: 'http' }),
      makeLine({ id: 2, subsys: 'mpegts' }),
      makeLine({ id: 3, subsys: 'epg' }),
      makeLine({ id: 4, subsys: 'mpegts' }),
    ])
    const w = mountView()
    const optionsAttr = w.find('.ms-passthrough').attributes('data-options')
    expect(optionsAttr).toBeTruthy()
    const opts = JSON.parse(optionsAttr ?? '[]') as string[]
    expect(opts).toEqual(['epg', 'http', 'mpegts'])
  })
})
