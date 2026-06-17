// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import type { BaseRow } from '@/types/grid'

/* Pure helpers for any idnode-grid view that wires Move Up /
 * Move Down buttons against `idnode/moveup` / `idnode/movedown`.
 * Shared by Configuration → Users → Access Entries
 * (`acleditor.js`) and the six Configuration → Stream ES-filter
 * grids (`esfilter.js`). The host view wires fetch +
 * scroll-into-view around these — these helpers do nothing IO. */

export type MoveDirection = 'up' | 'down'

/* Sort the selection's UUIDs by their position in the loaded
 * entries array, ascending for 'up' and descending for 'down'.
 *
 * The server's `idnode_moveup` / `idnode_movedown` callbacks
 * each swap a row with its immediate sibling. Sending two
 * adjacent rows in their natural order produces a swap-then-
 * unswap cascade — the second move undoes the first. Sorting
 * the list so each processed row finds a non-selected sibling
 * fixes that. ExtJS has the bug; this client-side sort avoids
 * it. */
export function sortUuidsForMove(
  selection: BaseRow[],
  entries: BaseRow[],
  direction: MoveDirection
): string[] {
  const positionByUuid = new Map<string, number>()
  entries.forEach((row, idx) => {
    if (typeof row.uuid === 'string' && row.uuid) positionByUuid.set(row.uuid, idx)
  })
  return selection
    .map((r) => r.uuid)
    .filter((u): u is string => typeof u === 'string' && !!u)
    .sort((a, b) => {
      const pa = positionByUuid.get(a) ?? 0
      const pb = positionByUuid.get(b) ?? 0
      return direction === 'up' ? pa - pb : pb - pa
    })
}

/* Boundary-disable predicate. Up disabled when any selected row
 * is already at position 0; Down disabled when any selected row
 * is at the last position. Mirrors the
 * `<IdnodeFieldEnumMultiOrdered>` boundary-disable convention.
 *
 * Returns true when the move IS allowed (i.e. no selected row
 * sits at the relevant boundary). */
export function canMoveSelection(
  selection: BaseRow[],
  entries: BaseRow[],
  direction: MoveDirection
): boolean {
  if (selection.length === 0) return false
  if (entries.length === 0) return true
  const boundaryUuid =
    direction === 'up' ? entries[0]?.uuid : entries[entries.length - 1]?.uuid
  if (typeof boundaryUuid !== 'string' || !boundaryUuid) return true
  return !selection.some((r) => r.uuid === boundaryUuid)
}

/* Pick which UUID to scroll into view after a successful move.
 * After moveup, the topmost moved row is the one most likely to
 * scroll off the top of the viewport — choose it so the user
 * sees the move's leading edge. Symmetric for movedown — choose
 * the bottommost moved row. The sorted list from
 * `sortUuidsForMove` already has the right element at index 0
 * (ascending for up = topmost, descending for down = bottommost). */
export function pickScrollUuid(sortedUuids: string[]): string | null {
  return sortedUuids[0] ?? null
}

/* Find a row's index in the loaded entries by uuid. Returns
 * null when not found (e.g. the row was deleted server-side
 * between the action and the refetch). */
export function findRowIndexByUuid(uuid: string, entries: BaseRow[]): number | null {
  const idx = entries.findIndex((e) => e.uuid === uuid)
  return idx >= 0 ? idx : null
}
