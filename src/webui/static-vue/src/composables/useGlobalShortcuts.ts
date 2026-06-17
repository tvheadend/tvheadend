// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useGlobalShortcuts — app-level keydown registry.
 *
 * First global shortcut surface in the Vue UI. Until now, every
 * keyboard binding was a per-component `document.addEventListener`
 * (e.g. BandwidthChartView, EditableTagChipCell). Cmd-K /
 * Ctrl-K (command palette) is the first shortcut that lives at
 * the application chrome level and needs a coordinated owner.
 *
 * Lazy-attached single listener: the first `registerShortcut`
 * call installs `keydown` on `window`; the last `unregister`
 * removes it. Matches the helpers in `useStickyBottom` and
 * `useNowCursor` — no work done before there's a consumer.
 *
 * The binding spec carries an `eitherMetaOrCtrl` shorthand so a
 * single call covers both Mac (`Cmd-K`) and Windows / Linux
 * (`Ctrl-K`) without forcing each caller to register two
 * bindings. `ignoreInEditable` suppresses the binding while
 * focus is in a text input — needed for the bare-character
 * shortcuts (`/` to open the palette) that would otherwise eat
 * the user's typing.
 */

export interface ShortcutBinding {
  /* The KeyboardEvent.key value to match. Case-insensitive for
   * single letters; literal otherwise (e.g. `/`, `Escape`). */
  key: string
  /* Require the `Meta` modifier (Cmd on Mac). Mutually exclusive
   * with `eitherMetaOrCtrl`. */
  meta?: boolean
  /* Require the `Control` modifier. Mutually exclusive with
   * `eitherMetaOrCtrl`. */
  ctrl?: boolean
  /* Match when EITHER `Meta` or `Control` is held. The convention
   * for "the Cmd-K shortcut" — fires for Mac Cmd-K AND for
   * Win/Linux Ctrl-K from a single registration. */
  eitherMetaOrCtrl?: boolean
  /* Skip the binding when an editable element (input / textarea /
   * contenteditable) has focus. Required for bare-character
   * shortcuts that would otherwise interfere with typing. */
  ignoreInEditable?: boolean
}

type Handler = (event: KeyboardEvent) => void

interface RegisteredShortcut {
  binding: ShortcutBinding
  handler: Handler
}

const registered = new Set<RegisteredShortcut>()
let listenerAttached = false

function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) return false
  const tag = target.tagName
  if (tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT') return true
  if (target.isContentEditable) return true
  return false
}

function matches(event: KeyboardEvent, binding: ShortcutBinding): boolean {
  /* Single-letter keys come through with the casing the user typed;
   * normalize so `key: 'k'` matches both `'k'` and `'K'` (Shift+K). */
  const eventKey = event.key.length === 1 ? event.key.toLowerCase() : event.key
  const bindingKey = binding.key.length === 1 ? binding.key.toLowerCase() : binding.key
  if (eventKey !== bindingKey) return false

  if (binding.eitherMetaOrCtrl) {
    /* XOR — exactly one of Meta or Ctrl. Cmd-Ctrl-K is a different
     * shortcut and shouldn't fire this one. */
    if (event.metaKey === event.ctrlKey) return false
  } else {
    if (!!binding.meta !== event.metaKey) return false
    if (!!binding.ctrl !== event.ctrlKey) return false
  }

  if (binding.ignoreInEditable && isEditableTarget(event.target)) return false

  return true
}

function onKeydown(event: KeyboardEvent): void {
  for (const { binding, handler } of registered) {
    if (matches(event, binding)) {
      event.preventDefault()
      handler(event)
      /* One binding per keystroke — first match wins. Avoids the
       * surprise of two registered handlers both firing on the
       * same Cmd-K. */
      return
    }
  }
}

function ensureAttached(): void {
  if (listenerAttached) return
  if (globalThis.window === undefined) return
  globalThis.window.addEventListener('keydown', onKeydown)
  listenerAttached = true
}

function detachIfEmpty(): void {
  if (registered.size > 0) return
  if (!listenerAttached) return
  if (globalThis.window === undefined) return
  globalThis.window.removeEventListener('keydown', onKeydown)
  listenerAttached = false
}

/*
 * Register a shortcut. Returns a cleanup function that removes
 * this binding. Callers in components typically pair this with
 * `onMounted` / `onBeforeUnmount`; module-level callers (none yet)
 * can hold the cleanup indefinitely.
 */
export function registerShortcut(binding: ShortcutBinding, handler: Handler): () => void {
  const entry: RegisteredShortcut = { binding, handler }
  registered.add(entry)
  ensureAttached()
  return () => {
    registered.delete(entry)
    detachIfEmpty()
  }
}

/* Test helper — drop all registrations between tests so leaked
 * handlers from one test don't fire in another. Not exported as
 * part of the public surface (no use case outside test isolation),
 * but importable from the test file via the dedicated path. */
export function __resetShortcutsForTests(): void {
  registered.clear()
  detachIfEmpty()
}
