// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * SubscriptionCacheCell — the Channel cache column of Status >
 * Subscriptions: the role, how far behind live or how long a hold
 * has left, and the cache drawn when the cache list has it.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import SubscriptionCacheCell from '../SubscriptionCacheCell.vue'
import { useStatusStore } from '@/stores/status'
import type { CacheEntry } from '@/types/cache'

vi.mock('@/api/client', () => ({ apiCall: vi.fn() }))
vi.mock('@/api/comet', () => ({ cometClient: { on: () => () => {} } }))

const cache = {
  uuid: 'u1',
  map: [1, 1],
  span: 1200,
  viewers: [720],
  recordings: [],
} as unknown as CacheEntry

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('SubscriptionCacheCell', () => {
  it('names a viewer and how far behind live it is, over its cache', () => {
    useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid').entries = [cache]
    const w = mount(SubscriptionCacheCell, {
      props: { row: { cache_role: 'viewer', cache_behind: 720, cache: 'u1' } },
    })
    expect(w.find('.sub-cache__text').text()).toBe('Viewer −12 min')
    expect(w.find('.cache-state-mark--playing').exists()).toBe(true)
    /* the bar is too small for markers: the text carries the state */
    expect(w.find('.cache-bar__marker').exists()).toBe(false)
    expect(w.find('.cache-bar').exists()).toBe(true)
  })

  it('marks a paused viewer as such', () => {
    const w = mount(SubscriptionCacheCell, {
      props: { row: { cache_role: 'viewer', cache_behind: 300, cache_paused: true, cache: 'u1' } },
    })
    expect(w.find('.sub-cache__text').text()).toBe('Viewer (paused) −5 min')
    expect(w.find('.cache-state-mark--paused').attributes('aria-label')).toBe('Paused')
  })

  it('gives a hold the time it has left, without a bar', () => {
    const w = mount(SubscriptionCacheCell, {
      props: { row: { cache_role: 'keepalive', cache_expires: 540, cache: 'u1' } },
    })
    expect(w.find('.sub-cache__text').text()).toBe('Kept idle · 9 min left')
    expect(w.find('.cache-bar').exists()).toBe(false)
  })

  it('says a stream played live past the cache, over that cache', () => {
    useStatusStore<CacheEntry>('status/caches', 'caches', 'uuid').entries = [cache]
    const w = mount(SubscriptionCacheCell, {
      props: { row: { cache_role: 'live', cache_behind: 0, cache: 'u1' } },
    })
    expect(w.find('.sub-cache__text').text()).toBe('Live')
    expect(w.find('.cache-bar').exists()).toBe(true)
  })

  it('renders nothing for a subscription without a cache', () => {
    const w = mount(SubscriptionCacheCell, { props: { row: { id: 3 } } })
    expect(w.find('.sub-cache').exists()).toBe(false)
  })
})
