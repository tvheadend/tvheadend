// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * streamProfiles store unit tests.
 *
 * Browser-playability detection is currently disabled (see the
 * store header): the store fetches the streamable-profile list from
 * `profile/list`, sorts it, and offers every profile to both the
 * external picker and the in-browser player.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import { flushPromises } from '@vue/test-utils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Capture the store's `'profile'` Comet handler so a test can fire
 * it (standing in for a profile added / changed elsewhere). */
let cometHandler: ((msg: unknown) => void) | null = null
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (cls: string, fn: (msg: unknown) => void) => {
      if (cls === 'profile') cometHandler = fn
      return () => {}
    },
  },
}))

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  cometHandler = null
})

afterEach(() => {
  apiMock.mockReset()
})

/* Wire apiMock to answer profile/list with the given entries. */
function mockProfileList(list: { key: string; val: string }[]) {
  apiMock.mockImplementation((endpoint: string) => {
    if (endpoint === 'profile/list') {
      return Promise.resolve({ entries: list })
    }
    return Promise.resolve({ entries: [] })
  })
}

async function importStore() {
  const mod = await import('../streamProfiles')
  return mod.useStreamProfilesStore()
}

describe('useStreamProfilesStore', () => {
  it('canPlayInBrowser is false before ensure() resolves', async () => {
    const store = await importStore()
    expect(store.canPlayInBrowser).toBe(false)
  })

  it('exposes profileNames from profile/list, sorted alphabetically', async () => {
    /* profile/list returns registration order; the pickers present
     * them sorted. */
    mockProfileList([
      { key: 'p2', val: 'webtv' },
      { key: 'p1', val: 'pass' },
      { key: 'p3', val: 'audio-stereo' },
    ])
    const store = await importStore()
    await store.ensure()
    expect(store.profileNames).toEqual(['audio-stereo', 'pass', 'webtv'])
  })

  it('offers every profile to the in-browser player', async () => {
    mockProfileList([
      { key: 'p1', val: 'pass' },
      { key: 'p2', val: 'webtv' },
    ])
    const store = await importStore()
    await store.ensure()
    expect(store.canPlayInBrowser).toBe(true)
    expect(store.playableProfiles).toEqual([
      { name: 'pass', label: 'pass' },
      { name: 'webtv', label: 'webtv' },
    ])
  })

  it('canPlayInBrowser is false when the server offers no profile', async () => {
    mockProfileList([])
    const store = await importStore()
    await store.ensure()
    expect(store.canPlayInBrowser).toBe(false)
    expect(store.playableProfiles).toEqual([])
  })

  it('empty on fetch failure, error set', async () => {
    apiMock.mockRejectedValue(new Error('network down'))
    const store = await importStore()
    await store.ensure()
    expect(store.canPlayInBrowser).toBe(false)
    expect(store.profileNames).toEqual([])
    expect(store.error).toBeInstanceOf(Error)
  })

  it('ensure() fetches once and caches', async () => {
    mockProfileList([])
    const store = await importStore()
    await store.ensure()
    await store.ensure()
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('markProfileFailed flags a profile for the session', async () => {
    mockProfileList([{ key: 'p1', val: 'webtv' }])
    const store = await importStore()
    await store.ensure()
    expect(store.failedProfiles.has('webtv')).toBe(false)
    store.markProfileFailed('webtv')
    expect(store.failedProfiles.has('webtv')).toBe(true)
  })

  it('markProfileFailed ignores an empty name', async () => {
    const store = await importStore()
    store.markProfileFailed('')
    expect(store.failedProfiles.size).toBe(0)
  })

  it('clearProfileFailed removes a profile flag', async () => {
    const store = await importStore()
    store.markProfileFailed('webtv')
    expect(store.failedProfiles.has('webtv')).toBe(true)
    store.clearProfileFailed('webtv')
    expect(store.failedProfiles.has('webtv')).toBe(false)
  })

  it('re-fetches profile/list on a "profile" Comet notification', async () => {
    mockProfileList([{ key: 'p1', val: 'pass' }])
    const store = await importStore()
    await store.ensure()
    expect(store.profileNames).toEqual(['pass'])
    expect(apiMock).toHaveBeenCalledTimes(1)

    /* A profile added elsewhere — e.g. via the legacy ExtJS UI. */
    mockProfileList([
      { key: 'p1', val: 'pass' },
      { key: 'p2', val: 'webtv' },
    ])
    cometHandler?.({ create: ['p2'] })
    await flushPromises()

    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(store.profileNames).toEqual(['pass', 'webtv'])
  })

  it('ignores a "profile" notification carrying no create/change/delete', async () => {
    mockProfileList([{ key: 'p1', val: 'pass' }])
    const store = await importStore()
    await store.ensure()
    expect(apiMock).toHaveBeenCalledTimes(1)

    cometHandler?.({})
    await flushPromises()
    expect(apiMock).toHaveBeenCalledTimes(1)
  })
})
