// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ConnectionsView — the Drop action. StatusGrid is stubbed: the
 * tests render its toolbarActions slot with a fixed selection and
 * read the actions handed to the stubbed ActionMenu.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import ConnectionsView from '../ConnectionsView.vue'
import ActionMenu from '@/components/ActionMenu.vue'
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
  vi.unstubAllGlobals()
  vi.restoreAllMocks()
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
