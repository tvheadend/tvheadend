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
  { field: 'bps', label: 'Bandwidth', sortable: true, minVisible: 'tablet' },
]

function setViewport(width: number) {
  Object.defineProperty(globalThis, 'innerWidth', {
    writable: true,
    configurable: true,
    value: width,
  })
}

function mountGrid(slots: Record<string, string> = {}) {
  return mount(StatusGrid, {
    props: {
      endpoint: 'status/inputs',
      notificationClass: 'input_status',
      columns: cols,
      keyField: 'uuid' as const,
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
  })

  afterEach(() => {
    vi.clearAllMocks()
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
    /* Bandwidth is minVisible:'tablet', so it's hidden on phone cards. */
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
