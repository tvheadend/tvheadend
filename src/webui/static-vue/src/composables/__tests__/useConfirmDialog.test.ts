// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useConfirmDialog — unit tests for the Promise wrapper over
 * PrimeVue's confirm service (mocked here). Covers what each answer
 * resolves to, and that Escape closes only the confirmation: the
 * player and the EPG drawer it is asked from listen for Escape on the
 * document and must not see it. `ask()` targets the default dialog;
 * `askWithDetail()` targets the `detail` group and hangs the block and
 * the list on the confirmation object for that dialog's message slot.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import type { ConfirmationOptions } from 'primevue/confirmationoptions'
import { useConfirmDialog } from '../useConfirmDialog'

const requireMock = vi.fn()
const closeMock = vi.fn()
vi.mock('primevue/useconfirm', () => ({
  useConfirm: () => ({ require: requireMock, close: closeMock }),
}))

/* Stands in for the Escape listener of the player or the EPG drawer. */
const outerKeydown = vi.fn()

beforeEach(() => {
  requireMock.mockReset()
  closeMock.mockReset()
  outerKeydown.mockReset()
  document.addEventListener('keydown', outerKeydown)
})

afterEach(() => {
  document.removeEventListener('keydown', outerKeydown)
})

/* The options of the last confirm.require() call. */
function lastOptions(): ConfirmationOptions {
  return requireMock.mock.lastCall![0]
}

function pressEscape(): void {
  document.body.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))
}

describe('useConfirmDialog', () => {
  it('resolves true on accept and false on reject or hide', async () => {
    const { ask } = useConfirmDialog()
    const accepted = ask('Stop recording?')
    lastOptions().accept!()
    await expect(accepted).resolves.toBe(true)
    const rejected = ask('Stop recording?')
    lastOptions().reject!()
    await expect(rejected).resolves.toBe(false)
    const hidden = ask('Stop recording?')
    lastOptions().onHide!()
    await expect(hidden).resolves.toBe(false)
  })

  it('closes only the confirmation on Escape', async () => {
    const { ask } = useConfirmDialog()
    const answer = ask('Stop recording?')
    pressEscape()
    expect(outerKeydown).not.toHaveBeenCalled()
    expect(closeMock).toHaveBeenCalledOnce()
    await expect(answer).resolves.toBe(false)
  })

  it('lets Escape through again once answered', async () => {
    const { ask } = useConfirmDialog()
    const answer = ask('Stop recording?')
    lastOptions().reject!()
    await answer
    pressEscape()
    expect(outerKeydown).toHaveBeenCalledOnce()
    expect(closeMock).not.toHaveBeenCalled()
  })

  it('ask() targets the default dialog', async () => {
    const { ask } = useConfirmDialog()
    const p = ask('Sure?', { acceptLabel: 'Go' })
    const opts = requireMock.mock.lastCall![0]
    expect(opts.group).toBeUndefined()
    expect(opts.message).toBe('Sure?')
    expect(opts.acceptLabel).toBe('Go')
    opts.accept()
    opts.onHide()
    await expect(p).resolves.toBe(true)
  })

  it('askWithDetail() carries the block and the list to the detail group', async () => {
    const { askWithDetail } = useConfirmDialog()
    const p = askWithDetail('Convert?', {
      detail: '{ "title": "polar" }',
      items: ['one', 'two'],
      severity: 'danger',
    })
    const opts = requireMock.mock.lastCall![0]
    expect(opts.group).toBe('detail')
    expect(opts.detail).toBe('{ "title": "polar" }')
    expect(opts.items).toEqual(['one', 'two'])
    expect(opts.acceptProps).toEqual({ severity: 'danger' })
    opts.reject()
    await expect(p).resolves.toBe(false)
  })
})
