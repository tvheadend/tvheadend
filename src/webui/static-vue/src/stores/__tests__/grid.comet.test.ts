// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useGridStore Comet-listener path tests.
 *
 * The store registers a Comet listener for the inferred entity class
 * when the factory runs. Notifications carry `create` / `change` /
 * `delete` UUID lists. The store routes each kind through a different
 * apply path:
 *
 *   - delete : in-place filter of `entries.value` (no server call).
 *   - create : full fetch (existing path, loading mask flips).
 *   - change : targeted `idnode/load?uuid=…&grid=1` fetch that merges
 *              the returned rows by uuid into `entries.value`. Does
 *              NOT flip `loading.value` — this is what removes the
 *              every-5-second flash on grids subscribing to entities
 *              that emit periodic status pushes (DVR Upcoming during
 *              an active recording is the canonical case;
 *              `dvr_notify()` at src/dvr/dvr_rec.c:1328-1335 fires
 *              every 5 s per active recording).
 *
 * Notifications accumulate inside a 500 ms debounce window before
 * the apply runs.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

type Listener = (msg: unknown) => void
const handlers = new Map<string, Listener>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: Listener) => {
      handlers.set(klass, listener)
      return () => handlers.delete(klass)
    },
  },
}))

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  handlers.clear()
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

async function makeStore(
  initial: Array<{ uuid: string; [k: string]: unknown }>,
  endpoint = 'dvr/entry/grid_upcoming',
  cometClass = 'dvrentry',
) {
  /* First fetch lands the seed entries. The `entries` field on the
   * server response matches what the user sees in the grid. */
  apiMock.mockResolvedValueOnce({ entries: initial, total: initial.length })
  vi.resetModules()
  const mod = await import('../grid')
  const store = mod.useGridStore(endpoint)
  await store.fetch()
  return { store, fire: (msg: unknown) => handlers.get(cometClass)?.(msg) }
}

describe('useGridStore — Comet delete notifications', () => {
  it('removes rows by uuid without firing a server call', async () => {
    const { store, fire } = await makeStore([
      { uuid: 'a', title: 'Alpha' },
      { uuid: 'b', title: 'Bravo' },
      { uuid: 'c', title: 'Charlie' },
    ])
    apiMock.mockClear()

    fire({ delete: ['b'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).not.toHaveBeenCalled()
    expect(store.entries.map((e) => e.uuid)).toEqual(['a', 'c'])
    /* Total tracks deletes so the count chip stays accurate. */
    expect(store.total).toBe(2)
  })

  it('handles multiple deletes in one notification', async () => {
    const { store, fire } = await makeStore([
      { uuid: 'a' },
      { uuid: 'b' },
      { uuid: 'c' },
    ])
    apiMock.mockClear()
    fire({ delete: ['a', 'c'] })
    await vi.advanceTimersByTimeAsync(500)
    expect(store.entries.map((e) => e.uuid)).toEqual(['b'])
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('refetches for a deleted uuid the grid never held instead of guessing the total', async () => {
    /* With a server-side filter active, a deleted row the grid
     * never loaded may not have matched the filter — whether it
     * was counted in `total` is unknowable client-side, so
     * blindly subtracting would undercount the chip. */
    const { store, fire } = await makeStore([
      { uuid: 'a', title: 'Alpha' },
      { uuid: 'b', title: 'Bravo' },
    ])
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', title: 'Alpha' },
        { uuid: 'b', title: 'Bravo' },
      ],
      total: 2,
    })
    store.setFilter([{ type: 'string', value: 'r', field: 'title' }])
    await vi.runAllTimersAsync()
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', title: 'Alpha' },
        { uuid: 'b', title: 'Bravo' },
      ],
      total: 2,
    })

    fire({ delete: ['never-loaded-uuid'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* The unseen delete routes through a full fetch; the server's
     * authoritative total stands. */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('dvr/entry/grid_upcoming')
    expect(store.total).toBe(2)
    expect(store.entries.map((e) => e.uuid)).toEqual(['a', 'b'])
  })

  it('mixed wave: subtracts held deletes, refetches for the unseen rest', async () => {
    const { store, fire } = await makeStore([
      { uuid: 'a' },
      { uuid: 'b' },
    ])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'b' }], total: 1 })

    fire({ delete: ['a', 'never-loaded-uuid'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('dvr/entry/grid_upcoming')
    expect(store.entries.map((e) => e.uuid)).toEqual(['b'])
    expect(store.total).toBe(1)
  })
})

