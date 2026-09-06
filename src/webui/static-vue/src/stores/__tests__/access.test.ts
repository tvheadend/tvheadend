// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Access store — the two fill paths. `preloadFromHttp()` hydrates from
 * `api/access/whoami` (API v20) before the SPA mounts; Comet's
 * `accessUpdate` remains the live-update channel and the only path on
 * pre-v20 servers (where whoami 404s and must be swallowed).
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'

const h = vi.hoisted(() => ({
  apiCall: vi.fn(),
  listeners: new Map<string, (msg: unknown) => void>(),
}))

vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => h.apiCall(...args),
}))
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (cls: string, fn: (msg: unknown) => void) => {
      h.listeners.set(cls, fn)
      return () => h.listeners.delete(cls)
    },
  },
}))

import { useAccessStore } from '../access'

const WHOAMI = {
  username: 'alice',
  admin: 1,
  dvr: 1,
  uilevel: 'expert',
  theme: 'access',
  page_size: 50,
}

beforeEach(() => {
  setActivePinia(createPinia())
  h.apiCall.mockReset()
  h.listeners.clear()
  delete document.documentElement.dataset.theme
})

describe('access store — preloadFromHttp', () => {
  it('hydrates the store from access/whoami before Comet', async () => {
    h.apiCall.mockResolvedValue(WHOAMI)
    const store = useAccessStore()
    expect(store.loaded).toBe(false)

    await store.preloadFromHttp()

    expect(h.apiCall).toHaveBeenCalledWith('access/whoami')
    expect(store.loaded).toBe(true)
    expect(store.uilevel).toBe('expert')
    expect(store.has('admin')).toBe(true)
    expect(store.authMode).toBe('authenticated')
    /* The theme watcher applies the server theme without waiting
     * for the first Comet message — no blue flash. */
    await nextTick()
    expect(document.documentElement.dataset.theme).toBe('access')
  })

  it('swallows failures on pre-v20 servers and leaves Comet in charge', async () => {
    h.apiCall.mockRejectedValue(new Error('404'))
    const store = useAccessStore()

    await expect(store.preloadFromHttp()).resolves.toBeUndefined()
    expect(store.loaded).toBe(false)

    /* Comet's accessUpdate still populates the store as before. */
    h.listeners.get('accessUpdate')?.({
      notificationClass: 'accessUpdate',
      ...WHOAMI,
      uilevel: 'advanced',
    })
    expect(store.loaded).toBe(true)
    expect(store.uilevel).toBe('advanced')
  })

  it('lets a later Comet accessUpdate overwrite the preloaded state', async () => {
    h.apiCall.mockResolvedValue(WHOAMI)
    const store = useAccessStore()
    await store.preloadFromHttp()

    h.listeners.get('accessUpdate')?.({
      notificationClass: 'accessUpdate',
      ...WHOAMI,
      theme: 'dark',
    })
    await nextTick()
    expect(document.documentElement.dataset.theme).toBe('dark')
  })
})
