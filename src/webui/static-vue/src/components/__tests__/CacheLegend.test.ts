// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheLegend from '../CacheLegend.vue'
import { useCacheView } from '@/composables/useCacheView'
import { CACHE_PALETTE } from '@/utils/cacheMap'
import type { CacheEntry } from '@/types/cache'

const { view } = useCacheView()

function cache(uuid: string, channel: string): CacheEntry {
  return {
    uuid,
    channel,
    size: 0,
    ram: 0,
    staging: 0,
    disk: 0,
    segments: 0,
    oldest: 0,
    newest: 0,
    span: 0,
    gap: false,
    write_error: false,
    ram_only: false,
    dropped: 0,
    viewers: [],
    recordings: [],
    readers: 0,
    gates: 0,
    map: [],
    subscriptions: 0,
  }
}

const titles = (w: ReturnType<typeof mount>) =>
  w.findAll('.cache-legend__title').map((n) => n.text())

const swatches = (w: ReturnType<typeof mount>) =>
  w.findAll('.cache-legend__swatch').map((s) => s.classes().find((c) => c.includes('--'))!)

describe('CacheLegend', () => {
  afterEach(() => {
    view.value = 'use'
  })

  it('names RAM and disk when both are in use', () => {
    const w = mount(CacheLegend)
    expect(swatches(w)).toEqual([
      'cache-legend__swatch--ram',
      'cache-legend__swatch--disk',
      'cache-legend__swatch--rec-ram',
      'cache-legend__swatch--rec-disk',
      'cache-legend__swatch--staging',
      'cache-legend__swatch--gap',
    ])
  })

  it('under RAM only, leaves out the disk and its buffer', () => {
    const w = mount(CacheLegend, { props: { mode: 'ram-only' } })
    expect(swatches(w)).toEqual([
      'cache-legend__swatch--ram',
      'cache-legend__swatch--rec-ram',
      'cache-legend__swatch--gap',
    ])
    expect(w.text()).not.toContain('Disk')
    expect(w.text()).not.toContain('Buffering')
  })

  it('with no RAM, leaves out the RAM', () => {
    const w = mount(CacheLegend, { props: { mode: 'disk-only' } })
    expect(swatches(w)).toEqual([
      'cache-legend__swatch--disk',
      'cache-legend__swatch--rec-disk',
      'cache-legend__swatch--staging',
      'cache-legend__swatch--gap',
    ])
    expect(w.text()).not.toContain('RAM')
  })

  it('by use, says what the colours mean, with no names for the two shades', () => {
    const w = mount(CacheLegend)
    expect(titles(w)).toEqual(['Timeshift', 'Recording', 'Buffering', 'Gap'])
    /* the shades are told apart by the meters' names; pointing says which */
    expect(w.find('.cache-legend__swatch--ram').attributes('title')).toBe('RAM')
    expect(w.find('.cache-legend__swatch--disk').attributes('title')).toBe('Disk')
  })

  it('switches to the caches, each named beside its colour, and remembers the choice', async () => {
    const w = mount(CacheLegend, {
      props: { caches: [cache('b', 'Noovo'), cache('a', 'ICI Radio-Canada')] },
    })
    const [byUse, byCache] = w.findAll('.cache-legend__view')
    expect(byUse.attributes('aria-pressed')).toBe('true')
    await byCache.trigger('click')
    expect(byCache.attributes('aria-pressed')).toBe('true')
    expect(globalThis.window.localStorage.getItem('tvh-cache-view')).toBe('cache')
    /* in the order of their names, in the palette's colours, then the key in grey */
    expect(titles(w)).toEqual(['ICI Radio-Canada', 'Noovo', 'Timeshift', 'Recording'])
    /* each cache plain, then hatched as a recording */
    const plain = w
      .findAll('.cache-legend__swatch')
      .filter((n) => n.attributes('title') === 'Timeshift')
    const styles = plain.map((n) => n.attributes('style') ?? '')
    const rgb = (hex: string) =>
      `rgb(${[1, 3, 5].map((i) => Number.parseInt(hex.slice(i, i + 2), 16)).join(', ')})`
    const isPalette = (style: string, i: number) =>
      style.includes(CACHE_PALETTE[i]) || style.includes(rgb(CACHE_PALETTE[i]))
    expect(isPalette(styles[0], 0)).toBe(true)
    expect(isPalette(styles[1], 1)).toBe(true)
    /* the two caches hatched, and the grey key: one plain, one hatched */
    expect(w.findAll('.cache-legend__swatch[title="Recording"]').length).toBe(3)
    expect(w.findAll('.cache-legend__swatch--tone').length).toBe(1)
    expect(w.findAll('.cache-legend__swatch--tone-rec').length).toBe(1)
    await byUse.trigger('click')
    expect(titles(w)).toEqual(['Timeshift', 'Recording', 'Buffering', 'Gap'])
  })
})
