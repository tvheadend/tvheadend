// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, describe, expect, it, vi } from 'vitest'
import {
  __resetShortcutsForTests,
  registerShortcut,
} from '../useGlobalShortcuts'

function dispatchKey(
  key: string,
  modifiers: { meta?: boolean; ctrl?: boolean; shift?: boolean } = {},
  target?: EventTarget,
): KeyboardEvent {
  const event = new KeyboardEvent('keydown', {
    key,
    metaKey: !!modifiers.meta,
    ctrlKey: !!modifiers.ctrl,
    shiftKey: !!modifiers.shift,
    bubbles: true,
    cancelable: true,
  })
  if (target) {
    /* `KeyboardEvent.target` is read-only by spec, but happy-dom lets
     * us override it via Object.defineProperty for the focus-target
     * tests. */
    Object.defineProperty(event, 'target', { value: target, configurable: true })
  }
  globalThis.window.dispatchEvent(event)
  return event
}

describe('useGlobalShortcuts', () => {
  afterEach(() => {
    __resetShortcutsForTests()
  })

  it('fires the handler for Cmd-K when eitherMetaOrCtrl is set (Mac)', () => {
    const handler = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    dispatchKey('k', { meta: true })
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('fires the handler for Ctrl-K when eitherMetaOrCtrl is set (Win/Linux)', () => {
    const handler = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    dispatchKey('k', { ctrl: true })
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('does not fire when no modifier is held (eitherMetaOrCtrl)', () => {
    const handler = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    dispatchKey('k')
    expect(handler).not.toHaveBeenCalled()
  })

  it('does not fire when BOTH Meta and Ctrl are held (eitherMetaOrCtrl is XOR)', () => {
    /* Cmd-Ctrl-K is conventionally a different shortcut (Mac apps
     * use it for fullscreen toggles etc.). */
    const handler = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    dispatchKey('k', { meta: true, ctrl: true })
    expect(handler).not.toHaveBeenCalled()
  })

  it('matches case-insensitively for single letters (Shift-K still fires K)', () => {
    const handler = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    /* User pressing Cmd-Shift-K — the event's key value is 'K'
     * (uppercase). We want the binding to match. */
    dispatchKey('K', { meta: true, shift: true })
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('calls preventDefault when a shortcut fires (overrides browser Cmd-K)', () => {
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, vi.fn())
    const event = dispatchKey('k', { meta: true })
    expect(event.defaultPrevented).toBe(true)
  })

  it('does not preventDefault when no binding matches', () => {
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, vi.fn())
    const event = dispatchKey('j', { meta: true })
    expect(event.defaultPrevented).toBe(false)
  })

  it('fires the bare-character binding when no modifier is held', () => {
    const handler = vi.fn()
    registerShortcut({ key: '/' }, handler)
    dispatchKey('/')
    expect(handler).toHaveBeenCalledTimes(1)
  })

  it('ignoreInEditable suppresses the binding when focus is in an input', () => {
    const handler = vi.fn()
    registerShortcut({ key: '/', ignoreInEditable: true }, handler)
    const input = document.createElement('input')
    document.body.appendChild(input)
    dispatchKey('/', {}, input)
    expect(handler).not.toHaveBeenCalled()
    input.remove()
  })

  it('ignoreInEditable suppresses the binding when focus is in a textarea', () => {
    const handler = vi.fn()
    registerShortcut({ key: '/', ignoreInEditable: true }, handler)
    const textarea = document.createElement('textarea')
    document.body.appendChild(textarea)
    dispatchKey('/', {}, textarea)
    expect(handler).not.toHaveBeenCalled()
    textarea.remove()
  })

  it('ignoreInEditable suppresses the binding when focus is in a contenteditable', () => {
    const handler = vi.fn()
    registerShortcut({ key: '/', ignoreInEditable: true }, handler)
    const div = document.createElement('div')
    div.contentEditable = 'true'
    document.body.appendChild(div)
    dispatchKey('/', {}, div)
    expect(handler).not.toHaveBeenCalled()
    div.remove()
  })

  it('fires the bare-character binding when ignoreInEditable + focus is on a non-editable', () => {
    const handler = vi.fn()
    registerShortcut({ key: '/', ignoreInEditable: true }, handler)
    const div = document.createElement('div')
    document.body.appendChild(div)
    dispatchKey('/', {}, div)
    expect(handler).toHaveBeenCalledTimes(1)
    div.remove()
  })

  it('cleanup function removes the binding (no further fires)', () => {
    const handler = vi.fn()
    const cleanup = registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, handler)
    cleanup()
    dispatchKey('k', { meta: true })
    expect(handler).not.toHaveBeenCalled()
  })

  it('first matching binding wins; later bindings on the same key do not fire', () => {
    const first = vi.fn()
    const second = vi.fn()
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, first)
    registerShortcut({ key: 'k', eitherMetaOrCtrl: true }, second)
    dispatchKey('k', { meta: true })
    expect(first).toHaveBeenCalledTimes(1)
    expect(second).not.toHaveBeenCalled()
  })
})
