// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbMuxesView — the Enable/Disable submenu. Each item sets the
 * tri-state `enabled` (Ignore -1, Disable 0, Enable 1) on the
 * selected muxes with one idnode/save. Ignore removes the services
 * of the mux, so it asks first.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import DvbMuxesView from '../DvbMuxesView.vue'
import type { ActionDef } from '@/types/action'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/IdnodePickEntityDialog.vue', () => stubs.emptyComponent)
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
  captured.selection = uuids.map((uuid) => ({ uuid, enabled: 1 }))
  return mount(DvbMuxesView)
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

describe('DvbMuxesView Enable/Disable', () => {
  it('groups Enable, Disable and Ignore as the last action, disabled with nothing selected', () => {
    mountSelected([])
    expect(captured.actions[captured.actions.length - 1]?.id).toBe('enabled')
    const parent = stubs.toolbarAction('enabled')
    expect(parent.children?.map((a) => a.id)).toEqual(['enable', 'disable', 'ignore'])
    expect(parent.children?.every((a) => a.disabled)).toBe(true)
  })

  it('enables the selected muxes without asking', async () => {
    mountSelected(['a', 'b'])
    await item('enable').onClick?.()

    expect(ask).not.toHaveBeenCalled()
    expect(savedNode()).toEqual({ uuid: ['a', 'b'], enabled: 1 })
    expect(toast.success).toHaveBeenCalledWith('Enabled 2 muxes.')
  })

  it('disables the selected muxes without asking', async () => {
    mountSelected(['a'])
    await item('disable').onClick?.()

    expect(ask).not.toHaveBeenCalled()
    expect(savedNode()).toEqual({ uuid: ['a'], enabled: 0 })
    expect(toast.success).toHaveBeenCalledWith('Disabled 1 mux.')
  })

  it('asks before Ignore and says that it removes the services and their mappings', async () => {
    mountSelected(['a', 'b'])
    ask.mockResolvedValue(true)
    await item('ignore').onClick?.()

    const [text, opts] = ask.mock.calls[0]
    expect(text).toContain('removes their services')
    expect(text).toContain('channel mappings')
    expect(opts).toEqual({ severity: 'danger' })
    expect(savedNode()).toEqual({ uuid: ['a', 'b'], enabled: -1 })
    expect(toast.success).toHaveBeenCalledWith('Ignored 2 muxes.')
  })

  it('leaves the muxes alone when Ignore is declined', async () => {
    mountSelected(['a'])
    ask.mockResolvedValue(false)
    await item('ignore').onClick?.()

    expect(apiMock).not.toHaveBeenCalled()
    expect(toast.success).not.toHaveBeenCalled()
  })
})
