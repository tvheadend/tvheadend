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
import { defineComponent } from 'vue'
import { flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import {
  useEpgViewState,
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

vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: () => false,
    quicktips: true,
    chnameNum: false,
  }),
}))

vi.mock('@/stores/dvrEntries', () => ({
  useDvrEntriesStore: () => ({ entries: [], ensure: vi.fn() }),
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
 * `epg/events/grid` per scenario. */
function answerWith(
  eventsImpl: (params: Record<string, unknown>) => unknown,
): void {
  apiMock.mockImplementation((endpoint: string, params: Record<string, unknown>) => {
    if (endpoint === 'epg/events/grid') return Promise.resolve(eventsImpl(params))
    return Promise.resolve({ entries: [], total: 0, totalCount: 0 })
  })
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
    cometStateListener?.('disconnected')
    cometStateListener?.('connected')
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
