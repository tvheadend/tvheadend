/*
 * Shared selection predicates for DVR sub-tab toolbars. Mirrors the
 * gating logic in src/webui/static/app/dvr.js verbatim so the Vue
 * and ExtJS UIs feel identical to a tester.
 */
import type { BaseRow } from '@/types/grid'

/*
 * "First selected row has a non-zero filesize" — gates the
 * file-touching actions: Download, Remove, Re-record,
 * Move-to-failed (Finished tab); Download (Failed tab).
 *
 * ExtJS's `selected` callback at dvr.js:759-766 inspects ONLY
 * `r[0].data.filesize` — not all selected rows. We mirror that
 * for the multi-row actions (Remove / Re-record / Move-to-failed)
 * because their server endpoints take a UUID array and skip
 * entries without files server-side, so a multi-selection with
 * the first row having a file is acceptable.
 *
 * Download is gated more strictly at the call site (exactly 1
 * selected row, not just ≥1) because the handler only ever
 * downloads `selected[0]` — the legacy ExtJS UI accepts
 * multi-select on Download but silently drops every row past the
 * first; that's a Classic-UI bug tracked in BACKLOG.
 */
export function firstRowHasFile(selection: BaseRow[]): boolean {
  if (selection.length === 0) return false
  const fs = selection[0].filesize
  return typeof fs === 'number' && fs > 0
}
