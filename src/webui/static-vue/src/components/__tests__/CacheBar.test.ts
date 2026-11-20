// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * CacheBar / CacheBudget — what a cache and its budgets look like:
 * one segment per run of the map, the recordings' share in reds,
 * play / pause markers where viewers stand, meters that read in bytes.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheBar from '../CacheBar.vue'
import CacheBudget from '../CacheBudget.vue'
import type { CacheBudget as Budget, CacheLocCode } from '@/types/cache'

/* a marker's place along the bar, 0 to 1, as the stylesheet reads it */
function pos(w: { element: Element }): string {
  return (w.element as HTMLElement).style.getPropertyValue('--pos')
}

describe('CacheBar', () => {
  const entry = {
    map: [3, 3, 1, 1, 0] as CacheLocCode[],
    span: 100,
    viewers: [50],
    recordings: [0],
  }

  it('draws one segment per run, sized by its slots', () => {
    const runs = mount(CacheBar, { props: { entry } }).findAll('.cache-bar__run')
    expect(runs.map((r) => r.classes().find((c) => c.startsWith('cache-bar__run--')))).toEqual([
      'cache-bar__run--disk',
      'cache-bar__run--ram',
      'cache-bar__run--gap',
    ])
    expect(runs.map((r) => (r.element as HTMLElement).style.flexGrow)).toEqual(['2', '2', '1'])
  })

  it('puts a viewer half-way back halfway along, on the bar', () => {
    const w = mount(CacheBar, { props: { entry } })
    expect(pos(w.find('.cache-bar__marker--viewer'))).toBe('0.5000')
    /* a recording is drawn as the part of the bar it is made of, not as a marker */
    expect(w.find('.cache-bar__marker--recording').exists()).toBe(false)
  })

  it('draws a paused viewer as "pause", a playing one as "play"', () => {
    const w = mount(CacheBar, {
      props: { entry: { ...entry, viewers: [50, 10], paused: [true, false] } },
    })
    const m = w.findAll('.cache-bar__marker')
    expect(m.map((n) => n.classes()[1])).toEqual([
      'cache-bar__marker--paused',
      'cache-bar__marker--viewer',
    ])
    expect(m[0].attributes('title')).toBe('Viewer 1: (paused) −50 s')
    /* who it is, when the server says, on a line of its own */
    const named = mount(CacheBar, {
      props: { entry: { ...entry, clients: ['Kodi Media Center (192.168.1.20)'] } },
    })
    expect(named.find('.cache-bar__marker--viewer').attributes('title')).toBe(
      'Viewer 1: −50 s\nKodi Media Center (192.168.1.20)'
    )
  })

  it('leads from a marker to Status > Subscriptions, and can leave markers out', async () => {
    const push = vi.fn()
    const w = mount(CacheBar, {
      props: { entry },
      /* only push is used: a partial router, cast */
      global: { config: { globalProperties: { $router: { push } } as never } },
    })
    await w.find('.cache-bar__marker--viewer').trigger('click')
    expect(push).toHaveBeenCalledWith({ name: 'status-subscriptions' })
    /* a real link: the keyboard follows it as any other */
    expect(w.find('.cache-bar__marker--viewer').element.tagName).toBe('A')
    const bare = mount(CacheBar, { props: { entry, markers: false } })
    expect(bare.find('.cache-bar__marker').exists()).toBe(false)
  })

  it('colours what a recording is made of in greens, by where it lives', () => {
    /* 5 slots of 20 s over 1000..1099; the programme began at 1030 */
    const w = mount(CacheBar, {
      props: { entry: { ...entry, oldest: 1000, span: 99, recorded: [[1030, 9999]] } },
    })
    const runs = w.findAll('.cache-bar__run')
    expect(runs.map((r) => r.classes().filter((c) => c !== 'cache-bar__run'))).toEqual([
      ['cache-bar__run--disk'],
      ['cache-bar__run--disk', 'cache-bar__run--rec'],
      ['cache-bar__run--ram', 'cache-bar__run--rec'],
      ['cache-bar__run--gap'],
    ])
    expect(runs[1].attributes('title')).toBe('Recording, disk: 20 s')
  })

  it('fills its share of the period against the right edge, the past it lacks left empty', () => {
    const w = mount(CacheBar, { props: { entry, period: 400 } })
    expect((w.find('.cache-bar__fill').element as HTMLElement).style.width).toBe('25.00%')
    expect(w.find('.cache-bar__rest').exists()).toBe(true)
    /* half-way back in a cache filling the rightmost quarter of the bar */
    expect(pos(w.find('.cache-bar__marker--viewer'))).toBe('0.8750')
  })

  it('is 24 px high, 8 compact, or the height asked for, its markers drawn to it', () => {
    const h = (props: Record<string, unknown>) =>
      (
        mount(CacheBar, { props: { entry, ...props } }).element as HTMLElement
      ).style.getPropertyValue('--cache-bar-h')
    expect(h({})).toBe('24px')
    expect(h({ compact: true })).toBe('8px')
    expect(h({ height: 16 })).toBe('16px')
  })

  it('is the whole bar once the cache has reached its period', () => {
    const w = mount(CacheBar, { props: { entry, period: 60 } })
    expect((w.find('.cache-bar__fill').element as HTMLElement).style.width).toBe('100.00%')
    expect(w.find('.cache-bar__rest').exists()).toBe(false)
  })

  it('picks out highlighted positions, and says what it shows', () => {
    const w = mount(CacheBar, { props: { entry, highlights: [25] } })
    expect(pos(w.find('.cache-bar__marker--self'))).toBe('0.7500')
    expect(w.find('.cache-bar__summary').text()).toBe(
      'Channel cache of 1 min, 1 viewer(s), 1 recording(s)'
    )
    expect(w.find('.cache-bar__track').attributes('aria-hidden')).toBe('true')
  })
})

