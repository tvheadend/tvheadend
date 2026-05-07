/*
 * DVR Removed toolbar actions — minimal action set, three buttons.
 *
 * Mirrors `tvheadend.dvr_removed` in src/webui/static/app/dvr.js
 * (toolbar at line 1009: `tbar: [rerecordButton]`; gating at
 * 975-978 — count-only). The framework adds Edit (single-row) and
 * Delete (≥1) automatically based on `edit:` and `del: true`. We
 * surface those explicitly to match the pattern Failed and Finished
 * use.
 *
 * Final order: Edit, Delete, Re-record. No Download (file is gone),
 * no Move (no destination), no Abort (recording is over). The
 * tab itself is gated to expert users only — see DvrLayout.vue.
 *
 * No file-existence predicate is needed: every action gates on
 * selection count alone.
 */
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { buildEditDeleteRerecordActions } from './dvrToolbarHelpers'

export interface RemovedActionDeps {
  selection: BaseRow[]
  clearSelection: () => void
  deleting: boolean
  rerecording: boolean
  onEdit: (selection: BaseRow[]) => void
  onDelete: (selection: BaseRow[], clear: () => void) => void
  onRerecord: (selection: BaseRow[], clear: () => void) => void
}

export function buildRemovedActions(d: RemovedActionDeps): ActionDef[] {
  return buildEditDeleteRerecordActions(d)
}
