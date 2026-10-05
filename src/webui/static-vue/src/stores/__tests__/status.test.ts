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

/* Capture the handler each store registers so tests can drive Comet
 * events. Keyed by notification class; last registration wins (each
 * test uses a unique endpoint, so its store's handler is the latest). */
const cometMock = vi.hoisted(() => {
  const handlers = new Map<string, (msg: unknown) => void>()
  return {
    handlers,
    on: (cls: string, fn: (msg: unknown) => void) => handlers.set(cls, fn),
  }
})
vi.mock('@/api/comet', () => ({
  cometClient: { on: cometMock.on },
}))

interface Row extends Record<string, unknown> {
  uuid: string
  name: string
  bps?: number
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
})

afterEach(() => {
  apiMock.mockReset()
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

  /*
   * input_status regression: the bps counter is reset on every read
   * (api/status/inputs resets it), so a refetch triggered by the
   * notification would return a fraction of a second's data. The
   * store must apply the pushed payload in place and NOT re-poll.
   */
  describe('input_status applies the notification in place (no refetch)', () => {
    beforeEach(() => vi.useFakeTimers())
    afterEach(() => vi.useRealTimers())

    it('updates the row from the payload without calling apiCall again', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'i1', name: 'Tuner', bps: 8_000_000 }],
      })
      const store = useStatusStore<Row>('status/inputs-a', 'input_status', 'uuid')
      const handler = cometMock.handlers.get('input_status')!
      await store.fetch()
      expect(apiMock).toHaveBeenCalledTimes(1) /* the initial load */

      /* Server's 1-second tick pushes the full-second value — the
       * real message carries the `notificationClass` envelope
       * discriminator alongside the data fields. */
      handler({
        notificationClass: 'input_status',
        uuid: 'i1',
        bps: 6_000_000,
        signal: 900,
        update: 1,
      })
      vi.advanceTimersByTime(500) /* well past the refetch debounce */

      const row = store.entries.find((r) => r.uuid === 'i1')!
      expect(row.bps).toBe(6_000_000) /* applied from the payload */
      expect(row.signal).toBe(900)
      expect(row.update).toBeUndefined() /* control flag stripped */
      expect(row.notificationClass).toBeUndefined() /* envelope stripped */
      expect(apiMock).toHaveBeenCalledTimes(1) /* NO counter-resetting refetch */
    })

    it('falls back to a refetch on a reload notification', async () => {
      apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'i1', name: 'Tuner' }] })
      const store = useStatusStore<Row>('status/inputs-b', 'input_status', 'uuid')
      const handler = cometMock.handlers.get('input_status')!
      await store.fetch()
      expect(apiMock).toHaveBeenCalledTimes(1)

      apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'i2', name: 'Tuner 2' }] })
      handler({ reload: 1 })
      await vi.advanceTimersByTimeAsync(200) /* debounce elapses */
      expect(apiMock).toHaveBeenCalledTimes(2) /* structural change refetches */
    })

    it('falls back to a refetch when the pushed row is unknown', async () => {
      apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'i1', name: 'Tuner' }] })
      const store = useStatusStore<Row>('status/inputs-c', 'input_status', 'uuid')
      const handler = cometMock.handlers.get('input_status')!
      await store.fetch()

      apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'i1' }, { uuid: 'i9' }] })
      handler({ uuid: 'i9', bps: 1000, update: 1 }) /* i9 not loaded yet */
      await vi.advanceTimersByTimeAsync(200)
      expect(apiMock).toHaveBeenCalledTimes(2)
    })
  })
})
