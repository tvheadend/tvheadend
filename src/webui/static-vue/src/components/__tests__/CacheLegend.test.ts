// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import CacheLegend from '../CacheLegend.vue'

const swatches = (w: ReturnType<typeof mount>) =>
  w.findAll('.cache-legend__swatch').map((s) => s.classes().find((c) => c.includes('--'))!)

describe('CacheLegend', () => {
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
})
