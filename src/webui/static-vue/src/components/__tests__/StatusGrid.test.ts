// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * StatusGrid unit tests.
 *
 * Strategy parallels IdnodeGrid.test.ts: mock @/stores/status to
 * return a controllable plain object, mock the comet client so we
 * don't open WebSocket connections during tests. Verify selection,
 * the phone-card layout, the #toolbarActions slot wiring, and that
 * fetch() runs on mount.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import PrimeVue from 'primevue/config'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the component's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref } = await import('vue')
  const isPhone = ref(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import StatusGrid from '../StatusGrid.vue'
import type { ColumnDef } from '@/types/column'

interface MockRow extends Record<string, unknown> {
  uuid: string
  input: string
  bps: number
}

interface MockStore {
  entries: MockRow[]
  loading: boolean
  error: Error | null
  isEmpty: boolean
  fetch: ReturnType<typeof vi.fn>
}

let mockStore: MockStore

vi.mock('@/stores/status', () => ({
  useStatusStore: () => mockStore,
}))

vi.mock('@/api/comet', () => ({
  cometClient: {
    on: vi.fn(),
  },
}))

function makeStore(overrides: Partial<MockStore> = {}): MockStore {
  return {
    entries: [],
    loading: false,
    error: null,
    isEmpty: true,
    fetch: vi.fn(),
    ...overrides,
  }
}

const cols: ColumnDef[] = [
  { field: 'input', label: 'Input', sortable: true, minVisible: 'phone' },
  { field: 'bps', label: 'Bandwidth', sortable: true, minVisible: 'desktop' },
]

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
  phoneFlag.set(width < 768)
}

function mountGrid(
  slots: Record<string, string> = {},
  propOverrides: { storageKey?: string } = {},
) {
  return mount(StatusGrid, {
    props: {
      endpoint: 'status/inputs',
      notificationClass: 'input_status',
      columns: cols,
      keyField: 'uuid' as const,
      storageKey: propOverrides.storageKey ?? 'status-test',
    },
    slots,
    global: {
      plugins: [[PrimeVue, {}]],
    },
  })
}

