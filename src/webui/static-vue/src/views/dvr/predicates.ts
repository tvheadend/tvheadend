// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Shared selection predicates for DVR sub-tab toolbars.
 */
import type { BaseRow } from '@/types/grid'

/*
 * "First selected row has a non-zero filesize" — gates the
 * file-touching actions: Download, Remove, Re-record,
 * Move-to-failed (Finished tab); Download (Failed tab).
 *
 * Inspects ONLY the first selected row's `filesize` — not all
 * selected rows — for the multi-row actions (Remove / Re-record /
 * Move-to-failed) because their server endpoints take a UUID array
 * and skip entries without files server-side, so a multi-selection
 * with the first row having a file is acceptable.
 *
 * Download is gated more strictly at the call site (exactly 1
 * selected row, not just ≥1) because the handler only ever
 * downloads `selected[0]`; accepting a multi-select would silently
 * drop every row past the first.
 */
export function firstRowHasFile(selection: BaseRow[]): boolean {
  if (selection.length === 0) return false
  const fs = selection[0].filesize
  return typeof fs === 'number' && fs > 0
}
