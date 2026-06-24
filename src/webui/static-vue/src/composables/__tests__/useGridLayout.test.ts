// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- Test file uses
 * multiple harness Vue components to drive different fixture
 * scenarios; extracting each to a separate file would balloon
 * the test footprint without making the stubs easier to maintain. */

/*
 * useGridLayout — persistence + reactivity tests.
 *
 * The composable is the shared layout layer behind StatusGrid and
 * IdnodeGrid. Tests pin the four persistence axes (sort, hidden
 * columns, order, widths) plus the cascade in `isHidden`, the
 * default-elision logic in `setSort`/`setColumnOrder`, and the
 * `reset` / `isAtDefaults` behaviour.
 *
 * The CSS-injection branch (gridKey-driven) is exercised only when
 * gridKey is set; lifecycle (onMounted/onBeforeUnmount) requires a
 * harness component for those assertions.
 */

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, ref, h } from 'vue'
import { useGridLayout, type LayoutBlob } from '../useGridLayout'
import type { ColumnDef } from '@/types/column'

const KEY = 'tvh-test-grid'

const COLS: ColumnDef[] = [
  { field: 'a', label: 'A', sortable: true },
  { field: 'b', label: 'B', sortable: true },
  { field: 'c', label: 'C', sortable: true, hiddenByDefault: true },
  { field: 'd', label: 'D', sortable: true, width: 200 },
]

beforeEach(() => {
  localStorage.clear()
})

afterEach(() => {
  localStorage.clear()
})

/* Pure-state harness — mounts the composable without invoking the
 * width-injection lifecycle. Tests that need DOM widths pass
 * gridKey via the second harness below. */
function setup(initialBlob?: LayoutBlob, defaultSort?: { field: string; dir: 'ASC' | 'DESC' }) {
  if (initialBlob) localStorage.setItem(KEY, JSON.stringify(initialBlob))
  const cols = ref<ColumnDef[]>([...COLS])
  let api!: ReturnType<typeof useGridLayout>
  const Harness = defineComponent({
    setup() {
      api = useGridLayout({
        storageKey: KEY,
        columns: () => cols.value,
        defaultSort,
      })
      return () => h('div')
    },
  })
  const w = mount(Harness)
  return { api, cols, wrapper: w }
}

describe('useGridLayout — defaults', () => {
  it('reads as empty when localStorage is unset', () => {
    const { api } = setup()
    expect(api.sort.value).toBeNull()
    expect(api.isAtDefaults.value).toBe(true)
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['a', 'b', 'c', 'd'])
  })

  it('returns defaultSort.field+order when no sort is persisted', () => {
    const { api } = setup(undefined, { field: 'a', dir: 'DESC' })
    expect(api.sort.value).toEqual({ field: 'a', order: -1 })
    /* defaultSort doesn't count as a "user pick" for isAtDefaults. */
    expect(api.isAtDefaults.value).toBe(true)
  })

  it('returns null sort with no defaultSort and no persisted sort', () => {
    const { api } = setup()
    expect(api.sort.value).toBeNull()
  })
})

describe('useGridLayout — sort persistence', () => {
  it('persists a non-default pick', () => {
    const { api } = setup(undefined, { field: 'a', dir: 'ASC' })
    api.setSort('b', 'DESC')
    expect(api.sort.value).toEqual({ field: 'b', order: -1 })
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.sort).toEqual({ field: 'b', dir: 'DESC' })
  })

  it('drops the slot when the pick matches defaultSort', () => {
    const { api } = setup({ sort: { field: 'b', dir: 'DESC' } }, { field: 'a', dir: 'ASC' })
    api.setSort('a', 'ASC')
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.sort).toBeUndefined()
    /* Fallback to defaultSort. */
    expect(api.sort.value).toEqual({ field: 'a', order: 1 })
  })

  it('clearSort drops the slot', () => {
    const { api } = setup({ sort: { field: 'b', dir: 'DESC' } })
    api.clearSort()
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.sort).toBeUndefined()
  })
})

