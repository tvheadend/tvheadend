// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useStatusStore unit tests.
 *
 * Coverage focuses on the parts that aren't trivial:
 *
 *   1. Initial fetch populates entries.
 *   2. Subsequent refetch MERGES BY KEY — preserves row object
 *      identity for keys that survive across responses, mutates
 *      fields onto the existing object, appends new keys, drops
 *      missing ones. This identity-preservation is what makes the
 *      "no flicker on Comet refresh" behavior work; if a future
 *      change reverts to array-replace, this test catches it.
 *   3. Silent fetch() doesn't toggle the `loading` ref. PrimeVue
 *      shows a spinner overlay whenever loading is true, so silent
 *      refresh is what keeps Comet-driven updates from flashing.
 *   4. The Comet listener is ref-counted by retain() / release, so
 *      a store left behind by an unmounted view stops refetching.
 *
 * Each test uses a unique endpoint string so the module-level
 * storeFactoryCache doesn't leak state between cases.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useStatusStore } from '../status'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Map-backed Comet stub: `on` records the listener and returns an
 * unsubscribe, so the tests can check when the store holds a
 * listener and drive notifications through it. */
type Listener = (msg: Record<string, unknown>) => void
const cometListeners = new Map<string, Set<Listener>>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, fn: Listener) => {
      let set = cometListeners.get(klass)
      if (!set) {
        set = new Set()
        cometListeners.set(klass, set)
      }
      set.add(fn)
      return () => cometListeners.get(klass)?.delete(fn)
    },
  },
}))

function listenerCount(klass: string): number {
  return cometListeners.get(klass)?.size ?? 0
}

function fireComet(klass: string, payload: Record<string, unknown>): void {
  for (const l of cometListeners.get(klass) ?? []) l({ notificationClass: klass, ...payload })
}

interface Row extends Record<string, unknown> {
  uuid: string
  name: string
  bps?: number
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  cometListeners.clear()
})

afterEach(() => {
  apiMock.mockReset()
  vi.useRealTimers()
})

describe('useStatusStore', () => {
  it('initial fetch populates entries from api/<endpoint>', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha', bps: 100 },
        { uuid: 'b', name: 'Beta', bps: 200 },
      ],
    })
    const store = useStatusStore<Row>('status/test-1', 'cls', 'uuid')
    await store.fetch()
    expect(apiMock).toHaveBeenCalledWith('status/test-1')
    expect(store.entries).toHaveLength(2)
    expect(store.entries[0].name).toBe('Alpha')
  })

  it('preserves row object identity across refetch (merge by key)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha', bps: 100 },
        { uuid: 'b', name: 'Beta', bps: 200 },
      ],
    })
    const store = useStatusStore<Row>('status/test-2', 'cls', 'uuid')
    await store.fetch()
    /* Capture the original object references. */
    const originalA = store.entries.find((r) => r.uuid === 'a')!
    const originalB = store.entries.find((r) => r.uuid === 'b')!

    /* Second fetch: same uuids, updated bps fields. */
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha', bps: 150 },
        { uuid: 'b', name: 'Beta', bps: 250 },
      ],
    })
    await store.fetch({ silent: true })

    /* The new entries array contains the SAME object references as
     * before (identity preserved), with the bps field updated in
     * place via Object.assign. */
    const newA = store.entries.find((r) => r.uuid === 'a')!
    const newB = store.entries.find((r) => r.uuid === 'b')!
    expect(newA).toBe(originalA) /* same identity */
    expect(newB).toBe(originalB)
    expect(newA.bps).toBe(150) /* field updated */
    expect(newB.bps).toBe(250)
  })

  it('appends rows whose keys are new in the refetch response', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a', name: 'Alpha' }],
    })
    const store = useStatusStore<Row>('status/test-3', 'cls', 'uuid')
    await store.fetch()
    expect(store.entries).toHaveLength(1)

    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'c', name: 'Gamma' },
      ],
    })
    await store.fetch({ silent: true })
    expect(store.entries).toHaveLength(2)
    expect(store.entries.find((r) => r.uuid === 'c')?.name).toBe('Gamma')
  })

  it('drops rows whose keys disappear from the refetch response', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    const store = useStatusStore<Row>('status/test-4', 'cls', 'uuid')
    await store.fetch()
    expect(store.entries).toHaveLength(2)

    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await store.fetch({ silent: true })
    expect(store.entries).toHaveLength(1)
    expect(store.entries[0].uuid).toBe('a')
  })

  it('silent fetch never sets loading=true', async () => {
    /* Use a deferred mock so we can observe the `loading` value
     * MID-fetch — when it'd normally be true. */
    let resolveFn: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFn = resolve
      })
    )
    const store = useStatusStore<Row>('status/test-5', 'cls', 'uuid')
    const inflight = store.fetch({ silent: true })
    /* While the API call is pending, loading should still be false
     * because silent skips the toggle. */
    expect(store.loading).toBe(false)
    resolveFn({ entries: [] })
    await inflight
    expect(store.loading).toBe(false)
  })

  it('non-silent fetch sets loading=true while in flight, false after', async () => {
    let resolveFn: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFn = resolve
      })
    )
    const store = useStatusStore<Row>('status/test-6', 'cls', 'uuid')
    const inflight = store.fetch()
    expect(store.loading).toBe(true)
    resolveFn({ entries: [] })
    await inflight
    expect(store.loading).toBe(false)
  })

  it('holds no Comet listener until a consumer retains the store', () => {
    useStatusStore<Row>('status/test-7', 'cls-7', 'uuid')
    expect(listenerCount('cls-7')).toBe(0)
  })

  it('ref-counts the Comet listener across consumers', () => {
    const store = useStatusStore<Row>('status/test-8', 'cls-8', 'uuid')
    const releaseA = store.retain()
    const releaseB = store.retain()
    expect(listenerCount('cls-8')).toBe(1)
    releaseA()
    /* A second release of the same handle must not drop B's claim. */
    releaseA()
    expect(listenerCount('cls-8')).toBe(1)
    releaseB()
    expect(listenerCount('cls-8')).toBe(0)
    store.retain()
    expect(listenerCount('cls-8')).toBe(1)
  })

  it('refetches on a notification only while retained', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValue({ entries: [] })
    const store = useStatusStore<Row>('status/test-9', 'cls-9', 'uuid')
    const release = store.retain()
    fireComet('cls-9', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expect(apiMock).toHaveBeenCalledTimes(1)

    /* A notification debounced just before the last release must
     * not fire after it either. */
    fireComet('cls-9', { reload: 1 })
    release()
    await vi.advanceTimersByTimeAsync(200)
    fireComet('cls-9', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })
})
