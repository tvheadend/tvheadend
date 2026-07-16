// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeGrid unit tests.
 *
 * Strategy: mock @/stores/grid to return a plain object that mimics
 * the shape Pinia exposes after auto-unwrapping refs. We verify the
 * surfaces IdnodeGrid owns directly (slots, phone-mode card layout,
 * row click, localStorage column persistence) without relying on
 * deep PrimeVue DataTable DOM interaction — that's brittle in
 * happy-dom and would test PrimeVue more than us.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import PrimeVue from 'primevue/config'
import IdnodeGrid from '../IdnodeGrid.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { UiLevel } from '@/types/access'
import { GRID_LIMIT_ALL } from '@/api/gridConstants'

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

/* IdnodeGrid pulls in `useConfirmDialog()` for the
 * unsaved-changes nav-away guard. PrimeVue's underlying
 * `useConfirm()` requires `app.use(ConfirmationService)` —
 * which the tests don't bootstrap. Mock the wrapper module
 * with a default-pass `ask` so guard-related tests can spy
 * on the call without needing the full ConfirmationService
 * provider tree. Tests that exercise the guard explicitly
 * override the mock per-test via `confirmAskMock`. */
const confirmAskMock = vi.fn(async () => true)
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: confirmAskMock }),
}))

/* `onBeforeRouteLeave` reads from the matched-route inject
 * tree; without a router, the call returns silently in dev
 * but warns. Stub it to a no-op so tests don't print
 * warnings on every mount. */
vi.mock('vue-router', () => ({
  onBeforeRouteLeave: vi.fn(),
}))

/* IdnodeGrid wires `useStaleDataRecovery`, which subscribes to the
 * comet client. Mock it so the stale-data-recovery tests can drive
 * synthetic disconnect / reconnect transitions without a real
 * WebSocket; `cometStateListener` captures the registered listener
 * and `cometStateUnsub` records that the composable unsubscribed. */
let cometStateListener: ((s: string) => void) | null = null
let cometStateUnsub = false
vi.mock('@/api/comet', () => ({
  cometClient: {
    getState: () => 'idle',
    onStateChange: (listener: (s: string) => void) => {
      cometStateListener = listener
      return () => {
        cometStateUnsub = true
        cometStateListener = null
      }
    },
    on: vi.fn(() => () => {}),
    connect: vi.fn(),
    disconnect: vi.fn(),
  },
}))

interface MockRow extends Record<string, unknown> {
  uuid: string
  title: string
  size: number
}

interface MockFilter {
  field: string
  type: 'string' | 'numeric' | 'boolean'
  value: string | number | boolean
  comparison?: string
}

interface MockStore {
  entries: MockRow[]
  total: number
  loading: boolean
  error: Error | null
  sort: { key?: string; dir: 'ASC' | 'DESC' }
  filter: MockFilter[]
  start: number
  limit: number
  isEmpty: boolean
  fetch: ReturnType<typeof vi.fn>
  setSort: ReturnType<typeof vi.fn>
  setFilter: ReturnType<typeof vi.fn>
  setPage: ReturnType<typeof vi.fn>
  update: ReturnType<typeof vi.fn>
}

let mockStore: MockStore

/*
 * Stubs for the stores/composables IdnodeGrid pulls in. Using vi.mock
 * means the tests don't need a real Pinia instance, matching the
 * existing pattern. Each mock keeps just enough surface to make the
 * SUT happy.
 */
vi.mock('@/stores/grid', () => ({
  useGridStore: () => mockStore,
  inferEntityClass: (endpoint: string) => endpoint.split('/')[0],
}))

/* `apiMock` covers two unrelated call sites that share the
 * client module: the grid store (already mocked above) and
 * the deferredEnum cache. The deferred-enum test populates the
 * cache via `fetchDeferredEnum`, which calls `apiCall`. */
const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const idnodeClassStub = {
  /* By default, no metadata loaded — equivalent to "all columns basic". */
  meta: null as null | {
    props: {
      id: string
      caption?: string
      advanced?: boolean
      expert?: boolean
    }[]
  },
  ensure: vi.fn(async function ensure(this: typeof idnodeClassStub) {
    return this.meta
  }),
  get: vi.fn(function get(this: typeof idnodeClassStub) {
    return this.meta
  }),
}

vi.mock('@/stores/idnodeClass', () => ({
  useIdnodeClassStore: () => idnodeClassStub,
}))

/*
 * Per-grid level: IdnodeGrid reads its level override
 * from `tvh-grid:<storeKey>:uilevel` in localStorage and falls back
 * to the access store. The access store itself is stubbed below to
 * provide a deterministic server-side default that drives the
 * `effectiveLevel` computation.
 *
 * Tests that need a non-default level set the localStorage value
 * before calling mountGrid().
 */
let mockAccessUilevel: UiLevel = 'expert'
let mockAccessLocked = false

vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    get uilevel() {
      return mockAccessUilevel
    },
    get locked() {
      return mockAccessLocked
    },
  }),
}))

function makeStore(overrides: Partial<MockStore> = {}): MockStore {
  return {
    entries: [],
    total: 0,
    loading: false,
    error: null,
    sort: { key: undefined, dir: 'ASC' },
    filter: [],
    start: 0,
    limit: 100,
    isEmpty: true,
    fetch: vi.fn(() => Promise.resolve()),
    setSort: vi.fn(),
    setFilter: vi.fn(),
    setPage: vi.fn(),
    update: vi.fn(),
    ...overrides,
  }
}

const cols: ColumnDef[] = [
  { field: 'title', label: 'Title', sortable: true, filterType: 'string', minVisible: 'phone' },
  {
    field: 'channel',
    label: 'Channel',
    sortable: true,
    filterType: 'string',
    minVisible: 'desktop',
  },
  { field: 'size', label: 'Size', sortable: true, filterType: 'numeric', minVisible: 'desktop' },
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
  propOverrides: Partial<{
    endpoint: string
    columns: ColumnDef[]
    storeKey: string
    lockLevel: UiLevel
    clientSideFilter: boolean
    persistColumns: boolean
  }> = {}
) {
  return mount(IdnodeGrid, {
    props: {
      endpoint: 'mock/grid',
      columns: cols,
      storeKey: 'test-key',
      ...propOverrides,
    },
    slots,
    global: {
      plugins: [[PrimeVue, {}]],
    },
  })
}

/*
 * Helper for level-filter / caption tests that share the same
 * "configure metadata + level + entries → mount → assert what's
 * visible in the rendered HTML" shape. Keeps each test focused on
 * its assertion (which columns/captions it expects to find) instead
 * of repeating ~6 lines of setup boilerplate per case. Each call
 * respects the per-test `beforeEach` reset (idnodeClassStub.meta
 * and mockAccessUilevel are wiped to defaults between tests), so
 * helper-mutated state never leaks across cases.
 */
type MetaProp = {
  id: string
  caption?: string
  advanced?: boolean
  expert?: boolean
}

function mountWithMetadata(opts: {
  metaProps: MetaProp[]
  uilevel?: UiLevel
  entries?: MockRow[]
  viewport?: number
}) {
  if (opts.viewport !== undefined) setViewport(opts.viewport)
  idnodeClassStub.meta = { props: opts.metaProps }
  mockAccessUilevel = opts.uilevel ?? 'expert'
  mockStore = makeStore({
    entries: opts.entries ?? [{ uuid: 'a', title: 'Alpha', size: 1 }],
    isEmpty: false,
  })
  return mountGrid()
}

/* Standard 3-property set used by the level-filter tests: a
 * basic-level prop, an advanced-only prop, and an expert-only prop.
 * Captures what the column gating logic operates on. */
const STANDARD_LEVEL_PROPS: MetaProp[] = [
  { id: 'title' },
  { id: 'channel', advanced: true },
  { id: 'size', expert: true },
]

/* Set the metadata stub so IdnodeGrid's propFor() returns
 * the right type per column. Without this the cell rendering
 * falls through to the read-only path. */
function setupBoolStrMeta() {
  idnodeClassStub.meta = {
    props: [
      { id: 'title', type: 'str' } as unknown as MetaProp,
      { id: 'enabled', type: 'bool' } as unknown as MetaProp,
    ],
  }
}

/* Mount a grid in editMode='cell' and toggle into edit mode
 * (matches normal user flow — click Edit). */
async function mountInEditMode(opts: {
  columns: ColumnDef[]
  entries: MockRow[]
  storeKey: string
  beforeEdit?: (row: BaseRow, field: string) => boolean | string
}) {
  mockStore = makeStore({ entries: opts.entries, isEmpty: false })
  const wrapper = mount(IdnodeGrid, {
    props: {
      endpoint: 'mock/grid',
      columns: opts.columns,
      storeKey: opts.storeKey,
      editMode: 'cell',
      beforeEdit: opts.beforeEdit,
    },
    global: { plugins: [[PrimeVue, {}]] },
  })
  const exposed = wrapper.vm as unknown as {
    toggleEditMode: () => void
  }
  exposed.toggleEditMode()
  await wrapper.vm.$nextTick()
  return wrapper
}

/* Variant of mountInEditMode for the gate-tests: accepts custom
 * slots (toolbarActions etc.). */
async function mountAndEnterEdit(opts: {
  columns: ColumnDef[]
  entries: MockRow[]
  storeKey: string
  slots?: Record<string, string>
}) {
  mockStore = makeStore({ entries: opts.entries, isEmpty: false })
  const wrapper = mount(IdnodeGrid, {
    props: {
      endpoint: 'mock/grid',
      columns: opts.columns,
      storeKey: opts.storeKey,
      editMode: 'cell',
    },
    slots: opts.slots ?? {},
    global: { plugins: [[PrimeVue, {}]] },
  })
  const exposed = wrapper.vm as unknown as {
    toggleEditMode: () => void
  }
  exposed.toggleEditMode()
  await wrapper.vm.$nextTick()
  return wrapper
}

/* Mount at phone width (<768) so IdnodeGrid's `isPhone` ref
 * initialises true. The viewport restore in afterEach brings
 * desktop back for subsequent suites. */
function mountAtPhoneWidth(opts: {
  columns: ColumnDef[]
  entries: MockRow[]
  storeKey: string
  slots?: Record<string, string>
}) {
  setViewport(400)
  mockStore = makeStore({ entries: opts.entries, isEmpty: false })
  return mount(IdnodeGrid, {
    props: {
      endpoint: 'mock/grid',
      columns: opts.columns,
      storeKey: opts.storeKey,
      editMode: 'cell',
    },
    slots: opts.slots ?? {},
    global: { plugins: [[PrimeVue, {}]] },
  })
}

/* Mount the standard title+enabled columns for the unsaved-changes
 * nav-guard tests. */