describe('useGridLayout — column visibility', () => {
  it('isHidden returns hiddenByDefault when no user pref is set', () => {
    const { api } = setup()
    expect(api.isHidden(COLS[0])).toBe(false)
    expect(api.isHidden(COLS[2])).toBe(true) /* c has hiddenByDefault */
  })

  it('user pref wins over hiddenByDefault', () => {
    const { api } = setup({ cols: { c: { hidden: false } } })
    expect(api.isHidden(COLS[2])).toBe(false)
  })

  it('setColumnHidden persists', () => {
    const { api } = setup()
    api.setColumnHidden('a', true)
    expect(api.isHidden(COLS[0])).toBe(true)
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.cols.a.hidden).toBe(true)
  })

  it('setColumnHidden preserves an existing width on the same field', () => {
    const { api } = setup({ cols: { a: { width: 240 } } })
    api.setColumnHidden('a', true)
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.cols.a).toEqual({ width: 240, hidden: true })
  })
})

describe('useGridLayout — column widths', () => {
  it('setColumnWidth persists and feeds columnWidths', () => {
    const { api } = setup()
    api.setColumnWidth('a', 180)
    expect(api.columnWidths.value.get('a')).toBe(180)
    expect(api.isWidthCustom('a')).toBe(true)
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.cols.a.width).toBe(180)
  })

  it('clearColumnWidth drops the width but keeps hidden state intact', () => {
    const { api } = setup({ cols: { a: { width: 180, hidden: true } } })
    api.clearColumnWidth('a')
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.cols.a).toEqual({ hidden: true })
    expect(api.isWidthCustom('a')).toBe(false)
    expect(api.isHidden(COLS[0])).toBe(true)
  })

  it('clearColumnWidth removes the field entry entirely when no other keys remain', () => {
    const { api } = setup({ cols: { a: { width: 180 } } })
    api.clearColumnWidth('a')
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.cols?.a).toBeUndefined()
  })

  it('setColumnWidth rejects non-positive / non-finite values', () => {
    const { api } = setup()
    api.setColumnWidth('a', 0)
    api.setColumnWidth('a', -10)
    api.setColumnWidth('a', Number.NaN)
    expect(api.isWidthCustom('a')).toBe(false)
  })
})

describe('useGridLayout — column order', () => {
  it('orderedColumns falls through source order when no persisted order', () => {
    const { api } = setup()
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['a', 'b', 'c', 'd'])
  })

  it('orderedColumns applies the persisted order', () => {
    const { api } = setup({ order: ['c', 'a', 'b', 'd'] })
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['c', 'a', 'b', 'd'])
  })

  it('orderedColumns appends source-only fields after the persisted order', () => {
    /* Persisted order references only 'b' + 'a'; source has 'c' + 'd' too. */
    const { api } = setup({ order: ['b', 'a'] })
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['b', 'a', 'c', 'd'])
  })

  it('orderedColumns drops stale fields no longer in the source array', () => {
    const { api } = setup({ order: ['nonexistent', 'b', 'a'] })
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['b', 'a', 'c', 'd'])
  })

  it('setColumnOrder drops the slot when the pick matches source order', () => {
    const { api } = setup({ order: ['c', 'a', 'b', 'd'] })
    api.setColumnOrder(['a', 'b', 'c', 'd'])
    const stored = JSON.parse(localStorage.getItem(KEY) ?? '{}')
    expect(stored.order).toBeUndefined()
  })

  it('moveColumn swaps adjacent fields and persists', () => {
    const { api } = setup()
    api.moveColumn('b', 'up')
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['b', 'a', 'c', 'd'])
  })

  it('moveColumn excludes uuid by default', () => {
    /* Inject a uuid column at position 0. moveColumn('a', 'up')
     * should be a no-op (a is at position 0 among non-uuid). */
    const cols = ref<ColumnDef[]>([{ field: 'uuid', label: 'uuid' }, ...COLS])
    let api!: ReturnType<typeof useGridLayout>
    const Harness = defineComponent({
      setup() {
        api = useGridLayout({ storageKey: KEY, columns: () => cols.value })
        return () => h('div')
      },
    })
    mount(Harness)
    api.moveColumn('a', 'up')
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual([
      'uuid',
      'a',
      'b',
      'c',
      'd',
    ])
    api.moveColumn('b', 'up')
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual([
      'uuid',
      'b',
      'a',
      'c',
      'd',
    ])
  })
})

