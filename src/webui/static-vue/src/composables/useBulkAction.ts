/*
 * useBulkAction — shared helper for DVR-style bulk operations on a
 * grid selection.
 *
 * Every DVR sub-view declares 1–4 toolbar actions that follow the
 * same shape: pull UUIDs from the selected rows, optionally confirm
 * with the user, fire a single POST to a server bulk endpoint with
 * `{ uuid: JSON.stringify(uuids) }`, clear the grid selection on
 * success, surface failures via globalThis.alert, and toggle a
 * reactive in-flight flag so the toolbar button disables while the
 * request is in flight. Until this composable existed, that
 * boilerplate was either inlined per action (Upcoming, Autorecs,
 * Timers) or wrapped by a private per-view `bulkAction()` helper
 * (Failed, Finished, Removed) — three near-identical copies of the
 * same code, plus the inline cases. Extracted to one place to
 * remove the duplication.
 *
 * Toolbar-action *builders* (failedActions.ts / finishedActions.ts /
 * removedActions.ts) are intentionally NOT changed. Those compose
 * `ActionDef[]` arrays from the inflight booleans + onClick
 * callbacks; they take primitives so they remain unit-testable
 * without mounting a real Pinia / Vue env. The view glues the two
 * layers: per-action `useBulkAction(...)` returns the inflight ref
 * + the run function, which the view passes into the builder.
 */
import { ref, type Ref } from 'vue'
import { apiCall } from '@/api/client'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import type { BaseRow } from '@/types/grid'

export interface BulkActionConfig {
  /** API endpoint relative to /api/, e.g. `'dvr/entry/cancel'`. */
  endpoint: string
  /**
   * Optional confirmation text. Shown via the themed PrimeVue
   * `<ConfirmDialog>` before firing the request — declining cancels
   * the run as a no-op (no API call, no inflight toggle, no
   * selection clear). Omit for fire-and-forget actions (none today,
   * but Add / Schedule-from-EPG flows in the future).
   */
  confirmText?: string
  /**
   * Severity for the confirm dialog's accept button. Pass
   * `'danger'` for destructive actions (delete / abort) so the
   * primary-action button renders red — mirrors common admin-UI
   * convention. Omit for neutral confirms.
   */
  confirmSeverity?: 'danger'
  /**
   * Prefix for the user-facing failure toast. The error message is
   * appended after `: `. E.g. `'Failed to abort'` produces a toast
   * with `Failed to abort: <message>` as the detail. Mirrors the
   * verbatim copy each view used pre-extraction so existing
   * translations apply once vue-i18n lands.
   */
  failPrefix: string
}

export interface BulkActionHandle {
  /**
   * Reactive in-flight state for the toolbar button. True from the
   * moment the request fires until the response (success or failure)
   * lands. Read this as `.value` inside the toolbar builder so the
   * button label / disabled state re-renders.
   */
  inflight: Ref<boolean>
  /**
   * Execute the action against a selection.
   *   - Filters rows missing a uuid; no-op if none remain.
   *   - Honours the optional confirm dialog (themed PrimeVue
   *     `<ConfirmDialog>`).
   *   - POSTs `{ uuid: JSON.stringify(uuids) }` to `endpoint`.
   *   - Calls `clear()` on success.
   *   - Surfaces failures via PrimeVue Toast (`useToastNotify`).
   *
   * Resolves once the request settles, regardless of outcome.
   */
  run: (selected: BaseRow[], clear: () => void) => Promise<void>
}

export function useBulkAction(config: BulkActionConfig): BulkActionHandle {
  const inflight = ref(false)
  /* `useConfirmDialog` and `useToastNotify` both read injection from
   * the current Vue setup context. Calling useBulkAction inside a
   * component / composable's setup is the established convention
   * (every existing consumer already does); keep that contract. */
  const confirmDialog = useConfirmDialog()
  const toast = useToastNotify()

  async function run(selected: BaseRow[], clear: () => void) {
    const uuids = selected.map((r) => r.uuid).filter((u): u is string => !!u)
    if (uuids.length === 0) return
    if (config.confirmText) {
      const ok = await confirmDialog.ask(config.confirmText, {
        severity: config.confirmSeverity,
      })
      if (!ok) return
    }
    inflight.value = true
    try {
      await apiCall(config.endpoint, { uuid: JSON.stringify(uuids) })
      clear()
    } catch (err) {
      toast.error(`${config.failPrefix}: ${err instanceof Error ? err.message : String(err)}`)
    } finally {
      inflight.value = false
    }
  }

  return { inflight, run }
}
