// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheBudget from '../CacheBudget.vue'
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

describe('CacheBudget', () => {
  it('shows the timeshift, the RAM and the buffer, with no notice, when both are in use', () => {
    const w = mount(CacheBudget, { props: { budget: base } })
    expect(labels(w)).toEqual(['Timeshift', 'RAM', 'Buffering'])
    expect(w.find('.cache-budget__mode').exists()).toBe(false)
  })

  it('under RAM only, shows the RAM alone, split by cache, with the even shares marked', () => {
    const w = mount(CacheBudget, {
      props: {
        budget: { ...base, ram_only: true, ram_used: 180 * MB },
        caches: [cache('b', 'ICI', 30 * MB), cache('a', 'CBC', 60 * MB), cache('c', 'TVA', 0)],
      },
    })
    expect(labels(w)).toEqual(['RAM'])
    expect(w.find('.cache-budget__mode').text()).toContain('RAM only')
    expect(w.find('.cache-budget__mode').text()).toContain('3 caches')
    /* the caches holding RAM, in name order, each its proportion of what is used */
    const parts = w.findAll('.cache-budget__fill--cache')
    expect(parts.map((p) => p.attributes('title'))).toEqual(['CBC: 60.0 MiB', 'ICI: 30.0 MiB'])
    expect(parts[0].attributes('style')).toContain('width: 60.0%')
    expect(parts[1].attributes('style')).toContain('width: 30.0%')
    /* three caches, two places where a share ends */
    expect(w.findAll('.cache-budget__mark')).toHaveLength(2)
    expect(w.findAll('.cache-budget__key li').map((l) => l.text())).toEqual([
      'CBC 60.0 MiB',
      'ICI 30.0 MiB',
    ])
  })

  it('with no RAM budget, says the caches go to disk and leaves the RAM out', () => {
    const w = mount(CacheBudget, { props: { budget: { ...base, ram_max: 0, ram_used: 0 } } })
    expect(labels(w)).toEqual(['Timeshift', 'Buffering'])
    expect(w.find('.cache-budget__mode').text()).toContain('Disk only')
  })

  it('with no RAM budget, splits the timeshift by cache, the even shares marked', () => {
    const disk = (uuid: string, channel: string, size: number) => ({
      ...cache(uuid, channel, 0),
      size,
      disk: size,
    })
    const w = mount(CacheBudget, {
      props: {
        budget: { ...base, ram_max: 0, ram_used: 0, size_used: 400 * MB },
        caches: [disk('a', 'CBC', 300 * MB), disk('b', 'ICI', 100 * MB)],
      },
    })
    expect(labels(w)).toEqual(['Timeshift', 'Buffering'])
    expect(w.find('.cache-budget__mode').text()).toContain('2 caches share')
    const parts = w.findAll('.cache-budget__fill--cache')
    expect(parts.map((p) => p.attributes('style'))).toEqual([
      expect.stringContaining('width: 30.0%'),
      expect.stringContaining('width: 10.0%'),
    ])
    expect(w.findAll('.cache-budget__mark')).toHaveLength(1)
    expect(w.findAll('.cache-budget__key li').map((l) => l.text())).toEqual([
      'CBC 300.0 MiB',
      'ICI 100.0 MiB',
    ])
  })
})