describe('useGridLayout — reset / isAtDefaults', () => {
  it('isAtDefaults true with nothing persisted', () => {
    const { api } = setup()
    expect(api.isAtDefaults.value).toBe(true)
  })

  it('isAtDefaults false when sort persisted', () => {
    const { api } = setup({ sort: { field: 'b', dir: 'DESC' } })
    expect(api.isAtDefaults.value).toBe(false)
  })

  it('isAtDefaults false when columns dict has any field', () => {
    const { api } = setup({ cols: { a: { width: 200 } } })
    expect(api.isAtDefaults.value).toBe(false)
  })

  it('isAtDefaults false when order persisted', () => {
    const { api } = setup({ order: ['b', 'a', 'c', 'd'] })
    expect(api.isAtDefaults.value).toBe(false)
  })

  it('reset clears every axis and removes the localStorage entry', () => {
    const { api } = setup({
      sort: { field: 'b', dir: 'DESC' },
      cols: { a: { hidden: true } },
      order: ['b', 'a', 'c', 'd'],
    })
    expect(api.isAtDefaults.value).toBe(false)
    api.reset()
    expect(api.isAtDefaults.value).toBe(true)
    expect(localStorage.getItem(KEY)).toBeNull()
  })
})

describe('useGridLayout — corrupt blob handling', () => {
  it('treats invalid JSON as empty defaults', () => {
    localStorage.setItem(KEY, '{ not valid json')
    /* No throw → silent reset. */
    const cols = ref<ColumnDef[]>([...COLS])
    let api!: ReturnType<typeof useGridLayout>
    const Harness = defineComponent({
      setup() {
        api = useGridLayout({ storageKey: KEY, columns: () => cols.value })
        return () => h('div')
      },
    })
    mount(Harness)
    expect(api.isAtDefaults.value).toBe(true)
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['a', 'b', 'c', 'd'])
  })

  it('treats a stored literal "null" (valid JSON, wrong shape) as empty defaults', () => {
    localStorage.setItem(KEY, 'null')
    /* Parses fine but fails the object guard — must fall back to
     * defaults instead of letting `blob.value.sort` reads throw. */
    const { api } = setup()
    expect(api.isAtDefaults.value).toBe(true)
    expect(api.sort.value).toBeNull()
    expect(api.orderedColumns.value.map((c) => c.field)).toEqual(['a', 'b', 'c', 'd'])
  })
})

describe('useGridLayout — width CSS injection', () => {
  it('installs a <style> element on mount when gridKey is set', () => {
    const cols = ref<ColumnDef[]>([...COLS])
    const Harness = defineComponent({
      setup() {
        useGridLayout({
          storageKey: KEY,
          columns: () => cols.value,
          gridKey: 'my-grid',
        })
        return () => h('div')
      },
    })
    const w = mount(Harness)
    const styleEl = document.head.querySelector(
      'style[data-tvh-grid-layout="my-grid"]'
    )
    expect(styleEl).not.toBeNull()
    /* The injector should emit at least the fallback rule for 'a'
     * and the explicit-width rule for 'd' (col.width: 200). */
    expect(styleEl?.textContent).toMatch(/data-grid-key="my-grid"/)
    expect(styleEl?.textContent).toMatch(/data-field="d"/)
    expect(styleEl?.textContent).toMatch(/200px/)
    w.unmount()
    expect(
      document.head.querySelector('style[data-tvh-grid-layout="my-grid"]')
    ).toBeNull()
  })

  it('does not install a <style> element when gridKey is omitted', () => {
    const cols = ref<ColumnDef[]>([...COLS])
    const Harness = defineComponent({
      setup() {
        useGridLayout({ storageKey: KEY, columns: () => cols.value })
        return () => h('div')
      },
    })
    mount(Harness)
    const styleEl = document.head.querySelector('style[data-tvh-grid-layout]')
    expect(styleEl).toBeNull()
  })
})
