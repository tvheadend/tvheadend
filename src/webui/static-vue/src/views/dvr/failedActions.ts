/*
 * DVR Failed toolbar actions — builder split out from FailedView.vue
 * so the gating logic is unit-testable without mounting the view.
 *
 * Mirrors `tvheadend.dvr_failed` in src/webui/static/app/dvr.js
 * (toolbar at line 945, gating at line 892-898). Five actions:
 *
 *   - Edit (single-row, drawer-edit via the limited Failed edit list)
 *   - Delete (≥1, api/idnode/delete with the verbatim two-line
 *     confirmation copy from dvr.js:910-911)
 *   - Re-record (≥1, api/dvr/entry/rerecord/toggle — count-only gate)
 *   - Move to finished (≥1, api/dvr/entry/move/finished — count-only)
 *   - Download (1-row, file>0, /dvrfile/<uuid> in a new tab)
 *
 * Critical difference from Finished: Re-record and Move-to-finished
 * are gated on selection count alone. Only Download requires the
 * first row's filesize > 0 (verified at dvr.js:892-898 — Failed's
 * `selected` callback differs from Finished's, even though the same
 * `firstRowHasFile` predicate applies to Download).
 */
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { firstRowHasFile } from './predicates'
import { buildEditDeleteRerecordActions } from './dvrToolbarHelpers'

export interface FailedActionDeps {
  selection: BaseRow[]
  clearSelection: () => void
  deleting: boolean
  rerecording: boolean
  moving: boolean
  onEdit: (selection: BaseRow[]) => void
  onDelete: (selection: BaseRow[], clear: () => void) => void
  onDownload: (selection: BaseRow[]) => void
  onRerecord: (selection: BaseRow[], clear: () => void) => void
  onMoveToFinished: (selection: BaseRow[], clear: () => void) => void
}

export function buildFailedActions(d: FailedActionDeps): ActionDef[] {
  const fileGated = !firstRowHasFile(d.selection)
  return [
    ...buildEditDeleteRerecordActions(d),
    {
      id: 'move',
      label: d.moving ? 'Moving…' : 'Move to finished',
      tooltip: 'Mark the selected recording as finished',
      disabled: d.selection.length === 0 || d.moving,
      onClick: () => d.onMoveToFinished(d.selection, d.clearSelection),
    },
    /* Download is single-row only — see the matching note in
     * finishedActions.ts. */
    {
      id: 'download',
      label: 'Download',
      tooltip: 'Download the selected recording',
      disabled: d.selection.length !== 1 || fileGated,
      onClick: () => d.onDownload(d.selection),
    },
  ]
}
