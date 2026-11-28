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
 *   5. A failed silent refetch keeps the rows (and so the grid
 *      selection and chart history). Only a non-silent fetch
 *      clears them.
 *   6. An input_status update is applied to its row in place with
 *      no API call, because a status/inputs read resets the
 *      server's bandwidth counter. An unknown row, a structural
 *      notification or a store in error still refetches, and so
 *      does every subscriptions notification. Such a refetch adds
 *      and drops rows and fills the rows that have not received a
 *      push. The pushed rows keep their values until the last
 *      release.
 *
 * Each test uses a unique endpoint string so the module-level
 * storeFactoryCache doesn't leak state between cases.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { watch } from 'vue'
import { setActivePinia, createPinia } from 'pinia'
import { useStatusStore, type StatusEntry } from '../status'

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

/* An active mux row with the keys tvh_input_stream_create_msg
 * (src/input.c) gives it. A status/inputs row and an input_status
 * push both have exactly these, and the push adds `update`. */
function inputRow(uuid: string, bps: number, subs = 1): StatusEntry {
  return {
    uuid,
    input: 'Tuner',
    stream: 'Mux',
    subs,
    weight: 0,
    pids: [65535],
    signal: 0,
    signal_scale: 1,
    ber: 0,
    snr: 0,
    snr_scale: 1,
    unc: 0,
    bps,
    te: 0,
    cc: 0,
    ec_bit: 0,
    tc_bit: 0,
    ec_block: 0,
    tc_block: 0,
  }
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
    expect(apiMock).toHaveBeenNthCalledWith(1, 'status/test-9')

    /* A notification debounced just before the last release must
     * not fire after it either. */
    fireComet('cls-9', { reload: 1 })
    release()
    await vi.advanceTimersByTimeAsync(200)
    fireComet('cls-9', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('keeps the rows when a silent refetch fails, and clears the error on the next success', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    const store = useStatusStore<Row>('status/test-10', 'cls', 'uuid')
    await store.fetch()
    const originalA = store.entries[0]

    apiMock.mockRejectedValueOnce(new Error('API 502'))
    await store.fetch({ silent: true })
    expect(store.entries).toHaveLength(2)
    expect(store.entries[0]).toBe(originalA)
    expect(store.error?.message).toBe('API 502')

    /* The error stays up while the next silent refetch is in flight,
     * so a sustained outage does not blink the banner. */
    let resolveFn: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFn = resolve
      })
    )
    const inflight = store.fetch({ silent: true })
    expect(store.error?.message).toBe('API 502')
    resolveFn({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await inflight
    expect(store.error).toBeNull()
    expect(store.entries).toHaveLength(1)
  })

  it('clears the rows when a non-silent fetch fails', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    const store = useStatusStore<Row>('status/test-11', 'cls', 'uuid')
    await store.fetch()
    apiMock.mockRejectedValueOnce(new Error('API 403'))
    await store.fetch()
    expect(store.entries).toHaveLength(0)
    expect(store.error?.message).toBe('API 403')
  })

  it('applies an input_status update to its row in place without an API call', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 100), inputRow('b', 200)] })
    const store = useStatusStore<StatusEntry>('status/test-12', 'input_status', 'uuid')
    store.retain()
    await store.fetch()
    const rowA = store.entries[0]
    /* The grid cells and the bandwidth chart follow the rows through
     * a deep watch, so the write must go through the reactive row. */
    const seen: unknown[] = []
    const stop = watch(
      () => store.entries,
      (rows) => {
        seen.push(rows[0].bps)
      },
      { deep: true }
    )

    /* The 1-second push carries the whole row, as the server builds it. */
    fireComet('input_status', { update: 1, ...inputRow('a', 2_500_000, 2) })
    await vi.advanceTimersByTimeAsync(500)
    stop()

    expect(seen).toEqual([2_500_000])
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(store.entries[0]).toBe(rowA)
    /* The row's own keys only, without `update` and the Comet class. */
    expect(rowA).toEqual(inputRow('a', 2_500_000, 2))
    expect(store.entries[1].bps).toBe(200)
  })

  it('refetches when an input_status update names an unknown row', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    const store = useStatusStore<Row>('status/test-13', 'input_status', 'uuid')
    store.retain()
    await store.fetch()

    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'c', name: 'Gamma', bps: 300 },
      ],
    })
    fireComet('input_status', { update: 1, uuid: 'c', name: 'Gamma', bps: 300 })
    await vi.advanceTimersByTimeAsync(200)

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-13')
    expect(store.entries.map((r) => r.uuid)).toEqual(['a', 'c'])
  })

  it.each([
    ['reload', { reload: 1 }],
    ['create', { create: ['c'] }],
    ['change', { change: ['a'] }],
    ['delete', { delete: ['a'] }],
    ['update with reload', { update: 1, uuid: 'a', reload: 1 }],
  ])('refetches on an input_status %s notification', async (name, payload) => {
    vi.useFakeTimers()
    apiMock.mockResolvedValue({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    const store = useStatusStore<Row>(`status/test-14-${name}`, 'input_status', 'uuid')
    store.retain()
    await store.fetch()

    fireComet('input_status', payload)
    await vi.advanceTimersByTimeAsync(200)

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, `status/test-14-${name}`)
  })

  /* The server sends `updateEntry` for subscriptions. A plain `update`
   * on that class is not applied either, because only input_status
   * pushes whole rows. */
  it.each([['updateEntry'], ['update']])(
    'keeps refetching on a subscriptions %s instead of applying it',
    async (flag) => {
      vi.useFakeTimers()
      apiMock.mockResolvedValueOnce({ entries: [{ id: 1, in: 100 }] })
      const store = useStatusStore<StatusEntry>(`status/test-15-${flag}`, 'subscriptions', 'id')
      store.retain()
      await store.fetch()
      const row = store.entries[0]

      apiMock.mockResolvedValueOnce({ entries: [{ id: 1, in: 500 }] })
      fireComet('subscriptions', { [flag]: 1, id: 1, in: 999 })
      /* Nothing is copied from the notification itself. */
      expect(row.in).toBe(100)
      await vi.advanceTimersByTimeAsync(200)

      expect(apiMock).toHaveBeenCalledTimes(2)
      expect(apiMock).toHaveBeenNthCalledWith(2, `status/test-15-${flag}`)
      expect(store.entries[0]).toBe(row)
      expect(row.in).toBe(500)
    }
  )

  it('keeps a pending reload refetch and the pushed values when a push lands before it fires', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha', bps: 100 },
        { uuid: 'b', name: 'Beta', bps: 200 },
      ],
    })
    const store = useStatusStore<Row>('status/test-16', 'input_status', 'uuid')
    store.retain()
    await store.fetch()

    /* Mux b stops, and the input of mux a pushes within the debounce.
     * The reply's bps covers only the time since that push. */
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha', bps: 120 }] })
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(50)
    fireComet('input_status', { update: 1, uuid: 'a', name: 'Alpha', bps: 2500 })
    await vi.advanceTimersByTimeAsync(500)

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-16')
    expect(store.entries.map((r) => r.uuid)).toEqual(['a'])
    expect(store.entries[0].bps).toBe(2500)
  })

  it('retries a failed refetch on the next input_status update', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha', bps: 100 },
        { uuid: 'b', name: 'Beta', bps: 200 },
      ],
    })
    const store = useStatusStore<Row>('status/test-17', 'input_status', 'uuid')
    store.retain()
    await store.fetch()
    const rowA = store.entries[0]

    /* Mux b stops, and the refetch for its reload fails. */
    apiMock.mockRejectedValueOnce(new Error('API 503'))
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-17')
    expect(store.error?.message).toBe('API 503')
    expect(store.entries).toHaveLength(2)

    /* The next push is still applied in place, and it refetches.
     * The refetch keeps the pushed value. */
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha', bps: 300 }] })
    fireComet('input_status', { update: 1, uuid: 'a', name: 'Alpha', bps: 250 })
    expect(rowA.bps).toBe(250)
    await vi.advanceTimersByTimeAsync(200)

    expect(apiMock).toHaveBeenCalledTimes(3)
    expect(apiMock).toHaveBeenNthCalledWith(3, 'status/test-17')
    expect(store.error).toBeNull()
    expect(store.entries.map((r) => r.uuid)).toEqual(['a'])
    expect(store.entries[0]).toBe(rowA)
    expect(rowA.bps).toBe(250)

    /* Once a fetch succeeded, a push is applied without a refetch again. */
    fireComet('input_status', { update: 1, uuid: 'a', name: 'Alpha', bps: 400 })
    await vi.advanceTimersByTimeAsync(500)
    expect(apiMock).toHaveBeenCalledTimes(3)
    expect(rowA.bps).toBe(400)
  })

  it('keeps the pushed values when the refetch reply lands after the push', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 100), inputRow('b', 200)] })
    const store = useStatusStore<StatusEntry>('status/test-18', 'input_status', 'uuid')
    store.retain()
    await store.fetch()
    const rowA = store.entries[0]

    /* Mux b stops. The reload refetch is sent, and a push for a
     * arrives before its reply. */
    let resolveFn: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFn = resolve
      })
    )
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(150)
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-18')
    fireComet('input_status', { update: 1, ...inputRow('a', 2500, 2) })
    resolveFn({ entries: [inputRow('a', 120)] })
    await vi.advanceTimersByTimeAsync(0)

    expect(store.entries.map((r) => r.uuid)).toEqual(['a'])
    expect(store.entries[0]).toBe(rowA)
    expect(rowA).toEqual(inputRow('a', 2500, 2))
    expect(apiMock).toHaveBeenCalledTimes(2)
  })

  it('fills the rows without a push from a refetch and keeps the pushed ones', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 100), inputRow('b', 200)] })
    const store = useStatusStore<StatusEntry>('status/test-19', 'input_status', 'uuid')
    store.retain()
    await store.fetch()
    const [rowA, rowB] = store.entries
    fireComet('input_status', { update: 1, ...inputRow('a', 2500) })

    /* A mux starts on another input, and its reload reads every row. */
    apiMock.mockResolvedValueOnce({
      entries: [inputRow('a', 120), inputRow('b', 220), inputRow('c', 300)],
    })
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-19')
    expect(store.entries.map((r) => r.uuid)).toEqual(['a', 'b', 'c'])
    expect(store.entries[0]).toBe(rowA)
    expect(rowA.bps).toBe(2500)
    expect(store.entries[1]).toBe(rowB)
    expect(rowB.bps).toBe(220)
    expect(store.entries[2].bps).toBe(300)

    /* Mux c stops before mux a pushes again. That refetch fills b
     * again and still keeps the pushed values of a. */
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 130), inputRow('b', 230)] })
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)

    expect(apiMock).toHaveBeenCalledTimes(3)
    expect(apiMock).toHaveBeenNthCalledWith(3, 'status/test-19')
    expect(store.entries.map((r) => r.uuid)).toEqual(['a', 'b'])
    expect(store.entries[0]).toBe(rowA)
    expect(rowA.bps).toBe(2500)
    expect(store.entries[1]).toBe(rowB)
    expect(rowB.bps).toBe(230)
  })

  it('fills every row from the next fetch after the last release', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 100)] })
    const store = useStatusStore<StatusEntry>('status/test-20', 'input_status', 'uuid')
    const release = store.retain()
    await store.fetch()
    const rowA = store.entries[0]
    fireComet('input_status', { update: 1, ...inputRow('a', 2500) })
    release()

    /* Another view mounts later. No push reached the store meanwhile. */
    apiMock.mockResolvedValueOnce({ entries: [inputRow('a', 120)] })
    store.retain()
    await store.fetch()

    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-20')
    expect(store.entries[0]).toBe(rowA)
    expect(rowA.bps).toBe(120)
  })

  it('drops a refetch pending at the last release, so the next consumer fetches alone', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    const store = useStatusStore<Row>('status/test-21', 'input_status', 'uuid')
    const releaseA = store.retain()
    await store.fetch()

    /* Mux b stops, and the view unmounts within the debounce. */
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(50)
    releaseA()

    /* Another view mounts. Its fetch is held open past the old
     * debounce deadline. */
    let resolveFn: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFn = resolve
      })
    )
    const releaseB = store.retain()
    const inflight = store.fetch()
    await vi.advanceTimersByTimeAsync(500)
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock).toHaveBeenNthCalledWith(2, 'status/test-21')

    /* An old timer would have superseded this fetch and left
     * loading at true. */
    resolveFn({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await inflight
    expect(store.loading).toBe(false)
    expect(store.entries.map((r) => r.uuid)).toEqual(['a'])

    /* The new consumer listens: a reload refetches once. */
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expect(apiMock).toHaveBeenCalledTimes(3)
    expect(apiMock).toHaveBeenNthCalledWith(3, 'status/test-21')
    releaseB()
  })

  it('ignores a refetch still in flight at the last release once the next consumer fetched', async () => {
    vi.useFakeTimers()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    const store = useStatusStore<Row>('status/test-22', 'input_status', 'uuid')
    const releaseA = store.retain()
    await store.fetch()

    /* The reload refetch is sent, and the view unmounts before it
     * answers. */
    let resolveOld: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveOld = resolve
      })
    )
    fireComet('input_status', { reload: 1 })
    await vi.advanceTimersByTimeAsync(150)
    expect(apiMock).toHaveBeenCalledTimes(2)
    releaseA()

    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', name: 'Alpha 2' }] })
    store.retain()
    await store.fetch()
    resolveOld({
      entries: [
        { uuid: 'a', name: 'Alpha 1' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    await vi.advanceTimersByTimeAsync(0)

    expect(apiMock).toHaveBeenCalledTimes(3)
    expect(apiMock).toHaveBeenNthCalledWith(3, 'status/test-22')
    expect(store.entries.map((r) => [r.uuid, r.name])).toEqual([['a', 'Alpha 2']])
    expect(store.loading).toBe(false)
  })
})
