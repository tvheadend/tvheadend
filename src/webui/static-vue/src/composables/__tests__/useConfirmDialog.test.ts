// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * useConfirmDialog: the Promise wrapper over PrimeVue's confirm
 * service. `ask()` targets the default dialog; `askWithDetail()`
 * targets the `detail` group and hangs the block and the list on
 * the confirmation object for that dialog's message slot.
 */
import { describe, expect, it, vi } from 'vitest'

const requireMock = vi.fn()
vi.mock('primevue/useconfirm', () => ({
  useConfirm: () => ({ require: requireMock }),
}))

import { useConfirmDialog } from '../useConfirmDialog'

describe('useConfirmDialog', () => {
  it('ask() resolves true on accept and targets the default dialog', async () => {
    requireMock.mockReset()
    const { ask } = useConfirmDialog()
    const p = ask('Sure?', { acceptLabel: 'Go' })
    const opts = requireMock.mock.calls[0]![0]
    expect(opts.group).toBeUndefined()
    expect(opts.message).toBe('Sure?')
    expect(opts.acceptLabel).toBe('Go')
    opts.accept()
    opts.onHide()
    await expect(p).resolves.toBe(true)
  })

  it('askWithDetail() carries the block and the list to the detail group', async () => {
    requireMock.mockReset()
    const { askWithDetail } = useConfirmDialog()
    const p = askWithDetail('Convert?', {
      detail: '{ "title": "polar" }',
      items: ['one', 'two'],
      severity: 'danger',
    })
    const opts = requireMock.mock.calls[0]![0]
    expect(opts.group).toBe('detail')
    expect(opts.detail).toBe('{ "title": "polar" }')
    expect(opts.items).toEqual(['one', 'two'])
    expect(opts.acceptProps).toEqual({ severity: 'danger' })
    opts.reject()
    await expect(p).resolves.toBe(false)
  })

  it('resolves false when the dialog is dismissed', async () => {
    requireMock.mockReset()
    const { ask } = useConfirmDialog()
    const p = ask('Sure?')
    requireMock.mock.calls[0]![0].onHide()
    await expect(p).resolves.toBe(false)
  })
})
