// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useConfirmDialog — Promise-returning wrapper over PrimeVue's
 * `useConfirm()` injection so call sites stay shaped almost
 * identically to the legacy `globalThis.confirm()` pattern they
 * replace.
 *
 * Why a wrapper:
 *   - PrimeVue's API is callback-based (`confirm.require({ accept,
 *     reject })`). Most of our consumers (`useBulkAction`,
 *     `IdnodeEditor`, `EpgEventDrawer`, `StreamView`) flow as
 *     synchronous-feeling guard checks — `if (!confirm(...)) return`.
 *     A Promise<boolean> preserves that read order with one `await`.
 *   - `onHide` covers click-outside / backdrop-tap dismissal, and
 *     our own Escape listener covers Esc — we resolve those as
 *     `false` so the consumer's "no action" branch fires correctly.
 *     PrimeVue calls `onHide` AFTER `accept`/`reject` too, so we
 *     guard against double-resolve with a flag.
 *
 * Must be called from a Vue setup context (component or composable
 * `setup()`) — `useConfirm()` reads from the current instance's
 * inject tree, populated by `app.use(ConfirmationService)` in
 * main.ts. Calling outside setup throws.
 *
 * Severity defaults to undefined (PrimeVue's neutral primary). Pass
 * `severity: 'danger'` for destructive confirms (Delete recording,
 * Discard unsaved changes) so the accept button renders red.
 */
import { useConfirm } from 'primevue/useconfirm'

export interface ConfirmDialogOptions {
  /** Header text (defaults to "Confirm"). */
  header?: string
  /** Accept-button label (defaults to "Yes"). */
  acceptLabel?: string
  /** Reject-button label (defaults to "No"). */
  rejectLabel?: string
  /** Severity for the accept button — `'danger'` renders it red. */
  severity?: 'danger'
}

export function useConfirmDialog() {
  const confirm = useConfirm()

  function ask(message: string, opts: ConfirmDialogOptions = {}): Promise<boolean> {
    return new Promise((resolve) => {
      let settled = false
      const settle = (v: boolean) => {
        if (settled) return
        settled = true
        window.removeEventListener('keydown', onEscape, true)
        resolve(v)
      }
      /* Escape closes only the confirmation. The dialog or drawer it
       * was asked from (the player, the EPG drawer) listens for Escape
       * on the document too and would close as well, so take the key
       * in the capture phase on the window first. */
      const onEscape = (ev: KeyboardEvent) => {
        if (ev.key !== 'Escape') return
        ev.stopPropagation()
        ev.preventDefault()
        confirm.close()
        settle(false)
      }
      window.addEventListener('keydown', onEscape, true)
      confirm.require({
        message,
        header: opts.header ?? 'Confirm',
        acceptLabel: opts.acceptLabel ?? 'Yes',
        rejectLabel: opts.rejectLabel ?? 'No',
        acceptProps: opts.severity === 'danger' ? { severity: 'danger' } : undefined,
        accept: () => settle(true),
        reject: () => settle(false),
        onHide: () => settle(false),
      })
    })
  }

  return { ask }
}
