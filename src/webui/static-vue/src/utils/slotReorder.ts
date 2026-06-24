// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Slot-based row reorder + bulk renumber utilities for the
 * Channel Manage drawer.
 *
 * Three exports:
 *   - `reorderRowsBySlot`        — drag-drop reorder, optionally
 *                                  preserving each row's minor
 *                                  (.N suffix) across slots.
 *   - `renumberAsIntegers`       — bulk: flatten all visible rows
 *                                  to sequential integers.
 *   - `renumberPreservingMinors` — bulk: sequential majors that
 *                                  keep each row's original minor
 *                                  AND group contiguous-major
 *                                  runs as a single sub-channel
 *                                  cluster.
 *
 * Channel numbers are stored as floats on the wire: integer N or
 * dotted N.M (e.g. 5.1 for the second feed of major 5). The
 * helpers below parse them via string ops to avoid IEEE-754
 * imprecision (5.1 → "5" + ".1" → Number("7.1") not 7 + 0.1).
 */

export interface SlotReorderRow {
  /* Optional uuid: matches `BaseRow`'s shape so the algorithm can
   * take the grid's row arrays directly. Rows without a uuid are
   * skipped — they can't be addressed by the commit callback. */
  uuid?: string
  [field: string]: unknown
}

export interface ReorderOptions<Row extends SlotReorderRow = SlotReorderRow> {
  /* When true, each ROW's original minor (.N) follows it across
   * the reorder — dragging row "5.1" to a new slot whose major
   * is 7 commits the row as 7.1, not 7. Rows that originally had
   * no minor strip any minor from the slot value too. */
  preserveMinor?: boolean
  /* Reads the value a row currently DISPLAYS. Defaults to the raw
   * row field. Callers that overlay unsaved edits on top of the
   * rows (the manage drawer's dirty store) MUST pass the overlay-
   * aware getter: after an unsaved move the display order is
   * driven by the overlay, and computing the next move's slot
   * values from the raw rows would renumber from stale data. */
  getValue?: (row: Row) => unknown
}

/**
 * Apply slot-based renumbering for a row reorder.
 *
 * @param originalRows Row order BEFORE the reorder, in displayed
 *                     sequence (post-filter, post-sort).
 * @param newRows      Row order AFTER the reorder, same length.
 * @param field        Field name on each row carrying the value to
 *                     shuffle (typically `'number'`).
 * @param commit       Callback fired once per row whose value
 *                     actually changes. Typically wired to
 *                     `useInlineEdit.commitCell`.
 * @param options      Optional behavior tweaks. See
 *                     `ReorderOptions`.
 */
export function reorderRowsBySlot<Row extends SlotReorderRow>(
  originalRows: readonly Row[],
  newRows: readonly Row[],
  field: string,
  commit: (uuid: string, field: string, value: unknown) => void,
  options: ReorderOptions<Row> = {},
): void {
  if (originalRows.length !== newRows.length) return
  const getValue = options.getValue ?? ((r: Row) => r[field])
  const originalNumbers = originalRows.map((r) => getValue(r))
  /* Lookup: uuid → row's pre-reorder display value. Only built for
   * the preserveMinor path because the plain slot-swap doesn't
   * need row identity beyond position. */
  const rowOriginalValue = options.preserveMinor
    ? new Map(originalRows.map((r) => [r.uuid, getValue(r)] as const))
    : null
  for (let i = 0; i < newRows.length; i++) {
    const nextUuid = newRows[i].uuid
    if (!nextUuid) continue /* defensive — uuidless rows can't be saved */
    if (nextUuid !== originalRows[i].uuid) {
      let nextValue: unknown = originalNumbers[i]
      if (options.preserveMinor && rowOriginalValue) {
        const slotMajor = extractChannelMajor(originalNumbers[i])
        const rowOrigMinor = extractChannelMinorString(
          rowOriginalValue.get(nextUuid),
        )
        nextValue = combineChannelNumber(slotMajor, rowOrigMinor)
      }
      commit(nextUuid, field, nextValue)
    }
  }
}

