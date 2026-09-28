// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ChannelsView — the Map services submenu. The grid stub renders the
 * toolbar slot, the ActionMenu stub captures the actions and the
 * ServiceMapperDialog stub the props it is opened with.
 */
import { describe, expect, it, vi } from 'vitest'
import { nextTick } from 'vue'
import { mount } from '@vue/test-utils'
import ChannelsView from '../ChannelsView.vue'
import type { ActionDef } from '@/types/action'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/ServiceMapperDialog.vue', () => stubs.serviceMapperDialog)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/views/configuration/ChannelManageDrawer.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)
vi.mock('@/api/client', () => ({
  apiCall: vi.fn(() => Promise.resolve({ entries: [] })),
}))

stubs.resetEachTest()

const { captured } = stubs

function mapChild(id: string): ActionDef {
  const child = stubs.toolbarAction('map-services').children?.find((c) => c.id === id)
  if (!child) throw new Error(`no ${id} action captured`)
  return child
}

describe('ChannelsView Map services menu', () => {
  it('offers Map all services, which preselects every service', async () => {
    mount(ChannelsView)
    const all = mapChild('map-services-all')
    expect(all.label).toBe('Map all services')
    await all.onClick?.()
    await nextTick()
    expect(captured.mapper.visible).toBe(true)
    expect(captured.mapper.mapAll).toBe(true)
  })

  it('still opens the empty picker from Map services…', async () => {
    mount(ChannelsView)
    await mapChild('map-services-open').onClick?.()
    await nextTick()
    expect(captured.mapper.visible).toBe(true)
    expect(captured.mapper.mapAll).toBe(false)
  })
}, 30_000)
