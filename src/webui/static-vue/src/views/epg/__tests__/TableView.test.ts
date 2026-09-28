// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * TableView — hand-offs from the command palette and the filters
 * they turn into.
 *
 * The server compiles the EPG channelName filter and the title
 * search as caseless, unanchored regexes. These tests pin down that
 * an exact channel pick goes out as the server's exact `channel`
 * param (never as a channelName regex that would also match
 * "CT 1 HD"), and that a title taken from a known event is
 * regex-escaped for the server while the Search Title box keeps
 * the raw text.
 *
 * `useEpgViewState` is replaced by a minimal fake, `apiCall` and
 * vue-router's `useRoute` are mocked, and the heavy children
 * (DataGrid, EpgTableOptions, the drawer) are stubbed. The DataGrid
 * stub exposes its props and re-emits `filter` / row-group events
 * so the tests drive the same paths the real grid does.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { computed, defineComponent, type DirectiveBinding, h, ref } from 'vue'
import { flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import type { ChannelRow, EpgRow } from '@/composables/useEpgViewState'
import type { EpgViewOptions } from '@/views/epg/epgViewOptions'
import type { FilterDef } from '@/types/grid'

const mocks = vi.hoisted(() => ({
  routeQuery: null as unknown as Record<string, unknown>,
  apiCall: vi.fn(),
  confirmAsk: vi.fn(),
}))

vi.mock('vue-router', async () => {
  const { reactive: vueReactive } = await import('vue')
  mocks.routeQuery = vueReactive({})
  return { useRoute: () => ({ query: mocks.routeQuery }) }
})

vi.mock('@/api/client', () => ({ apiCall: mocks.apiCall }))

vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: mocks.confirmAsk }),
}))

vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: vi.fn(), error: vi.fn(), warn: vi.fn(), info: vi.fn() }),
}))

let fakeState: ReturnType<typeof makeFakeState>

vi.mock('@/composables/useEpgViewState', () => ({
  useEpgViewState: () => fakeState,
}))

import TableView from '../TableView.vue'
import { useAccessStore } from '@/stores/access'

const CHANNELS: ChannelRow[] = [
  { uuid: 'u-ct1', name: 'CT 1', number: 1 },
  { uuid: 'u-ct1hd', name: 'CT 1 HD', number: 2 },
]

function defaultViewOptions(): EpgViewOptions {
  return {
    tagFilter: { tag: null },
    channelDisplay: { logo: true, name: true, number: false },
    channelSort: 'number',
    tooltipMode: 'off',
    density: { timeline: 'default', magazine: 'default' },
    dvrOverlay: 'event',
    dvrOverlayShowDisabled: false,
    progressDisplay: 'bar',
    progressColoured: false,
    stickyTitles: true,
    darkChannelBackground: false,
    columnVisibility: {},
    groupField: null,
    groupOrder: 'ASC',
    titleSearchMode: 'title',
    timeWindow: 'all',
    genre: [],
    newOnly: false,
    durationMinMinutes: null,
    durationMaxMinutes: null,
  }
}

function makeFakeState(opts: { viewOptions?: Partial<EpgViewOptions> } = {}) {
  const viewOptions = ref<EpgViewOptions>({ ...defaultViewOptions(), ...opts.viewOptions })
  const defaults = defaultViewOptions()
  const channels = ref<ChannelRow[]>(CHANNELS)
  return {
    trackStart: computed(() => 1_700_000_000),
    trackEnd: computed(() => 1_700_000_000 + 14 * 86400),
    loadedDays: ref(new Set<number>()),
    loadingDays: ref(new Set<number>()),
    ensureDaysLoaded: vi.fn(),
    saveLastView: vi.fn(),
    channels,
    channelsLoading: ref(false),
    channelsError: ref<Error | null>(null),
    events: ref<EpgRow[]>([]),
    filteredChannels: computed(() => channels.value),
    tags: computed(() => []),
    loading: computed(() => false),
    error: computed(() => null),
    eventsTotalCount: ref(0),
    refreshMatchedCount: vi.fn(),
    loadPage: vi.fn(),
    selectedEvent: computed(() => null),
    toggleDrawer: vi.fn(),
    closeDrawer: vi.fn(),
    viewOptions,
    setViewOptions: vi.fn((next: EpgViewOptions) => {
      viewOptions.value = next
    }),
    currentDefaults: computed(() => defaults),
  }
}