/**
 * Bulk: assign sequential integers 1..N (or `startFrom`..) to
 * each row in display order, ignoring minors entirely. The
 * "clean slate" button.
 *
 * @param rows      Rows in current display order.
 * @param field     Field to write (typically `'number'`).
 * @param startFrom First integer to assign (typically 1).
 * @param commit    Per-row commit callback.
 */
export function renumberAsIntegers<Row extends SlotReorderRow>(
  rows: readonly Row[],
  field: string,
  startFrom: number,
  commit: (uuid: string, field: string, value: unknown) => void,
): void {
  /* `assigned` advances per addressable row only — uuidless rows
   * skip the index so the next legit row gets the next integer,
   * not a gap. */
  let assigned = startFrom
  for (const row of rows) {
    if (!row.uuid) continue
    commit(row.uuid, field, assigned)
    assigned++
  }
}

/**
 * Bulk: assign sequential majors `startFrom`..N, preserving
 * each row's original minor AND grouping contiguous-original-
 * major runs as a single sub-channel cluster.
 *
 * Example with rows in display order
 *   [3, 5, 5.1, 7, 10, 10.1, 10.2]
 * (startFrom = 1) produces
 *   [1, 2, 2.1, 3, 4, 4.1, 4.2].
 *
 * Unnumbered rows (major == 0) are treated as distinct cells —
 * three contiguous `0`-rows become 1, 2, 3, not all "1".
 */
export function renumberPreservingMinors<Row extends SlotReorderRow>(
  rows: readonly Row[],
  field: string,
  startFrom: number,
  commit: (uuid: string, field: string, value: unknown) => void,
): void {
  let newMajor = startFrom - 1
  let prevOrigMajor: number | null = null
  let prevWasUnnumbered = false
  for (const row of rows) {
    if (!row.uuid) continue
    const v = row[field]
    const origMajor = extractChannelMajor(v)
    const origMinor = extractChannelMinorString(v)
    const isUnnumbered = origMajor === 0
    /* Group rule: a NEW group starts when (a) this is the first
     * row, or (b) the row is unnumbered (each unnumbered cell is
     * its own group), or (c) the previous row was unnumbered, or
     * (d) the original major changed. */
    const startsNewGroup =
      prevOrigMajor === null ||
      isUnnumbered ||
      prevWasUnnumbered ||
      origMajor !== prevOrigMajor
    if (startsNewGroup) newMajor++
    const newValue = combineChannelNumber(newMajor, origMinor)
    commit(row.uuid, field, newValue)
    prevOrigMajor = origMajor
    prevWasUnnumbered = isUnnumbered
  }
}

/* --- Channel-number string helpers ----------------------------
 *
 * Channel numbers come in as JS Number (e.g. 5.1) or as strings
 * (the dirty store passes whatever was committed). Direct float
 * math is lossy — 5 + 0.1 == 5.1 happens to work but 7 + 0.2
 * yields 7.199999…. Stringify, split on the dot, recompose.
 */

/** Extract integer major part. 0 / null / NaN / object → 0. */
export function extractChannelMajor(v: unknown): number {
  if (typeof v !== 'string' && typeof v !== 'number') return 0
  const str = String(v)
  if (str === '') return 0
  const dot = str.indexOf('.')
  const head = dot < 0 ? str : str.slice(0, dot)
  const n = Number(head)
  return Number.isFinite(n) ? Math.trunc(n) : 0
}

/** Extract the dotted minor portion including the leading dot
 * (".1", ".25", "") for clean re-attachment. Empty string when
 * the number has no minor. */
export function extractChannelMinorString(v: unknown): string {
  if (typeof v !== 'string' && typeof v !== 'number') return ''
  const str = String(v)
  const dot = str.indexOf('.')
  return dot < 0 ? '' : str.slice(dot)
}

/** Combine a major + dotted-minor-string back into a Number.
 * `combineChannelNumber(7, ".1")` → 7.1.
 * `combineChannelNumber(0, "")`   → 0. */
export function combineChannelNumber(major: number, minorStr: string): number {
  const n = Number(`${major}${minorStr}`)
  return Number.isFinite(n) ? n : major
}
