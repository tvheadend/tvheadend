// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheBudget from '../CacheBudget.vue'
import { useCacheView } from '@/composables/useCacheView'
import { CACHE_PALETTE } from '@/utils/cacheMap'
import type { CacheBudget as Budget, CacheEntry } from '@/types/cache'

const MB = 1024 * 1024

const base: Budget = {
  enabled: true,
  ram_only: false,
  unlimited: false,
  size_used: 300 * MB,
  size_max: 1000 * MB,
  ram_used: 100 * MB,
  ram_max: 200 * MB,
  staging_used: 1 * MB,
  staging_max: 32 * MB,
  keepalive: 15,
  period: 3600,
}

function cache(uuid: string, channel: string, ram: number): CacheEntry {
  return {
    uuid,
    channel,
    size: ram,
    ram,
    staging: 0,
    disk: 0,
    segments: 0,
    oldest: 0,
    newest: 0,
    span: 60,
    gap: false,
    write_error: false,
    ram_only: true,
    dropped: 0,
    viewers: [],
    recordings: [],
    readers: 0,
    gates: 0,
    map: [],
    subscriptions: 0,
  }
}

const labels = (w: ReturnType<typeof mount>) =>
  w.findAll('.cache-budget__label > span:first-child').map((s) => s.text())

const { view } = useCacheView()

describe('CacheBudget', () => {
  afterEach(() => {
    view.value = 'use'
  })

  it('shows the RAM, the disk and the buffer, with no notice, when both are in use', () => {
    const w = mount(CacheBudget, { props: { budget: base } })
    expect(labels(w)).toEqual(['RAM', 'Disk', 'Buffering'])
    expect(w.find('.cache-budget__mode').exists()).toBe(false)
    expect(w.findAll('.cache-budget__mark')).toHaveLength(0)
  })

  it('under RAM only, shows the RAM alone; by cache, split in the order of the names', () => {
    view.value = 'cache'
    const w = mount(CacheBudget, {
      props: {
        budget: { ...base, ram_only: true, ram_used: 180 * MB },
        caches: [cache('b', 'ICI', 30 * MB), cache('a', 'CBC', 60 * MB), cache('c', 'TVA', 0)],
      },
    })
    expect(labels(w)).toEqual(['RAM'])
    expect(w.find('.cache-budget__mode').text()).toContain('RAM only')
    expect(w.find('.cache-budget__mode').text()).toContain('3 caches')
    /* the caches holding RAM, each its proportion of what is used, in the
     * colour the legend gives it */
    const parts = w.findAll('.cache-budget__fill--cache')
    expect(parts.map((p) => p.attributes('title'))).toEqual(['CBC: 60.0 MiB', 'ICI: 30.0 MiB'])
    expect(parts[0].attributes('style')).toContain('width: 60.0%')
    expect(parts[1].attributes('style')).toContain('width: 30.0%')
    expect(parts[0].attributes('style')).toMatch(/background: (#6d2f9c|rgb\(109, 47, 156\))/)
    /* three caches, two places where a share ends */
    expect(w.findAll('.cache-budget__mark')).toHaveLength(2)
  })

  it('with no RAM budget, says the caches go to disk and leaves the RAM out', () => {
    const w = mount(CacheBudget, { props: { budget: { ...base, ram_max: 0, ram_used: 0 } } })
    expect(labels(w)).toEqual(['Disk', 'Buffering'])
    expect(w.find('.cache-budget__mode').text()).toContain('Disk only')
  })

  it('with no RAM budget, splits the disk by cache, the even shares marked', () => {
    view.value = 'cache'
    const disk = (uuid: string, channel: string, size: number) => ({
      ...cache(uuid, channel, 0),
      size,
      disk: size,
    })
    const w = mount(CacheBudget, {
      props: {
        budget: { ...base, ram_max: 0, ram_used: 0, size_used: 401 * MB },
        caches: [disk('a', 'CBC', 300 * MB), disk('b', 'ICI', 100 * MB)],
      },
    })
    expect(labels(w)).toEqual(['Disk', 'Buffering'])
    expect(w.find('.cache-budget__mode').text()).toContain('2 caches share')
    /* 400 MiB written of 1000, the buffer's 1 MiB aside */
    const parts = w.findAll('.cache-budget__fill--cache')
    expect(parts.map((p) => p.attributes('style'))).toEqual([
      expect.stringContaining('width: 30.0%'),
      expect.stringContaining('width: 10.0%'),
    ])
    expect(w.findAll('.cache-budget__mark')).toHaveLength(1)
    /* the buffer stays yellow, by cache as by use */
    expect(w.find('.cache-budget__fill--staging').exists()).toBe(true)
  })

  it('with RAM and a disk, splits both by cache and keeps the colours of the legend', () => {
    view.value = 'cache'
    const both = (uuid: string, channel: string, ram: number, size: number) => ({
      ...cache(uuid, channel, ram),
      ram_only: false,
      size,
      disk: size,
    })
    const w = mount(CacheBudget, {
      props: {
        budget: { ...base, ram_used: 100 * MB, size_used: 501 * MB },
        caches: [both('b', 'ICI', 40 * MB, 300 * MB), both('a', 'CBC', 60 * MB, 200 * MB)],
      },
    })
    const [ram, disk] = w.findAll('.cache-budget__track')
    expect(ram.findAll('.cache-budget__fill--cache').map((p) => p.attributes('title'))).toEqual([
      'CBC: 60.0 MiB',
      'ICI: 40.0 MiB',
    ])
    expect(disk.findAll('.cache-budget__fill--cache').map((p) => p.attributes('title'))).toEqual([
      'CBC: 200.0 MiB',
      'ICI: 300.0 MiB',
    ])
    /* each cache in a colour of the palette, never the blue, green or yellow
     * of the meters drawn by use */
    const rgb = (hex: string) =>
      `rgb(${[1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16)).join(', ')})`
    for (const p of w.findAll('.cache-budget__fill--cache')) {
      const style = p.attributes('style') ?? ''
      expect(CACHE_PALETTE.some((c) => style.includes(c) || style.includes(rgb(c)))).toBe(true)
    }
    expect(ram.findAll('.cache-budget__mark')).toHaveLength(1)
    expect(disk.findAll('.cache-budget__mark')).toHaveLength(1)
  })
})