/* v-tooltip stub that keeps the tooltip text in data-tooltip, so
 * tests can read what a hover would show. */
function keepTooltip(el: HTMLElement, binding: DirectiveBinding<string>): void {
  el.dataset.tooltip = binding.value ?? ''
}
const tooltipStub = { mounted: keepTooltip, updated: keepTooltip }

/* DataGrid stub — renders the toolbar-right slot and the empty
 * slot, and keeps the props for assertions. */
const DataGridStub = defineComponent({
  name: 'DataGrid',
  props: {
    entries: { type: Array, default: () => [] },
    filters: { type: Object, default: () => ({}) },
    total: { type: Number, default: 0 },
  },
  emits: ['filter', 'rowgroup-expand', 'rowgroup-collapse'],
  setup(props, { slots }) {
    return () =>
      h('div', { class: 'data-grid-stub' }, [
        h('div', { class: 'toolbar-right' }, slots.toolbarRight?.()),
        props.entries.length === 0
          ? h('div', { class: 'empty' }, slots.empty?.())
          : h('div', { class: 'rows' }, String(props.entries.length)),
      ])
  },
})

function mountTable(query: Record<string, string> = {}): VueWrapper {
  for (const k of Object.keys(mocks.routeQuery)) delete mocks.routeQuery[k]
  Object.assign(mocks.routeQuery, query)
  return mount(TableView, {
    global: {
      stubs: {
        DataGrid: DataGridStub,
        SearchInput: true,
        Select: true,
        Popover: true,
        EpgTableOptions: true,
        EpgEventDrawer: true,
        LiveTvButton: true,
      },
      directives: { tooltip: tooltipStub },
    },
  })
}

type LoadPageArgs = { filter?: FilterDef[]; extraParams?: Record<string, unknown> }

function lastLoadPage(): LoadPageArgs {
  const calls = fakeState.loadPage.mock.calls
  expect(calls.length).toBeGreaterThan(0)
  return calls[calls.length - 1][0] as LoadPageArgs
}

function epgGridCalls(): Record<string, unknown>[] {
  return mocks.apiCall.mock.calls
    .filter((c) => c[0] === 'epg/events/grid')
    .map((c) => c[1] as Record<string, unknown>)
}

beforeEach(() => {
  setActivePinia(createPinia())
  useAccessStore().data = { dvr: true } as never
  mocks.apiCall.mockReset()
  mocks.apiCall.mockResolvedValue({ entries: [], totalCount: 0 })
  mocks.confirmAsk.mockReset()
  mocks.confirmAsk.mockResolvedValue(false)
  fakeState = makeFakeState()
})

