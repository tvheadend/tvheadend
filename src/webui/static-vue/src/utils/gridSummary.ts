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
 *   - selected > 0, all visible selected   → `All selected: N`
 *   - selected > 0, partial                → `M of N selected`
 *   - selected = 0, total > entries (filt) → `{Label}: M / N`
 *   - selected = 0, no filter              → `{Label}: N`
 *
 * The `{Label}` forms leave the noun uninflected by the count. The
 * label is a plural noun ("recordings") chosen by the caller, so
 * "N {label}" would read "1 recordings" for a single row, while
 * "Recordings: 1" is right for any count and any language. The
 * selection forms are whole messages, and `M of N selected` takes
 * the plural form for M.
 *
 * `total` is the server-side total when known (paginator-off
 * mode loads all rows, so `total === entries` in steady state;
 * `total > entries` only when a filter narrowed the visible
 * subset out of a larger loaded set). Pass `undefined` when the
 * caller has no separate total to surface — the function then
 * never renders the `M / N` split form. */
import { t, tn } from '@/composables/useI18n'

export interface GridSummaryInput {
  /** Number of rows currently in `entries` (visible / loaded). */
  entries: number
  /** Server-side total. Undefined collapses to entries. */
  total?: number
  /** Number of selected rows. */
  selected: number
  /** True when every visible row is selected. */
  allVisibleSelected: boolean
  /** Plural noun describing the row type. Default 'entries'. */
  label?: string
}

/* Upper-case the first letter only, so "IP blocks" keeps its
 * acronym and a translated label keeps the rest of its casing. */
function capitalise(label: string): string {
  return label.charAt(0).toLocaleUpperCase() + label.slice(1)
}

export function summaryText(input: GridSummaryInput): string {
  if (input.selected > 0) {
    if (input.allVisibleSelected) {
      return t('All selected: {0}', input.entries)
    }
    return tn('{0} of {1} selected', '{0} of {1} selected', input.selected, input.selected, input.entries)
  }
  const count =
    input.total !== undefined && input.total > input.entries
      ? `${input.entries} / ${input.total}`
      : `${input.entries}`
  const label = input.label ?? t('entries')
  return label ? `${capitalise(label)}: ${count}` : count
}
