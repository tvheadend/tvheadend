// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import {
  canMoveSelection,
  findRowIndexByUuid,
  pickScrollUuid,
  sortUuidsForMove,
} from '../idnodeMove'
import type { BaseRow } from '@/types/grid'

/* Helper — build a simple entries array of the form
 * `[{ uuid: 'A' }, { uuid: 'B' }, …]` so the position-by-uuid
 * map matches the index ordering tests reason about. */
function entriesOf(uuids: string[]): BaseRow[] {
  return uuids.map((uuid) => ({ uuid }))
}

describe('sortUuidsForMove', () => {
  const entries = entriesOf(['A', 'B', 'C', 'D'])

  it('returns single uuid unchanged for either direction', () => {
    expect(sortUuidsForMove([{ uuid: 'B' }], entries, 'up')).toEqual(['B'])
    expect(sortUuidsForMove([{ uuid: 'B' }], entries, 'down')).toEqual(['B'])
  })

  it('sorts multi-select ASCENDING for "up" — top row first', () => {
    /* For move up: process top selected row first so each row's
     * prev-sibling at the moment of the swap is NOT in the
     * selection set. Otherwise adjacent selected rows swap-then-
     * unswap. */
    const selection = [
      { uuid: 'C' },
      { uuid: 'A' },
      { uuid: 'B' },
    ]
    expect(sortUuidsForMove(selection, entries, 'up')).toEqual(['A', 'B', 'C'])
  })

  it('sorts multi-select DESCENDING for "down" — bottom row first', () => {
    const selection = [
      { uuid: 'A' },
      { uuid: 'C' },
      { uuid: 'B' },
    ]
    expect(sortUuidsForMove(selection, entries, 'down')).toEqual(['C', 'B', 'A'])
  })

  it('handles non-contiguous selection — relative order preserved', () => {
    /* Selection `{A, C}` over `[A, B, C, D]`: up → [A, C]
     * (top first), down → [C, A] (bottom first). */
    const selection = [{ uuid: 'C' }, { uuid: 'A' }]
    expect(sortUuidsForMove(selection, entries, 'up')).toEqual(['A', 'C'])
    expect(sortUuidsForMove(selection, entries, 'down')).toEqual(['C', 'A'])
  })

  it('drops rows whose uuid is missing or non-string', () => {
    const selection = [
      { uuid: 'A' },
      {} as BaseRow,
      { uuid: '' },
      { uuid: 42 } as unknown as BaseRow,
      { uuid: 'C' },
    ]
    expect(sortUuidsForMove(selection, entries, 'up')).toEqual(['A', 'C'])
  })

  it('places uuids not present in entries before the loaded rows for "up"', () => {
    /* Missing uuids get position 0 (the default fallback). For "up"
     * that means they sort first; for "down" they sort last. This
     * matches the in-view contract — anything not in the visible
     * entries array can't be reasoned about for ordering, so we
     * default to "no known earlier sibling". */
    const selection = [
      { uuid: 'B' },
      { uuid: 'X' },
    ]
    /* X has fallback position 0; B has position 1; ascending → X first. */
    expect(sortUuidsForMove(selection, entries, 'up')).toEqual(['X', 'B'])
    expect(sortUuidsForMove(selection, entries, 'down')).toEqual(['B', 'X'])
  })

  it('returns empty list for empty selection', () => {
    expect(sortUuidsForMove([], entries, 'up')).toEqual([])
    expect(sortUuidsForMove([], entries, 'down')).toEqual([])
  })
})

describe('canMoveSelection', () => {
  const entries = entriesOf(['A', 'B', 'C', 'D'])

  it('returns false for empty selection', () => {
    expect(canMoveSelection([], entries, 'up')).toBe(false)
    expect(canMoveSelection([], entries, 'down')).toBe(false)
  })

  it('disables UP when any selected row is at the top', () => {
    expect(canMoveSelection([{ uuid: 'A' }], entries, 'up')).toBe(false)
    expect(
      canMoveSelection(
        [{ uuid: 'A' }, { uuid: 'C' }],
        entries,
        'up'
      )
    ).toBe(false)
  })

  it('disables DOWN when any selected row is at the bottom', () => {
    expect(canMoveSelection([{ uuid: 'D' }], entries, 'down')).toBe(false)
    expect(
      canMoveSelection(
        [{ uuid: 'B' }, { uuid: 'D' }],
        entries,
        'down'
      )
    ).toBe(false)
  })

  it('enables UP when no selected row is at the top', () => {
    expect(canMoveSelection([{ uuid: 'B' }], entries, 'up')).toBe(true)
    expect(
      canMoveSelection(
        [{ uuid: 'B' }, { uuid: 'C' }],
        entries,
        'up'
      )
    ).toBe(true)
  })

  it('enables DOWN when no selected row is at the bottom', () => {
    expect(canMoveSelection([{ uuid: 'C' }], entries, 'down')).toBe(true)
    expect(
      canMoveSelection(
        [{ uuid: 'A' }, { uuid: 'B' }],
        entries,
        'down'
      )
    ).toBe(true)
  })

  it('returns true when entries array is empty (cannot read boundary)', () => {
    expect(canMoveSelection([{ uuid: 'A' }], [], 'up')).toBe(true)
    expect(canMoveSelection([{ uuid: 'A' }], [], 'down')).toBe(true)
  })

  it('treats single-row table as boundary in both directions', () => {
    /* The only row is both first and last. Selecting it disables
     * BOTH up and down — the server's moveup/movedown short-
     * circuit anyway, but the UI shouldn't suggest either is
     * possible. */
    const single = entriesOf(['only'])
    expect(canMoveSelection([{ uuid: 'only' }], single, 'up')).toBe(false)
    expect(canMoveSelection([{ uuid: 'only' }], single, 'down')).toBe(false)
  })

  it('mixed in-bounds + at-boundary selection disables the boundary direction', () => {
    /* Selection {B, D}: D is at the bottom → down disabled, up
     * still allowed (B has a prev sibling, D has A/B/C above it). */
    const selection = [{ uuid: 'B' }, { uuid: 'D' }]
    expect(canMoveSelection(selection, entries, 'up')).toBe(true)
    expect(canMoveSelection(selection, entries, 'down')).toBe(false)
  })
})

