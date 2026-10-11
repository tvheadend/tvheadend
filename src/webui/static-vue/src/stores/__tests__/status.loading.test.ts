// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useStatusStore unit tests for the `loading` flag when fetches
 * overlap.
 *
 * A Status tab runs a non-silent fetch when it opens, which sets
 * `loading`. A Comet notification during that fetch starts a silent
 * refetch, which supersedes it, so the first reply is dropped. The
 * latest request then has to clear `loading`, or the grid keeps its
 * loading overlay until the tab opens again. A silent fetch that a
 * newer non-silent fetch supersedes leaves the flag to that fetch.
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

/* Comet stub: `on` keeps the listener with its class and returns an
 * unsubscribe, so the tests can drive notifications through it. */
type Listener = (msg: Record<string, unknown>) => void
const cometListeners = new Set<{ klass: string; fn: Listener }>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, fn: Listener) => {
      const entry = { klass, fn }
      cometListeners.add(entry)
      return () => cometListeners.delete(entry)
    },
  },
}))

function fireComet(klass: string, payload: Record<string, unknown>): void {
  for (const l of cometListeners) {
    if (l.klass === klass) l.fn({ notificationClass: klass, ...payload })
  }
}

/* An API reply that the test settles when it wants to. */
function deferred() {
  let resolve: (v: unknown) => void = () => {}
  let reject: (e: unknown) => void = () => {}
  const promise = new Promise((res, rej) => {
    resolve = res
    reject = rej
  })
  return { promise, resolve, reject }
}

/* Both fetches asked for the store's endpoint. */
function expectTwoFetchesOf(endpoint: string): void {
  expect(apiMock.mock.calls).toEqual([[endpoint], [endpoint]])
}

interface Row extends Record<string, unknown> {
  uuid: string
  name: string
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

describe('useStatusStore loading', () => {
  it('clears loading when a notification refetch supersedes the first fetch', async () => {
    vi.useFakeTimers()
    const first = deferred()
    const refetch = deferred()
    apiMock.mockReturnValueOnce(first.promise).mockReturnValueOnce(refetch.promise)
    const store = useStatusStore<Row>('status/loading-1', 'cls-loading-1', 'uuid')
    store.retain()
    const opening = store.fetch()
    expect(store.loading).toBe(true)

    /* A reload during the first fetch starts a silent refetch after
     * the debounce. */
    fireComet('cls-loading-1', { reload: 1 })
    await vi.advanceTimersByTimeAsync(200)
    expectTwoFetchesOf('status/loading-1')

    /* The first reply is superseded and dropped. The refetch still
     * runs, so the grid is still loading. */
    first.resolve({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await opening
    expect(store.loading).toBe(true)
    expect(store.entries).toHaveLength(0)

    refetch.resolve({
      entries: [
        { uuid: 'a', name: 'Alpha' },
        { uuid: 'b', name: 'Beta' },
      ],
    })
    await vi.waitFor(() => expect(store.loading).toBe(false))
    expect(store.entries.map((r) => r.uuid)).toEqual(['a', 'b'])
  })

  it('clears loading when the refetch answers before the first fetch', async () => {
    const first = deferred()
    const refetch = deferred()
    apiMock.mockReturnValueOnce(first.promise).mockReturnValueOnce(refetch.promise)
    const store = useStatusStore<Row>('status/loading-2', 'cls', 'uuid')
    const opening = store.fetch()
    const refetching = store.fetch({ silent: true })
    expectTwoFetchesOf('status/loading-2')

    refetch.resolve({ entries: [{ uuid: 'b', name: 'Beta' }] })
    await refetching
    expect(store.loading).toBe(false)
    expect(store.entries.map((r) => r.uuid)).toEqual(['b'])

    /* The late first reply is dropped and changes nothing. */
    first.resolve({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await opening
    expect(store.loading).toBe(false)
    expect(store.entries.map((r) => r.uuid)).toEqual(['b'])
  })

  it('clears loading when the superseding refetch fails', async () => {
    const first = deferred()
    const refetch = deferred()
    apiMock.mockReturnValueOnce(first.promise).mockReturnValueOnce(refetch.promise)
    const store = useStatusStore<Row>('status/loading-3', 'cls', 'uuid')
    const opening = store.fetch()
    const refetching = store.fetch({ silent: true })
    expectTwoFetchesOf('status/loading-3')

    first.resolve({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await opening
    refetch.reject(new Error('API 502'))
    await refetching
    /* The grid shows the error instead of loading for good. */
    expect(store.loading).toBe(false)
    expect(store.error?.message).toBe('API 502')
  })

  it('leaves loading to a newer non-silent fetch', async () => {
    const refetch = deferred()
    const reopen = deferred()
    apiMock.mockReturnValueOnce(refetch.promise).mockReturnValueOnce(reopen.promise)
    const store = useStatusStore<Row>('status/loading-4', 'cls', 'uuid')
    const refetching = store.fetch({ silent: true })
    const reopening = store.fetch()
    expectTwoFetchesOf('status/loading-4')

    refetch.resolve({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await refetching
    expect(store.loading).toBe(true)

    reopen.resolve({ entries: [{ uuid: 'a', name: 'Alpha' }] })
    await reopening
    expect(store.loading).toBe(false)
  })
})
