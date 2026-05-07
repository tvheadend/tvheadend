/*
 * dvrEntries store unit tests. Focused on the Comet self-subscription
 * surface: any of `dvrentry` / `dvrconfig` / `channel` change/create/
 * delete notifications must trigger a re-fetch of `grid_upcoming`,
 * because each can change a row's effective `start_real`/`stop_real`
 * window (per-entry override, DVR config defaults, channel defaults
 * respectively).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Capture the listener that the store registers per Comet class so
 * tests can fire synthetic notifications at it without going through
 * the real WebSocket / long-poll machinery. */
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
})

afterEach(() => {
  apiMock.mockReset()
})

async function importStore() {
  /* Re-import per test to ensure the Pinia setup runs against a fresh
   * Pinia + fresh handler map. The store factory registers Comet
   * subscriptions when the store is first accessed. */
  const mod = await import('../dvrEntries')
  return mod.useDvrEntriesStore()
}

describe('useDvrEntriesStore — Comet refresh subscriptions', () => {
  it('registers handlers for dvrentry, dvrconfig, channel', async () => {
    const store = await importStore()
    /* Touch reactive state to ensure the store factory has fully run
     * (Pinia setup-stores execute their factory the first time any
     * exposed property is read). The expect() reads `entries`, which
     * is enough to trigger the cometClient.on() registrations. */
    expect(store.entries).toEqual([])

    expect(handlers.has('dvrentry')).toBe(true)
    expect(handlers.has('dvrconfig')).toBe(true)
    expect(handlers.has('channel')).toBe(true)
  })

  it('refreshes after ensure() when dvrentry change fires', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const store = await importStore()
    await store.ensure()
    expect(apiMock).toHaveBeenCalledTimes(1)

    handlers.get('dvrentry')!({
      notificationClass: 'dvrentry',
      change: ['some-uuid'],
    })
    /* refresh() is async and the subscription is fire-and-forget; flush
     * the microtask queue to let the inflight assignment + fetchOnce
     * call register. */
    await Promise.resolve()
    expect(apiMock).toHaveBeenCalledTimes(2)
  })

  it('refreshes when dvrconfig change fires (config-default padding edits)', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const store = await importStore()
    await store.ensure()

    handlers.get('dvrconfig')!({
      notificationClass: 'dvrconfig',
      change: ['cfg-uuid'],
    })
    await Promise.resolve()
    expect(apiMock).toHaveBeenCalledTimes(2)
  })

  it('refreshes when channel change fires (per-channel default padding)', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const store = await importStore()
    await store.ensure()

    handlers.get('channel')!({
      notificationClass: 'channel',
      change: ['ch-uuid'],
    })
    await Promise.resolve()
    expect(apiMock).toHaveBeenCalledTimes(2)
  })

  it('does not refresh before ensure() — no point fetching what nothing has read', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const store = await importStore()
    /* Touch reactive state to register the comet handlers without
     * calling ensure() — see the first test for the rationale. */
    expect(store.entries).toEqual([])

    handlers.get('dvrconfig')!({
      notificationClass: 'dvrconfig',
      change: ['cfg-uuid'],
    })
    await Promise.resolve()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('ignores empty notifications (no create/change/delete)', async () => {
    apiMock.mockResolvedValue({ entries: [] })
    const store = await importStore()
    await store.ensure()
    expect(apiMock).toHaveBeenCalledTimes(1)

    handlers.get('dvrconfig')!({ notificationClass: 'dvrconfig' })
    await Promise.resolve()
    expect(apiMock).toHaveBeenCalledTimes(1)
  })
})
