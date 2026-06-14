// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Format the summary line shown in the DataGrid's list-header
 * strip (above the table on desktop / above the card list on
 * phone). The strip is the single source-of-truth for "how many
 * rows are loaded?" and "how many are selected?" — what was
 * previously split between the toolbar count chip and the
 * in-header selection pill.
 *
 * Output shape (in priority order):
 *   - selected > 0, all visible selected   → `All N {label} selected`
 *   - selected > 0, partial                → `M of N selected`
 *   - selected = 0, total > entries (filt) → `M / N {label}`
 *   - selected = 0, no filter              → `N {label}`
 *
 * The selection-state messages intentionally drop the label —
 * "All 147 recordings selected" reads cleanly, but "1 of 147
 * recordings selected" gets verbose. Mirrors the pre-refactor
 * phone-list-summary shape verbatim so existing eyeballs see
 * the same words.
 *
 * `total` is the server-side total when known (paginator-off
 * mode loads all rows, so `total === entries` in steady state;
 * `total > entries` only when a filter narrowed the visible
 * subset out of a larger loaded set). Pass `undefined` when the
 * caller has no separate total to surface — the function then
 * never renders the `M / N` split form. */
export interface GridSummaryInput {
  /** Number of rows currently in `entries` (visible / loaded). */
  entries: number
  /** Server-side total. Undefined collapses to entries. */
  total?: number
  /** Number of selected rows. */
  selected: number
  /** True when every visible row is selected. */
  allVisibleSelected: boolean
  /** Singular noun describing the row type. Default 'entries'. */
  label?: string
}

export function summaryText(input: GridSummaryInput): string {
  const label = input.label ?? 'entries'
  if (input.selected > 0) {
    if (input.allVisibleSelected) {
      return `All ${input.entries} ${label} selected`
    }
    return `${input.selected} of ${input.entries} selected`
  }
  if (input.total !== undefined && input.total > input.entries) {
    return `${input.entries} / ${input.total} ${label}`
  }
  return `${input.entries} ${label}`
}
