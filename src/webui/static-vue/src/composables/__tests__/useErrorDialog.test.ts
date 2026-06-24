// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Unit tests for the useErrorDialog singleton. Exercises the
 * show/dismiss state machine + the replace-on-second-show
 * concurrency rule. The dialog DOM (`ErrorDialog.vue`) is covered
 * separately via eyeball.
 *
 * Tests share module state — the singleton's `_internal.dismiss()`
 * is called in `beforeEach` to reset any in-flight dialog from a
 * previous test.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { _internal, useErrorDialog } from '../useErrorDialog'

beforeEach(() => {
  _internal.dismiss()
})

describe('useErrorDialog', () => {
  it('show() opens the dialog with the supplied content', () => {
    const { show } = useErrorDialog()
    void show({ title: 'Save failed', message: 'Invalid cron' })
    expect(_internal.isOpen.value).toBe(true)
    expect(_internal.state.value).toEqual({
      title: 'Save failed',
      message: 'Invalid cron',
      detail: null,
    })
  })

  it('show() defaults the title to "Error" when omitted', () => {
    const { show } = useErrorDialog()
    void show({ message: 'oops' })
    expect(_internal.state.value?.title).toBe('Error')
  })

  it('show() returns a Promise that resolves on dismiss', async () => {
    const { show } = useErrorDialog()
    const settled = vi.fn()
    show({ message: 'x' }).then(settled)
    expect(settled).not.toHaveBeenCalled()
    _internal.dismiss()
    /* Microtask flush so the .then handler runs. */
    await Promise.resolve()
    expect(settled).toHaveBeenCalledOnce()
  })

  it('dismiss() closes the dialog', () => {
    const { show } = useErrorDialog()
    void show({ message: 'x' })
    expect(_internal.isOpen.value).toBe(true)
    _internal.dismiss()
    expect(_internal.isOpen.value).toBe(false)
  })

  it('second show() replaces the first dialog and resolves the first Promise', async () => {
    const { show } = useErrorDialog()
    const firstSettled = vi.fn()
    show({ message: 'first' }).then(firstSettled)
    show({ message: 'second' })
    /* First promise resolves to clear the previous pending
     * resolver (per the concurrency rule). */
    await Promise.resolve()
    expect(firstSettled).toHaveBeenCalledOnce()
    expect(_internal.isOpen.value).toBe(true)
    expect(_internal.state.value?.message).toBe('second')
  })

  it('dismiss() resolves a still-pending show() promise', async () => {
    const { show } = useErrorDialog()
    const settled = vi.fn()
    show({ message: 'x' }).then(settled)
    _internal.dismiss()
    await Promise.resolve()
    expect(settled).toHaveBeenCalledOnce()
  })

  it('dismiss() with no open dialog is a no-op', () => {
    /* Defensive: a stray Esc / close handler firing after the
     * dialog is already closed must not throw. */
    expect(() => _internal.dismiss()).not.toThrow()
  })
})
