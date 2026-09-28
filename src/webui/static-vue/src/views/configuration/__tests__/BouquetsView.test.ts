// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * BouquetsView — the mapping-option columns. The grid endpoint
 * returns `mapopt` / `chtag` as arrays of internal option ids; the
 * labels come from the `bouquet` class metadata.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import BouquetsView from '../BouquetsView.vue'
import { useIdnodeClassStore } from '@/stores/idnodeClass'
import type { BaseRow } from '@/types/grid'
import type { IdnodeClassMeta } from '@/types/idnode'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

const column = stubs.gridColumn

/* Trimmed from a live `api/idnode/class?name=bouquet` response. */
const BOUQUET_META: IdnodeClassMeta = {
  class: 'bouquet',
  props: [
    {
      id: 'mapopt',
      type: 'int',
      caption: 'Channel mapping options',
      list: true,
      enum: [
        { key: 'mapnolcn', val: 'Map zero-numbered channels' },
        { key: 'mapradio', val: 'Map radio channels' },
        { key: 'encrypted', val: 'Map encrypted services' },
      ],
    },
    {
      id: 'chtag',
      type: 'int',
      caption: 'Create tags',
      list: true,
      enum: [
        { key: 'bouquet_tag', val: 'Create bouquet tag' },
        { key: 'type_tags', val: 'Create type-based tags' },
      ],
    },
  ],
} as IdnodeClassMeta

const ROW = { uuid: 'b1' } as BaseRow

describe('BouquetsView mapping option columns', () => {
  it('shows the option labels instead of the internal ids', () => {
    useIdnodeClassStore().cache.set('bouquet', BOUQUET_META)
    mount(BouquetsView)
    expect(column('mapopt')?.format?.(['mapradio', 'encrypted'], ROW)).toBe(
      'Map radio channels, Map encrypted services',
    )
    expect(column('chtag')?.format?.(['bouquet_tag'], ROW)).toBe('Create bouquet tag')
  })

  it('picks the labels up once the class metadata arrives', () => {
    mount(BouquetsView)
    const fmt = column('mapopt')?.format
    expect(fmt?.(['mapradio'], ROW)).toBe('mapradio')
    useIdnodeClassStore().cache.set('bouquet', BOUQUET_META)
    expect(fmt?.(['mapradio'], ROW)).toBe('Map radio channels')
  })

  it('renders an empty option list as an empty cell', () => {
    mount(BouquetsView)
    expect(column('mapopt')?.format?.([], ROW)).toBe('')
  })
})