describe('TableView — channel hand-off (?channel=<uuid>)', () => {
  it('sends the exact channel param, not a channelName regex', async () => {
    const wrapper = mountTable({ channel: 'u-ct1' })
    await flushPromises()
    const args = lastLoadPage()
    expect(args.extraParams?.channel).toBe('u-ct1')
    expect(args.filter?.some((f) => f.field === 'channelName') ?? false).toBe(false)
    /* The Channel funnel shows the channel's name, not the uuid. */
    const grid = wrapper.findComponent(DataGridStub)
    const filters = grid.props('filters') as Record<string, { value: unknown }>
    expect(filters.channelName.value).toBe('CT 1')
  })

  it('keeps the exact pick when another funnel is applied with the name unchanged', async () => {
    const wrapper = mountTable({ channel: 'u-ct1' })
    await flushPromises()
    const grid = wrapper.findComponent(DataGridStub)
    grid.vm.$emit('filter', {
      filters: {
        channelName: { value: 'CT 1' },
        episodeOnscreen: { value: 'S01' },
      },
    })
    await flushPromises()
    const args = lastLoadPage()
    expect(args.extraParams?.channel).toBe('u-ct1')
    expect(args.filter?.some((f) => f.field === 'channelName') ?? false).toBe(false)
  })

  it('turns into a free-text channel filter when the funnel text changes', async () => {
    const wrapper = mountTable({ channel: 'u-ct1' })
    await flushPromises()
    const grid = wrapper.findComponent(DataGridStub)
    grid.vm.$emit('filter', { filters: { channelName: { value: 'CT' } } })
    await flushPromises()
    const args = lastLoadPage()
    expect(args.extraParams?.channel).toBeUndefined()
    expect(args.filter).toContainEqual({ field: 'channelName', type: 'string', value: 'CT' })
  })

  it('shows only rows of the picked channel', async () => {
    fakeState.events.value = [
      { eventId: 1, channelUuid: 'u-ct1', channelName: 'CT 1', title: 'A' },
      { eventId: 2, channelUuid: 'u-ct1hd', channelName: 'CT 1 HD', title: 'B' },
    ]
    const wrapper = mountTable({ channel: 'u-ct1' })
    await flushPromises()
    const entries = wrapper.findComponent(DataGridStub).props('entries') as EpgRow[]
    expect(entries.map((e) => e.eventId)).toEqual([1])
  })

  /* The server ignores a `channel` it does not know and returns
   * every channel, which the exact filter would hide. */
  function channelList(keys: string[]) {
    mocks.apiCall.mockImplementation((path: string) =>
      Promise.resolve(
        path === 'channel/list'
          ? { entries: keys.map((key) => ({ key, val: key })) }
          : { entries: [], totalCount: 0 },
      ),
    )
  }

  it('drops a pick of a channel the server does not know', async () => {
    channelList(['u-ct1', 'u-ct1hd'])
    const wrapper = mountTable({ channel: 'u-deleted' })
    await flushPromises()
    expect(mocks.apiCall).toHaveBeenCalledWith('channel/list')
    expect(lastLoadPage().extraParams?.channel).toBeUndefined()
    const filters = wrapper.findComponent(DataGridStub).props('filters') as Record<
      string,
      { value: unknown }
    >
    expect(filters.channelName.value).toBeNull()
  })

  it('keeps a pick of a disabled channel', async () => {
    channelList(['u-ct1', 'u-ct1hd', 'u-off'])
    mountTable({ channel: 'u-off' })
    await flushPromises()
    expect(lastLoadPage().extraParams?.channel).toBe('u-off')
  })

  it('fetches a channel cluster by uuid when grouped by channel', async () => {
    fakeState = makeFakeState({ viewOptions: { groupField: 'channelName' } })
    const wrapper = mountTable()
    await flushPromises()
    mocks.apiCall.mockClear()
    wrapper.findComponent(DataGridStub).vm.$emit('rowgroup-expand', 'CT 1')
    await flushPromises()
    const calls = epgGridCalls()
    expect(calls).toHaveLength(1)
    expect(calls[0].channel).toBe('u-ct1')
    const filter = JSON.parse(calls[0].filter as string) as FilterDef[]
    expect(filter.some((f) => f.field === 'channelName')).toBe(false)
  })
})

describe('TableView — title hand-off (?title=<title>)', () => {
  it('escapes the title for the server and shows it raw in Search Title', async () => {
    const wrapper = mountTable({ title: 'Afrika z výšky (S1, E3)' })
    await flushPromises()
    const titleCalls = epgGridCalls().filter((p) => 'title' in p)
    expect(titleCalls.length).toBeGreaterThan(0)
    expect(titleCalls[titleCalls.length - 1].title).toBe(String.raw`Afrika z výšky \(S1, E3\)`)
    /* Inline and phone-popover Search Title boxes share the model. */
    const box = wrapper.findComponent({ name: 'SearchInput' })
    expect(box.props('modelValue')).toBe('Afrika z výšky (S1, E3)')
  })

  it('a title the user types stays a regex', async () => {
    const wrapper = mountTable({ title: 'News (Late)' })
    await flushPromises()
    mocks.apiCall.mockClear()
    wrapper.findComponent(DataGridStub).vm.$emit('filter', {
      filters: { title: { value: '^News' } },
    })
    await flushPromises()
    const titleCalls = epgGridCalls().filter((p) => 'title' in p)
    expect(titleCalls[titleCalls.length - 1].title).toBe('^News')
  })

  it('a channel hand-off replaces an earlier title hand-off', async () => {
    mountTable({ title: 'News (Late)' })
    await flushPromises()
    mocks.routeQuery.channel = 'u-ct1'
    await flushPromises()
    const args = lastLoadPage()
    expect(args.extraParams?.channel).toBe('u-ct1')
  })
})

