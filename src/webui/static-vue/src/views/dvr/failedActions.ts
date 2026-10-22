// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DVR Failed toolbar actions — builder split out from FailedView.vue
 * so the gating logic is unit-testable without mounting the view.
 *
 * Five actions:
 *
 *   - Edit (single-row, drawer-edit via the limited Failed edit list)
 *   - Delete (≥1, api/idnode/delete with a two-line confirmation)
 *   - Re-record (≥1, api/dvr/entry/rerecord/toggle — count-only gate)
 *   - Move to finished (≥1, api/dvr/entry/move/finished — count-only)
 *   - Download (1-row, file>0, /dvrfile/<uuid> in a new tab)
 *
 * Critical difference from Finished: Re-record and Move-to-finished
 * are gated on selection count alone. Only Download requires the
 * first row's filesize > 0 (the same `firstRowHasFile` predicate
 * Finished uses).
 */
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { firstRowHasFile } from './predicates'
import { buildEditDeleteRerecordActions } from './dvrToolbarHelpers'
import { t } from '@/composables/useI18n'

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
      label: d.moving ? t('Moving…') : t('Move to finished'),
      tooltip: t('Mark the selected recording as finished'),
      disabled: d.selection.length === 0 || d.moving,
      onClick: () => d.onMoveToFinished(d.selection, d.clearSelection),
    },
    /* Download is single-row only — see the matching note in
     * finishedActions.ts. */
    {
      id: 'download',
      label: t('Download'),
      tooltip: t('Download the selected recording'),
      disabled: d.selection.length !== 1 || fileGated,
      onClick: () => d.onDownload(d.selection),
    },
  ]
}