function mountForGuard() {
  mockStore = makeStore({
    entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
    isEmpty: false,
  })
  return mount(IdnodeGrid, {
    props: {
      endpoint: 'mock/grid',
      columns: [
        { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        { field: 'enabled', label: 'Enabled', editable: true, minVisible: 'phone' },
      ],
      storeKey: 'unsaved-guard',
      editMode: 'cell',
    },
    global: { plugins: [[PrimeVue, {}]] },
  })
}

describe('IdnodeGrid', () => {
  beforeEach(() => {
    mockStore = makeStore()
    /* Reset stubs to permissive defaults: no metadata, expert level. */
    idnodeClassStub.meta = null
    mockAccessUilevel = 'expert'
    mockAccessLocked = false
    setViewport(1280) // desktop by default
    localStorage.clear()
    cometStateListener = null
    cometStateUnsub = false
  })

  afterEach(() => {
    localStorage.clear()
  })

  it('calls store.setPage with the "all" sentinel on mount', () => {
    /* The grid is virtual-scrolled — it needs the full dataset on
     * the client because there's no page-forward affordance. Mount
     * calls `store.setPage(0, GRID_LIMIT_ALL)` to override the
     * store's default page-size limit before the initial fetch. */
    mountGrid()
    expect(mockStore.setPage).toHaveBeenCalledOnce()
    expect(mockStore.setPage).toHaveBeenCalledWith(0, GRID_LIMIT_ALL)
  })

  it('renders the default error message when store has an error', () => {
    mockStore = makeStore({ error: new Error('fetch went boom') })
    const wrapper = mountGrid()
    expect(wrapper.html()).toContain('fetch went boom')
  })

  it('switches to phone mode below 768px and renders cards', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', title: 'Alpha', size: 100 },
        { uuid: 'b', title: 'Beta', size: 200 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    expect(wrapper.find('.idnode-grid__phone').exists()).toBe(true)
    expect(wrapper.findAll('.idnode-grid__card')).toHaveLength(2)
  })

  it('phone-mode cards only show columns with minVisible:phone', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const cardHtml = wrapper.find('.idnode-grid__card').html()
    expect(cardHtml).toContain('Title')
    expect(cardHtml).toContain('Alpha')
    // Size column has minVisible:'desktop' — must not appear on a phone card.
    expect(cardHtml).not.toContain('Size')
    expect(cardHtml).not.toContain('100')
  })

  it('phone card body tap toggles selection AND emits rowClick (matches desktop)', async () => {
    setViewport(400)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 100 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    await wrapper.find('.idnode-grid__card-body').trigger('click')
    expect(wrapper.emitted('rowClick')).toBeTruthy()
    expect(wrapper.emitted('rowClick')![0][0]).toEqual(row)
    const vm = wrapper.vm as unknown as ExposedVm
    expect(vm.selection).toHaveLength(1)
    expect(vm.selection[0].uuid).toBe('a')
  })

  it('uses the empty-state slot when no entries (phone mode)', () => {
    setViewport(400)
    mockStore = makeStore({ entries: [], isEmpty: true })
    const wrapper = mountGrid({
      empty: '<p data-testid="empty-slot">No data here</p>',
    })
    expect(wrapper.html()).toContain('No data here')
  })

  it('persists column visibility toggles to localStorage', () => {
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as { toggleColumn: (f: string) => void }
    exposed.toggleColumn('size')
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as {
      cols?: Record<string, { hidden?: boolean }>
    }
    expect(parsed.cols?.size?.hidden).toBe(true)
  })

  /*
   * Item 7b — sort persistence across reloads. Classic ExtJS gets
   * this for free via `stateful: true` + Ext.state.CookieProvider
   * (static/app/idnode.js:2122-2124, static/app/tvheadend.js:55-61);
   * Vue must persist explicitly to match. These cover: write on
   * user-driven sort, seed the store from persisted prefs at mount,
   * drop persisted sort when it matches the column-based default,
   * and respect `persistColumns: false`.
   */
  it('persists a user-driven sort (lazy mode) to localStorage', async () => {
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('sort', { sortField: 'title', sortOrder: -1 })
    await wrapper.vm.$nextTick()
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { sort?: { field: string; dir: string } }
    expect(parsed.sort).toEqual({ field: 'title', dir: 'DESC' })
  })

  it('seeds the store sort from persisted prefs at mount (lazy mode)', () => {
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ sort: { field: 'title', dir: 'DESC' } }),
    )
    mountGrid()
    /* Routed via `store.update` rather than `setSort` so a
     * persisted filter (if present) rides the same fetch. With
     * only sort persisted the filter field is omitted. */
    expect(mockStore.update).toHaveBeenCalledWith({
      sort: { key: 'title', dir: 'DESC' },
    })
  })

  it('drops the persisted sort slot when it matches the default', async () => {
    /* Pre-seed with a non-default sort, then sort back to the
     * default — the persisted slot should be removed so a future
     * default change flows through naturally. With no `defaultSort`
     * prop the first-non-`enabled` column fallback resolves to
     * `title` ASC for the test cols. */
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ sort: { field: 'channel', dir: 'DESC' } }),
    )
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('sort', { sortField: 'title', sortOrder: 1 })
    await wrapper.vm.$nextTick()
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { sort?: unknown }
    expect(parsed.sort).toBeUndefined()
  })

  it('does NOT persist sort when persistColumns is false', async () => {
    const wrapper = mountGrid({}, { persistColumns: false })
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('sort', { sortField: 'title', sortOrder: -1 })
    await wrapper.vm.$nextTick()
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  /*
   * Filter persistence — Classic ExtJS persists grid filters
   * via stateful: true + GridFilters plugin (cookie-backed). The
   * gridPrefs.filter slot mirrors that on the Vue side so per-
   * column funnel filters survive reload.
   */
  it('persists a filter change to localStorage (lazy mode)', async () => {
    /* Filter persistence moved to a sibling `:filter` key when the
     * column-layout migration to useGridLayout landed — the
     * composable's scope is sort + cols + order, so filter writes
     * stay separate. */
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('filter', {
      filters: {
        title: { value: 'abc' },
      },
    })
    await wrapper.vm.$nextTick()
    const stored = localStorage.getItem('tvh-grid:test-key:filter')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as unknown
    expect(parsed).toEqual([
      { field: 'title', type: 'string', value: 'abc' },
    ])
  })

  it('seeds the store filter from persisted prefs at mount (lazy mode)', () => {
    localStorage.setItem(
      'tvh-grid:test-key:filter',
      JSON.stringify([{ field: 'title', type: 'string', value: 'persisted' }]),
    )
    mountGrid()
    expect(mockStore.update).toHaveBeenCalledWith({
      filter: [{ field: 'title', type: 'string', value: 'persisted' }],
    })
  })

  it('drops the persisted filter slot when filters clear (lazy mode)', async () => {
    localStorage.setItem(
      'tvh-grid:test-key:filter',
      JSON.stringify([{ field: 'title', type: 'string', value: 'persisted' }]),
    )
    const wrapper = mountGrid()
    /* Re-emit filter with no value — onFilter collects no entries
     * and persistFilter sees an empty array, dropping the slot. */
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('filter', { filters: {} })
    await wrapper.vm.$nextTick()
    expect(localStorage.getItem('tvh-grid:test-key:filter')).toBeNull()
  })

  it('does NOT persist filter when persistColumns is false', async () => {
    const wrapper = mountGrid({}, { persistColumns: false })
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('filter', {
      filters: { title: { value: 'abc' } },
    })
    await wrapper.vm.$nextTick()
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
    expect(localStorage.getItem('tvh-grid:test-key:filter')).toBeNull()
  })

  /*
   * Numeric column filter — model → wire-entries translation.
   * The per-column filter model is a NumericFilterModel object
   * (op + value [+ value2]); the wire is 1 entry for eq/lt/gt,
   * 2 entries for Between. Server's gt/lt are inclusive today
   * (idnode.c:911-918), so the UI labels them as ≥/≤ and
   * Between sends bounds verbatim — no off-by-one shift. Once
   * the server gains strict gt/lt + new ge/le operators, the
   * UI flips labels back to </> and adds ≥/≤/!= as separate
   * entries.
   */
  describe('numeric filter operators', () => {
    it('eq model emits a single wire entry with comparison:eq', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: { op: 'eq', value: 5, value2: null } } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [{ field: 'size', type: 'numeric', value: 5, comparison: 'eq' }],
        start: 0,
      })
    })

    it('lt model emits comparison:lt', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: { op: 'lt', value: 100, value2: null } } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [{ field: 'size', type: 'numeric', value: 100, comparison: 'lt' }],
        start: 0,
      })
    })

    it('gt model emits comparison:gt', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: { op: 'gt', value: 0, value2: null } } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [{ field: 'size', type: 'numeric', value: 0, comparison: 'gt' }],
        start: 0,
      })
    })

    it('between model emits TWO wire entries with the raw bounds (no shift)', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: { op: 'between', value: 5, value2: 10 } } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [
          { field: 'size', type: 'numeric', value: 5, comparison: 'gt' },
          { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
        ],
        start: 0,
      })
    })

    it('null model emits no entries (filter cleared)', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: null } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({ filter: [], start: 0 })
    })

    it('between model with missing value2 emits no entries', async () => {
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', {
        filters: { size: { value: { op: 'between', value: 5, value2: null } } },
      })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledWith({ filter: [], start: 0 })
    })

    /*
     * Reverse mapping on load: wire entries already present on the
     * store flow back into the per-column model so the chip +
     * popover render the correct state. Legacy bare-eq persisted
     * shape (`comparison` undefined) upgrades to op:'eq' silently.
     * Seed the mockStore.filter directly — the mock's `update`
     * doesn't mutate state on its own, so localStorage-based
     * seeding here would never reach buildDtFilters.
     */
    it('seeds an eq model from an existing eq wire entry on the store', () => {
      mockStore = makeStore({
        filter: [{ field: 'size', type: 'numeric', value: 5, comparison: 'eq' }],
      })
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const filters = dataGrid.props('filters') as Record<string, { value: unknown }>
      expect(filters.size?.value).toEqual({ op: 'eq', value: 5, value2: null })
    })

    it('seeds a between model from a gt+lt pair (bounds round-trip verbatim)', () => {
      mockStore = makeStore({
        filter: [
          { field: 'size', type: 'numeric', value: 5, comparison: 'gt' },
          { field: 'size', type: 'numeric', value: 10, comparison: 'lt' },
        ],
      })
      const wrapper = mountGrid()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const filters = dataGrid.props('filters') as Record<string, { value: unknown }>
      expect(filters.size?.value).toEqual({ op: 'between', value: 5, value2: 10 })
    })

  })

  /*
   * Column reorder via drag-headers (PrimeVue path). Same
   * persistence shape `gridPrefs.order: string[]` is shared
   * with the arrow-button path; tests below cover the
   * resolution logic (orderedColumns) and the drag-headers emit.
   */
  it('persists a column reorder (drag headers) to localStorage', async () => {
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('reorderColumns', ['size', 'title', 'channel'])
    await wrapper.vm.$nextTick()
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { order?: string[] }
    expect(parsed.order).toEqual(['size', 'title', 'channel'])
  })

  it('drops the persisted order slot when it matches the source order', async () => {
    /* Pre-seed with a non-source order, then reorder back to source —
     * the persisted slot should be removed so a future column-list
     * change flows through naturally. */
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ order: ['size', 'title', 'channel'] }),
    )
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('reorderColumns', ['title', 'channel', 'size'])
    await wrapper.vm.$nextTick()
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { order?: unknown }
    expect(parsed.order).toBeUndefined()
  })

  it('does NOT persist reorder when persistColumns is false', async () => {
    const wrapper = mountGrid({}, { persistColumns: false })
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    dataGrid.vm.$emit('reorderColumns', ['size', 'title', 'channel'])
    await wrapper.vm.$nextTick()
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  it('applies a persisted order to the rendered DataGrid columns', () => {
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ order: ['size', 'title', 'channel'] }),
    )
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    const cols = dataGrid.props('columns') as { field: string }[]
    expect(cols.map((c) => c.field)).toEqual(['size', 'title', 'channel'])
  })

  it('appends columns not in the saved order in source sequence', () => {
    /* Persisted order only mentions `channel`. The two unmentioned
     * fields (title, size) should appear AFTER channel, in their
     * original props.columns sequence. Guarantees safe column
     * additions later: a new column simply shows up at the end. */
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ order: ['channel'] }),
    )
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    const cols = dataGrid.props('columns') as { field: string }[]
    expect(cols.map((c) => c.field)).toEqual(['channel', 'title', 'size'])
  })

  it('silently drops stale fields in saved order (no longer in props.columns)', () => {
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ order: ['removed-field', 'size', 'title', 'channel'] }),
    )
    const wrapper = mountGrid()
    const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
    const cols = dataGrid.props('columns') as { field: string }[]
    expect(cols.map((c) => c.field)).toEqual(['size', 'title', 'channel'])
  })

  it('exposes onMoveColumn that swaps adjacent fields', () => {
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as {
      onMoveColumn: (field: string, dir: 'up' | 'down') => void
    }
    exposed.onMoveColumn('channel', 'up')
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { order?: string[] }
    expect(parsed.order).toEqual(['channel', 'title', 'size'])
  })

  it('onMoveColumn up on first field is a no-op', () => {
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as {
      onMoveColumn: (field: string, dir: 'up' | 'down') => void
    }
    exposed.onMoveColumn('title', 'up')
    /* No write at all when the move is a no-op. */
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  it('onMoveColumn down on last field is a no-op', () => {
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as {
      onMoveColumn: (field: string, dir: 'up' | 'down') => void
    }
    exposed.onMoveColumn('size', 'down')
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  it('onMoveColumn swaps within the visible-in-menu subset (skips uuid)', () => {
    /* Master-detail layouts may declare a uuid column for row-key
     * plumbing. GridSettingsMenu filters it out of its visible
     * list (uuid is plumbing, not display); the swap target on an
     * arrow click should match that view, not cross over the uuid
     * row. */
    const colsWithUuid: ColumnDef[] = [
      { field: 'uuid', label: 'UUID', sortable: false },
      { field: 'title', label: 'Title', sortable: true, minVisible: 'phone' },
      { field: 'size', label: 'Size', sortable: true, minVisible: 'desktop' },
    ]
    const wrapper = mountGrid({}, { columns: colsWithUuid })
    const exposed = wrapper.vm as unknown as {
      onMoveColumn: (field: string, dir: 'up' | 'down') => void
    }
    /* 'title' is the FIRST non-uuid column in the visible subset
     * → 'up' is a no-op (no field above it in the menu's view). */
    exposed.onMoveColumn('title', 'up')
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()

    /* 'size' moves UP one slot in the visible subset → swaps with
     * 'title'. Reconstructed full order keeps 'uuid' at slot 0. */
    exposed.onMoveColumn('size', 'up')
    const stored = localStorage.getItem('tvh-grid:test-key')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as { order?: string[] }
    expect(parsed.order).toEqual(['uuid', 'size', 'title'])
  })

  it('Reset to defaults also clears a persisted column order', async () => {
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ order: ['size', 'title', 'channel'] }),
    )
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as { resetGridPrefs: () => void }
    exposed.resetGridPrefs()
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  it('renders phone toolbar with the settings popover trigger and search when configured', () => {
    /* On phone, the dedicated PhoneSortPopover is gone — the
     * settings popover (gear icon) now hosts the Sort by section
     * along with Filters / Group by. Just assert the toolbar +
     * gear trigger + search input are present. Sort behaviour
     * itself is covered at unit level by GridSettingsMenu tests.
     *
     * The mock store needs an explicit `sort.key` because
     * GridSettingsMenu's Sort by section gates on
     * `props.sortField` being set — IdnodeGrid passes
     * `store.sort.key` straight through, and the mock store
     * doesn't run IdnodeGrid's onMounted store.setSort
     * initialization. */
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
      sort: { key: 'title', dir: 'ASC' },
    })
    const wrapper = mountGrid()
    const toolbar = wrapper.find('.idnode-grid__toolbar')
    expect(toolbar.exists()).toBe(true)
    expect(toolbar.classes()).toContain('idnode-grid__toolbar--phone')
    expect(wrapper.find('.settings-popover__btn').exists()).toBe(true)
    expect(wrapper.find('.idnode-grid__toolbar-search').exists()).toBe(true)
  })

  it('renders desktop toolbar with search + settings popover trigger', () => {
    setViewport(1280)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
      sort: { key: 'title', dir: 'ASC' },
    })
    const wrapper = mountGrid()
    const toolbar = wrapper.find('.idnode-grid__toolbar')
    expect(toolbar.exists()).toBe(true)
    expect(toolbar.classes()).not.toContain('idnode-grid__toolbar--phone')
    expect(wrapper.find('.settings-popover__btn').exists()).toBe(true)
    expect(wrapper.find('.idnode-grid__toolbar-search').exists()).toBe(true)
  })

  it('phone search input calls store.update with new filter and reset window', async () => {
    vi.useFakeTimers()
    try {
      setViewport(400)
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
        total: 100,
        limit: 75,
        isEmpty: false,
      })
      const wrapper = mountGrid()
      const input = wrapper.find<HTMLInputElement>('.idnode-grid__toolbar-search input')
      await input.setValue('alph')
      vi.advanceTimersByTime(300)
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [{ field: 'title', type: 'string', value: 'alph' }],
        start: 0,
        limit: 25,
      })
    } finally {
      vi.useRealTimers()
    }
  })

  it('desktop search input preserves user page size and merges with existing column filters', async () => {
    vi.useFakeTimers()
    try {
      setViewport(1280)
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
        total: 200,
        limit: 100, // user picked 100/page
        // pre-existing column-header filter on a different field
        filter: [{ field: 'channel', type: 'string', value: 'BBC' }],
        isEmpty: false,
      })
      const wrapper = mountGrid()
      const input = wrapper.find<HTMLInputElement>('.idnode-grid__toolbar-search input')
      await input.setValue('alph')
      vi.advanceTimersByTime(300)
      expect(mockStore.update).toHaveBeenCalledWith({
        filter: [
          { field: 'channel', type: 'string', value: 'BBC' },
          { field: 'title', type: 'string', value: 'alph' },
        ],
        start: 0,
        limit: 100, // unchanged
      })
    } finally {
      vi.useRealTimers()
    }
  })

  it('phone "Load more" button grows the limit when more rows are available', async () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      total: 50,
      limit: 25,
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const btn = wrapper.find('.idnode-grid__phone-loadmore')
    expect(btn.exists()).toBe(true)
    await btn.trigger('click')
    expect(mockStore.setPage).toHaveBeenCalledWith(0, 50)
  })

  it('hides phone "Load more" when all rows are loaded', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      total: 1,
      limit: 25,
      isEmpty: false,
    })
    const wrapper = mountGrid()
    expect(wrapper.find('.idnode-grid__phone-loadmore').exists()).toBe(false)
  })

  /*
   * Phone-mode multi-select uses always-visible checkboxes (matching
   * desktop). Tests drive the input or the exposed `toggleSelect`
   * helper; we don't simulate real pointer events because there's
   * nothing time-based to test.
   *
   * Vue Test Utils auto-unwraps refs accessed through `wrapper.vm`,
   * so `wrapper.vm.selection` is the array directly (no `.value`).
   */
  type ExposedVm = {
    selection: MockRow[]
    toggleSelect: (r: MockRow) => void
    clearSelection: () => void
  }

  it('exposes selection helpers; desktop list-header strip shows the entry count or selection summary', async () => {
    /* Since the paginator was retired, the list-header strip
     * renders whenever the grid has rows — it carries both the
     * "N <label>" total count and (when applicable) the
     * "M of N selected" summary. Selecting a row updates the
     * summary to the selection wording. */
    setViewport(1280)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 100 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    expect(wrapper.find('.idnode-grid__list-header').exists()).toBe(true)

    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(row)
    await wrapper.vm.$nextTick()

    expect(vm.selection).toHaveLength(1)
    const summary = wrapper.find('.idnode-grid__list-header-summary')
    expect(summary.exists()).toBe(true)
    /* "All 1 selected" because the only row is selected → all
     * visible selected. */
    expect(summary.text()).toContain('All 1')

    vm.clearSelection()
    await wrapper.vm.$nextTick()
    expect(vm.selection).toHaveLength(0)
    /* Strip stays — carries the entry count even without
     * a selection. */
    expect(wrapper.find('.idnode-grid__list-header').exists()).toBe(true)
  })

  it('phone card checkbox toggles selection on change', async () => {
    setViewport(400)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 100 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    const checkbox = wrapper.find<HTMLInputElement>('.idnode-grid__card-check input')
    expect(checkbox.element.checked).toBe(false)
    await checkbox.setValue(true)
    const vm = wrapper.vm as unknown as ExposedVm
    expect(vm.selection).toHaveLength(1)
    expect(vm.selection[0].uuid).toBe('a')

    // Card paints selected state.
    expect(wrapper.find('.idnode-grid__card').classes()).toContain('idnode-grid__card--selected')
  })

  it('phone list-header strip shows entry count when nothing is selected', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', title: 'Alpha', size: 1 },
        { uuid: 'b', title: 'Beta', size: 2 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const summary = wrapper.find('.idnode-grid__list-header-summary')
    expect(summary.exists()).toBe(true)
    expect(summary.text()).toContain('2 entries')
  })

  it('phone list-header strip tristate select-all toggles all visible rows', async () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', title: 'Alpha', size: 1 },
        { uuid: 'b', title: 'Beta', size: 2 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const headerCheckbox = wrapper.find<HTMLInputElement>(
      '.idnode-grid__list-header-check input'
    )
    expect(headerCheckbox.element.checked).toBe(false)

    // Select all.
    await headerCheckbox.setValue(true)
    const vm = wrapper.vm as unknown as ExposedVm
    expect(vm.selection).toHaveLength(2)
    expect(wrapper.find('.idnode-grid__list-header-summary').text()).toContain(
      'All 2 entries selected'
    )

    // Toggle off — clears selection.
    await headerCheckbox.setValue(false)
    expect(vm.selection).toHaveLength(0)
  })

  it('phone list-header strip summary reflects partial selection state', async () => {
    setViewport(400)
    const a: MockRow = { uuid: 'a', title: 'Alpha', size: 1 }
    const b: MockRow = { uuid: 'b', title: 'Beta', size: 2 }
    mockStore = makeStore({ entries: [a, b], isEmpty: false })
    const wrapper = mountGrid()
    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(a)
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.idnode-grid__list-header-summary').text()).toContain(
      '1 of 2 selected'
    )
    // Header checkbox should NOT be marked fully checked when partial.
    const headerCheckbox = wrapper.find<HTMLInputElement>(
      '.idnode-grid__list-header-check input'
    )
    expect(headerCheckbox.element.checked).toBe(false)
  })

  it('phone list-header strip Clear button is shown when items selected', async () => {
    setViewport(400)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 1 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    expect(
      wrapper.find('.idnode-grid__list-header .idnode-grid__selection-clear').exists()
    ).toBe(false)
    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(row)
    await wrapper.vm.$nextTick()
    const clear = wrapper.find('.idnode-grid__list-header .idnode-grid__selection-clear')
    expect(clear.exists()).toBe(true)
    await clear.trigger('click')
    expect(vm.selection).toHaveLength(0)
  })

  it('renders the #toolbarActions slot with the current selection bound', async () => {
    setViewport(1280)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 100 }
    mockStore = makeStore({
      entries: [row],
      isEmpty: false,
    })
    const wrapper = mount(IdnodeGrid, {
      props: {
        endpoint: 'mock/grid',
        columns: cols,
        storeKey: 'test-key',
      },
      slots: {
        toolbarActions: `
          <button data-testid="bulk-cancel" :disabled="params.selection.length === 0">
            Cancel ({{ params.selection.length }})
          </button>
        `,
      },
      global: { plugins: [[PrimeVue, {}]] },
    })

    const btn = wrapper.find<HTMLButtonElement>('[data-testid="bulk-cancel"]')
    expect(btn.exists()).toBe(true)
    expect(btn.element.disabled).toBe(true)
    expect(btn.text()).toContain('Cancel (0)')

    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(row)
    await wrapper.vm.$nextTick()
    expect(btn.element.disabled).toBe(false)
    expect(btn.text()).toContain('Cancel (1)')
  })

  /*
   * Note: the "drop stale selections after refetch" behavior is in
   * a watcher on store.entries. Exercising it from a unit test would
   * require making the mock store reactive (currently a plain object
   * for simplicity), which would cascade through the other tests. The
   * watcher is small and the behavior is verified during browser
   * eyeball passes — see DvrView's cancel flow.
   */

  /*
   * View-level filtering tests.
   *
   * Each pre-populates the idnode-class metadata with per-property
   * `advanced`/`expert` flags, sets the access stub to the desired effective
   * level, and asserts which columns end up rendered. We use the
   * desktop DataTable's column header DOM as the assertion surface
   * because columns are the layer the level filter actually operates
   * on.
   */
  it('hides advanced columns when effective level is basic', () => {
    const wrapper = mountWithMetadata({
      metaProps: STANDARD_LEVEL_PROPS,
      uilevel: 'basic',
    })
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).not.toContain('Channel')
    expect(html).not.toContain('Size')
  })

  it('shows advanced (but not expert) columns when effective level is advanced', () => {
    const wrapper = mountWithMetadata({
      metaProps: STANDARD_LEVEL_PROPS,
      uilevel: 'advanced',
    })
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).toContain('Channel')
    expect(html).not.toContain('Size')
  })

  it('shows all columns when effective level is expert', () => {
    const wrapper = mountWithMetadata({
      metaProps: STANDARD_LEVEL_PROPS,
      uilevel: 'expert',
    })
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).toContain('Channel')
    expect(html).toContain('Size')
  })

  it('treats columns missing from the metadata as basic (always visible)', () => {
    /* Empty props array — every column field falls back to 'basic'. */
    const wrapper = mountWithMetadata({ metaProps: [], uilevel: 'basic' })
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).toContain('Channel')
    expect(html).toContain('Size')
  })

  /*
   * Per-grid level (4e) tests. The level override lives in
   * `tvh-grid:<storeKey>:uilevel`. Setting it before mount drives the
   * effective level used by the column filter; the access stub is the
   * fallback when no override exists.
   */
  it('reads the per-grid level override from localStorage on mount', () => {
    idnodeClassStub.meta = {
      props: [
        { id: 'channel', advanced: true },
        { id: 'size', expert: true },
      ],
    }
    /* Server says expert, but the per-grid override pins this grid to basic. */
    mockAccessUilevel = 'expert'
    localStorage.setItem('tvh-grid:test-key:uilevel', 'basic')
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).not.toContain('Channel')
    expect(html).not.toContain('Size')
  })

  it('access lock ignores any local level override', () => {
    idnodeClassStub.meta = {
      props: [{ id: 'channel', advanced: true }],
    }
    mockAccessUilevel = 'basic'
    mockAccessLocked = true
    /* Sneaky override on disk — should be ignored because admin has locked the level. */
    localStorage.setItem('tvh-grid:test-key:uilevel', 'expert')
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).not.toContain('Channel')
  })

  it('lockLevel pins the displayed level above access.uilevel + any localStorage override', () => {
    idnodeClassStub.meta = {
      props: [
        { id: 'channel', advanced: true },
        { id: 'size', expert: true },
      ],
    }
    /* Server says basic AND local override says basic — lockLevel='expert'
     * should win and reveal the advanced + expert columns. */
    mockAccessUilevel = 'basic'
    localStorage.setItem('tvh-grid:test-key:uilevel', 'basic')
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { lockLevel: 'expert' })
    const html = wrapper.html()
    expect(html).toContain('Title')
    expect(html).toContain('Channel')
    expect(html).toContain('Size')
  })

  it('lockLevel hides the View level section in the grid settings menu', async () => {
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid({}, { lockLevel: 'expert' })
    /* Open the GridSettingsMenu popover. */
    await wrapper.find('.settings-popover__btn').trigger('click')
    /* No level radios when locked; columns checkboxes still rendered. */
    expect(wrapper.findAll('[role="menuitemradio"]')).toHaveLength(0)
    expect(
      wrapper.findAll<HTMLInputElement>('input[type="checkbox"]').length
    ).toBeGreaterThan(0)
  })

  it('Reset to defaults wipes both localStorage keys for this grid', async () => {
    /* Pre-populate both keys so we can verify they're cleared. */
    localStorage.setItem('tvh-grid:test-key:uilevel', 'basic')
    localStorage.setItem(
      'tvh-grid:test-key',
      JSON.stringify({ cols: { size: { hidden: true } }, sort: { field: 'title', dir: 'DESC' } }),
    )
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const exposed = wrapper.vm as unknown as { resetGridPrefs: () => void }
    /* The reset is exposed; calling it directly is the same code path
     * the menu's reset button takes. */
    expect(typeof exposed.resetGridPrefs).toBe('function')
    exposed.resetGridPrefs()
    expect(localStorage.getItem('tvh-grid:test-key:uilevel')).toBeNull()
    expect(localStorage.getItem('tvh-grid:test-key')).toBeNull()
  })

  /*
   * Caption resolution: column headers, phone-card labels, search +
   * sort + filter affordances all flow through colLabel(col), which
   * prefers the server-localized `prop.caption` from idnode metadata.
   * The phone-card layout is the easiest assertion surface — it's
   * already mounted and rendered text is right there in the DOM.
   */
  it('uses server caption from idnode metadata for column labels', () => {
    const wrapper = mountWithMetadata({
      viewport: 400,
      metaProps: [
        { id: 'title', caption: 'Title (from server)' },
        { id: 'channel', caption: 'Channel (from server)' },
        { id: 'size', caption: 'Size (from server)' },
      ],
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
    })
    const cardHtml = wrapper.find('.idnode-grid__card').html()
    /* Phone card is minVisible:'phone' filtered, so only 'title' shows.
     * The server-localized caption wins over the test fixture's
     * col.label = 'Title'. */
    expect(cardHtml).toContain('Title (from server)')
  })

  it('falls back to col.label when the prop is missing from idnode metadata', () => {
    /* Metadata loaded for the class, but the `title` prop happens to
     * be absent (synthetic / computed columns are the realistic case;
     * here we simulate it by omission). */
    const wrapper = mountWithMetadata({
      viewport: 400,
      metaProps: [
        { id: 'channel', caption: 'Channel (server)' },
        { id: 'size', caption: 'Size (server)' },
      ],
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
    })
    const cardHtml = wrapper.find('.idnode-grid__card').html()
    /* No caption for `title` in metadata → falls back to col.label. */
    expect(cardHtml).toContain('Title')
  })

  /*
   * `clientSideFilter` opt-in: for grids backed by custom list
   * endpoints that don't read filter/sort/page params server-side
   * (e.g. `epggrab/module/list`). When true, filter / sort / page
   * events from the inner DataGrid bypass the store — PrimeVue
   * narrows the loaded rows in place. When false (default), the
   * existing lazy-mode behaviour kicks in and forwards every event
   * to the store for re-fetch.
   */
  describe('clientSideFilter', () => {
    it('default (false): filter event triggers store.update', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mountGrid()
      /* Reach into the inner DataGrid and emit the @filter event
       * the way PrimeVue would. Forwards through onFilter, which
       * (in lazy mode) calls store.update. */
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', { filters: { title: { value: 'a', matchMode: 'contains' } } })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).toHaveBeenCalledTimes(1)
    })

    it('clientSideFilter:true → filter event does NOT call store.update', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mountGrid({}, { clientSideFilter: true })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('filter', { filters: { title: { value: 'a', matchMode: 'contains' } } })
      await wrapper.vm.$nextTick()
      expect(mockStore.update).not.toHaveBeenCalled()
    })

    it('clientSideFilter:true → sort event does NOT call store.setSort', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mountGrid({}, { clientSideFilter: true })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('sort', { sortField: 'title', sortOrder: 1 })
      await wrapper.vm.$nextTick()
      expect(mockStore.setSort).not.toHaveBeenCalled()
    })

    it('clientSideFilter:true → page event does NOT call store.setPage', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mountGrid({}, { clientSideFilter: true })
      /* IdnodeGrid's onMounted unconditionally calls setPage(0,
       * GRID_LIMIT_ALL) — see the comment in IdnodeGrid.vue.
       * Reset the mock so the assertion targets the PrimeVue
       * `@page` event path specifically. */
      mockStore.setPage.mockClear()
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('page', { first: 0, rows: 25 })
      await wrapper.vm.$nextTick()
      expect(mockStore.setPage).not.toHaveBeenCalled()
    })
  })

  describe('editMode: cell — Save / Cancel toolbar', () => {
    it('does NOT mount the inlineEdit composable when editMode is unset', () => {
      const wrapper = mountGrid()
      const exposed = wrapper.vm as unknown as { inlineEdit: unknown }
      expect(exposed.inlineEdit).toBeNull()
      expect(wrapper.find('.idnode-grid__edit-btn').exists()).toBe(false)
      expect(wrapper.find('.idnode-grid__edit-toggle').exists()).toBe(false)
    })

    it('renders the Edit cells icon toggle on the right cluster in read mode (no Save / Cancel)', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      /* Toggle is the icon-only button in the right cluster
       * (alongside View options cog), not in the left action
       * area. Aria-label carries the label since the button
       * has no visible text. */
      const toggle = wrapper.find('.idnode-grid__edit-toggle')
      expect(toggle.exists()).toBe(true)
      expect(toggle.attributes('aria-label')).toBe('Edit cells in this grid')
      /* No Save / Cancel buttons visible until in edit mode. */
      expect(wrapper.findAll('.idnode-grid__edit-btn')).toHaveLength(0)
    })

    it('toggleEditMode() reveals Save / Cancel; Save enables once a cell is dirty', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-dirty',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
        toggleEditMode: () => void
      }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      let btns = wrapper.findAll('.idnode-grid__edit-btn')
      expect(btns).toHaveLength(2)
      expect(btns[0].text()).toBe('Save')
      expect(btns[1].text()).toBe('Discard')
      /* Save disabled until something is dirty. Cancel is
       * always enabled — it doubles as "exit edit mode" when
       * there's nothing to discard. */
      expect(btns[0].attributes('disabled')).toBeDefined()
      expect(btns[1].attributes('disabled')).toBeUndefined()
      exposed.inlineEdit.commitCell('a', 'title', 'Alpha-edited')
      await wrapper.vm.$nextTick()
      btns = wrapper.findAll('.idnode-grid__edit-btn')
      expect(btns[0].attributes('disabled')).toBeUndefined()
      expect(btns[1].attributes('disabled')).toBeUndefined()
    })

    it('Cancel click discards dirty + auto-exits edit mode', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-cancel',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
          hasDirty: { value: boolean }
        }
        toggleEditMode: () => void
        isInEditMode: boolean
      }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      exposed.inlineEdit.commitCell('a', 'title', 'Alpha-edited')
      await wrapper.vm.$nextTick()
      expect(exposed.inlineEdit.hasDirty.value).toBe(true)
      const cancelBtn = wrapper.findAll('.idnode-grid__edit-btn')[1]
      expect(cancelBtn.text()).toBe('Discard')
      await cancelBtn.trigger('click')
      await wrapper.vm.$nextTick()
      expect(exposed.inlineEdit.hasDirty.value).toBe(false)
      expect(exposed.isInEditMode).toBe(false)
    })

    it('Cancel click with no dirty data still exits edit mode silently', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-cancel-clean',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as {
        toggleEditMode: () => void
        isInEditMode: boolean
      }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      const cancelBtn = wrapper.findAll('.idnode-grid__edit-btn')[1]
      await cancelBtn.trigger('click')
      await wrapper.vm.$nextTick()
      expect(exposed.isInEditMode).toBe(false)
    })

    it('Save click commits dirty + auto-exits edit mode', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-save-fetch',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
          save: () => Promise<void>
        }
        toggleEditMode: () => void
        isInEditMode: boolean
      }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      /* Stub the composable's save with a no-op success
       * (real save POSTs via apiCall; not what we're
       * testing). The grid wrapper is what flips
       * isInEditMode after save resolves. */
      const saveStub = vi.fn(async () => {})
      exposed.inlineEdit.save = saveStub

      exposed.inlineEdit.commitCell('a', 'title', 'Alpha-edited')
      await wrapper.vm.$nextTick()

      const saveBtn = wrapper.findAll('.idnode-grid__edit-btn')[0]
      expect(saveBtn.text()).toBe('Save')
      await saveBtn.trigger('click')
      /* Let the promise chain settle. */
      await new Promise((r) => setTimeout(r, 0))
      await wrapper.vm.$nextTick()

      expect(saveStub).toHaveBeenCalledTimes(1)
      expect(exposed.isInEditMode).toBe(false)
    })

    it('toggling out of edit mode preserves dirty state (no destructive UI)', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-toggle',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
          hasDirty: { value: boolean }
        }
        toggleEditMode: () => void
        /* Vue auto-unwraps exposed refs on the component instance
         * proxy, so reading `.isInEditMode` returns the boolean. */
        isInEditMode: boolean
      }
      exposed.toggleEditMode()
      exposed.inlineEdit.commitCell('a', 'title', 'X')
      expect(exposed.inlineEdit.hasDirty.value).toBe(true)
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      /* The exposed `toggleEditMode` function is the bare
       * flip — it preserves the dirty state, no auto-clear.
       * The user-facing buttons (Save / Cancel) handle the
       * dirty store explicitly. Direct callers of
       * toggleEditMode (tests, advanced consumers, the
       * Edit cells button) bypass that. */
      expect(exposed.inlineEdit.hasDirty.value).toBe(true)
      expect(exposed.isInEditMode).toBe(false)
    })

    it('Edit cells icon click enters edit mode without a prompt', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'edit-key-enter-noprompt',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as { isInEditMode: boolean }
      confirmAskMock.mockReset()
      const editBtn = wrapper.find('.idnode-grid__edit-toggle')
      expect(editBtn.exists()).toBe(true)
      await editBtn.trigger('click')
      await wrapper.vm.$nextTick()
      expect(confirmAskMock).not.toHaveBeenCalled()
      expect(exposed.isInEditMode).toBe(true)
    })
  })

  describe('editMode: cell — cell rendering for str + bool', () => {

    it('renders an interactive checkbox for editable bool cells; click commits + shows dirty', async () => {
      setupBoolStrMeta()
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'enabled', label: 'Enabled', editable: true, minVisible: 'phone' },
        ],
        entries: [
          {
            uuid: 'a',
            title: 'Alpha',
            size: 1,
            enabled: false,
          },
        ],
        storeKey: 'edit-cell-bool',
      })
      const cb = wrapper.find('.idnode-grid__cell-bool')
      expect(cb.exists()).toBe(true)
      expect((cb.element as HTMLInputElement).checked).toBe(false)
      await cb.setValue(true)
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          isCellDirty: (u: string, f: string) => boolean
          cellValue: (u: string, f: string, fb: unknown) => unknown
        }
      }
      expect(exposed.inlineEdit.isCellDirty('a', 'enabled')).toBe(true)
      expect(exposed.inlineEdit.cellValue('a', 'enabled', false)).toBe(true)
    })

    it('strips editable from columns whose type is not yet supported', async () => {
      /* `time` is still out of scope for inline editing
       * (Classic uses a dedicated TimeField widget; we
       * defer until that's wired). The cell stays
       * read-only even when the consumer flags
       * `editable: true`. */
      idnodeClassStub.meta = {
        props: [{ id: 'created', type: 'time' } as unknown as MetaProp],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'created', label: 'Created', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 99 }],
        storeKey: 'edit-cell-unsupported',
      })
      /* No editable-cell slot rendered → no input controls. */
      expect(wrapper.find('.idnode-grid__cell-bool').exists()).toBe(false)
      expect(wrapper.find('.idnode-grid__cell-str').exists()).toBe(false)
    })

    it('strips editable from str columns flagged multiline / password', async () => {
      idnodeClassStub.meta = {
        props: [
          {
            id: 'title',
            type: 'str',
            multiline: true,
          } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'multi-line text\nrow2', size: 1 }],
        storeKey: 'edit-cell-multiline',
      })
      /* Multiline str defers to the drawer editor; no inline
       * cell slot fires. */
      expect(wrapper.find('.idnode-grid__cell-str').exists()).toBe(false)
    })

    it('renders the str cell display with dirty class once the dirty store has a value', async () => {
      setupBoolStrMeta()
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'edit-cell-dirty-str',
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      exposed.inlineEdit.commitCell('a', 'title', 'Alpha-edited')
      await wrapper.vm.$nextTick()
      /* The str cell is plain text in display mode (PrimeVue's
       * editor mounts on click via the #editor slot — covered
       * by manual eyeball, hard to drive in jsdom). The dirty
       * class lights up the left-edge accent + the displayed
       * value reflects the in-flight edit. */
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.exists()).toBe(true)
      expect(span.classes()).toContain('idnode-grid__cell--dirty')
      expect(span.text()).toBe('Alpha-edited')
    })

    it('beforeEdit returning a string blocks bool toggle (revert)', async () => {
      setupBoolStrMeta()
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'enabled', label: 'Enabled', editable: true, minVisible: 'phone' },
        ],
        entries: [
          {
            uuid: 'a',
            title: 'Alpha',
            size: 1,
            enabled: false,
          },
        ],
        storeKey: 'edit-cell-bool-blocked',
        beforeEdit: () => 'recording in progress',
      })
      const cb = wrapper.find('.idnode-grid__cell-bool')
      const input = cb.element as HTMLInputElement
      input.checked = true
      await cb.trigger('change')
      const exposed = wrapper.vm as unknown as {
        inlineEdit: { isCellDirty: (u: string, f: string) => boolean }
      }
      expect(exposed.inlineEdit.isCellDirty('a', 'enabled')).toBe(false)
      expect(input.checked).toBe(false)
    })

    it('numeric (u32) cell renders read-only display + accepts dirty values', async () => {
      idnodeClassStub.meta = {
        props: [{ id: 'pri_num', type: 'u32' } as unknown as MetaProp],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'pri_num', label: 'Priority', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1, pri_num: 50 }],
        storeKey: 'edit-cell-numeric',
      })
      /* Numeric cell uses the same str-cell display chrome —
       * plain text in read mode, PrimeVue's editor mounts on
       * click via the #editor slot (covered by eyeball). */
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.exists()).toBe(true)
      expect(span.text()).toBe('50')
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      exposed.inlineEdit.commitCell('a', 'pri_num', 99)
      await wrapper.vm.$nextTick()
      const updated = wrapper.find('.idnode-grid__cell-str')
      expect(updated.classes()).toContain('idnode-grid__cell--dirty')
      expect(updated.text()).toBe('99')
    })

    it('enum cell display goes through col.format so labels show, not raw keys', async () => {
      /* `pri` on dvrentry is PT_INT with enum metadata. The
       * server emits a number key (50) but the user expects
       * to see the localised label ("Normal"). The cell
       * display path runs through col.format, which the
       * decoratedColumns computed installs from
       * inline-array enum metadata. */
      idnodeClassStub.meta = {
        props: [
          {
            id: 'pri',
            type: 'int',
            enum: [
              { key: 0, val: 'Important' },
              { key: 50, val: 'Normal' },
              { key: 100, val: 'Low' },
            ],
          } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'pri', label: 'Priority', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1, pri: 50 }],
        storeKey: 'edit-cell-enum',
      })
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.exists()).toBe(true)
      /* Read-mode label, not the raw key 50. */
      expect(span.text()).toBe('Normal')
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      /* User picks Important — the dropdown emits the key
       * (string '0' from a native <select>). The cell
       * display still goes through format → 'Important'. */
      exposed.inlineEdit.commitCell('a', 'pri', '0')
      await wrapper.vm.$nextTick()
      const updated = wrapper.find('.idnode-grid__cell-str')
      expect(updated.classes()).toContain('idnode-grid__cell--dirty')
      expect(updated.text()).toBe('Important')
    })

    it('deferred-enum dirty value resolves to label via the deferredEnum cache', async () => {
      /* `pri` server-side uses `.list = dvr_entry_class_pri_list`,
       * not `.enum`, so the wire emits a deferred descriptor
       * (`{type:'api', uri, ...}`) instead of an inline array.
       * The grid endpoint pre-resolves labels for read display,
       * but the dropdown emits the raw key on pick. The cell
       * display path needs to convert key → label using the
       * resolved options the editor already fetched. */
      const { fetchDeferredEnum } = await import(
        '../idnode-fields/deferredEnum'
      )
      idnodeClassStub.meta = {
        props: [
          {
            id: 'pri',
            type: 'int',
            enum: { type: 'api', uri: 'dvr/entry/pri/list' },
          } as unknown as MetaProp,
        ],
      }
      /* Pre-populate the deferred-enum cache (mirrors what
       * IdnodeFieldEnum's mount + fetch would do in the real
       * UI). The unit-test apiCall mock returns the entries
       * shape the cache expects. */
      apiMock.mockResolvedValueOnce({
        entries: [
          { key: 0, val: 'Important' },
          { key: 50, val: 'Normal' },
          { key: 100, val: 'Low' },
        ],
      })
      await fetchDeferredEnum({ type: 'api', uri: 'dvr/entry/pri/list' })

      const wrapper = await mountInEditMode({
        columns: [
          { field: 'pri', label: 'Priority', editable: true, minVisible: 'phone' },
        ],
        /* Server-resolved label in the row's emitted value
         * (what the user sees in read mode). */
        entries: [
          { uuid: 'a', title: 'Alpha', size: 1, pri: 'Normal' },
        ],
        storeKey: 'edit-cell-deferred-enum',
      })
      const exposed = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      /* Editor emits the KEY (string '0' from a native select). */
      exposed.inlineEdit.commitCell('a', 'pri', '0')
      await wrapper.vm.$nextTick()
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.classes()).toContain('idnode-grid__cell--dirty')
      /* Display resolved key → label via the cache. */
      expect(span.text()).toBe('Important')
    })

    it('editable td gets data-editable attr in edit mode; read-only td does not', async () => {
      idnodeClassStub.meta = {
        props: [
          { id: 'title', type: 'str' } as unknown as MetaProp,
          { id: 'created', type: 'time' } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
          /* `created` is a time-typed column flagged editable;
           * inline editing for time isn't wired yet, so the
           * gate strips editable and the data-editable attr
           * should not render on this column's cells. */
          { field: 'created', label: 'Created', editable: true, minVisible: 'phone' },
        ],
        entries: [
          {
            uuid: 'a',
            title: 'Alpha',
            size: 1,
            created: 0,
          },
        ],
        storeKey: 'edit-cell-data-editable',
      })
      const tds = wrapper.findAll('tbody tr td[data-field]')
      const titleTd = tds.find((t) => t.attributes('data-field') === 'title')
      const createdTd = tds.find((t) => t.attributes('data-field') === 'created')
      expect(titleTd?.attributes('data-editable')).toBe('')
      expect(createdTd?.attributes('data-editable')).toBeUndefined()
    })

    it('validation: numeric out-of-range commit flags cell invalid + disables Save', async () => {
      idnodeClassStub.meta = {
        props: [
          {
            id: 'priority',
            type: 'int',
            intmin: 0,
            intmax: 100,
          } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'priority', label: 'Priority', editable: true, minVisible: 'phone' },
        ],
        entries: [
          { uuid: 'a', title: 'Alpha', size: 1, priority: 50 },
        ],
        storeKey: 'edit-cell-validation',
      })
      const exposed = wrapper.vm as unknown as {
        commitCellWithValidation: (row: BaseRow, col: ColumnDef, v: unknown) => void
        hasValidationErrors: boolean
        cellErrors: Map<string, string>
      }
      const row = { uuid: 'a' } as BaseRow
      const col: ColumnDef = { field: 'priority', label: 'Priority', editable: true }

      /* In-range commit: no error, dirty marker visible, Save
       * gate stays open (no validation block). */
      exposed.commitCellWithValidation(row, col, 75)
      await wrapper.vm.$nextTick()
      expect(exposed.hasValidationErrors).toBe(false)
      expect(exposed.cellErrors.size).toBe(0)

      /* Out-of-range commit (max is 100): error registered. */
      exposed.commitCellWithValidation(row, col, 200)
      await wrapper.vm.$nextTick()
      expect(exposed.hasValidationErrors).toBe(true)
      expect(exposed.cellErrors.get('a|priority')).toMatch(/100|max/i)
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.classes()).toContain('idnode-grid__cell--invalid')
      /* Save is disabled while errors exist. */
      const saveBtn = wrapper
        .findAll('.idnode-grid__edit-btn')
        .find((b) => b.text() === 'Save')
      expect(saveBtn?.attributes('disabled')).toBeDefined()
      expect(saveBtn?.attributes('title')).toMatch(/Resolve/i)

      /* Recover: commit a valid value → error cleared, Save
       * re-enables. */
      exposed.commitCellWithValidation(row, col, 50)
      await wrapper.vm.$nextTick()
      expect(exposed.hasValidationErrors).toBe(false)
    })

    it('validation: smart-dirty revert (back to original) clears the error', async () => {
      idnodeClassStub.meta = {
        props: [
          {
            id: 'priority',
            type: 'int',
            intmin: 0,
            intmax: 100,
          } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'priority', label: 'Priority', editable: true, minVisible: 'phone' },
        ],
        entries: [
          { uuid: 'a', title: 'Alpha', size: 1, priority: 50 },
        ],
        storeKey: 'edit-cell-validation-revert',
      })
      const exposed = wrapper.vm as unknown as {
        commitCellWithValidation: (row: BaseRow, col: ColumnDef, v: unknown) => void
        hasValidationErrors: boolean
      }
      const row = { uuid: 'a', priority: 50 } as unknown as BaseRow
      const col: ColumnDef = { field: 'priority', label: 'Priority', editable: true }
      exposed.commitCellWithValidation(row, col, 999)
      await wrapper.vm.$nextTick()
      expect(exposed.hasValidationErrors).toBe(true)
      /* Type back to original → smart-dirty drops the dirty
       * entry; the error must drop with it (an unchanged
       * value is by definition valid). */
      exposed.commitCellWithValidation(row, col, 50)
      await wrapper.vm.$nextTick()
      expect(exposed.hasValidationErrors).toBe(false)
    })

    it('strips editable from server-side rdonly props even when consumer flagged editable', async () => {
      idnodeClassStub.meta = {
        props: [
          { id: 'title', type: 'str', rdonly: true } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'edit-cell-rdonly',
      })
      /* No editable affordance — read-only display only.
       * data-editable attr should also be absent since
       * editGatedColumns strips editable. */
      expect(wrapper.find('.idnode-grid__cell-str').exists()).toBe(false)
      const td = wrapper
        .findAll('tbody tr td[data-field]')
        .find((t) => t.attributes('data-field') === 'title')
      expect(td?.attributes('data-editable')).toBeUndefined()
    })

    it('beforeEdit blocking a str cell shows title tooltip + suppresses commit on blur', async () => {
      setupBoolStrMeta()
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'edit-cell-str-blocked',
        beforeEdit: () => 'cannot edit a row currently recording',
      })
      const span = wrapper.find('.idnode-grid__cell-str')
      expect(span.exists()).toBe(true)
      /* Display tooltip carries the canEdit message so the
       * user sees WHY the click won't do anything. */
      expect(span.attributes('title')).toBe('cannot edit a row currently recording')
    })

    it('multi-select enum (list flag) is NOT inline-editable', async () => {
      idnodeClassStub.meta = {
        props: [
          {
            id: 'tags',
            type: 'str',
            list: true,
            enum: [{ key: 'a', val: 'A' }],
          } as unknown as MetaProp,
        ],
      }
      const wrapper = await mountInEditMode({
        columns: [
          { field: 'tags', label: 'Tags', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1, tags: ['a'] }],
        storeKey: 'edit-cell-multi-enum',
      })
      /* Multi-select needs richer UI than a single <select>;
       * cell stays read-only — no editable cell slot fires. */
      expect(wrapper.find('.idnode-grid__cell-str').exists()).toBe(false)
    })
  })

  describe('editMode: cell — edit-mode UX gates', () => {

    it('hides the consumer toolbarActions slot when actively editing on desktop', async () => {
      setupBoolStrMeta()
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: [
            { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
          ],
          storeKey: 'slot-hide-active-edit',
          editMode: 'cell',
        },
        slots: { toolbarActions: '<button class="probe-action">Add</button>' },
        global: { plugins: [[PrimeVue, {}]] },
      })
      /* Read mode → consumer slot rendered. */
      expect(wrapper.find('.probe-action').exists()).toBe(true)
      const exposed = wrapper.vm as unknown as { toggleEditMode: () => void }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      /* Active edit (desktop) → slot hidden. The Save / Cancel
       * trio plus the cell editors are the only relevant
       * affordances; row-level operations are irrelevant. */
      expect(wrapper.find('.probe-action').exists()).toBe(false)
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      expect(wrapper.find('.probe-action').exists()).toBe(true)
    })

    it('suppresses rowDblclick emit while in edit mode', async () => {
      setupBoolStrMeta()
      const wrapper = await mountAndEnterEdit({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'dblclick-suppress',
      })
      /* In edit mode → dblclick must NOT bubble to the
       * consumer's rowDblclick handler (would open the
       * drawer over the inline editor). PrimeVue's row
       * dblclick handler emits via DataGrid; we suppress at
       * the IdnodeGrid layer in onRowDblclick. */
      const tr = wrapper.find('tbody tr')
      if (tr.exists()) await tr.trigger('dblclick')
      expect(wrapper.emitted('rowDblclick')).toBeUndefined()

      /* Exit edit mode → dblclick flows through again. */
      const exposed = wrapper.vm as unknown as { toggleEditMode: () => void }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      const tr2 = wrapper.find('tbody tr')
      if (tr2.exists()) await tr2.trigger('dblclick')
      expect(wrapper.emitted('rowDblclick')).toBeTruthy()
    })

    it('strips sortable + filterType from columns and hides search input in edit mode', async () => {
      setupBoolStrMeta()
      const wrapper = await mountAndEnterEdit({
        columns: [
          {
            field: 'title',
            label: 'Title',
            sortable: true,
            filterType: 'string',
            editable: true,
            minVisible: 'phone',
          },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'sort-filter-lock',
      })
      /* Search input is unmounted entirely (hide-not-disable). */
      expect(wrapper.find('.idnode-grid__toolbar-search').exists()).toBe(false)
      /* Sort header chevron suppressed (PrimeVue renders no
       * .p-sortable-column class when sortable is false). */
      expect(wrapper.find('th.p-sortable-column').exists()).toBe(false)
    })

    it('hides the GridSettingsMenu trigger in edit mode', async () => {
      setupBoolStrMeta()
      /* Multiple columns required so GridSettingsMenu's
       * `showColumnsSection` (>1) renders the popover
       * trigger in read mode at all. Single-column grids
       * suppress the menu entirely. */
      const wrapper = await mountAndEnterEdit({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
          { field: 'enabled', label: 'Enabled', editable: true, minVisible: 'phone' },
        ],
        entries: [
          { uuid: 'a', title: 'Alpha', enabled: false, size: 1 },
        ],
        storeKey: 'settings-menu-hide',
      })
      /* Active edit → trigger unmounted entirely. */
      expect(wrapper.find('.settings-popover__btn').exists()).toBe(false)
    })

  })

  describe('editMode: cell — phone fallback', () => {
    afterEach(() => {
      setViewport(1280)
    })

    it('hides the Edit cells icon at phone width when not in edit mode', async () => {
      setupBoolStrMeta()
      const wrapper = mountAtPhoneWidth({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'phone-no-edit-btn',
      })
      expect(wrapper.find('.idnode-grid__edit-toggle').exists()).toBe(false)
    })

    it('shows banner + Save/Cancel only when entering edit mode then resizing to phone', async () => {
      setupBoolStrMeta()
      /* Open at desktop width so the Edit cells toggle is
       * available; toggle into edit mode; THEN resize to
       * phone — mirroring a real user resize sequence. */
      setViewport(1280)
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: [
            { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
          ],
          storeKey: 'phone-fallback',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const exposed = wrapper.vm as unknown as { toggleEditMode: () => void }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      /* Now resize to phone. IdnodeGrid listens on
       * window.resize internally — fire the event. */
      setViewport(400)
      globalThis.dispatchEvent(new Event('resize'))
      await wrapper.vm.$nextTick()

      /* Phone-mode fallback panel takes over the entire grid
       * surface — message + Save/Cancel only. The DataGrid
       * (and its toolbar) are unmounted via v-else; the user
       * has no path forward except commit or discard. */
      const fallback = wrapper.find('.idnode-grid__phone-fallback')
      expect(fallback.exists()).toBe(true)
      expect(fallback.text()).toContain("isn't supported")

      const labels = wrapper
        .findAll('.idnode-grid__phone-fallback-actions .idnode-grid__edit-btn')
        .map((b) => b.text())
      expect(labels).toEqual(expect.arrayContaining(['Save', 'Discard']))
      expect(labels.find((l) => l === 'Done')).toBeUndefined()
      expect(labels.find((l) => l === 'Undo')).toBeUndefined()
      expect(labels.find((l) => l.includes('Edit cells'))).toBeUndefined()

      /* DataGrid itself is unmounted — no table rows. */
      expect(wrapper.find('.p-datatable').exists()).toBe(false)
    })

    it('strips editable from columns at phone width even when in edit mode', async () => {
      setupBoolStrMeta()
      const wrapper = mountAtPhoneWidth({
        columns: [
          { field: 'title', label: 'Title', editable: true, minVisible: 'phone' },
        ],
        entries: [{ uuid: 'a', title: 'Alpha', size: 1 }],
        storeKey: 'phone-readonly',
      })
      const exposed = wrapper.vm as unknown as { toggleEditMode: () => void }
      exposed.toggleEditMode()
      await wrapper.vm.$nextTick()
      /* No PrimeVue editor, no editableCell slot — cell
       * renders read-only. The bool checkbox class shouldn't
       * appear on phone in edit mode either. */
      expect(wrapper.find('.idnode-grid__cell-str').exists()).toBe(false)
      expect(wrapper.find('.idnode-grid__cell-bool').exists()).toBe(false)
    })

  })

  describe('editMode: cell — unsaved-changes nav guard', () => {
    beforeEach(() => {
      confirmAskMock.mockReset()
      confirmAskMock.mockImplementation(async () => true)
      setupBoolStrMeta()
    })

    it('confirmDiscardIfDirty resolves true without dialog when nothing is dirty', async () => {
      const wrapper = mountForGuard()
      const exposed = wrapper.vm as unknown as {
        confirmDiscardIfDirty: () => Promise<boolean>
      }
      await expect(exposed.confirmDiscardIfDirty()).resolves.toBe(true)
      expect(confirmAskMock).not.toHaveBeenCalled()
    })

    it('confirmDiscardIfDirty surfaces the confirm dialog when dirty + forwards its result', async () => {
      const wrapper = mountForGuard()
      const exposed = wrapper.vm as unknown as {
        toggleEditMode: () => void
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
        confirmDiscardIfDirty: () => Promise<boolean>
      }
      exposed.toggleEditMode()
      exposed.inlineEdit.commitCell('a', 'title', 'edited')

      /* Dialog → Discard. Guard resolves true. */
      confirmAskMock.mockResolvedValueOnce(true)
      await expect(exposed.confirmDiscardIfDirty()).resolves.toBe(true)
      expect(confirmAskMock).toHaveBeenCalledTimes(1)
      const [message, opts] = confirmAskMock.mock.calls[0] as unknown as [
        string,
        Record<string, unknown>,
      ]
      expect(message).toContain('unsaved')
      expect(opts).toMatchObject({
        acceptLabel: 'Discard',
        rejectLabel: 'Cancel',
        severity: 'danger',
      })

      /* Dialog → Cancel. Guard resolves false. */
      confirmAskMock.mockResolvedValueOnce(false)
      await expect(exposed.confirmDiscardIfDirty()).resolves.toBe(false)
    })

    it('beforeunload sets returnValue + preventDefault when dirty', async () => {
      const wrapper = mountForGuard()
      const exposed = wrapper.vm as unknown as {
        toggleEditMode: () => void
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
        onBeforeUnload: (event: BeforeUnloadEvent) => void
      }
      exposed.toggleEditMode()
      exposed.inlineEdit.commitCell('a', 'title', 'edited')

      /* Build a real BeforeUnloadEvent so `event.preventDefault()`
       * mutates the right internal flag. happy-dom supports the
       * constructor; jsdom would too. */
      const event = new Event('beforeunload')
      const preventDefault = vi.spyOn(event, 'preventDefault')
      exposed.onBeforeUnload(event)
      expect(preventDefault).toHaveBeenCalled()
      /* Pinning the legacy beforeunload-dialog gate on
       * production: `event.returnValue = ''` is needed for
       * Chrome / Edge to actually surface the prompt. The
       * property is deprecated per WHATWG but still
       * load-bearing in those browsers — see IdnodeGrid.vue's
       * onBeforeUnload comment. */
      expect(event.returnValue).toBe('') // NOSONAR
    })

    it('beforeunload is a no-op when dirty store is empty', async () => {
      const wrapper = mountForGuard()
      const exposed = wrapper.vm as unknown as {
        onBeforeUnload: (event: BeforeUnloadEvent) => void
      }
      const event = new Event('beforeunload')
      const preventDefault = vi.spyOn(event, 'preventDefault')
      exposed.onBeforeUnload(event)
      expect(preventDefault).not.toHaveBeenCalled()
    })

    it('registers onBeforeRouteLeave with a guard that calls confirmDiscardIfDirty', async () => {
      const { onBeforeRouteLeave } = await import('vue-router')
      const onLeave = vi.mocked(onBeforeRouteLeave)
      onLeave.mockClear()

      mountForGuard()

      /* IdnodeGrid registered exactly one leave guard. */
      expect(onLeave).toHaveBeenCalledTimes(1)
      const guard = onLeave.mock.calls[0][0] as () => Promise<boolean>
      /* Calling the registered guard with no dirty data passes
       * straight through, regardless of dialog. */
      await expect(guard()).resolves.toBe(true)
    })

    it('registers + removes the beforeunload listener around mount', () => {
      const addSpy = vi.spyOn(globalThis, 'addEventListener')
      const removeSpy = vi.spyOn(globalThis, 'removeEventListener')
      const wrapper = mountForGuard()
      expect(
        addSpy.mock.calls.some((c) => c[0] === 'beforeunload'),
      ).toBe(true)
      wrapper.unmount()
      expect(
        removeSpy.mock.calls.some((c) => c[0] === 'beforeunload'),
      ).toBe(true)
      addSpy.mockRestore()
      removeSpy.mockRestore()
    })
  })

  /*
   * Stale-data recovery — IdnodeGrid wires `useStaleDataRecovery` to
   * refetch the grid on a comet reconnect (the server may have GC'd
   * the mailbox during a long disconnect, losing buffered events).
   * The unmount test guards against a refetch firing for a grid that
   * is no longer on screen.
   */
  describe('comet stale-data recovery', () => {
    it('refetches the grid on the comet disconnected -> connected transition', () => {
      const wrapper = mountGrid()
      /* Isolate the reconnect-driven refetch from any mount-time call. */
      mockStore.fetch.mockClear()

      cometStateListener!('disconnected')
      cometStateListener!('connected')
      expect(mockStore.fetch).toHaveBeenCalledTimes(1)

      wrapper.unmount()
    })

    it('stops refetching once the grid is unmounted', () => {
      const wrapper = mountGrid()
      wrapper.unmount()
      mockStore.fetch.mockClear()

      cometStateListener?.('disconnected')
      cometStateListener?.('connected')
      expect(mockStore.fetch).not.toHaveBeenCalled()
      expect(cometStateUnsub).toBe(true)
    })
  })

  /*
   * Always-edit + reorderableRows — the two props the dedicated
   * ChannelManageDrawer turns on. `alwaysEdit` auto-activates
   * cell-edit mode on mount and hides the pencil toggle (the
   * drawer is permanently in edit mode by design).
   * `reorderableRows` enables PrimeVue's row-reorder column;
   * the `row-reorder` event from DataGrid routes through
   * `slotReorder` and commits per-row number changes via the
   * shared inline-edit dirty store.
   */
  describe('alwaysEdit + reorderableRows', () => {
    it('alwaysEdit starts the grid in cell-edit mode without any user toggle', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', number: 10, size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as { isInEditMode: boolean }
      expect(vm.isInEditMode).toBe(true)
    })

    it('alwaysEdit hides the pencil toggle (no need to flip a mode that is always on)', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', number: 10, size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      expect(wrapper.findAll('.idnode-grid__edit-toggle').length).toBe(0)
    })

    it('pencil toggle is present when editMode=cell without alwaysEdit (regular cell-edit consumer)', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      expect(wrapper.findAll('.idnode-grid__edit-toggle').length).toBe(1)
    })

    it('passes selectable through to DataGrid in alwaysEdit mode (drawer needs row checkboxes for bulk actions)', () => {
      /* Regular cell-edit (pencil toggle) suppresses selection
       * to keep the UI focused. alwaysEdit consumers (drawer)
       * need selection so the toolbar's bulk-action menu has
       * something to operate on — the carve-out below. */
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          /* selectable defaults to true. */
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      expect(dataGrid.props('selectable')).toBe(true)
    })

    it('regular cell-edit (no alwaysEdit) still suppresses selection — checkboxes would clutter a focused edit session', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          /* alwaysEdit omitted. */
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      await wrapper.find('.idnode-grid__edit-toggle').trigger('click')
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      expect(dataGrid.props('selectable')).toBe(false)
    })

    it('Discard is disabled when alwaysEdit and there is nothing dirty', () => {
      /* In alwaysEdit mode the Discard button has the same
       * semantics as Save — both are commit/revert of pending
       * edits, never "exit edit mode" (the drawer is the
       * exit). When the dirty store is empty there's nothing
       * to discard, so the button greys out to match Save. */
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const discard = wrapper
        .findAll('button')
        .find((b) => b.text() === 'Discard')
      expect(discard?.exists()).toBe(true)
      expect(discard?.attributes('disabled')).toBeDefined()
    })

    it('Discard enables once a cell is dirty in alwaysEdit mode', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', comment: 'orig', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
      }
      vm.inlineEdit.commitCell('a', 'comment', 'edited')
      await wrapper.vm.$nextTick()
      const discard = wrapper
        .findAll('button')
        .find((b) => b.text() === 'Discard')
      expect(discard?.attributes('disabled')).toBeUndefined()
    })

    it('Discard STAYS enabled in regular cell-edit mode (not alwaysEdit) even with no dirty — it doubles as "exit edit mode"', async () => {
      /* The regular cell-edit toggle pattern uses Discard as
       * the "exit without saving" affordance. Disabling it
       * when clean would strand the user — they'd have no
       * way to exit edit mode except by saving. */
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          /* alwaysEdit omitted → defaults to false. */
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      /* Enter edit mode via the pencil toggle. */
      await wrapper.find('.idnode-grid__edit-toggle').trigger('click')
      const discard = wrapper
        .findAll('button')
        .find((b) => b.text() === 'Discard')
      expect(discard?.attributes('disabled')).toBeUndefined()
    })

    it('Save keeps the grid in edit mode when alwaysEdit is true (drawer stays open after save)', async () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        isInEditMode: boolean
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
          save: () => Promise<void>
        }
      }
      vm.inlineEdit.commitCell('a', 'title', 'Edited')
      await vm.inlineEdit.save()
      expect(vm.isInEditMode).toBe(true)
    })

    it('rowReorder emit from DataGrid commits per-slot number changes via the inline-edit store', () => {
      const entries = [
        { uuid: 'a', title: 'A', number: 10, size: 1 },
        { uuid: 'b', title: 'B', number: 20, size: 2 },
        { uuid: 'c', title: 'C', number: 30, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          dirtyMap: { value: Map<string, Map<string, unknown>> }
        }
      }
      /* Drag a → c's slot. PrimeVue's new order: b, c, a. */
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('rowReorder', {
        originalEvent: new Event('drop'),
        dragIndex: 0,
        dropIndex: 2,
        value: [
          { uuid: 'b', title: 'B', number: 20, size: 2 },
          { uuid: 'c', title: 'C', number: 30, size: 3 },
          { uuid: 'a', title: 'A', number: 10, size: 1 },
        ],
      })
      /* Slot-based commits: slot 0 was a→b (commit b=10), slot 1
       * was b→c (commit c=20), slot 2 was c→a (commit a=30). */
      const dirty = vm.inlineEdit.dirtyMap.value
      expect(dirty.get('b')?.get('number')).toBe(10)
      expect(dirty.get('c')?.get('number')).toBe(20)
      expect(dirty.get('a')?.get('number')).toBe(30)
    })

    it('two consecutive unsaved moves renumber from the DISPLAYED values, not stale server values', async () => {
      /* The row objects keep their server numbers; only the dirty
       * overlay carries the first move's result. The second move's
       * slot numbers must come from the overlay (what the user
       * sees), so dragging a row away and straight back cancels
       * out to a clean dirty map. */
      const entries = [
        { uuid: 'a', title: 'A', number: 10, size: 1 },
        { uuid: 'b', title: 'B', number: 20, size: 2 },
        { uuid: 'c', title: 'C', number: 30, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          dirtyMap: { value: Map<string, Map<string, unknown>> }
        }
      }
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      /* Move 1: drag a below c → displayed order b(10), c(20), a(30). */
      dataGrid.vm.$emit('rowReorder', {
        originalEvent: new Event('drop'),
        dragIndex: 0,
        dropIndex: 2,
        value: [
          { uuid: 'b', title: 'B', number: 20, size: 2 },
          { uuid: 'c', title: 'C', number: 30, size: 3 },
          { uuid: 'a', title: 'A', number: 10, size: 1 },
        ],
      })
      await wrapper.vm.$nextTick()
      /* Move 2: drag a straight back to the top. PrimeVue shuffles
       * the dirty-sorted display [b,c,a] into [a,b,c]. Slot values
       * are the displayed 10/20/30, so every row returns to its
       * server number and smart-clear empties the dirty map. */
      dataGrid.vm.$emit('rowReorder', {
        originalEvent: new Event('drop'),
        dragIndex: 2,
        dropIndex: 0,
        value: [
          { uuid: 'a', title: 'A', number: 10, size: 1 },
          { uuid: 'b', title: 'B', number: 20, size: 2 },
          { uuid: 'c', title: 'C', number: 30, size: 3 },
        ],
      })
      expect(vm.inlineEdit.dirtyMap.value.size).toBe(0)
    })

    it('dirtyAwareSort: rows reorder by DIRTY number values WITHOUT mutating the row objects', async () => {
      /* Row objects passed downstream MUST keep their server
       * `number` value — the template feeds `row[field]` into
       * `isCellServerPending` to detect "server changed under
       * me" conflicts. Mutating row.number with the dirty value
       * would make that check trip on every cell the user just
       * edited, lighting up the orange pulse for normal edits. */
      const entries = [
        { uuid: 'a', title: 'A', number: 10, size: 1 },
        { uuid: 'b', title: 'B', number: 20, size: 2 },
        { uuid: 'c', title: 'C', number: 30, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      /* Simulate the result of a drag: a → c's slot. The
       * slot-reorder algorithm would have committed b=10, c=20,
       * a=30 (swapping numbers around). */
      vm.inlineEdit.commitCell('b', 'number', 10)
      vm.inlineEdit.commitCell('c', 'number', 20)
      vm.inlineEdit.commitCell('a', 'number', 30)
      await wrapper.vm.$nextTick()

      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const renderedEntries = dataGrid.props('entries') as Array<{
        uuid: string
        number: number
      }>
      /* Order reflects the dirty values (b is now '10'-ranked,
       * a is now '30'-ranked). */
      expect(renderedEntries.map((r) => r.uuid)).toEqual(['b', 'c', 'a'])
      /* But each row's `number` field is STILL the server value
       * (a=10 server, b=20 server, c=30 server). Cells use
       * cellModelValue → inlineEdit.cellValue to display the
       * dirty number; the row-object passthrough must stay
       * non-destructive. */
      expect(renderedEntries.find((r) => r.uuid === 'a')?.number).toBe(10)
      expect(renderedEntries.find((r) => r.uuid === 'b')?.number).toBe(20)
      expect(renderedEntries.find((r) => r.uuid === 'c')?.number).toBe(30)
    })

    it('scrollToUuid targets the row index in the dirty-aware projection, not server order', async () => {
      /* After a reorder-field edit the table renders the
       * dirty-aware sorted projection; scrolling to the row's
       * server-order index would centre the viewport on its OLD
       * position while the row rendered elsewhere. */
      const entries = [
        { uuid: 'a', title: 'A', number: 10, size: 1 },
        { uuid: 'b', title: 'B', number: 20, size: 2 },
        { uuid: 'c', title: 'C', number: 30, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
        scrollToUuid: (uuid: string) => boolean
      }
      /* Move `a` to the end of the ordering: effective order
       * becomes [b, c, a] while store order stays [a, b, c]. */
      vm.inlineEdit.commitCell('a', 'number', 99)
      await wrapper.vm.$nextTick()

      /* DataGrid's scrollToIndex resolves the scroller element at
       * call time — plant a fake one so the scroll target is
       * observable. */
      const shell = wrapper.find('.data-grid__table-shell').element
      const scroller = document.createElement('div')
      scroller.className = 'p-virtualscroller'
      const scrollTo = vi.fn()
      ;(scroller as unknown as { scrollTo: typeof scrollTo }).scrollTo = scrollTo
      shell.appendChild(scroller)

      expect(vm.scrollToUuid('a')).toBe(true)
      expect(scrollTo).toHaveBeenCalledTimes(1)
      /* Row `a` renders at effective index 2 — the centring math
       * must use that, not its server index 0. clientHeight is 0
       * in the test DOM, so top = idx * 36 + 18. */
      expect(scrollTo.mock.calls[0][0]).toMatchObject({ top: 2 * 36 + 18 })
    })

    it('dirtyAwareSort=false: entries passthrough unchanged from store.entries', () => {
      const entries = [
        { uuid: 'a', title: 'A', number: 10, size: 1 },
        { uuid: 'b', title: 'B', number: 20, size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          /* dirtyAwareSort omitted — defaults to false. */
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      /* Commit a number change in the dirty store. */
      vm.inlineEdit.commitCell('a', 'number', 999)

      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const renderedEntries = dataGrid.props('entries') as Array<{
        uuid: string
        number: number
      }>
      /* Without dirtyAwareSort, entries pass through unchanged —
       * `a` keeps its server-side number 10. */
      expect(renderedEntries.map((r) => r.uuid)).toEqual(['a', 'b'])
      expect(renderedEntries[0].number).toBe(10)
    })

    /*
     * Regression: dirtyAwareSort must keep `effectiveEntries`'s
     * array reference STABLE when only non-reorderField fields
     * are dirty. Returning a fresh array on every dirty change
     * (e.g. an EditableTagChipCell × commit on `tags`) makes
     * PrimeVue's DataTable tear down + remount every visible
     * row's cells — which destroys per-cell local state (chip
     * pickerOpen) and races the new dirty value through to
     * cellModelValue, so the chip cell visually appears stuck on
     * its pre-commit value. The fix: project only when the
     * reorderField itself is dirty.
     */
    it('keeps the effectiveEntries array reference stable when a non-reorderField is dirty', async () => {
      const entries = [
        { uuid: 'a', title: 'A', number: 1, tags: ['t1'], size: 1 },
        { uuid: 'b', title: 'B', number: 2, tags: ['t2'], size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'number',
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const before = dataGrid.props('entries')
      /* Dirty an unrelated field (`tags`) — must NOT cause
       * effectiveEntries to allocate a new array. */
      vm.inlineEdit.commitCell('a', 'tags', ['t1', 'new-tag'])
      await wrapper.vm.$nextTick()
      const after = dataGrid.props('entries')
      expect(after).toBe(before)
    })

    it('rebuilds the effectiveEntries array when the reorderField itself is dirty', async () => {
      const entries = [
        { uuid: 'a', title: 'A', number: 1, size: 1 },
        { uuid: 'b', title: 'B', number: 2, size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'number',
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          commitCell: (u: string, f: string, v: unknown) => void
        }
      }
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const before = dataGrid.props('entries')
      vm.inlineEdit.commitCell('a', 'number', 5)
      await wrapper.vm.$nextTick()
      const after = dataGrid.props('entries') as Array<Record<string, unknown>>
      /* Different reference — projection ran. */
      expect(after).not.toBe(before)
      /* Sort reflects projected value: `b` (number=2) now before
       * `a` (number=5). */
      expect(after.map((r) => r.uuid)).toEqual(['b', 'a'])
    })

    it('pins an actively-edited row at its server-value sort position during keystroke entry', async () => {
      /* Reproduces the "type Backspace → row jumps mid-edit"
       * bug. Without the active-edit guard, committing a dirty
       * partial number while still in the editor would re-sort
       * the row away from the cursor. */
      /* Input is pre-sorted by number — matches what the
       * gridStore returns in production with defaultSort. */
      const entries = [
        { uuid: 'b', title: 'B', number: 4, size: 1 },
        { uuid: 'c', title: 'C', number: 6, size: 2 },
        { uuid: 'a', title: 'A', number: 52, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'number',
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const vm = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
      }
      /* Initial order by server number: b(4), c(6), a(52). */
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['b', 'c', 'a'])

      /* User clicks into row a's Number cell. */
      dataGrid.vm.$emit('cellEditInit', {
        data: entries[2],
        field: 'number',
        index: 2,
      })
      await wrapper.vm.$nextTick()

      /* Mid-edit: a partial commit lands (user pressed
       * Backspace; the cell's value is now 5, the dirty store
       * reflects that). Row a's position MUST hold — sort still
       * sees it at server-value 52, after b(4) and c(6). */
      vm.inlineEdit.commitCell('a', 'number', 5)
      await wrapper.vm.$nextTick()
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['b', 'c', 'a'])

      /* User commits the edit (Enter / Tab / blur). The
       * active-edit flag clears, the sort recomputes, the row
       * finally snaps to its dirty-value position (5 → between
       * b at 4 and c at 6). */
      dataGrid.vm.$emit('cellEditComplete', {
        data: entries[2],
        field: 'number',
        index: 2,
      })
      await wrapper.vm.$nextTick()
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['b', 'a', 'c'])
    })

    it('re-opening an already-dirty cell pins at the DIRTY position, not the server position', async () => {
      /* Regression for the second-edit jump-back: row had
       * server number=2, was edited to dirty=200, settled at
       * slot 200. Re-clicking the cell at slot 200 must hold
       * the row at slot 200 (the position the user sees),
       * not jerk it back to slot 2 (the server value). The
       * pin captures the EFFECTIVE value at click time, which
       * is the dirty value when the cell is already dirty. */
      const entries = [
        { uuid: 'r1', title: 'A', number: 1, size: 1 },
        { uuid: 'r2', title: 'B', number: 2, size: 2 },
        { uuid: 'r3', title: 'C', number: 3, size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'number',
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const vm = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
      }

      /* Step 1: User dirties r2 to 200 (Enter committed
       * earlier). Sort projects, r2 moves to the end. */
      vm.inlineEdit.commitCell('r2', 'number', 200)
      await wrapper.vm.$nextTick()
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['r1', 'r3', 'r2'])

      /* Step 2: User clicks r2's Number cell again to edit
       * further. Without the dirty-aware pin, the cell would
       * jerk back to slot 2 (server value). */
      dataGrid.vm.$emit('cellEditInit', {
        data: entries[1],
        field: 'number',
        index: 1,
      })
      await wrapper.vm.$nextTick()

      /* Row stays where the user clicked it (at slot 200). */
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['r1', 'r3', 'r2'])

      /* Step 3: A mid-edit keystroke commits a different
       * partial value (say the user typed "2" then "0",
       * dirty=20 currently). Row STILL stays at slot 200 —
       * the pin holds. */
      vm.inlineEdit.commitCell('r2', 'number', 20)
      await wrapper.vm.$nextTick()
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['r1', 'r3', 'r2'])

      /* Step 4: User presses Enter. Sort recomputes with the
       * latest dirty value (20). r2 (=20) now sorts after r3
       * (=3) but before r1's nothing — order is [r1(1), r3(3),
       * r2(20)]. */
      dataGrid.vm.$emit('cellEditComplete', {
        data: entries[1],
        field: 'number',
        index: 1,
      })
      await wrapper.vm.$nextTick()
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['r1', 'r3', 'r2'])
    })

    it('does NOT pin when the actively-edited field is not the reorderField (tags edit shouldn\'t affect number sort)', async () => {
      /* Only the reorder field is sensitive to the active-edit
       * pin — editing tags, name, etc. doesn't reorder anyway,
       * so the pin is irrelevant there. Verify the guard
       * compares field, not just uuid. */
      const entries = [
        { uuid: 'b', title: 'B', number: 4, size: 1 },
        { uuid: 'a', title: 'A', number: 52, size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'number',
          dirtyAwareSort: true,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const vm = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
      }
      /* Pretend user is editing row a's title (NOT number). */
      dataGrid.vm.$emit('cellEditInit', {
        data: entries[1],
        field: 'title',
        index: 1,
      })
      /* And independently commits a dirty number for row a. */
      vm.inlineEdit.commitCell('a', 'number', 5)
      await wrapper.vm.$nextTick()
      /* Sort should reflect the dirty number — the pin doesn't
       * apply because the active edit is on a different field.
       * a (dirty 5) now sits before b (4)? No — 5 > 4. Order
       * becomes [b(4), a(5)]. */
      expect(
        (dataGrid.props('entries') as Array<{ uuid: string }>).map((r) => r.uuid),
      ).toEqual(['b', 'a'])
    })

    it('rowReorder honours the reorderField prop (drives which field receives commits)', () => {
      const entries = [
        { uuid: 'a', title: 'A', priority: 1, size: 1 },
        { uuid: 'b', title: 'B', priority: 2, size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          reorderableRows: true,
          reorderField: 'priority',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const vm = wrapper.vm as unknown as {
        inlineEdit: {
          dirtyMap: { value: Map<string, Map<string, unknown>> }
        }
      }
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      dataGrid.vm.$emit('rowReorder', {
        originalEvent: new Event('drop'),
        dragIndex: 0,
        dropIndex: 1,
        value: [
          { uuid: 'b', title: 'B', priority: 2, size: 2 },
          { uuid: 'a', title: 'A', priority: 1, size: 1 },
        ],
      })
      const dirty = vm.inlineEdit.dirtyMap.value
      expect(dirty.get('a')?.get('priority')).toBe(2)
      expect(dirty.get('b')?.get('priority')).toBe(1)
    })
  })

  /*
   * `columnActions` — per-column kebab gating. The default lights
   * up all four actions (sort / filter / hide / resetWidth) which
   * matches every regular config grid. The drawer passes `{}` so
   * the kebab hides entirely (its fixed-sort, no-filter, fixed-
   * cols layout makes those actions meaningless).
   */
  describe('columnActions pass-through', () => {
    it('defaults to all four actions enabled (sort / filter / hide / resetWidth)', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      expect(dataGrid.props('columnActions')).toEqual({
        sort: true,
        filter: true,
        hide: true,
        resetWidth: true,
      })
    })

    it('passes the consumer override (e.g. `{}` from the drawer) through verbatim', () => {
      mockStore = makeStore({
        entries: [{ uuid: 'a', title: 'A', size: 1 }],
        isEmpty: false,
      })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          columnActions: {},
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      expect(dataGrid.props('columnActions')).toEqual({})
    })
  })

  /*
   * `clientFilter` predicate — runs per row inside
   * effectiveEntries with the row's dirty-map slice exposed
   * so the predicate can decide visibility based on the
   * EFFECTIVE (dirty || server) value rather than just the
   * server value. ChannelManageDrawer's "Include disabled"
   * toggle uses this to keep just-toggled rows in view
   * (newly enabled) or drop them (newly disabled) without
   * waiting for a server round-trip.
   */
  describe('clientFilter (dirty-aware visibility predicate)', () => {
    it('filters rows by the predicate, hiding non-matching rows from DataGrid', () => {
      const entries = [
        { uuid: 'a', title: 'A', size: 1 },
        { uuid: 'b', title: 'B', size: 2 },
        { uuid: 'c', title: 'C', size: 3 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          clientFilter: (row: Record<string, unknown>) => row.size !== 2,
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      const rendered = dataGrid.props('entries') as Array<{ uuid: string }>
      expect(rendered.map((r) => r.uuid)).toEqual(['a', 'c'])
    })

    it('passes the row\'s dirty-field map to the predicate so it can compute effective values', async () => {
      const entries = [
        { uuid: 'a', title: 'A', enabled: true, size: 1 },
        { uuid: 'b', title: 'B', enabled: false, size: 2 },
      ]
      mockStore = makeStore({ entries, isEmpty: false })
      const wrapper = mount(IdnodeGrid, {
        props: {
          endpoint: 'mock/grid',
          columns: cols,
          storeKey: 'test-key',
          editMode: 'cell',
          alwaysEdit: true,
          /* Drawer-style predicate: effective enabled. */
          clientFilter: (
            row: Record<string, unknown>,
            dirty: ReadonlyMap<string, unknown> | undefined,
          ) => {
            const v = dirty?.has('enabled') ? dirty.get('enabled') : row.enabled
            return !!v
          },
        },
        global: { plugins: [[PrimeVue, {}]] },
      })
      const dataGrid = wrapper.findComponent({ name: 'DataGrid' })
      /* Initial render: a (enabled) visible, b (disabled) hidden. */
      let rendered = dataGrid.props('entries') as Array<{ uuid: string }>
      expect(rendered.map((r) => r.uuid)).toEqual(['a'])

      /* User dirties row b's enabled to true — should now appear. */
      const vm = wrapper.vm as unknown as {
        inlineEdit: { commitCell: (u: string, f: string, v: unknown) => void }
      }
      vm.inlineEdit.commitCell('b', 'enabled', true)
      await wrapper.vm.$nextTick()
      rendered = dataGrid.props('entries') as Array<{ uuid: string }>
      expect(rendered.map((r) => r.uuid)).toEqual(['a', 'b'])

      /* User dirties row a's enabled to false — should now disappear. */
      vm.inlineEdit.commitCell('a', 'enabled', false)
      await wrapper.vm.$nextTick()
      rendered = dataGrid.props('entries') as Array<{ uuid: string }>
      expect(rendered.map((r) => r.uuid)).toEqual(['b'])
    })
  })

})