describe('TableView — Create AutoRec channel and count', () => {
  function autorecCreateConf(): Record<string, unknown> | undefined {
    const call = mocks.apiCall.mock.calls.find((c) => c[0] === 'dvr/autorec/create')
    return call ? (JSON.parse((call[1] as { conf: string }).conf) as Record<string, unknown>) : undefined
  }

  function lastCountCall(): Record<string, unknown> | undefined {
    const calls = epgGridCalls().filter((p) => p.limit === 0)
    return calls[calls.length - 1]
  }

  async function applyChannelFunnel(wrapper: VueWrapper, text: string, title?: string) {
    const filters: Record<string, { value: string }> = { channelName: { value: text } }
    if (title) filters.title = { value: title }
    wrapper.findComponent(DataGridStub).vm.$emit('filter', { filters })
    await flushPromises()
  }

  it('an exact channel name in the funnel becomes the rule channel (uuid)', async () => {
    fakeState = makeFakeState({ viewOptions: { timeWindow: 'today' } })
    mocks.apiCall.mockImplementation((path: string, params?: Record<string, unknown>) =>
      Promise.resolve(
        path === 'epg/events/grid' && params?.limit === 0
          ? { entries: [], totalCount: 222 }
          : { entries: [], totalCount: 0 },
      ),
    )
    mocks.confirmAsk.mockResolvedValue(true)
    const wrapper = mountTable()
    await flushPromises()
    await applyChannelFunnel(wrapper, 'CT 1')
    await wrapper.find('.epg-table-grid__autorec').trigger('click')
    await flushPromises()

    /* Count query = the rule's query: exact channel, no time window. */
    const count = lastCountCall()
    expect(count?.channel).toBe('u-ct1')
    expect(count?.filter).toBeUndefined()
    const summary = mocks.confirmAsk.mock.calls[0][0] as string
    expect(summary).toContain('Channel: CT 1')
    expect(summary).toContain('222')
    expect(autorecCreateConf()?.channel).toBe('u-ct1')
  })

  it('funnel text that is not one channel name is not saved and does not enable the button', async () => {
    const wrapper = mountTable()
    await flushPromises()
    await applyChannelFunnel(wrapper, 'CT')
    const button = wrapper.find('.epg-table-grid__autorec')
    expect(button.attributes('disabled')).toBeDefined()
  })

  it('the disabled button says why a partial channel alone is not enough', async () => {
    const wrapper = mountTable()
    await flushPromises()
    const button = wrapper.find('.epg-table-grid__autorec')
    expect(button.attributes('data-tooltip')).toContain('Set at least one filter')
    await applyChannelFunnel(wrapper, 'CT')
    expect(button.attributes('disabled')).toBeDefined()
    expect(button.attributes('data-tooltip')).toContain('not an exact channel name')
    expect(button.attributes('data-tooltip')).not.toContain('Set at least one filter')
  })

  it('with a title, a partial channel is reported as unsaved and left out of the rule', async () => {
    mocks.confirmAsk.mockResolvedValue(true)
    const wrapper = mountTable()
    await flushPromises()
    await applyChannelFunnel(wrapper, 'CT', 'News')
    await wrapper.find('.epg-table-grid__autorec').trigger('click')
    await flushPromises()
    const summary = mocks.confirmAsk.mock.calls[0][0] as string
    expect(summary).toContain("Active filter can't be saved (not an exact channel name)")
    const conf = autorecCreateConf()
    expect(conf?.title).toBe('News')
    expect(conf?.channel).toBeUndefined()
    const count = lastCountCall()
    expect(count?.title).toBe('News')
    expect(count?.channel).toBeUndefined()
  })

  it('an exact pick from a hand-off rides into the rule as its uuid', async () => {
    mocks.confirmAsk.mockResolvedValue(true)
    const wrapper = mountTable({ channel: 'u-ct1hd' })
    await flushPromises()
    await wrapper.find('.epg-table-grid__autorec').trigger('click')
    await flushPromises()
    const summary = mocks.confirmAsk.mock.calls[0][0] as string
    expect(summary).toContain('Channel: CT 1 HD')
    expect(autorecCreateConf()?.channel).toBe('u-ct1hd')
  })

  it('leaves the count sentence out when the count request fails', async () => {
    mocks.apiCall.mockImplementation((path: string, params?: Record<string, unknown>) =>
      path === 'epg/events/grid' && params?.limit === 0
        ? Promise.reject(new Error('boom'))
        : Promise.resolve({ entries: [], totalCount: 0 }),
    )
    const wrapper = mountTable()
    await flushPromises()
    await applyChannelFunnel(wrapper, 'CT 1')
    await wrapper.find('.epg-table-grid__autorec').trigger('click')
    await flushPromises()
    const summary = mocks.confirmAsk.mock.calls[0][0] as string
    expect(summary).not.toContain('Currently this will match')
    expect(summary).toContain('Are you sure?')
  })
})
