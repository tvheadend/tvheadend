// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * In-memory sort for the EPG Table view, extracted from
 * TableView.vue so the wire-shape handling is unit-testable.
 *
 * The Table view owns its sort (DataTable runs in :lazy mode).
 * Most columns use the generic three-way comparator below; a
 * column can override it with a direction-agnostic
 * ColumnDef.sortComparator (e.g. channelNumber, whose
 * "major.minor" string wire shape needs numeric ordering).
 */

/* Three-way comparator for the in-memory sort. Splits the
 * value-shape branching out of the comparator hot path so the
 * outer pipeline stays simple. Stable: callers `sort` on a
 * fresh array spread; equal-key rows preserve the composable's
 * start-ASC order. */
export function compareSortValues(av: unknown, bv: unknown, dir: number): number {
  if (av == null && bv == null) return 0
  if (av == null) return -dir
  if (bv == null) return dir
  if (typeof av === 'number' && typeof bv === 'number') return (av - bv) * dir
  return String(av).localeCompare(String(bv)) * dir
}

/* Apply the user's current in-memory sort. Skip the spread+sort
 * when the data is already start-ASC (the composable's own
 * ordering) — saves an N-element copy + sort during the
 * initial-load reactive cascade. `comparator` is the active
 * column's direction-agnostic ColumnDef.sortComparator, when set. */
export function applyInMemorySort<T>(
  rows: readonly T[],
  key: string | null,
  order: number,
  comparator?: (a: unknown, b: unknown) => number,
): { rows: readonly T[]; mutated: boolean } {
  if (!key) return { rows, mutated: false }
  if (key === 'start' && order === 1) return { rows, mutated: false }
  const sorted = [...rows].sort((a, b) => {
    const av = (a as unknown as Record<string, unknown>)[key]
    const bv = (b as unknown as Record<string, unknown>)[key]
    return comparator ? comparator(av, bv) * order : compareSortValues(av, bv, order)
  })
  return { rows: sorted, mutated: true }
}
