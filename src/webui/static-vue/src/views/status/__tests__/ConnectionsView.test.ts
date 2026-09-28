// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ConnectionsView — column wiring and the Drop action. StatusGrid is
 * stubbed: the column tests read the `columns` prop handed to it,
 * and the Drop tests render its toolbarActions slot with a fixed
 * selection and read the actions handed to the stubbed ActionMenu.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import ConnectionsView from '../ConnectionsView.vue'
import StatusGrid from '@/components/StatusGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'

const askMock = vi.fn()
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: askMock }),
}))

const toastError = vi.fn()
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ error: toastError }),
}))

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

function gridColumns(): ColumnDef[] {
  const wrapper = mount(ConnectionsView, {
    global: { stubs: { StatusGrid: true } },
  })
  return wrapper.findComponent(StatusGrid).props('columns') as ColumnDef[]
}

const clearSelection = vi.fn()
const StatusGridStub = defineComponent({
  name: 'StatusGrid',
  setup(_, { slots }) {
    return () => h('div', slots.toolbarActions?.({ selection: [{ id: 7 }], clearSelection }))
  },
})

function dropAction(): ActionDef {
  const wrapper = mount(ConnectionsView, {
    global: { stubs: { StatusGrid: StatusGridStub, ActionMenu: true } },
  })
  const actions = wrapper.findComponent(ActionMenu).props('actions') as ActionDef[]
  return actions.find((a) => a.id === 'drop')!
}

beforeEach(() => {
  askMock.mockReset()
  toastError.mockReset()
  apiMock.mockReset()
  clearSelection.mockReset()
})

afterEach(() => {
  delete (globalThis as { tvh_locale?: unknown }).tvh_locale
  vi.unstubAllGlobals()
  vi.restoreAllMocks()
})

describe('ConnectionsView — columns', () => {
  it('lists the client data ports per protocol', () => {
    const ports = gridColumns().find((c) => c.field === 'peer_extra_ports')
    expect(ports?.format?.({ tcp: [5000, 5001], udp: [6000] }, {})).toBe(
      'TCP: 5000, 5001; UDP: 6000',
    )
  })

  it('reuses the Classic TCP and UDP msgids in the client data ports', () => {
    ;(globalThis as { tvh_locale?: unknown }).tvh_locale = { TCP: 'tcp-x', UDP: 'udp-x' }
    const ports = gridColumns().find((c) => c.field === 'peer_extra_ports')
    expect(ports?.format?.({ tcp: [5000], udp: [6000] }, {})).toBe('tcp-x: 5000; udp-x: 6000')
  })
})

describe('ConnectionsView — Drop', () => {
  it('asks with the in-app dialog, not the browser confirm', async () => {
    const nativeConfirm = vi.fn(() => true)
    vi.stubGlobal('confirm', nativeConfirm)
    askMock.mockResolvedValue(true)
    apiMock.mockResolvedValue({})
    await dropAction().onClick?.()
    await flushPromises()
    expect(nativeConfirm).not.toHaveBeenCalled()
    expect(askMock).toHaveBeenCalledWith(
      'Drop the selected connection(s)?',
      expect.objectContaining({ header: 'Drop Connections', severity: 'danger' }),
    )
    expect(apiMock).toHaveBeenCalledWith('connections/cancel', { id: '[7]' })
    expect(clearSelection).toHaveBeenCalled()
  })

  it('does nothing when the dialog is dismissed', async () => {
    askMock.mockResolvedValue(false)
    await dropAction().onClick?.()
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('reports a failed drop in a toast, not the browser alert', async () => {
    const nativeAlert = vi.fn()
    vi.stubGlobal('alert', nativeAlert)
    askMock.mockResolvedValue(true)
    apiMock.mockRejectedValue(new Error('API 500'))
    await dropAction().onClick?.()
    await flushPromises()
    expect(nativeAlert).not.toHaveBeenCalled()
    expect(toastError).toHaveBeenCalledWith('Failed to drop connection(s): API 500')
  })
})
