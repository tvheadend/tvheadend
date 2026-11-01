// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgViewState — data-loading strategy tests.
 *
 * Mounts the composable inside a throwaway component (same harness
 * shape as useHomeState.test.ts) with the API client, Comet client
 * and stores mocked, then drives the load paths the views exercise:
 * lazy table paging, per-day continuous-scroll fetches, and the
 * full-refresh plumbing shared by the Comet / visibility handlers.
 *
 * The time zone is pinned to Europe/Berlin so the DST-transition
 * suite is deterministic regardless of the host TZ; the other
 * suites use relative epochs and are TZ-agnostic.
 */
process.env.TZ = 'Europe/Berlin'

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, reactive } from 'vue'
import { flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import {
  useEpgViewState,
  type ChannelRow,
  type ChannelTag,
  type UseEpgViewState,
  type UseEpgViewStateOpts,
} from '../useEpgViewState'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Comet stub — capture the per-class handlers and the connection-
 * state listener so tests can fire notifications / reconnects. */
let cometHandlers: Map<string, (msg: unknown) => void>
let cometStateListener: ((s: string) => void) | null
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (cls: string, fn: (msg: unknown) => void) => {
      cometHandlers.set(cls, fn)
      return () => cometHandlers.delete(cls)
    },
    onStateChange: (fn: (s: string) => void) => {
      cometStateListener = fn
      return () => {
        cometStateListener = null
      }
    },
    getState: () => 'connected',
  },
}))

/* DVR access is off unless a test turns it on. */
let hasDvrAccess = false
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: (key: string) => key === 'dvr' && hasDvrAccess,
    quicktips: true,
    chnameNum: false,
  }),
}))

/* Reactive, so a test can change the entries as a 'dvrentry'
 * notification does. */
const dvrEnsure = vi.fn()
const dvrStore = reactive({
  entries: [] as { uuid: string; sched_status: string }[],
  ensure: dvrEnsure,
})
vi.mock('@/stores/dvrEntries', () => ({
  useDvrEntriesStore: () => dvrStore,
}))

vi.mock('../useIsPhone', async () => {
  const { ref } = await import('vue')
  const phone = ref(false)
  return { useIsPhone: () => phone }
})

/* Persisted view options / sticky position are out of scope here —
 * always start from defaults. */
vi.mock('@/utils/storage', () => ({
  readStoredJson: () => null,
  writeStoredJson: () => undefined,
}))

interface GridCall {
  endpoint: string
  params: Record<string, unknown>
}

/* Default API behaviour: empty grids everywhere. Tests override
 * `epg/events/grid` per scenario, and other endpoints in `others`. */
function answerWith(
  eventsImpl: (params: Record<string, unknown>) => unknown,
  others: Record<string, () => unknown> = {},
): void {
  apiMock.mockImplementation((endpoint: string, params: Record<string, unknown>) => {
    if (endpoint === 'epg/events/grid') return Promise.resolve(eventsImpl(params))
    return Promise.resolve(others[endpoint]?.() ?? { entries: [], total: 0, totalCount: 0 })
  })
}

/* A lost Comet connection coming back. The client always goes
 * through connecting in between. */
function reconnect(): void {
  cometStateListener?.('disconnected')
  cometStateListener?.('connecting')
  cometStateListener?.('connected')
}

function eventsGridCalls(): GridCall[] {
  return apiMock.mock.calls
    .filter((c) => c[0] === 'epg/events/grid')
    .map((c) => ({ endpoint: c[0] as string, params: c[1] as Record<string, unknown> }))
}

/* Mount a throwaway component so the composable's onMounted fires. */
function mountState(opts: UseEpgViewStateOpts = {}): {
  state: UseEpgViewState
  wrapper: VueWrapper
} {
  let state!: UseEpgViewState
  const wrapper = mount(
    defineComponent({
      setup() {
        state = useEpgViewState(opts)
        return () => null
      },
    }),
  )
  return { state, wrapper }
}

function row(eventId: number, extra: Record<string, unknown> = {}) {
  return {
    eventId,
    start: 1_700_000_000 + eventId,
    stop: 1_700_000_600 + eventId,
    title: `event ${eventId}`,
    ...extra,
  }
}

let wrappers: VueWrapper[] = []

beforeEach(() => {
  cometHandlers = new Map()
  cometStateListener = null
  hasDvrAccess = false
  dvrEnsure.mockReset()
  dvrStore.entries = []
  apiMock.mockReset()
  answerWith(() => ({ entries: [], total: 0, totalCount: 0 }))
})

afterEach(() => {
  wrappers.forEach((w) => w.unmount())
  wrappers = []
  apiMock.mockReset()
})

