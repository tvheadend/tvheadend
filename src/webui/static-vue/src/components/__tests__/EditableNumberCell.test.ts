// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EditableNumberCell tests — covers the three display states
 * (numbered / unnumbered / duplicate-flagged) and verifies that
 * the duplicate badge tracks the injected reactive count map.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount, type VueWrapper } from '@vue/test-utils'
import { ref } from 'vue'
import EditableNumberCell from '../EditableNumberCell.vue'
import type { ColumnDef } from '@/types/column'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({
    t: (s: string, ...args: Array<string | number>) =>
      s.replace(/\{(\d+)\}/g, (_m, i) => String(args[Number(i)] ?? _m)),
  }),
}))

const COL: ColumnDef = { field: 'number' }

let wrapper: VueWrapper

function mountCell(opts: {
  value: unknown
  dupCounts?: Map<string, number> | null
}): VueWrapper {
  const provides: Record<string, unknown> = {}
  if (opts.dupCounts !== undefined) {
    provides.numberDuplicateCounts = ref(opts.dupCounts)
  }
  wrapper = mount(EditableNumberCell, {
    props: {
      value: opts.value,
      row: { uuid: 'r1' },
      col: COL,
    },
    global: { provide: provides },
  })
  return wrapper
}

afterEach(() => {
  wrapper?.unmount()
})

describe('EditableNumberCell — display states', () => {
  it('renders an integer number as plain text', () => {
    mountCell({ value: 5 })
    expect(wrapper.find('.editable-number-cell__value').text()).toBe('5')
    expect(wrapper.classes()).not.toContain('editable-number-cell--unset')
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(false)
  })

  it('renders a dotted minor number verbatim (5.1, 100.25)', () => {
    mountCell({ value: 5.1 })
    expect(wrapper.find('.editable-number-cell__value').text()).toBe('5.1')
    mountCell({ value: '100.25' })
    expect(wrapper.find('.editable-number-cell__value').text()).toBe('100.25')
  })

  it('collapses an integer-valued number to plain "5" via the isInteger gate', () => {
    /* Stringified "5" via Number — covers wire shapes that parse
     * to an integer with no fractional component. */
    mountCell({ value: Number('5') })
    expect(wrapper.find('.editable-number-cell__value').text()).toBe('5')
  })

  it('renders "—" for 0 and applies the unset modifier class', () => {
    mountCell({ value: 0 })
    expect(wrapper.find('.editable-number-cell__value').text()).toBe('—')
    expect(wrapper.classes()).toContain('editable-number-cell--unset')
  })

  it('renders "—" for null / undefined / "" / NaN', () => {
    for (const v of [null, undefined, '', 'not-a-number']) {
      mountCell({ value: v })
      expect(wrapper.find('.editable-number-cell__value').text()).toBe('—')
      expect(wrapper.classes()).toContain('editable-number-cell--unset')
    }
  })
})

describe('EditableNumberCell — duplicate detection', () => {
  it('shows the warning badge when count > 1', () => {
    mountCell({
      value: 5,
      dupCounts: new Map([['5', 3]]),
    })
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(true)
    expect(
      wrapper.find('.editable-number-cell__warn').attributes('aria-label'),
    ).toBe('Shares this number with 2 other channels')
  })

  it('singular "1 other channel" copy when count is exactly 2', () => {
    mountCell({
      value: 7,
      dupCounts: new Map([['7', 2]]),
    })
    expect(
      wrapper.find('.editable-number-cell__warn').attributes('aria-label'),
    ).toBe('Shares this number with 1 other channel')
  })

  it('no badge when count is 1 (this row is the only one with that number)', () => {
    mountCell({
      value: 5,
      dupCounts: new Map([['5', 1]]),
    })
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(false)
  })

  it('no badge when this row is unnumbered (multiple "—" rows are NOT duplicates)', () => {
    /* Twenty just-scanned channels all at 0 mustn't all light
     * up as duplicates of each other — they're "no number yet",
     * not "all share number zero". */
    mountCell({
      value: 0,
      dupCounts: new Map([['0', 20]]),
    })
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(false)
  })

  it('no badge when no duplicate map is provided (cell mounted outside the drawer)', () => {
    mountCell({ value: 5 })
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(false)
  })

  it('badge appears when the dupCounts map is later updated reactively', async () => {
    const map = ref<Map<string, number>>(new Map([['5', 1]]))
    wrapper = mount(EditableNumberCell, {
      props: { value: 5, row: { uuid: 'r1' }, col: COL },
      global: { provide: { numberDuplicateCounts: map } },
    })
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(false)
    /* Another row commits to number 5 — map updates → badge appears. */
    map.value = new Map([['5', 2]])
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.editable-number-cell__warn').exists()).toBe(true)
  })
})
