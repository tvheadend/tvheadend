// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DVR Finished toolbar actions — builder split out from FinishedView.vue
 * so the gating logic is unit-testable without mounting the view (and
 * without mocking IdnodeGrid / IdnodeEditor). The view passes the live
 * selection + reactive in-flight flags + per-action callbacks; the
 * builder returns a flat ActionDef[] with current-render disabled
 * states.
 *
 * Mirrors `tvheadend.dvr_finished` in src/webui/static/app/dvr.js
 * (toolbar at line 825, gating at line 759-766). Order matches ExtJS
 * with one swap — Edit comes first to match the order Upcoming uses;
 * ExtJS doesn't surface Edit in the tbar because the framework
 * auto-adds it. Edit's enable rule mirrors Upcoming's
 * (`dvrToolbarHelpers.ts`): `selection.length === 0` is the only
 * disable condition, so multi-row Edit is supported (IdnodeEditor
 * accepts a uuid array and applies changes across all rows).
 */
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { firstRowHasFile } from './predicates'
import { t } from '@/composables/useI18n'

export interface FinishedActionDeps {
  selection: BaseRow[]
  clearSelection: () => void
  removing: boolean
  rerecording: boolean
  moving: boolean
  onEdit: (selection: BaseRow[]) => void
  onRemove: (selection: BaseRow[], clear: () => void) => void
  onDownload: (selection: BaseRow[]) => void
  onRerecord: (selection: BaseRow[], clear: () => void) => void
  onMoveToFailed: (selection: BaseRow[], clear: () => void) => void
}

/* Play moved from the toolbar to a per-row inline icon column
 * (see FinishedView.vue's column array — synthetic `_play`
 * field with cellComponent: PlayCell). The filesize > 0 gate
 * the toolbar Play used now lives on the cell's playEnabled
 * predicate. */
export function buildFinishedActions(d: FinishedActionDeps): ActionDef[] {
  const fileGated = !firstRowHasFile(d.selection)
  return [
    {
      id: 'edit',
      label: t('Edit'),
      tooltip: t('Edit the selected entry'),
      disabled: d.selection.length === 0,
      onClick: () => d.onEdit(d.selection),
    },
    {
      id: 'remove',
      label: d.removing ? t('Removing…') : t('Remove'),
      tooltip: t('Remove the selected recording from storage'),
      disabled: d.selection.length === 0 || d.removing || fileGated,
      onClick: () => d.onRemove(d.selection, d.clearSelection),
    },
    /* Download is single-row only: the handler reads `selected[0]`
     * and constructs `/dvrfile/<uuid>` for one file. Allowing a
     * multi-row selection while only ever downloading the first
     * row would silently drop the rest — the legacy ExtJS UI does
     * exactly that (`dvr.js:668-674` + `:759-766`). */
    {
      id: 'download',
      label: t('Download'),
      tooltip: t('Download the selected recording'),
      disabled: d.selection.length !== 1 || fileGated,
      onClick: () => d.onDownload(d.selection),
    },
    {
      id: 'rerecord',
      label: d.rerecording ? t('Toggling…') : t('Re-record'),
      tooltip: t('Toggle re-record functionality'),
      disabled: d.selection.length === 0 || d.rerecording || fileGated,
      onClick: () => d.onRerecord(d.selection, d.clearSelection),
    },
    {
      id: 'move',
      label: d.moving ? t('Moving…') : t('Move to failed'),
      tooltip: t('Mark the selected recording as failed'),
      disabled: d.selection.length === 0 || d.moving || fileGated,
      onClick: () => d.onMoveToFailed(d.selection, d.clearSelection),
    },
  ]
}
