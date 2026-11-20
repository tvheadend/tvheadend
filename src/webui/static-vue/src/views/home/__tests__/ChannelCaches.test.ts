// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * ChannelCaches — Home's cache summary: hidden while the cache is off
 * or empty, longest-held channels first, at most six of them.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount, RouterLinkStub } from '@vue/test-utils'
import { computed, ref } from 'vue'
import ChannelCaches from '../ChannelCaches.vue'
import type { CacheBudget, CacheEntry } from '@/types/cache'

const entries = ref<CacheEntry[]>([])
const budget = ref<CacheBudget | null>(null)
vi.mock('@/composables/useCaches', () => ({
  useCaches: () => ({
    entries: computed(() => entries.value),
    budget: computed(() => budget.value),
    byUuid: computed(() => new Map()),
  }),
}))

function entry(uuid: string, span: number, subscriptions = 1): CacheEntry {
  return {
    uuid,
    channel: uuid.toUpperCase(),
    span,
    map: [1],
    viewers: [],
    recordings: [],
    subscriptions,
  } as unknown as CacheEntry
}

const on: CacheBudget = {
  enabled: true,
  ram_only: false,
  unlimited: false,
  size_used: 0,
  size_max: 1,
  ram_used: 0,
  ram_max: 1,
  staging_used: 0,
  staging_max: 1,
  keepalive: 15,
  period: 3600,
}

function render() {
  return mount(ChannelCaches, { global: { stubs: { 'router-link': RouterLinkStub } } })
}

describe('ChannelCaches', () => {
  it('renders nothing while the cache is off, or no channel holds one', () => {
    budget.value = { ...on, enabled: false }
    entries.value = [entry('a', 60)]
    expect(render().find('.channel-caches').exists()).toBe(false)
    budget.value = on
    entries.value = []
    expect(render().find('.channel-caches').exists()).toBe(false)
  })

  it('lists the longest-held channels first, six at most, and links to all', () => {
    budget.value = on
    entries.value = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'].map((u, i) => entry(u, i * 60))
    const w = render()
    expect(w.findAll('.channel-caches__name').map((n) => n.text())).toEqual([
      'H',
      'G',
      'F',
      'E',
      'D',
      'C',
    ])
    expect(w.findComponent(RouterLinkStub).text()).toBe('All 8')
  })

  it('leaves out the caches only kept tuned, and keeps the budget meters', () => {
    budget.value = on
    entries.value = [entry('a', 600, 0), entry('b', 60)]
    const w = render()
    expect(w.findAll('.channel-caches__name').map((n) => n.text())).toEqual(['B'])
    expect(w.find('.cache-budget').exists()).toBe(true)
    expect(w.find('.cache-legend--inline').exists()).toBe(true)
    /* nothing in use: no card at all */
    entries.value = [entry('a', 600, 0)]
    expect(render().find('.channel-caches').exists()).toBe(false)
  })
})
