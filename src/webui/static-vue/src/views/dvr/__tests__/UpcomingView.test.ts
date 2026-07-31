// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- inline stub components
 * for the grid and its heavy drawer/editor companions; each is a
 * throwaway harness, not a real component. */

/*
 * UpcomingView — dedup-skipped rerun handling (issue #2207).
 *
 * The server's grid_upcoming endpoint INCLUDES dedup-skipped rerun
 * entries by default; they'd render as ordinary "Scheduled for
 * recording" rows although they will not record. The view declares a
 * "Skipped reruns" GlobalFilterSpec (grid-settings popover, Filters
 * section); IdnodeGrid seeds each spec's `current` into the fetch
 * params — the production contract DvbServices' "Hide" select rides
 * daily. Pins:
 *   - the spec defaults to `duplicates` = '0' (hidden, ExtJS parity),
 *   - a filter-change flips it to '1' and persists across mounts,
 *   - skipped rows (duplicate > 0) classify as dimmed via the
 *     grid's rowClass hook; normal rows don't,
 *   - the status column formats skipped rows as "Will be skipped".
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick } from 'vue'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import UpcomingView from '../UpcomingView.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow, GlobalFilterSpec } from '@/types/grid'

/* Capture the props UpcomingView hands to the grid; the grid itself
 * (store wiring, fetching, virtual scroller, the filters→params
 * seeding) is out of scope here. */
const gridProps = vi.hoisted(() => ({ current: null as Record<string, unknown> | null }))
vi.mock('@/components/IdnodeGrid.vue', () => ({
  default: defineComponent({
    name: 'IdnodeGrid',
    inheritAttrs: false,
    setup(_, { attrs }) {
      gridProps.current = attrs as Record<string, unknown>
      return () => h('div', { class: 'idnode-grid-stub' })
    },
  }),
}))
/* The drawer/editor/dialog companions drag Pinia-heavy dependency
 * trees — stub them out; this test only inspects the grid mount. */
vi.mock('@/views/epg/EpgEventDrawer.vue', () => ({
  default: defineComponent({ name: 'EpgEventDrawer', render: () => null }),
}))
vi.mock('@/components/IdnodeEditor.vue', () => ({
  default: defineComponent({ name: 'IdnodeEditor', render: () => null }),
}))
vi.mock('@/components/EpgRelatedDialog.vue', () => ({
  default: defineComponent({ name: 'EpgRelatedDialog', render: () => null }),
}))
/* useEditorMode syncs the edit target with the route — no real
 * router in the harness. */
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: {}, hash: '', fullPath: '/dvr/upcoming' }),
  useRouter: () => ({ push: vi.fn(), replace: vi.fn() }),
}))
/* PrimeVue's injection-based confirm/toast services aren't installed
 * in the harness — same mock idiom as useBulkAction.test.ts. */
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(() => Promise.resolve(true)) }),
}))
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: vi.fn(), error: vi.fn(), info: vi.fn() }),
}))

enableAutoUnmount(afterEach)

beforeEach(() => {
  setActivePinia(createPinia())
  localStorage.clear()
  gridProps.current = null
})

const SKIPPED_ROW = { uuid: 'd1', duplicate: 1_700_000_000 } as BaseRow
const NORMAL_ROW = { uuid: 'n1', duplicate: 0 } as BaseRow

function skipFilterSpec(): GlobalFilterSpec | undefined {
  const specs = gridProps.current?.filters as GlobalFilterSpec[] | undefined
  return specs?.find((f) => f.key === 'duplicates')
}

describe('UpcomingView — dedup-skipped entries', () => {
  it('declares the duplicates filter defaulting to "0" (hidden, ExtJS parity)', () => {
    mount(UpcomingView)
    expect(gridProps.current?.endpoint).toBe('dvr/entry/grid_upcoming')
    const spec = skipFilterSpec()!
    expect(spec.current).toBe('0')
    /* First option is the default — drives the popover's accent chip. */
    expect(spec.options[0].value).toBe('0')
  })

  it('a filter change flips the spec to "1" and persists the choice', async () => {
    const w = mount(UpcomingView)
    const onChange = gridProps.current?.onFilterChange as (k: string, v: string) => void
    onChange('duplicates', '1')
    await nextTick()
    expect(skipFilterSpec()?.current).toBe('1')
    expect(localStorage.getItem('tvh-dvr-upcoming:show-skipped')).toBe('1')

    /* A fresh mount honours the persisted choice. */
    w.unmount()
    mount(UpcomingView)
    expect(skipFilterSpec()?.current).toBe('1')
  })

  it('ignores filter changes for other keys', async () => {
    mount(UpcomingView)
    const onChange = gridProps.current?.onFilterChange as (k: string, v: string) => void
    onChange('hidemode', '1')
    await nextTick()
    expect(skipFilterSpec()?.current).toBe('0')
  })

  it('classifies skipped rows as dimmed via the rowClass hook', () => {
    mount(UpcomingView)
    const rowClass = gridProps.current?.['row-class'] as (r: BaseRow) => string | undefined
    expect(rowClass(SKIPPED_ROW)).toBe('upcoming__row--skipped')
    expect(rowClass(NORMAL_ROW)).toBeUndefined()
    expect(rowClass({ uuid: 'x' } as BaseRow)).toBeUndefined()
  })

  it('formats the status column as "Will be skipped" for skipped rows', () => {
    mount(UpcomingView)
    const cols = gridProps.current?.columns as ColumnDef[]
    const status = cols.find((c) => c.field === 'sched_status')!
    expect(status.format?.('Scheduled for recording', SKIPPED_ROW)).toBe('Will be skipped')
    expect(status.format?.('Scheduled for recording', NORMAL_ROW)).toBe(
      'Scheduled for recording',
    )
  })
})
