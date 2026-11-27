// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbMuxesView — the Rescan toolbar action. It sets Scan status to
 * PEND on the selected muxes with one idnode/save. The server only
 * queues a mux that is set to Enable on an enabled network and
 * reports success for the others too, so the view leaves those out
 * and names them in a warning.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import DvbMuxesView from '../DvbMuxesView.vue'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGridWithToolbar)
vi.mock('@/components/ActionMenu.vue', () => stubs.actionMenu)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/IdnodePickEntityDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)

const toast = vi.hoisted(() => ({ success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }))
vi.mock('@/composables/useToastNotify', () => ({ useToastNotify: () => toast }))

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({ apiCall: (...args: unknown[]) => apiMock(...args) }))

stubs.resetEachTest()

const { captured } = stubs

function mountWith(rows: Array<Record<string, unknown>>) {
  apiMock.mockReset()
  apiMock.mockResolvedValue({})
  for (const fn of Object.values(toast)) fn.mockReset()
  captured.selection = rows
  return mount(DvbMuxesView)
}

function saveCalls() {
  return apiMock.mock.calls.filter(([endpoint]) => endpoint === 'idnode/save')
}

const FAILED_A = { uuid: 'a', name: '658MHz', enabled: 1, network_uuid: 'n1' }
const FAILED_B = { uuid: 'b', name: '666MHz', enabled: 1, network_uuid: 'n1' }

describe('DvbMuxesView Rescan', () => {
  it('is disabled with nothing selected and sits before Delete', () => {
    mountWith([])
    expect(stubs.toolbarAction('rescan').disabled).toBe(true)
    const ids = captured.actions.map((a) => a.id)
    expect(ids.indexOf('rescan')).toBe(ids.indexOf('edit') + 1)
    expect(ids.indexOf('rescan')).toBeLessThan(ids.indexOf('delete'))
  })

  it('sets Scan status to PEND on the selected muxes with one request', async () => {
    mountWith([FAILED_A, FAILED_B])
    const rescan = stubs.toolbarAction('rescan')
    expect(rescan.disabled).toBe(false)
    await rescan.onClick?.()

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(saveCalls()[0][1]).toEqual({ node: JSON.stringify({ uuid: ['a', 'b'], scan_state: 1 }) })
    expect(toast.success).toHaveBeenCalledWith('Scan queued for 2 muxes.')
    expect(toast.warn).not.toHaveBeenCalled()
  })

  it('leaves out and names the muxes that are not set to Enable', async () => {
    mountWith([
      FAILED_A,
      { uuid: 'c', name: '674MHz', enabled: 0, network_uuid: 'n1' },
      { uuid: 'd', name: '682MHz', enabled: -1, network_uuid: 'n1' },
    ])
    await stubs.toolbarAction('rescan').onClick?.()

    expect(saveCalls()).toHaveLength(1)
    expect(JSON.parse(saveCalls()[0][1].node).uuid).toEqual(['a'])
    expect(toast.warn.mock.calls[0][0]).toContain('Not scanned: 674MHz, 682MHz.')
    expect(toast.success).toHaveBeenCalledWith('Scan queued for 1 mux.')
  })

  it('sends nothing when no selected mux can be scanned', async () => {
    mountWith([{ uuid: 'c', name: '674MHz', enabled: 0, network_uuid: 'n1' }])
    await stubs.toolbarAction('rescan').onClick?.()

    expect(apiMock).not.toHaveBeenCalled()
    expect(toast.warn).toHaveBeenCalledOnce()
    expect(toast.success).not.toHaveBeenCalled()
  })

  it('with Hide: None also leaves out the muxes of a disabled network', async () => {
    mountWith([FAILED_A, { uuid: 'e', name: '690MHz', enabled: 1, network_uuid: 'off' }])
    apiMock.mockImplementation((endpoint: string) =>
      Promise.resolve(endpoint === 'mpegts/network/grid' ? { entries: [{ uuid: 'off' }] } : {}),
    )
    const onFilterChange = captured.gridProps?.onFilterChange as (k: string, v: string) => void
    onFilterChange('hidemode', 'none')
    await stubs.toolbarAction('rescan').onClick?.()

    const [endpoint, params] = apiMock.mock.calls[0]
    expect(endpoint).toBe('mpegts/network/grid')
    expect(JSON.parse(params.filter)).toEqual([{ field: 'enabled', type: 'boolean', value: false }])
    expect(JSON.parse(saveCalls()[0][1].node).uuid).toEqual(['a'])
    expect(toast.warn.mock.calls[0][0]).toContain('Not scanned: 690MHz.')
  })

  it('does not ask for the networks while the grid hides disabled networks', async () => {
    mountWith([FAILED_A])
    await stubs.toolbarAction('rescan').onClick?.()

    expect(apiMock.mock.calls.map(([endpoint]) => endpoint)).toEqual(['idnode/save'])
  })
})
