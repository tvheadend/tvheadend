// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — the column set and toolbar wiring the view hands
 * to IdnodeGrid. The grid itself (fetching, rendering) is out of
 * scope, so it is stubbed and its props are captured.
 */
import { describe, expect, it, vi } from 'vitest'
import { nextTick } from 'vue'
import { mount } from '@vue/test-utils'
import DvbServicesView from '../DvbServicesView.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'

/* The grid stub renders the toolbar slot with `captured.selection`,
 * ActionMenu captures the actions built for it and ServiceMapperDialog
 * the props it was opened with. */
const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceMapperDialog.vue', () => stubs.serviceMapperDialog)
vi.mock('@/components/ServiceStreamsDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

const { captured } = stubs

describe('DvbServicesView columns', () => {
  it('shows EIT processing, not the removed dvb_ignore_eit field', () => {
    mount(DvbServicesView)
    const fields = stubs.gridColumns().map((c) => c.field)
    expect(fields).not.toContain('dvb_ignore_eit')
    const col = stubs.gridColumn('dvb_eit_processing')
    expect(col).toBeDefined()
    expect(col?.cellComponent).toBe(EnumNameCell)
    expect(col?.filterType).toBe('enum')
    /* Keys of mpegts_service_eit_processing_list (mpegts_service.c). */
    const keys = (col?.enumSource as Array<{ key: number }>).map((o) => o.key)
    expect(keys).toEqual([0, 1, 2, 3, 4, 5])
  })
})

describe('DvbServicesView Map services action', () => {
  it('maps all services when nothing is selected', async () => {
    mount(DvbServicesView)
    const map = stubs.toolbarAction('map-services')
    expect(map.label).toBe('Map all services')
    await map.onClick?.()
    await nextTick()
    expect(captured.mapper.visible).toBe(true)
    expect(captured.mapper.mapAll).toBe(true)
  })

  it('preselects the selected services', async () => {
    captured.selection = [{ uuid: 's1' }, { uuid: 's2' }]
    mount(DvbServicesView)
    const map = stubs.toolbarAction('map-services')
    expect(map.label).toBe('Map 2 services')
    await map.onClick?.()
    await nextTick()
    expect(captured.mapper.visible).toBe(true)
    expect(captured.mapper.mapAll).toBe(false)
    expect(captured.mapper.preselect).toEqual({ services: ['s1', 's2'] })
  })
})
