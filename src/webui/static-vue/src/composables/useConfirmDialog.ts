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
 *   - `onHide` covers Esc / click-outside / backdrop-tap dismissal —
 *     we resolve those as `false` so the consumer's "no action"
 *     branch fires correctly. PrimeVue calls `onHide` AFTER `accept`/
 *     `reject` too, so we guard against double-resolve with a flag.
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
import type { ConfirmationOptions } from 'primevue/confirmationoptions'

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

export interface ConfirmDetailOptions extends ConfirmDialogOptions {
  /** Preformatted block shown under the message (an expression, a path). */
  detail?: string
  /** Bullet list shown under the block (warnings, consequences). */
  items?: string[]
}

/*
 * The extras ride on the confirmation object itself: PrimeVue hands
 * the whole object to the dialog's message slot, and the `detail`
 * group's dialog in AppShell renders them. The default dialog (no
 * group) never sees them.
 */
type DetailConfirmation = ConfirmationOptions & {
  detail?: string
  items?: string[]
}

export function useConfirmDialog() {
  const confirm = useConfirm()

  function run(options: DetailConfirmation, opts: ConfirmDialogOptions): Promise<boolean> {
    return new Promise((resolve) => {
      let settled = false
      const settle = (v: boolean) => {
        if (settled) return
        settled = true
        resolve(v)
      }
      options.header = opts.header ?? 'Confirm'
      options.acceptLabel = opts.acceptLabel ?? 'Yes'
      options.rejectLabel = opts.rejectLabel ?? 'No'
      options.acceptProps = opts.severity === 'danger' ? { severity: 'danger' } : undefined
      options.accept = () => settle(true)
      options.reject = () => settle(false)
      options.onHide = () => settle(false)
      confirm.require(options)
    })
  }

  function ask(message: string, opts: ConfirmDialogOptions = {}): Promise<boolean> {
    return run({ message }, opts)
  }

  /** `ask()` with a preformatted block and/or a bullet list under the message. */
  function askWithDetail(message: string, opts: ConfirmDetailOptions = {}): Promise<boolean> {
    return run({ message, group: 'detail', detail: opts.detail, items: opts.items }, opts)
  }

  return { ask, askWithDetail }
}