describe('useGridStore — Comet change notifications', () => {
  it('fires a targeted idnode/load?uuid=…&grid=1 instead of a full refetch', async () => {
    const { store, fire } = await makeStore([
      { uuid: 'a', title: 'Alpha', size: 1 },
      { uuid: 'b', title: 'Bravo', size: 2 },
    ])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'b', title: 'Bravo-updated', size: 999 }],
    })

    fire({ change: ['b'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('idnode/load')
    expect(apiMock.mock.calls[0][1]).toMatchObject({ uuid: ['b'], grid: 1 })
    /* Row b is patched in place; row a untouched. */
    const a = store.entries.find((e) => e.uuid === 'a')
    const b = store.entries.find((e) => e.uuid === 'b')
    expect(a).toMatchObject({ uuid: 'a', title: 'Alpha', size: 1 })
    expect(b).toMatchObject({ uuid: 'b', title: 'Bravo-updated', size: 999 })
  })

  it('patches an idnode/load grid in serialize0 shape — no grid:1', async () => {
    /* Grids on plain `idnode/load` (the profile + dvrconfig config
     * grids) hold serialize0-shape rows keyed on the top-level
     * `text` title. The patch must NOT request `grid:1` for them —
     * doing so returns the api_idnode_grid shape, the merge drops
     * `text`, and the row renders blank until a full reload. */
    const { store, fire } = await makeStore(
      [
        { uuid: 'a', text: 'Alpha' },
        { uuid: 'b', text: 'Bravo' },
      ],
      'idnode/load',
      'idnode',
    )
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'b', text: 'Bravo-renamed' }],
    })

    fire({ change: ['b'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('idnode/load')
    expect(apiMock.mock.calls[0][1]).toEqual({ uuid: ['b'] })
    expect(apiMock.mock.calls[0][1]).not.toHaveProperty('grid')
    /* The patched row keeps its `text` — column stays populated. */
    expect(store.entries.find((e) => e.uuid === 'b')).toMatchObject({
      uuid: 'b',
      text: 'Bravo-renamed',
    })
  })

  it('falls back to a full refetch for a bespoke list endpoint instead of patching via idnode/load', async () => {
    /* Bespoke list endpoints (`epggrab/module/list`, `caclient/list`,
     * `codec_profile/list`) hand-build each row with display-only
     * `title`/`status` fields synthesized server-side
     * (api_epggrab.c:45-47 et al.) that `idnode/load` never emits.
     * Patching a changed row through idnode/load would blank those
     * columns — and the emptied `title`, sorted ASC, floats the row
     * to the top — until a remount. The change must route through a
     * full refetch from the endpoint's own URL so the synthesized
     * fields survive. (This is th0ma7's "first line goes empty after
     * Save on an EPG grabber, fixed by switching tabs" report.) */
    const { store, fire } = await makeStore(
      [
        { uuid: 'a', title: 'Alpha', status: 'epggrabmodEnabled' },
        { uuid: 'b', title: 'Bravo', status: 'epggrabmodDisabled' },
      ],
      'epggrab/module/list',
      'epggrabmodule',
    )
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a', title: 'Alpha', status: 'epggrabmodDisabled' },
        { uuid: 'b', title: 'Bravo', status: 'epggrabmodDisabled' },
      ],
    })

    fire({ change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* Exactly one call, and it's the full grid endpoint — NOT a
     * targeted idnode/load patch that would drop title/status. */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('epggrab/module/list')
    /* Synthesized columns survive on every row. */
    const a = store.entries.find((e) => e.uuid === 'a')
    expect(a).toMatchObject({ title: 'Alpha', status: 'epggrabmodDisabled' })
  })

  it('does NOT flip loading.value during the patch (no flash)', async () => {
    const { store, fire } = await makeStore([{ uuid: 'a', size: 1 }])
    apiMock.mockClear()
    let loadingDuringRequest: boolean | null = null
    apiMock.mockImplementationOnce(async () => {
      loadingDuringRequest = store.loading
      return { entries: [{ uuid: 'a', size: 2 }] }
    })

    fire({ change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* If `loading.value = true` was flipped, the in-flight request
     * would have seen it. Pure change notifications must keep
     * loading false from start to finish — that's what removes the
     * visible flash. */
    expect(loadingDuringRequest).toBe(false)
    expect(store.loading).toBe(false)
  })

  it('skips uuids the user has not loaded into the current page', async () => {
    const { fire } = await makeStore([{ uuid: 'a' }])
    apiMock.mockClear()

    fire({ change: ['offscreen-uuid'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* Nothing on screen matched — no targeted load fired. */
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('debounces multiple change notifications into one request', async () => {
    const { fire } = await makeStore([
      { uuid: 'a' },
      { uuid: 'b' },
      { uuid: 'c' },
    ])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({ entries: [] })

    fire({ change: ['a'] })
    fire({ change: ['b'] })
    fire({ change: ['c'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(1)
    /* Order within the merged uuid list doesn't matter to the
     * server; assert as a set. */
    const sentUuids = apiMock.mock.calls[0][1].uuid as string[]
    expect(new Set(sentUuids)).toEqual(new Set(['a', 'b', 'c']))
  })

  it('keeps an in-flight user fetch valid when a patch lands mid-flight', async () => {
    /* A patch only rewrites rows in place — it must not invalidate
     * a user fetch already on the wire. If it did, the fetch's
     * response would be dropped and `loading` would stay true until
     * the next user gesture. */
    const { store, fire } = await makeStore([{ uuid: 'a', title: 'Old' }])
    apiMock.mockClear()

    let resolveFetch: (v: unknown) => void = () => {}
    apiMock.mockImplementationOnce(
      () => new Promise((res) => { resolveFetch = res }),
    )
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a', title: 'Patched' }],
    })

    const fetchPromise = store.fetch()
    expect(store.loading).toBe(true)

    fire({ change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* The patch applied in place while the fetch is still pending. */
    expect(store.entries[0]).toMatchObject({ title: 'Patched' })

    resolveFetch({ entries: [{ uuid: 'a', title: 'Fetched' }], total: 1 })
    await fetchPromise

    /* The fetch's response wins and the loading mask clears. */
    expect(store.loading).toBe(false)
    expect(store.entries[0]).toMatchObject({ title: 'Fetched' })
  })

  it('drops the patch when a user fetch fires during the patch round-trip', async () => {
    const { store, fire } = await makeStore([{ uuid: 'a', title: 'Old' }])
    apiMock.mockClear()

    let resolvePatch: (v: unknown) => void = () => {}
    apiMock.mockImplementationOnce(
      () => new Promise((res) => { resolvePatch = res }),
    )
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a', title: 'Fetched-newer' }],
      total: 1,
    })

    fire({ change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)

    /* User fetch fires and completes while the patch is pending. */
    await store.fetch()
    expect(store.entries[0]).toMatchObject({ title: 'Fetched-newer' })

    resolvePatch({ entries: [{ uuid: 'a', title: 'Patched-stale' }] })
    await vi.runAllTimersAsync()

    /* The stale patch must not overwrite the fresher fetch result. */
    expect(store.entries[0]).toMatchObject({ title: 'Fetched-newer' })
  })

  it('falls back to full fetch when the targeted load errors', async () => {
    const { store, fire } = await makeStore([{ uuid: 'a', title: 'Old' }])
    apiMock.mockClear()
    apiMock.mockRejectedValueOnce(new Error('server down'))
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a', title: 'Fresh' }],
      total: 1,
    })

    fire({ change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(apiMock.mock.calls[0][0]).toBe('idnode/load')
    expect(apiMock.mock.calls[1][0]).toBe('dvr/entry/grid_upcoming')
    expect(store.entries[0]).toMatchObject({ title: 'Fresh' })
  })
})

describe('useGridStore — Comet create notifications', () => {
  it('triggers a full fetch (loading mask is the right UX for new rows)', async () => {
    const { store, fire } = await makeStore([{ uuid: 'a' }])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a' }, { uuid: 'b' }],
      total: 2,
    })

    fire({ create: ['b'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('dvr/entry/grid_upcoming')
    expect(store.entries.map((e) => e.uuid)).toEqual(['a', 'b'])
  })

  it('falls back to full fetch when create + change land together', async () => {
    const { fire } = await makeStore([{ uuid: 'a' }])
    apiMock.mockClear()
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a' }, { uuid: 'b' }], total: 2 })

    fire({ create: ['b'], change: ['a'] })
    await vi.advanceTimersByTimeAsync(500)
    await vi.runAllTimersAsync()

    /* Create dominates: one full fetch covers both. */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('dvr/entry/grid_upcoming')
  })
})

