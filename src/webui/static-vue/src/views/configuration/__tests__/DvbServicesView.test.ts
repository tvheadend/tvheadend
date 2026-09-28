// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — the column set and toolbar wiring the view hands
 * to IdnodeGrid. The grid itself (fetching, rendering) is out of
 * scope, so it is stubbed and its props are captured.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import DvbServicesView from '../DvbServicesView.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceMapperDialog.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceStreamsDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

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