describe('CacheBudget', () => {
  const GiB = 1024 ** 3
  const budget: Budget = {
    enabled: true,
    ram_only: false,
    unlimited: false,
    size_used: GiB,
    size_max: 2 * GiB,
    ram_used: 0,
    ram_max: GiB / 2,
    staging_used: 0,
    staging_max: 32 * 1024 ** 2,
    keepalive: 15,
    period: 3600,
  }

  it('shows the size, RAM and write-behind budgets as used of max', () => {
    const w = mount(CacheBudget, { props: { budget } })
    /* side by side: the whole timeshift, the RAM, the buffer */
    expect(w.findAll('.cache-budget__label > span:first-child').map((n) => n.text())).toEqual([
      'Timeshift',
      'RAM',
      'Buffering',
    ])
    expect(w.findAll('.cache-budget__value').map((n) => n.text())).toEqual([
      '1.0 GiB of 2.0 GiB',
      '0 B of 512.0 MiB',
      '0 B of 32.0 MiB',
    ])
    expect(w.findAll('meter')[0].attributes('value')).toBe('50')
  })

  it('colours each meter like the cache bars, and splits the total by where it lives', () => {
    const MiB = 1024 ** 2
    const w = mount(CacheBudget, {
      props: {
        budget: {
          ...budget,
          ram_used: 256 * MiB,
          staging_used: 512 * 1024,
          rec_ram: 64 * MiB,
          rec_disk: 128 * MiB,
        },
      },
    })
    const [size, ram, staging] = w.findAll('.cache-budget__track')
    /* in the legend's order: cache, recording, buffer */
    expect(size.findAll('.cache-budget__fill').map((n) => n.classes()[1])).toEqual([
      'cache-budget__fill--ram',
      'cache-budget__fill--disk',
      'cache-budget__fill--rec-ram',
      'cache-budget__fill--rec-disk',
      'cache-budget__fill--staging',
    ])
    /* of 2 GiB: 256 MiB in RAM (64 of them recorded), half a MiB staged,
     * the rest of the GiB on disk (128 MiB of it recorded) */
    expect(
      size.findAll('.cache-budget__fill').map((n) => (n.element as HTMLElement).style.width)
    ).toEqual(['9.4%', '31.2%', '3.1%', '6.3%', '0.0%'])
    expect(ram.findAll('.cache-budget__fill').map((n) => n.classes()[1])).toEqual([
      'cache-budget__fill--ram',
      'cache-budget__fill--rec-ram',
    ])
    expect(staging.find('.cache-budget__fill--staging').exists()).toBe(true)
    /* a full RAM budget is its normal state: it keeps the RAM colour */
    const full = mount(CacheBudget, { props: { budget: { ...budget, ram_used: GiB / 2 } } })
    expect(full.findAll('.cache-budget__track')[1].find('.cache-budget__fill--ram').exists()).toBe(
      true
    )
    expect(full.find('.cache-budget__fill--high').exists()).toBe(false)
  })

  it('keeps only the RAM meter under "RAM only", and reads an unlimited size as such', () => {
    const ramOnly = mount(CacheBudget, { props: { budget: { ...budget, ram_only: true } } })
    expect(ramOnly.findAll('.cache-budget__meter')).toHaveLength(1)
    const unlimited = mount(CacheBudget, { props: { budget: { ...budget, unlimited: true } } })
    expect(unlimited.findAll('.cache-budget__value')[0].text()).toBe('1.0 GiB (unlimited)')
  })
})
