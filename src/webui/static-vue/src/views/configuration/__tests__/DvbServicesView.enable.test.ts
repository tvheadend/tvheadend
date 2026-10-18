// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — the Enable/Disable submenu. Each item sets
 * `enabled` on the selected services with one idnode/save and
 * confirms with a toast. Both are undone with the other one, so
 * neither asks first.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import DvbServicesView from '../DvbServicesView.vue'
import type { ActionDef } from '@/types/action'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/ServiceMapperDialog.vue', () => stubs.serviceMapperDialog)
vi.mock('@/components/ServiceStreamsDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)

const ask = vi.hoisted(() => vi.fn())
vi.mock('@/composables/useConfirmDialog', () => ({ useConfirmDialog: () => ({ ask }) }))

const toast = vi.hoisted(() => ({ success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }))
vi.mock('@/composables/useToastNotify', () => ({ useToastNotify: () => toast }))

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({ apiCall: (...args: unknown[]) => apiMock(...args) }))

stubs.resetEachTest()

const { captured } = stubs

function mountSelected(uuids: string[]) {
  apiMock.mockReset()
  apiMock.mockResolvedValue({})
  ask.mockReset()
  for (const fn of Object.values(toast)) fn.mockReset()
  captured.selection = uuids.map((uuid) => ({ uuid }))
  return mount(DvbServicesView)
}

function item(id: string): ActionDef {
  const found = stubs.toolbarAction('enabled').children?.find((a) => a.id === id)
  if (!found) throw new Error(`no ${id} item`)
  return found
}

function savedNode() {
  expect(apiMock).toHaveBeenCalledTimes(1)
  const [endpoint, params] = apiMock.mock.calls[0]
  expect(endpoint).toBe('idnode/save')
  return JSON.parse(params.node)
}

describe('DvbServicesView Enable/Disable', () => {
  it('groups Enable and Disable as the last action, disabled with nothing selected', () => {
    mountSelected([])
    expect(captured.actions[captured.actions.length - 1]?.id).toBe('enabled')
    const parent = stubs.toolbarAction('enabled')
    expect(parent.children?.map((a) => a.id)).toEqual(['enable', 'disable'])
    expect(parent.children?.every((a) => a.disabled)).toBe(true)
  })

  it('enables the selected services with one request', async () => {
    mountSelected(['s1', 's2'])
    await item('enable').onClick?.()

    expect(savedNode()).toEqual({ uuid: ['s1', 's2'], enabled: true })
    expect(toast.success).toHaveBeenCalledWith('Enabled 2 services.')
    expect(ask).not.toHaveBeenCalled()
  })

  it('disables the selected services with one request', async () => {
    mountSelected(['s1'])
    await item('disable').onClick?.()

    expect(savedNode()).toEqual({ uuid: ['s1'], enabled: false })
    expect(toast.success).toHaveBeenCalledWith('Disabled 1 service.')
    expect(ask).not.toHaveBeenCalled()
  })
})
