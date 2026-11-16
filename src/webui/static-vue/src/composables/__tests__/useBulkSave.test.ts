// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useBulkSave — one `idnode/save` in the Foreach shape for the whole
 * selection, a success toast with the row count, and the same
 * failure toast and in-flight flag as useBulkAction.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { useBulkSave } from '../useBulkSave'

const apiCallMock = vi.fn()

vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

const toast = vi.hoisted(() => ({ success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }))

vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => toast,
}))

beforeEach(() => {
  apiCallMock.mockReset()
  apiCallMock.mockResolvedValue({})
  for (const fn of Object.values(toast)) fn.mockReset()
})

function disable() {
  return useBulkSave({
    fields: { enabled: false },
    failPrefix: 'Failed to disable',
    successText: (n) => `Disabled ${n}.`,
  })
}

describe('useBulkSave', () => {
  it('saves the fields on every selected row with one request', async () => {
    const clear = vi.fn()
    const action = disable()

    await action.run([{ uuid: 'a' }, {}, { uuid: '' }, { uuid: 'b' }], clear)

    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('idnode/save', {
      node: JSON.stringify({ uuid: ['a', 'b'], enabled: false }),
    })
    expect(clear).toHaveBeenCalledOnce()
    expect(toast.success).toHaveBeenCalledWith('Disabled 2.')
    expect(action.inflight.value).toBe(false)
  })

  it('does nothing without a usable uuid', async () => {
    const clear = vi.fn()

    await disable().run([{}, { uuid: '' }], clear)

    expect(apiCallMock).not.toHaveBeenCalled()
    expect(clear).not.toHaveBeenCalled()
    expect(toast.success).not.toHaveBeenCalled()
  })

  it('is in flight until the request settles', async () => {
    let resolveApi!: (value: unknown) => void
    apiCallMock.mockReturnValueOnce(new Promise((res) => (resolveApi = res)))
    const action = disable()

    const running = action.run([{ uuid: 'a' }], () => {})
    await Promise.resolve()
    expect(action.inflight.value).toBe(true)

    resolveApi({})
    await running
    expect(action.inflight.value).toBe(false)
  })

  it('shows the failure, keeps the selection and skips the success toast', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('Bad Request'))
    const clear = vi.fn()
    const action = disable()

    await action.run([{ uuid: 'a' }], clear)

    expect(toast.error).toHaveBeenCalledWith('Failed to disable: Bad Request')
    expect(toast.success).not.toHaveBeenCalled()
    expect(clear).not.toHaveBeenCalled()
    expect(action.inflight.value).toBe(false)
  })
})
