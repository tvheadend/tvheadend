/*
 * Shared toolbar / editor helpers for DVR sub-views. Three pieces:
 *
 *   1. `adminAwareEditList()` — packages the 4-string concatenation
 *      pattern that builds the IdnodeEditor's `list` prop. Each view
 *      declared this same computed (5 LOC) inline, varying only in
 *      the four constants. Helper takes the constants in, returns the
 *      same admin-aware ComputedRef<string>.
 *
 *   2. `buildAddEditDeleteActions()` — returns the standard 3-button
 *      ActionDef[] (Add, Edit, Delete) that the Autorecs / Timers /
 *      Upcoming toolbars all render. Returned array can be spread
 *      into a larger toolbar (UpcomingView extends with Stop / Abort
 *      / Prevrec).
 *
 *   3. `buildEditDeleteRerecordActions()` — returns the Edit / Delete
 *      / Re-record triple shared between the Failed and Removed
 *      toolbars. Removed uses it as the full action set; Failed
 *      spreads it and appends Move-to-finished + Download.
 *
 * Both helpers were extracted because the underlying shapes were
 * structurally identical across multiple views — Add/Edit/Delete
 * appeared on AutorecsView, TimersView, and UpcomingView; the
 * Edit/Delete/Re-record triple lived in both failedActions.ts and
 * removedActions.ts.
 *
 * Lives next to the dvr views (rather than in `composables/`)
 * because the wording / labels are DVR-specific. If the pattern
 * appears outside DVR (e.g. on a future Configuration L3 leaf with
 * the same Add/Edit/Delete shape), promote the relevant helper to
 * `composables/`.
 */
import { computed, type ComputedRef, type Ref } from 'vue'
import { useAccessStore } from '@/stores/access'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'

/* ---------------------------------------------------------------- */
/*   adminAwareEditList                                             */
/* ---------------------------------------------------------------- */

export interface AdminAwareEditListOptions {
  /**
   * Field-list segment that goes BEFORE the base. Typically `'enabled'`
   * (so it appears at the top of the form) or `'enabled,start_extra,
   * stop_extra'`.
   */
  head: string
  /**
   * Main field set, common to both edit and create flows. e.g.
   * `'name,title,channel,start,stop,...'`.
   */
  base: string
  /**
   * Admin-only field set appended when `access.data.admin` is true.
   * Typically `'owner,creator'` or `'episode_disp,owner,creator'`.
   */
  adminExtra: string
  /**
   * Field-list segment that goes at the END (after admin extras).
   * Typically post-creation policy fields like `'retention,removal'`.
   */
  tail: string
}

/**
 * Returns a reactive `ComputedRef<string>` that joins the four
 * segments (head, base, adminExtra-iff-admin, tail) with commas.
 * Re-evaluates when the access store's admin flag flips.
 *
 * Replaces the inline pattern that was repeated across UpcomingView,
 * AutorecsView, and TimersView.
 */
export function adminAwareEditList(opts: AdminAwareEditListOptions): ComputedRef<string> {
  const access = useAccessStore()
  return computed(() => {
    const parts = [opts.head, opts.base]
    if (access.data?.admin) parts.push(opts.adminExtra)
    parts.push(opts.tail)
    return parts.join(',')
  })
}

/* ---------------------------------------------------------------- */
/*   buildAddEditDeleteActions                                      */
/* ---------------------------------------------------------------- */

export interface AddEditDeleteActionsOptions {
  /** Live grid selection — gates Edit (single-row) and Delete (≥1). */
  selection: BaseRow[]
  /** Grid's selection-clear callback — invoked by Delete on success. */
  clearSelection: () => void
  /**
   * Bulk-delete handle from `useBulkAction({ endpoint: 'idnode/delete', ... })`.
   * `inflight.value` drives the Delete button's "Deleting…" label and
   * disabled state; `run(...)` is the click handler.
   */
  remove: {
    inflight: Ref<boolean>
    run: (selected: BaseRow[], clear: () => void) => Promise<void>
  }
  /** Add-button click handler (typically `openCreate` from useEditorMode). */
  onAdd: () => void
  /** Edit-button click handler (typically `openEditor` from useEditorMode). */
  onEdit: (selected: BaseRow[]) => void
  /**
   * Per-view Add tooltip — wording differs ("Add a new recording" vs
   * "Add a new autorec entry" vs "Add a new timer entry") so existing
   * translations apply once vue-i18n lands.
   */
  addTooltip: string
}

