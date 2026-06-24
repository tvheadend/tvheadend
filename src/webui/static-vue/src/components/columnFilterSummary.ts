// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Active per-column filter summarisation.
 *
 * Consumed by GridSettingsMenu's PER COLUMN sub-block (inside the
 * Filters section) so the view-options popover surfaces the
 * column funnels the user has currently set without making them
 * scroll the table to find each header's funnel-icon highlight.
 *
 * Pure helpers — no Vue, no DOM. Inputs: the canonical
 * `FilterDef[]` shape IdnodeGrid keeps on `store.filter`, plus
 * the per-field display label map the parent already builds for
 * the Columns section. Outputs: one display row per
 * field-that-has-active-filters with a humanised summary string
 * + the field key (used by the ✕ clear emit).
 *
 * String values get quoted; numeric operators get a glyph
 * (`= 5`, `≥ 100`, `≤ 50`, `5 – 10` for a min+max pair); boolean
 * resolves to a localised Yes/No via a caller-supplied formatter
 * so this file stays locale-agnostic.
 */

import type { FilterDef } from '@/types/grid'

export interface ColumnFilterRow {
  /** Field key — passed back in the clear-filter / edit-filter emit. */
  field: string
  /** Display label sourced from the parent's column-label map. */
  label: string
  /** Humanised filter value (e.g. `"news"`, `≥ 100`, `5 – 10`, `Yes`). */
  summary: string
  /**
   * True when the filtered column isn't currently visible in the grid
   * (user-hidden or level-locked). The popover renders such a row
   * non-interactive — there's no column header to open a funnel
   * against — but still offers the ✕ to clear the filter.
   */
  hidden: boolean
}

/* Localised Yes / No labels passed by the caller — keeps this
 * module free of any i18n binding. */
export interface ColumnFilterLabels {
  yes: string
  no: string
  /* Optional per-field enum-key→label resolver. When a numeric
   * `eq` FilterDef sits on a field whose column declared
   * `filterType: 'enum'`, the caller resolves the key to its
   * human label so the chip renders `= Important` instead of
   * `= 0`. Returns null when the key isn't known to the
   * resolver (deferred options still loading, stale value
   * after the enum set shrank server-side) — falls back to
   * the raw `= <num>` format. */
  resolveEnumLabel?: (field: string, value: number | string) => string | null
  /* Optional predicate: is this field's column currently hidden from
   * the grid (user-hidden or level-locked)? Drives
   * `ColumnFilterRow.hidden`. Absent → every row is treated as
   * visible. */
  isFieldHidden?: (field: string) => boolean
}

/*
 * Group FilterDef entries by their `field`. Numeric Between is
 * encoded as TWO entries on the same field (one `gt:min`, one
 * `lt:max` — see `IdnodeGrid`'s `numericModelToEntries`), so the
 * summary needs them together to render `min – max` instead of
 * two separate rows.
 */
export function groupFiltersByField(
  filters: readonly FilterDef[],
): Map<string, FilterDef[]> {
  const out = new Map<string, FilterDef[]>()
  for (const f of filters) {
    const arr = out.get(f.field)
    if (arr) arr.push(f)
    else out.set(f.field, [f])
  }
  return out
}

/*
 * Render a single field's worth of FilterDef entries as the
 * summary string shown in the popover row. Returns the empty
 * string when there's nothing to show — caller should skip the
 * row in that case.
 *
 *   string                → `"value"`
 *   boolean               → caller-supplied Yes/No
 *   numeric eq            → `= value`
 *   numeric gt            → `≥ value` (server's `gt` is inclusive — bug
 *                           tracked upstream; until then label what it
 *                           actually does)
 *   numeric lt            → `≤ value` (same inclusive-bug caveat)
 *   numeric gt + lt pair  → `min – max` (Between)
 *   anything else         → empty
 */
export function summarizeFilterEntries(
  entries: readonly FilterDef[],
  labels: ColumnFilterLabels,
): string {
  if (entries.length === 0) return ''
  if (entries.length === 2) return formatBetween(entries) ?? ''
  if (entries.length === 1) return formatSingleEntry(entries[0], labels)
  return ''
}

/*
 * Numeric Between: two entries on the same field, one `gt`
 * (lower bound) + one `lt` (upper bound). Returns null when the
 * pair doesn't fit that shape (e.g. two `gt` entries) so the
 * caller can fall through to "empty" rather than rendering
 * something misleading.
 */
function formatBetween(entries: readonly FilterDef[]): string | null {
  if (!entries.every((e) => e.type === 'numeric')) return null
  const gt = entries.find((e) => e.comparison === 'gt')
  const lt = entries.find((e) => e.comparison === 'lt')
  if (!gt || !lt) return null
  return `${formatNumeric(gt.value)} – ${formatNumeric(lt.value)}`
}

/*
 * Single FilterDef → display string. String → quoted, boolean →
 * caller-supplied Yes/No, numeric → operator glyph + value.
 * Unknown comparisons (defensive) render empty.
 */
function formatSingleEntry(e: FilterDef, labels: ColumnFilterLabels): string {
  if (e.type === 'string') return `"${e.value}"`
  if (e.type === 'boolean') return e.value ? labels.yes : labels.no
  if (e.type === 'numeric') return formatNumericEntry(e, labels)
  return ''
}

function formatNumericEntry(e: FilterDef, labels: ColumnFilterLabels): string {
  const cmp = e.comparison ?? 'eq'
  if (cmp === 'eq') {
    /* Enum columns ride on the numeric wire (`type: 'numeric',
     * comparison: 'eq', value: <key>`) but want the human
     * label in the summary chip. Defer to the caller-supplied
     * resolver; fall back to the raw `= <num>` format when the
     * resolver doesn't claim the field. */
    if (typeof e.value === 'number' || typeof e.value === 'string') {
      const enumLabel = labels.resolveEnumLabel?.(e.field, e.value)
      if (enumLabel) return `= ${enumLabel}`
    }
    return `= ${formatNumeric(e.value)}`
  }
  if (cmp === 'gt') return `≥ ${formatNumeric(e.value)}`
  if (cmp === 'lt') return `≤ ${formatNumeric(e.value)}`
  return ''
}

function formatNumeric(v: string | number | boolean): string {
  if (typeof v === 'number') return String(v)
  if (typeof v === 'string') return v
  return ''
}

/*
 * Top-level transformer: take the canonical FilterDef[] +
 * field→label map + Yes/No labels, return the rows the popover
 * renders. Skips fields with an empty summary (defensive: e.g. a
 * lone `null`-valued numeric entry that the input shouldn't
 * contain but might).
 */
export function buildActiveColumnFilterRows(
  filters: readonly FilterDef[],
  fieldLabels: Record<string, string>,
  labels: ColumnFilterLabels,
): ColumnFilterRow[] {
  const grouped = groupFiltersByField(filters)
  const out: ColumnFilterRow[] = []
  for (const [field, entries] of grouped) {
    const summary = summarizeFilterEntries(entries, labels)
    if (!summary) continue
    out.push({
      field,
      label: fieldLabels[field] ?? field,
      summary,
      hidden: labels.isFieldHidden?.(field) ?? false,
    })
  }
  return out
}
