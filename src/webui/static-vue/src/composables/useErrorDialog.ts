// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useErrorDialog — module-level singleton driving the global
 * `<ErrorDialog />` mounted in `AppShell.vue`. Any caller (a save
 * catch in IdnodeEditor, useInlineEdit's save(), a batch op in a
 * view) can pop a modal with a server-side error reason without
 * threading props through the component tree.
 *
 * Pattern mirrors `useToastNotify` / `useConfirmDialog` — composable
 * returns helpers that mutate module-level reactive state. Unlike
 * those two (which sit on PrimeVue services), the dialog body is
 * our own simple component; one place to control wording, layout,
 * and the OK-to-dismiss contract.
 *
 * Concurrency rule: a second `show()` while a dialog is already open
 * REPLACES the open one. The first call's Promise resolves on
 * dismiss either way (the runtime collapses both to one OK click,
 * which is acceptable for "save failed" semantics — the user is
 * acknowledging "I saw the error" regardless of which save failed).
 * This is intentional: batch operations that fail across many rows
 * shouldn't queue ten dialogs for the user to dismiss serially.
 *
 * No setup-context dependency. The composable can be called from
 * outside Vue's component tree (e.g. inside a pure composable like
 * useInlineEdit's save function) because the underlying state is
 * just a module-scoped ref.
 */

import { ref } from 'vue'

export interface ErrorDialogOptions {
  /** Modal header. Defaults to "Error". Pass a context-specific
   *  title like "Save failed" / "Could not create". */
  title?: string
  /** Body text. Required — typically `apiErrorMessage(e)`. */
  message: string
  /** Optional secondary line shown smaller / dimmer beneath the
   *  message. Use for a hint like "Check the cron expression
   *  format." */
  detail?: string
}

interface DialogState extends Required<Pick<ErrorDialogOptions, 'message'>> {
  title: string
  detail: string | null
}

/* Module-level singleton. The composable returns helpers that
 * mutate these refs; the mounted ErrorDialog component reads them. */
const isOpen = ref(false)
const state = ref<DialogState | null>(null)
let pendingResolve: (() => void) | null = null

function show(opts: ErrorDialogOptions): Promise<void> {
  /* Resolve any in-flight promise so a replaced dialog doesn't
   * leak its resolver. Acceptable per the concurrency rule above
   * — the previous caller's "wait for dismiss" effectively ends
   * when its dialog is replaced. */
  if (pendingResolve) {
    pendingResolve()
    pendingResolve = null
  }
  state.value = {
    title: opts.title ?? 'Error',
    message: opts.message,
    detail: opts.detail ?? null,
  }
  isOpen.value = true
  return new Promise<void>((resolve) => {
    pendingResolve = resolve
  })
}

function dismiss(): void {
  isOpen.value = false
  if (pendingResolve) {
    pendingResolve()
    pendingResolve = null
  }
}

export function useErrorDialog() {
  return { show }
}

/* Internal accessors for the ErrorDialog component. Not exported
 * from the public composable so consumers can't peek at internal
 * state — they only get `show`. */
export const _internal = {
  isOpen,
  state,
  dismiss,
}