/**
 * Returns the standard 3-action toolbar array (Add, Edit, Delete) used
 * by Autorecs / Timers and as the leading three actions of Upcoming.
 *
 * Spread into a larger array if the view adds more actions (Upcoming
 * appends Stop / Abort / Prevrec):
 *
 *   ```ts
 *   return [
 *     ...buildAddEditDeleteActions({ ... }),
 *     { id: 'stop', ... },
 *     { id: 'abort', ... },
 *   ]
 *   ```
 */
export function buildAddEditDeleteActions(opts: AddEditDeleteActionsOptions): ActionDef[] {
  return [
    {
      id: 'add',
      label: 'Add',
      tooltip: opts.addTooltip,
      onClick: () => opts.onAdd(),
    },
    {
      id: 'edit',
      label: 'Edit',
      tooltip: 'Edit the selected entry',
      disabled: opts.selection.length !== 1,
      onClick: () => opts.onEdit(opts.selection),
    },
    {
      id: 'delete',
      label: opts.remove.inflight.value ? 'Deleting…' : 'Delete',
      tooltip: 'Delete the selected entries',
      disabled: opts.selection.length === 0 || opts.remove.inflight.value,
      onClick: () => opts.remove.run(opts.selection, opts.clearSelection),
    },
  ]
}

/* ---------------------------------------------------------------- */
/*   buildEditDeleteRerecordActions                                 */
/* ---------------------------------------------------------------- */

export interface EditDeleteRerecordActionsOptions {
  /** Live grid selection — gates Edit (single-row) and bulk actions. */
  selection: BaseRow[]
  /** Grid's selection-clear callback — invoked after each bulk run. */
  clearSelection: () => void
  /** True while a delete is in flight — drives label flip + disabled. */
  deleting: boolean
  /** True while a re-record toggle is in flight — same role as above. */
  rerecording: boolean
  /** Edit-button click handler. */
  onEdit: (selected: BaseRow[]) => void
  /** Delete-button click handler. */
  onDelete: (selected: BaseRow[], clear: () => void) => void
  /** Re-record toggle click handler. */
  onRerecord: (selected: BaseRow[], clear: () => void) => void
}

/**
 * Returns the Edit / Delete / Re-record action triple shared by the
 * Failed and Removed toolbars. Removed uses it as its full toolbar;
 * Failed spreads it and appends Move-to-finished + Download:
 *
 *   ```ts
 *   return [
 *     ...buildEditDeleteRerecordActions({ ... }),
 *     { id: 'move', ... },
 *     { id: 'download', ... },
 *   ]
 *   ```
 *
 * Plain-boolean inflight + plain-callback handler shape mirrors the
 * existing `failedActions.ts` / `removedActions.ts` deps interfaces
 * (no Ref unwrapping required at the call site).
 */
export function buildEditDeleteRerecordActions(
  opts: EditDeleteRerecordActionsOptions
): ActionDef[] {
  return [
    {
      id: 'edit',
      label: 'Edit',
      tooltip: 'Edit the selected entry',
      disabled: opts.selection.length !== 1,
      onClick: () => opts.onEdit(opts.selection),
    },
    {
      id: 'delete',
      label: opts.deleting ? 'Deleting…' : 'Delete',
      tooltip: 'Delete the selected entries',
      disabled: opts.selection.length === 0 || opts.deleting,
      onClick: () => opts.onDelete(opts.selection, opts.clearSelection),
    },
    {
      id: 'rerecord',
      label: opts.rerecording ? 'Toggling…' : 'Re-record',
      tooltip: 'Toggle re-record functionality',
      disabled: opts.selection.length === 0 || opts.rerecording,
      onClick: () => opts.onRerecord(opts.selection, opts.clearSelection),
    },
  ]
}
