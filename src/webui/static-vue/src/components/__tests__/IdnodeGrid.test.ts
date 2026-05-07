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
import type { UiLevel } from '@/types/access'

interface MockRow extends Record<string, unknown> {
  uuid: string
  title: string
  size: number
}

interface MockFilter {
  field: string
  type: 'string' | 'numeric' | 'boolean'
  value: string | number | boolean
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
    fetch: vi.fn(),
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
    minVisible: 'tablet',
  },
  { field: 'size', label: 'Size', sortable: true, filterType: 'numeric', minVisible: 'desktop' },
]

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
}

function mountGrid(
  slots: Record<string, string> = {},
  propOverrides: Partial<{
    endpoint: string
    columns: ColumnDef[]
    storeKey: string
    lockLevel: UiLevel
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

describe('IdnodeGrid', () => {
  beforeEach(() => {
    mockStore = makeStore()
    /* Reset stubs to permissive defaults: no metadata, expert level. */
    idnodeClassStub.meta = null
    mockAccessUilevel = 'expert'
    mockAccessLocked = false
    setViewport(1280) // desktop by default
    localStorage.clear()
  })

  afterEach(() => {
    localStorage.clear()
  })

  it('calls store.fetch on mount', () => {
    mountGrid()
    expect(mockStore.fetch).toHaveBeenCalledOnce()
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
    const stored = localStorage.getItem('tvh-grid:test-key:cols')
    expect(stored).not.toBeNull()
    const parsed = JSON.parse(stored!) as Record<string, { hidden?: boolean }>
    expect(parsed.size?.hidden).toBe(true)
  })

  it('renders phone toolbar with sort dropdown and search when configured', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const toolbar = wrapper.find('.idnode-grid__toolbar')
    expect(toolbar.exists()).toBe(true)
    expect(toolbar.classes()).toContain('idnode-grid__toolbar--phone')
    expect(wrapper.find('.idnode-grid__phone-select').exists()).toBe(true)
    expect(wrapper.find('.idnode-grid__toolbar-search').exists()).toBe(true)
  })

  it('renders desktop toolbar with search but no sort dropdown', () => {
    setViewport(1280)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const toolbar = wrapper.find('.idnode-grid__toolbar')
    expect(toolbar.exists()).toBe(true)
    expect(toolbar.classes()).not.toContain('idnode-grid__toolbar--phone')
    expect(wrapper.find('.idnode-grid__phone-select').exists()).toBe(false)
    expect(wrapper.find('.idnode-grid__toolbar-search').exists()).toBe(true)
  })

  it('phone sort dropdown defaults to "(no sort)" and disables the direction button', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const select = wrapper.find<HTMLSelectElement>('.idnode-grid__phone-select')
    expect(select.element.value).toBe('')
    const dirBtn = wrapper.find<HTMLButtonElement>('.idnode-grid__phone-sort-dir')
    expect(dirBtn.element.disabled).toBe(true)
  })

  it('phone sort change calls store.update and resets the window to top page', async () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      total: 100,
      limit: 75, // user had loaded extra rows
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const select = wrapper.find<HTMLSelectElement>('.idnode-grid__phone-select')
    await select.setValue('title')
    expect(mockStore.update).toHaveBeenCalledWith({
      sort: { key: 'title', dir: 'ASC' },
      start: 0,
      limit: 25,
    })
  })

  it('phone sort-dir toggle flips direction and resets the window', async () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [{ uuid: 'a', title: 'Alpha', size: 100 }],
      isEmpty: false,
      sort: { key: 'title', dir: 'ASC' },
    })
    const wrapper = mountGrid()
    await wrapper.find('.idnode-grid__phone-sort-dir').trigger('click')
    expect(mockStore.update).toHaveBeenCalledWith({
      sort: { key: 'title', dir: 'DESC' },
      start: 0,
      limit: 25,
    })
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
      const input = wrapper.find<HTMLInputElement>('.idnode-grid__toolbar-search')
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
      const input = wrapper.find<HTMLInputElement>('.idnode-grid__toolbar-search')
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

  it('exposes selection helpers; header count badge shows on desktop only when items are selected', async () => {
    setViewport(1280)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 100 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    expect(wrapper.find('.idnode-grid__head-count').exists()).toBe(false)

    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(row)
    await wrapper.vm.$nextTick()

    expect(vm.selection).toHaveLength(1)
    const count = wrapper.find('.idnode-grid__head-count')
    expect(count.exists()).toBe(true)
    expect(count.text()).toBe('1')

    /* No dedicated "Clear selection" button on desktop any more —
     * the header checkbox (auto-rendered by PrimeVue's selection-
     * mode="multiple") toggles all-vs-none, which is what users
     * actually use to clear. Verify clearing via the exposed
     * helper (the actual UI checkbox is inside PrimeVue's stub
     * which jsdom doesn't fully render). */
    vm.clearSelection()
    await wrapper.vm.$nextTick()
    expect(vm.selection).toHaveLength(0)
    expect(wrapper.find('.idnode-grid__head-count').exists()).toBe(false)
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

  it('phone list header shows entry count when nothing is selected', () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', title: 'Alpha', size: 1 },
        { uuid: 'b', title: 'Beta', size: 2 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const summary = wrapper.find('.idnode-grid__phone-list-summary')
    expect(summary.exists()).toBe(true)
    expect(summary.text()).toContain('2 entries')
    // Header count badge is desktop-only; phone uses the list-header instead.
    expect(wrapper.find('.idnode-grid__head-count').exists()).toBe(false)
  })

  it('phone list header tristate select-all toggles all visible rows', async () => {
    setViewport(400)
    mockStore = makeStore({
      entries: [
        { uuid: 'a', title: 'Alpha', size: 1 },
        { uuid: 'b', title: 'Beta', size: 2 },
      ],
      isEmpty: false,
    })
    const wrapper = mountGrid()
    const headerCheckbox = wrapper.find<HTMLInputElement>('.idnode-grid__phone-list-check input')
    expect(headerCheckbox.element.checked).toBe(false)

    // Select all.
    await headerCheckbox.setValue(true)
    const vm = wrapper.vm as unknown as ExposedVm
    expect(vm.selection).toHaveLength(2)
    expect(wrapper.find('.idnode-grid__phone-list-summary').text()).toContain('All 2 selected')

    // Toggle off — clears selection.
    await headerCheckbox.setValue(false)
    expect(vm.selection).toHaveLength(0)
  })

  it('phone list header summary reflects partial selection state', async () => {
    setViewport(400)
    const a: MockRow = { uuid: 'a', title: 'Alpha', size: 1 }
    const b: MockRow = { uuid: 'b', title: 'Beta', size: 2 }
    mockStore = makeStore({ entries: [a, b], isEmpty: false })
    const wrapper = mountGrid()
    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(a)
    await wrapper.vm.$nextTick()
    expect(wrapper.find('.idnode-grid__phone-list-summary').text()).toContain('1 of 2 selected')
    // Header checkbox should NOT be marked fully checked when partial.
    const headerCheckbox = wrapper.find<HTMLInputElement>('.idnode-grid__phone-list-check input')
    expect(headerCheckbox.element.checked).toBe(false)
  })

  it('phone list header Clear button is shown when items selected', async () => {
    setViewport(400)
    const row: MockRow = { uuid: 'a', title: 'Alpha', size: 1 }
    mockStore = makeStore({ entries: [row], isEmpty: false })
    const wrapper = mountGrid()
    expect(
      wrapper.find('.idnode-grid__phone-list-header .idnode-grid__selection-clear').exists()
    ).toBe(false)
    const vm = wrapper.vm as unknown as ExposedVm
    vm.toggleSelect(row)
    await wrapper.vm.$nextTick()
    const clear = wrapper.find('.idnode-grid__phone-list-header .idnode-grid__selection-clear')
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
    localStorage.setItem('tvh-grid:test-key:cols', JSON.stringify({ size: { hidden: true } }))
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
    expect(localStorage.getItem('tvh-grid:test-key:cols')).toBeNull()
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
})
