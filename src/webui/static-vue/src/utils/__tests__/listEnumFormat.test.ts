// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { defineComponent, h, nextTick } from 'vue'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import { listEnumFormat } from '../listEnumFormat'
import { useIdnodeClassStore } from '@/stores/idnodeClass'
import type { IdnodeClassMeta } from '@/types/idnode'

const META: IdnodeClassMeta = {
  class: 'bouquet',
  props: [
    {
      id: 'mapopt',
      type: 'int',
      caption: 'Channel mapping options',
      list: true,
      enum: [
        { key: 'mapradio', val: 'Map radio channels' },
        { key: 'encrypted', val: 'Map encrypted services' },
      ],
    },
  ],
} as IdnodeClassMeta

describe('listEnumFormat', () => {
  it('joins the labels of the listed ids', () => {
    const fmt = listEnumFormat(() => META, 'mapopt')
    expect(fmt(['encrypted', 'mapradio'])).toBe('Map encrypted services, Map radio channels')
  })

  it('keeps an id the metadata does not know', () => {
    const fmt = listEnumFormat(() => META, 'mapopt')
    expect(fmt(['mapradio', 'future_opt'])).toBe('Map radio channels, future_opt')
  })

  it('shows the raw ids while the metadata is missing', () => {
    const fmt = listEnumFormat(() => undefined, 'mapopt')
    expect(fmt(['mapradio'])).toBe('mapradio')
  })

  it('renders empty and non-list values as an empty string', () => {
    const fmt = listEnumFormat(() => META, 'mapopt')
    expect(fmt([])).toBe('')
    expect(fmt(null)).toBe('')
    expect(fmt('mapradio')).toBe('')
  })

  it('re-renders a cell when the class metadata reaches the store', async () => {
    setActivePinia(createPinia())
    const store = useIdnodeClassStore()
    const fmt = listEnumFormat(() => store.get('bouquet'), 'mapopt')
    const Cell = defineComponent({ render: () => h('span', fmt(['mapradio'])) })
    const wrapper = mount(Cell)
    expect(wrapper.text()).toBe('mapradio')
    store.cache.set('bouquet', META)
    await nextTick()
    expect(wrapper.text()).toBe('Map radio channels')
  })
})
