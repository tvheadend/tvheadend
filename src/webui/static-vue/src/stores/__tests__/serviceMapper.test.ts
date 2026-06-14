// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { applyStatusUpdate, jobIsActive, processedCount } from '../serviceMapper'

/* Mock cometClient so the store doesn't subscribe to a real
 * connection during tests. We import the store dynamically per
 * test to pick up the mock + a fresh pinia. */
const cometOn = vi.fn()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (cls: string, listener: unknown) => {
      cometOn(cls, listener)
      return () => {}
    },
  },
}))

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  cometOn.mockReset()
})

afterEach(() => {
  apiMock.mockReset()
  cometOn.mockReset()
})

describe('applyStatusUpdate', () => {
  it('coerces numeric fields with sensible defaults', () => {
    expect(applyStatusUpdate({ notificationClass: 'servicemapper' })).toEqual({
      total: 0,
      ok: 0,
      fail: 0,
      ignore: 0,
      active: null,
    })
  })

  it('reads counters + active uuid from a typical comet payload', () => {
    expect(
      applyStatusUpdate({
        notificationClass: 'servicemapper',
        total: 50,
        ok: 12,
        fail: 1,
        ignore: 3,
        active: 'svc-uuid-active',
      }),
    ).toEqual({ total: 50, ok: 12, fail: 1, ignore: 3, active: 'svc-uuid-active' })
  })

  it('treats an empty `active` string as null (idle)', () => {
    /* Server omits the key entirely when idle, but defend
     * against a stray empty string from a future change. */
    expect(
      applyStatusUpdate({ notificationClass: 'servicemapper', active: '' }).active,
    ).toBeNull()
  })

  it('coerces non-number counter fields to 0 (defensive)', () => {
    expect(
      applyStatusUpdate({
        notificationClass: 'servicemapper',
        total: 'oops' as unknown as number,
        ok: undefined as unknown as number,
      }).total,
    ).toBe(0)
  })
})

describe('jobIsActive', () => {
  it('true when active uuid is set, regardless of counters', () => {
    expect(jobIsActive({ total: 0, ok: 0, fail: 0, ignore: 0, active: 'x' })).toBe(true)
    expect(jobIsActive({ total: 100, ok: 50, fail: 0, ignore: 0, active: 'x' })).toBe(true)
  })

  it('false when active is null even with prior counters', () => {
    /* Counters stick at their final values between jobs — we
     * treat "active uuid set" as the sole truthy signal of
     * in-flight, so the page reads "Idle" with last-run counts
     * after a completed job. */
    expect(jobIsActive({ total: 100, ok: 95, fail: 2, ignore: 3, active: null })).toBe(false)
    expect(jobIsActive({ total: 0, ok: 0, fail: 0, ignore: 0, active: null })).toBe(false)
  })
})

describe('processedCount', () => {
  it('sums ok + fail + ignore', () => {
    expect(processedCount({ total: 0, ok: 10, fail: 2, ignore: 3, active: null })).toBe(15)
  })

  it('returns 0 for a fresh status', () => {
    expect(processedCount({ total: 0, ok: 0, fail: 0, ignore: 0, active: null })).toBe(0)
  })
})

describe('useServiceMapperStore', () => {
  it('subscribes to the "servicemapper" comet class on first instantiation', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    useServiceMapperStore()
    expect(cometOn).toHaveBeenCalledTimes(1)
    expect(cometOn.mock.calls[0][0]).toBe('servicemapper')
  })

  it('fetchInitial loads the snapshot from service/mapper/status', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    apiMock.mockResolvedValueOnce({ total: 10, ok: 5, fail: 0, ignore: 1 })
    const store = useServiceMapperStore()
    await store.fetchInitial()
    expect(apiMock).toHaveBeenCalledWith('service/mapper/status', {})
    expect(store.status.total).toBe(10)
    expect(store.status.ok).toBe(5)
    expect(store.status.active).toBeNull()
  })

  it('records fetchInitial errors on the store', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    apiMock.mockRejectedValueOnce(new Error('boom'))
    const store = useServiceMapperStore()
    await store.fetchInitial()
    expect(store.error).toBe('boom')
  })

  it('comet listener applies the new payload to status', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    /* idnode/load may fire when active uuid is set — return a
     * resolved name so the resolution path doesn't reject. */
    apiMock.mockResolvedValueOnce({ entries: [{ text: 'BBC One' }] })
    const store = useServiceMapperStore()
    /* The listener is the second arg of the first cometOn call. */
    const listener = cometOn.mock.calls[0][1] as (
      msg: Record<string, unknown>,
    ) => void
    listener({
      notificationClass: 'servicemapper',
      total: 50,
      ok: 12,
      fail: 1,
      ignore: 0,
      active: 'svc-1',
    })
    expect(store.status.total).toBe(50)
    expect(store.status.ok).toBe(12)
    expect(store.status.active).toBe('svc-1')
    expect(store.isActive).toBe(true)
  })

  it('stop() POSTs service/mapper/stop and tracks inflight flag', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    apiMock.mockResolvedValueOnce({})
    const store = useServiceMapperStore()
    await store.stop()
    expect(apiMock).toHaveBeenCalledWith('service/mapper/stop', {})
    expect(store.stopping).toBe(false)
  })

  it('progressFraction uses processed / total, capped at 1', async () => {
    const { useServiceMapperStore } = await import('../serviceMapper')
    const store = useServiceMapperStore()
    store.status.total = 100
    store.status.ok = 30
    store.status.fail = 5
    store.status.ignore = 15
    expect(store.progressFraction).toBe(0.5)

    /* total 0 → 0 (avoid NaN). */
    store.status.total = 0
    expect(store.progressFraction).toBe(0)
  })
})