function setTag(state: UseEpgViewState, tag: string | null): void {
  state.setViewOptions({
    ...state.viewOptions.value,
    tagFilter: { tag },
  })
}

describe('useEpgViewState — DVR entries priming', () => {
  it('primes the DVR store during setup when DVR access is already known', async () => {
    /* Navigating to the EPG from another view: access has landed, so
     * the immediate access watcher fires inside setup. */
    hasDvrAccess = true
    const { wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()
    expect(dvrEnsure).toHaveBeenCalled()
  })
})

describe('useEpgViewState — channel-tag change (per-day mode)', () => {
  it('discards a stale in-flight day fetch resolved after the tag change', async () => {
    /* Day fetch for the old tag is still in flight when the user
     * picks a new tag. Its late resolve must neither merge the
     * old tag's events nor re-mark the day as loaded (which would
     * pin the wrong tag's events on that day for the session). */
    const pending: { params: Record<string, unknown>; resolve: (v: unknown) => void }[] = []
    answerWith((params) => new Promise((resolve) => pending.push({ params, resolve })))

    const { state, wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()
    /* Mount kicked off today + tomorrow — both still pending. */
    const oldFetches = pending.splice(0)
    expect(oldFetches.length).toBe(2)

    /* A refetch for the old tag, asked for while they are in flight,
     * is dropped with them. */
    reconnect()
    setTag(state, 'tag-1')
    await flushPromises()
    /* New-tag fetches for the same viewport days dispatched. */
    const newFetches = pending.splice(0)
    expect(newFetches.length).toBe(2)
    for (const f of newFetches) expect(f.params.channelTag).toBe('tag-1')

    /* Old-tag responses land late — discarded. */
    oldFetches.forEach((f) => f.resolve({ entries: [row(100)], totalCount: 1 }))
    await flushPromises()
    expect(state.events.value).toEqual([])
    expect(state.loadedDays.value.size).toBe(0)

    /* New-tag responses populate normally. */
    newFetches.forEach((f) => f.resolve({ entries: [row(200)], totalCount: 1 }))
    await flushPromises()
    expect(state.events.value.map((e) => e.eventId)).toEqual([200])
    expect(state.loadedDays.value.size).toBe(2)
    expect(pending).toHaveLength(0)
  })

  it('reloads the last-known viewport days without waiting for a scroll', async () => {
    answerWith(() => ({ entries: [], totalCount: 0 }))
    const { state, wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()

    /* User scrolled to day +3 / +4 — the scroll listener's
     * ensureDaysLoaded recorded the viewport range. */
    const d3 = state.dayStartForOffset(3)
    const d4 = state.dayStartForOffset(4)
    state.ensureDaysLoaded([d3, d4])
    await flushPromises()

    apiMock.mockClear()
    setTag(state, 'tag-2')
    await flushPromises()

    /* The viewport days re-fetch under the new tag immediately —
     * not only after the next scroll event. */
    const calls = eventsGridCalls()
    expect(calls.length).toBe(2)
    for (const c of calls) {
      expect(c.params.channelTag).toBe('tag-2')
      const filter = JSON.parse(String(c.params.filter)) as {
        field: string
        value: number
        comparison: string
      }[]
      const stopGt = filter.find((f) => f.field === 'stop' && f.comparison === 'gt')
      expect([d3, d4]).toContain(stopGt?.value)
    }
  })
})

describe('useEpgViewState — lazy table full refresh', () => {
  it('refetches the loaded page window in place when data changes', async () => {
    /* Mount in tableLazyPaging mode: the composable loads page 0
     * (100 rows) and never touches loadedDays. A full-refresh
     * trigger (here: Comet reconnect — same refreshAllEvents the
     * DVR diff watcher and visibility wake use) must re-fetch the
     * loaded window rather than silently no-oping on the empty
     * loadedDays set. */
    answerWith(() => ({
      entries: [row(1, { dvrState: '' }), row(2)],
      totalCount: 5,
    }))
    const { state, wrapper } = mountState({ tableLazyPaging: true })
    wrappers.push(wrapper)
    await flushPromises()

    expect(state.events.value.map((e) => e.eventId)).toEqual([1, 2])
    const callsBefore = eventsGridCalls().length

    /* Server-side change (e.g. a recording was scheduled from the
     * event drawer) — fresh rows carry the new dvrState. */
    answerWith(() => ({
      entries: [row(1, { dvrState: 'scheduled' }), row(2)],
      totalCount: 5,
    }))
    reconnect()
    await flushPromises()

    const calls = eventsGridCalls()
    expect(calls.length).toBe(callsBefore + 1)
    /* Replace fetch over the loaded window — offset 0, limit
     * covering at least the loaded rows, NOT a page-zero reset
     * with a tiny limit. */
    const refresh = calls[calls.length - 1].params
    expect(refresh.start).toBe(0)
    expect(Number(refresh.limit)).toBeGreaterThanOrEqual(2)
    expect(refresh.sort).toBe('start')
    /* Fresh rows merged in place. */
    expect(state.events.value.map((e) => e.eventId)).toEqual([1, 2])
    expect(state.events.value[0].dvrState).toBe('scheduled')
  })
})

describe('useEpgViewState — refetch of the loaded days', () => {
  /* What the server holds. Tests change it while the connection is
   * lost or while a day fetch is on the way. */
  let events: ReturnType<typeof row>[]
  let channels: ChannelRow[]
  let tags: ChannelTag[]
  /* Day fetches wait in `held` while `holdReplies` is set. */
  let holdReplies: boolean
  let held: { params: Record<string, unknown>; resolve: (reply: unknown) => void }[]

  /* A day fetch gets the events that overlap its day. The server's
   * comparators are inclusive (`_eq_comp_num` in src/epg.c). */
  function dayReply(params: Record<string, unknown>) {
    const filter = JSON.parse(String(params.filter ?? '[]')) as {
      field: string
      value: number
    }[]
    const dayEnd = filter.find((f) => f.field === 'start')?.value ?? Infinity
    const dayStart = filter.find((f) => f.field === 'stop')?.value ?? -Infinity
    const entries = events.filter((e) => e.start <= dayEnd && e.stop >= dayStart)
    return { entries, totalCount: entries.length }
  }

  /* The server reads the day of a held fetch now. Its reply arrives
   * when the returned function is called. */
  function read(fetch: (typeof held)[number]): () => void {
    const reply = dayReply(fetch.params)
    return () => fetch.resolve(reply)
  }

  /* A DVR change as the 'dvrentry' notification brings it. The DVR
   * store refetches its entries and the EPG its days 500 ms later. */
  async function dvrChange(): Promise<void> {
    dvrStore.entries = [{ uuid: 'dvr-1', sched_status: 'scheduled' }]
    await flushPromises()
    await vi.advanceTimersByTimeAsync(500)
  }

  /* Pinned now: 2026-11-10 12:00 local, so the composable loads
   * November 10 and 11 on mount. Date and the debounce timers are
   * faked, flushPromises keeps using setImmediate. */
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date', 'setTimeout', 'clearTimeout'] })
    vi.setSystemTime(new Date(2026, 10, 10, 12, 0, 0))
    events = []
    channels = []
    tags = []
    holdReplies = false
    held = []
    const reply = (params: Record<string, unknown>) =>
      holdReplies ? new Promise((resolve) => held.push({ params, resolve })) : dayReply(params)
    answerWith(reply, {
      'channel/grid': () => ({ entries: channels }),
      'channeltag/grid': () => ({ entries: tags }),
      'epg/events/load': () => ({ entries: events }),
    })
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  /* Epoch of a local time on November 10 (day 0) or 11 (day 1). */
  const at = (day: number, hour: number, minute = 0) =>
    Math.floor(new Date(2026, 10, 10 + day, hour, minute).getTime() / 1000)

  async function mountLoaded(): Promise<UseEpgViewState> {
    const { state, wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()
    return state
  }

  it('drops the events deleted during the outage and keeps each event once', async () => {
    const crossesMidnight = row(3, { start: at(0, 23, 30), stop: at(1, 0, 30) })
    events = [
      row(1, { start: at(0, 14), stop: at(0, 15) }),
      row(2, { start: at(0, 16), stop: at(0, 17) }),
      crossesMidnight,
      row(4, { start: at(1, 10), stop: at(1, 11) }),
    ]
    const state = await mountLoaded()
    /* Both day replies list the event across midnight. */
    expect(state.events.value.map((e) => e.eventId)).toEqual([1, 2, 3, 4])

    /* Event 2 deleted, event 4 moved to the evening before. */
    events = [events[0], row(4, { start: at(0, 20), stop: at(0, 21) }), crossesMidnight]
    reconnect()
    await flushPromises()

    expect(state.events.value.map((e) => e.eventId)).toEqual([1, 4, 3])
    expect(state.events.value[1].start).toBe(at(0, 20))
  })

  it('shows the channel and tag names changed during the outage', async () => {
    channels = [{ uuid: 'ch-1', name: 'One', number: 1, tags: ['tag-1'] }]
    tags = [{ uuid: 'tag-1', name: 'Sport' }]
    const state = await mountLoaded()
    expect(state.channels.value[0].name).toBe('One')
    expect(state.tags.value[0].name).toBe('Sport')

    channels = [{ ...channels[0], name: 'One HD' }]
    tags = [{ uuid: 'tag-1', name: 'Sports' }]
    reconnect()
    await flushPromises()

    expect(state.channels.value[0].name).toBe('One HD')
    expect(state.tags.value[0].name).toBe('Sports')
  })

  it('fetches a day again when a change comes in while its first reply is on the way', async () => {
    const crossesMidnight = { start: at(0, 23, 30), stop: at(1, 0, 30) }
    events = [row(3, { ...crossesMidnight, dvrState: '' })]
    holdReplies = true
    const state = await mountLoaded()
    const [today, tomorrow] = held.splice(0)

    /* The server reads today, the programme across midnight is
     * scheduled, the server reads tomorrow. Tomorrow's reply and its
     * refetch arrive before the old reply of today. */
    const oldToday = read(today)
    events = [row(3, { ...crossesMidnight, dvrState: 'scheduled', dvrUuid: 'dvr-1' })]
    read(tomorrow)()
    await dvrChange()
    held.splice(0).forEach((f) => read(f)())
    await flushPromises()
    oldToday()
    await flushPromises()
    held.splice(0).forEach((f) => read(f)())
    await flushPromises()

    expect(state.events.value.map((e) => e.dvrState)).toEqual(['scheduled'])
    expect(eventsGridCalls()).toHaveLength(4)
  })

  it('fetches a day in flight once more, however many refetches ask for it', async () => {
    const afternoon = { start: at(0, 14), stop: at(0, 15) }
    events = [row(1, { ...afternoon, dvrState: '' })]
    const state = await mountLoaded()
    holdReplies = true
    reconnect()
    await flushPromises()
    /* The server reads both days, then the programme is scheduled. */
    const oldReplies = held.splice(0).map(read)
    events = [row(1, { ...afternoon, dvrState: 'scheduled', dvrUuid: 'dvr-1' })]
    await dvrChange()
    reconnect()
    await flushPromises()
    expect(held).toHaveLength(0)

    oldReplies.forEach((deliver) => deliver())
    await flushPromises()
    expect(held).toHaveLength(2)
    held.splice(0).forEach((f) => read(f)())
    await flushPromises()

    expect(state.events.value.map((e) => e.dvrState)).toEqual(['scheduled'])
  })

  it('keeps a programme created while the day is on the way', async () => {
    holdReplies = true
    const state = await mountLoaded()
    const oldReplies = held.splice(0).map(read)
    events = [row(5, { start: at(0, 14), stop: at(0, 15) })]
    cometHandlers.get('epg')?.({ create: ['5'] })
    await vi.advanceTimersByTimeAsync(500)

    oldReplies.forEach((deliver) => deliver())
    await flushPromises()
    held.splice(0).forEach((f) => read(f)())
    await flushPromises()

    expect(state.events.value.map((e) => e.eventId)).toEqual([5])
  })
})

describe('useEpgViewState — day keys across a DST transition', () => {
  /* Pinned now: 2026-10-22 12:00 local — three days before the
   * CET/CEST fall-back (2026-10-25 is a 25-hour day), so the
   * 14-day track straddles the transition. Only Date is faked;
   * timers stay real for flushPromises. */
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] })
    vi.setSystemTime(new Date(2026, 9, 22, 12, 0, 0))
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('dayStartForOffset returns true local midnights past the boundary', async () => {
    const { state, wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()

    for (let offset = 0; offset <= 13; offset++) {
      const epoch = state.dayStartForOffset(offset)
      /* True local midnight of the same calendar day — naive
       * `+ offset * 86400` would land at 01:00 past the
       * fall-back and never match the scroll writeback's keys. */
      expect(new Date(epoch * 1000).getHours()).toBe(0)
      expect(epoch).toBe(Math.floor(new Date(2026, 9, 22 + offset).getTime() / 1000))
    }
    /* Offset 3 is the fall-back day itself: 25 hours long. */
    expect(state.dayStartForOffset(4) - state.dayStartForOffset(3)).toBe(25 * 3600)
  })

  it('fetches a 25h day with the real next midnight as its end bound', async () => {
    const { state, wrapper } = mountState()
    wrappers.push(wrapper)
    await flushPromises()

    apiMock.mockClear()
    const fallBackDay = state.dayStartForOffset(3)
    state.ensureDaysLoaded([fallBackDay])
    await flushPromises()

    const calls = eventsGridCalls()
    expect(calls.length).toBe(1)
    const filter = JSON.parse(String(calls[0].params.filter)) as {
      field: string
      value: number
      comparison: string
    }[]
    const startLt = filter.find((f) => f.field === 'start' && f.comparison === 'lt')
    /* `start + 86400` would cut the day's final hour off the
     * fetch window. */
    expect(startLt?.value).toBe(fallBackDay + 25 * 3600)
  })
})
