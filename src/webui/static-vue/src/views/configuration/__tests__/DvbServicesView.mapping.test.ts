// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — the Mapping filter next to Hide in View options.
 * The grid sends its value as the `mapping` param, which the server's
 * service grid uses to list only the unmapped or the mapped services.
 */
import { describe, expect, it, vi } from 'vitest'
import { nextTick } from 'vue'
import { mount } from '@vue/test-utils'
import DvbServicesView from '../DvbServicesView.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceMapperDialog.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceStreamsDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

const { captured } = stubs

describe('DvbServicesView Mapping filter', () => {
  it('follows Hide and lists every service by default', () => {
    mount(DvbServicesView)
    const keys = (captured.gridProps?.filters as Array<{ key: string }>).map((f) => f.key)
    expect(keys).toEqual(['hidemode', 'mapping'])
    const mapping = stubs.gridFilter('mapping')
    expect(mapping?.kind).toBe('select')
    if (mapping?.kind !== 'select') return
    expect(mapping.current).toBe('all')
    expect(mapping.options.map((o) => o.value)).toEqual(['all', 'unmapped', 'mapped'])
  })

  it('takes the picked value back from the grid', async () => {
    mount(DvbServicesView)
    const onFilterChange = captured.gridProps?.onFilterChange as (k: string, v: string) => void
    onFilterChange('mapping', 'unmapped')
    await nextTick()
    expect(stubs.gridFilter('mapping')?.current).toBe('unmapped')
    expect(stubs.gridFilter('hidemode')?.current).toBe('default')
  })
})