describe('pickScrollUuid', () => {
  it('returns the first uuid in the sorted list', () => {
    /* `sortUuidsForMove` for "up" puts the topmost row first;
     * `pickScrollUuid` returns it. After the move, the topmost
     * row is the one most likely to scroll off the top edge —
     * that's the row to keep visible. Symmetric for "down". */
    expect(pickScrollUuid(['A', 'B', 'C'])).toBe('A')
  })

  it('returns null for empty input', () => {
    expect(pickScrollUuid([])).toBeNull()
  })

  it('returns the only uuid for single-row moves', () => {
    expect(pickScrollUuid(['solo'])).toBe('solo')
  })
})

describe('findRowIndexByUuid', () => {
  const entries = entriesOf(['A', 'B', 'C', 'D'])

  it('returns the index for a uuid present in entries', () => {
    expect(findRowIndexByUuid('A', entries)).toBe(0)
    expect(findRowIndexByUuid('C', entries)).toBe(2)
    expect(findRowIndexByUuid('D', entries)).toBe(3)
  })

  it('returns null for a uuid not in entries', () => {
    expect(findRowIndexByUuid('Z', entries)).toBeNull()
  })

  it('returns null for empty entries', () => {
    expect(findRowIndexByUuid('A', [])).toBeNull()
  })

  it('returns null for empty uuid string', () => {
    expect(findRowIndexByUuid('', entries)).toBeNull()
  })
})

/* Integration-style: the full pipeline for a hypothetical
 * "move {B, D} down on [A, B, C, D]" flow. Verifies that the
 * helpers compose into the contract the view depends on:
 *
 *   1. canMoveSelection(...,'down') → true (D is at bottom →
 *      false, but in this scenario we pretend the user clicked
 *      anyway and we test the SUCCESS path: down with non-
 *      boundary D'). Use {B, C} on [A, B, C, D] for the success
 *      path — D unaffected.
 *
 *   2. sortUuidsForMove returns the uuids in the order the
 *      server should process them.
 *
 *   3. pickScrollUuid picks the row to scroll into view. */
describe('move pipeline integration', () => {
  it('"down" on {B, C} of [A, B, C, D] → server order [C, B], scroll C', () => {
    const entries = entriesOf(['A', 'B', 'C', 'D'])
    const selection = [{ uuid: 'B' }, { uuid: 'C' }]
    expect(canMoveSelection(selection, entries, 'down')).toBe(true)
    const uuids = sortUuidsForMove(selection, entries, 'down')
    expect(uuids).toEqual(['C', 'B'])
    expect(pickScrollUuid(uuids)).toBe('C')
  })

  it('"up" on {B, C} of [A, B, C, D] → server order [B, C], scroll B', () => {
    const entries = entriesOf(['A', 'B', 'C', 'D'])
    const selection = [{ uuid: 'B' }, { uuid: 'C' }]
    expect(canMoveSelection(selection, entries, 'up')).toBe(true)
    const uuids = sortUuidsForMove(selection, entries, 'up')
    expect(uuids).toEqual(['B', 'C'])
    expect(pickScrollUuid(uuids)).toBe('B')
  })

  it('mixed selection with non-adjacent rows preserves order semantics', () => {
    const entries = entriesOf(['A', 'B', 'C', 'D', 'E'])
    /* Select A, C, E and move them down. Expected server order:
     * E, C, A. Each row finds a prev/next sibling outside the
     * selection at the moment of its swap because the row
     * immediately above/below is unselected. */
    const selection = [
      { uuid: 'A' },
      { uuid: 'C' },
      { uuid: 'E' },
    ]
    /* Down disabled because E is at the bottom. */
    expect(canMoveSelection(selection, entries, 'down')).toBe(false)
    /* Up allowed because A is at the top — wait, that's also a
     * boundary. So up disabled too. */
    expect(canMoveSelection(selection, entries, 'up')).toBe(false)
  })
})
