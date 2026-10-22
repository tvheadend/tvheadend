// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DVR Removed toolbar actions — minimal action set, three buttons.
 *
 * Re-record is gated on selection count only. Edit (single-row)
 * and Delete (≥1) are listed explicitly, as on Failed and
 * Finished.
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
