/*
 * Idnode-class store unit tests. We mock the API client and assert
 * the store's caching + dedup behavior — the part that's not just
 * "trivial passthrough to fetch()".
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { useIdnodeClassStore } from '../idnodeClass'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
})

afterEach(() => {
  apiMock.mockReset()
})

describe('useIdnodeClassStore', () => {
  it('caches the metadata after the first ensure() call', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrentry', props: [] })
    const store = useIdnodeClassStore()

    const first = await store.ensure('dvrentry')
    const second = await store.ensure('dvrentry')

    expect(first).toEqual({ class: 'dvrentry', props: [] })
    expect(second).toEqual(first)
    /* The fetch fires only once — the second call hit the cache. */
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('dedups concurrent ensure() calls into a single fetch', async () => {
    let resolveFetch: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFetch = resolve
      })
    )
    const store = useIdnodeClassStore()

    const p1 = store.ensure('dvrentry')
    const p2 = store.ensure('dvrentry')
    expect(apiMock).toHaveBeenCalledTimes(1)

    resolveFetch({ class: 'dvrentry', props: [] })
    const [r1, r2] = await Promise.all([p1, p2])
    expect(r1).toBe(r2)
  })

  it('caches fetch failures as null (no infinite retries)', async () => {
    apiMock.mockRejectedValueOnce(new Error('network down'))
    const store = useIdnodeClassStore()

    const result = await store.ensure('dvrentry')
    expect(result).toBeNull()

    /* Second call hits the cache, no second fetch. */
    const second = await store.ensure('dvrentry')
    expect(second).toBeNull()
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('get() returns undefined for un-fetched classes and the cached value otherwise', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrentry', props: [] })
    const store = useIdnodeClassStore()

    expect(store.get('dvrentry')).toBeUndefined()
    await store.ensure('dvrentry')
    expect(store.get('dvrentry')).toEqual({ class: 'dvrentry', props: [] })
  })
})