describe('StatusGrid', () => {
  beforeEach(() => {
    mockStore = makeStore()
    setViewport(1280)
    localStorage.clear()
  })

  afterEach(() => {
    vi.clearAllMocks()
    localStorage.clear()
  })

  it('calls store.fetch on mount', () => {
    mountGrid()
    expect(mockStore.fetch).toHaveBeenCalledOnce()
  })

  it('renders the error banner when the store has an error', () => {
    mockStore = makeStore({ error: new Error('inputs API down') })
    const wrapper = mountGrid()
    expect(wrapper.html()).toContain('inputs API down')
  })

  it('switches to phone-card layout below 768px', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', input: 'DVB-S #0', bps: 100 },
        { uuid: 'b', input: 'DVB-S #1', bps: 200 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    expect(wrapper.find('.status-grid__phone').exists()).toBe(true)
    expect(wrapper.findAll('.status-grid__card')).toHaveLength(2)
  })

  it('phone-mode cards include only minVisible:phone columns', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'DVB-S #0', bps: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const cardHtml = wrapper.find('.status-grid__card').html()
    expect(cardHtml).toContain('Input')
    expect(cardHtml).toContain('DVB-S #0')
    /* Bandwidth is minVisible:'desktop', so it's hidden on phone cards. */
    expect(cardHtml).not.toContain('Bandwidth')
  })

  it('toolbar slot receives selection + clearSelection', async () => {
    /*
     * The slot exposes the live `selection` ref and a `clearSelection`
     * function. We verify the slot exists when provided (so views can
     * compose their own action menu) without relying on PrimeVue's
     * internal selection event firing in happy-dom.
     */
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'DVB-S #0', bps: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({
      toolbarActions:
        '<template #default="{ selection, clearSelection }"><button class="t-action" :data-count="selection.length" @click="clearSelection">act</button></template>',
    })
    /* Toolbar slot rendered, button visible. */
    expect(wrapper.find('.t-action').exists()).toBe(true)
    expect(wrapper.find('.status-grid__toolbar').exists()).toBe(true)
  })

  it('renders GridSettingsMenu in the toolbar on desktop', () => {
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    expect(wrapper.find('.grid-settings').exists()).toBe(true)
  })

  it('sorts client-side — lazy=false reaches the DataTable and the sort engages', () => {
    /* Status data is fully client-held; in lazy mode PrimeVue
     * skips its client-side sort pass, so a header click would
     * flip the chevron and persist the pick without reordering
     * any rows. */
    localStorage.setItem(
      'test-client-sort',
      JSON.stringify({ sort: { field: 'bps', dir: 'DESC' } }),
    )
    mockStore = makeStore({
      entries: [
        { uuid: 'a', input: 'Tuner A', bps: 100 },
        { uuid: 'b', input: 'Tuner B', bps: 300 },
        { uuid: 'c', input: 'Tuner C', bps: 200 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-client-sort' })
    const dataTable = wrapper.findComponent({ name: 'DataTable' })
    expect(dataTable.props('lazy')).toBe(false)
    /* Rows render in bps DESC order even though the entries array
     * arrived unsorted. */
    const rows = wrapper.findAll('tbody tr').map((r) => r.text())
    expect(rows).toHaveLength(3)
    expect(rows[0]).toContain('Tuner B')
    expect(rows[1]).toContain('Tuner C')
    expect(rows[2]).toContain('Tuner A')
  })

  it('persists a sort pick to localStorage under storageKey', async () => {
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-persist-sort' })
    /* StatusGrid's onSortChange writes through useGridLayout —
     * mimic PrimeVue's sort event emission by calling the wrapper's
     * exposed handler indirectly via the component's vnode emit. */
    await wrapper.findComponent({ name: 'DataGrid' }).vm.$emit('sort', {
      sortField: 'bps',
      sortOrder: -1,
    })
    const stored = JSON.parse(localStorage.getItem('test-persist-sort') ?? '{}')
    expect(stored.sort).toEqual({ field: 'bps', dir: 'DESC' })
  })

  it('loads persisted sort from localStorage on mount', () => {
    localStorage.setItem(
      'test-load-sort',
      JSON.stringify({ sort: { field: 'bps', dir: 'DESC' } }),
    )
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-load-sort' })
    const dg = wrapper.findComponent({ name: 'DataGrid' })
    expect(dg.props('sortField')).toBe('bps')
    expect(dg.props('sortOrder')).toBe(-1)
  })

  it('persists a column hide via the GridSettingsMenu toggle', async () => {
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-hide' })
    /* GridSettingsMenu emits toggleColumn(field); StatusGrid's
     * handler resolves the current visibility (false → true) and
     * writes through useGridLayout. */
    await wrapper.findComponent({ name: 'GridSettingsMenu' }).vm.$emit(
      'toggleColumn',
      'bps',
    )
    const stored = JSON.parse(localStorage.getItem('test-hide') ?? '{}')
    expect(stored.cols.bps.hidden).toBe(true)
  })

  it('persists a column reorder', async () => {
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-reorder' })
    await wrapper
      .findComponent({ name: 'DataGrid' })
      .vm.$emit('reorderColumns', ['bps', 'input'])
    const stored = JSON.parse(localStorage.getItem('test-reorder') ?? '{}')
    expect(stored.order).toEqual(['bps', 'input'])
  })

  it('reset wipes every persisted axis and removes the localStorage entry', async () => {
    localStorage.setItem(
      'test-reset',
      JSON.stringify({
        sort: { field: 'bps', dir: 'DESC' },
        cols: { input: { hidden: true } },
        order: ['bps', 'input'],
      }),
    )
    mockStore = makeStore({
      entries: [{ uuid: 'a', input: 'A', bps: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { storageKey: 'test-reset' })
    await wrapper.findComponent({ name: 'GridSettingsMenu' }).vm.$emit('reset')
    expect(localStorage.getItem('test-reset')).toBeNull()
  })

  it('toggleSelect adds and removes rows by keyField', () => {
    mockStore = makeStore({
      entries: [
        { uuid: 'a', input: 'A', bps: 1 },
        { uuid: 'b', input: 'B', bps: 2 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    /* Vue unwraps refs on `vm` access, so `selection` reads as the
     * array directly. */
    const exposed = wrapper.vm as unknown as {
      selection: MockRow[]
      toggleSelect: (r: MockRow) => void
    }
    const a = mockStore.entries[0]
    exposed.toggleSelect(a)
    expect(exposed.selection.map((r) => r.uuid)).toContain('a')
    /* Toggling again removes it. */
    exposed.toggleSelect(a)
    expect(exposed.selection.map((r) => r.uuid)).not.toContain('a')
  })
})
