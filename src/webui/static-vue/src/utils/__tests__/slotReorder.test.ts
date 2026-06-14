// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * slotReorder unit tests — the pure number-reassignment logic used
 * by IdnodeGrid's Manage-mode @row-reorder handler. No DOM, no Vue;
 * just an algorithm + a callback.
 */

import { describe, expect, it, vi } from 'vitest'
import {
  combineChannelNumber,
  extractChannelMajor,
  extractChannelMinorString,
  renumberAsIntegers,
  renumberPreservingMinors,
  reorderRowsBySlot,
  type SlotReorderRow,
} from '../slotReorder'

interface NumRow extends SlotReorderRow {
  number: number
}

describe('reorderRowsBySlot', () => {
  it('drag DOWN: source moves to a later slot; intermediates shift up', () => {
    /* (a=10, b=20, c=30, d=40, e=50). Drag a into c's slot.
     * New order: b, c, a, d, e.
     *   slot 0: b ← was a → commit b=10
     *   slot 1: c ← was b → commit c=20
     *   slot 2: a ← was c → commit a=30
     *   slot 3: d unchanged
     *   slot 4: e unchanged
     */
    const original: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'd', number: 40 },
      { uuid: 'e', number: 50 },
    ]
    const next: NumRow[] = [
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'a', number: 10 },
      { uuid: 'd', number: 40 },
      { uuid: 'e', number: 50 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit)

    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('b', 'number', 10)
    expect(commit).toHaveBeenCalledWith('c', 'number', 20)
    expect(commit).toHaveBeenCalledWith('a', 'number', 30)
  })

  it('drag UP: source moves to an earlier slot; intermediates shift down', () => {
    /* (a=10, b=20, c=30, d=40, e=50). Drag e into b's slot.
     * New order: a, e, b, c, d.
     *   slot 0: a unchanged
     *   slot 1: e ← was b → commit e=20
     *   slot 2: b ← was c → commit b=30
     *   slot 3: c ← was d → commit c=40
     *   slot 4: d ← was e → commit d=50
     */
    const original: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'd', number: 40 },
      { uuid: 'e', number: 50 },
    ]
    const next: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'e', number: 50 },
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'd', number: 40 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit)

    expect(commit).toHaveBeenCalledTimes(4)
    expect(commit).toHaveBeenCalledWith('e', 'number', 20)
    expect(commit).toHaveBeenCalledWith('b', 'number', 30)
    expect(commit).toHaveBeenCalledWith('c', 'number', 40)
    expect(commit).toHaveBeenCalledWith('d', 'number', 50)
  })

  it('no-op when the row order is unchanged', () => {
    const rows: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(rows, [...rows], 'number', commit)
    expect(commit).not.toHaveBeenCalled()
  })

  it('preserves sparse / dotted-minor distributions (gaps stay where they were)', () => {
    /* Channel numbers with dotted minors (PT_S64 + CHANNEL_SPLIT
     * stores 1, 2.5, 100, 100.1). Drag d (100.1) into b's slot.
     * Slots stay (1, 2.5, 100, 100.1); only the occupants shift.
     *   slot 0: a unchanged
     *   slot 1: d ← was b → commit d=2.5
     *   slot 2: b ← was c → commit b=100
     *   slot 3: c ← was d → commit c=100.1
     */
    const original: NumRow[] = [
      { uuid: 'a', number: 1 },
      { uuid: 'b', number: 2.5 },
      { uuid: 'c', number: 100 },
      { uuid: 'd', number: 100.1 },
    ]
    const next: NumRow[] = [
      { uuid: 'a', number: 1 },
      { uuid: 'd', number: 100.1 },
      { uuid: 'b', number: 2.5 },
      { uuid: 'c', number: 100 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit)

    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('d', 'number', 2.5)
    expect(commit).toHaveBeenCalledWith('b', 'number', 100)
    expect(commit).toHaveBeenCalledWith('c', 'number', 100.1)
  })

  it('rows outside the [src, dst] window receive no commits', () => {
    /* (a, b, c, d, e). Drag b → c. Only slots 1, 2 change. */
    const original: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'd', number: 40 },
      { uuid: 'e', number: 50 },
    ]
    const next: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'c', number: 30 },
      { uuid: 'b', number: 20 },
      { uuid: 'd', number: 40 },
      { uuid: 'e', number: 50 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit)

    const committedUuids = commit.mock.calls.map((c) => c[0])
    expect(committedUuids).not.toContain('a')
    expect(committedUuids).not.toContain('d')
    expect(committedUuids).not.toContain('e')
    expect(commit).toHaveBeenCalledTimes(2)
  })

  it('honours the custom field name', () => {
    const original = [
      { uuid: 'a', sortKey: 1 },
      { uuid: 'b', sortKey: 2 },
    ] satisfies SlotReorderRow[]
    const next = [
      { uuid: 'b', sortKey: 2 },
      { uuid: 'a', sortKey: 1 },
    ] satisfies SlotReorderRow[]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'sortKey', commit)

    expect(commit).toHaveBeenCalledTimes(2)
    expect(commit).toHaveBeenCalledWith('b', 'sortKey', 1)
    expect(commit).toHaveBeenCalledWith('a', 'sortKey', 2)
  })

  it('returns silently when array lengths disagree (defensive)', () => {
    const a: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
    ]
    const b: NumRow[] = [{ uuid: 'a', number: 10 }]
    const commit = vi.fn()
    reorderRowsBySlot(a, b, 'number', commit)
    expect(commit).not.toHaveBeenCalled()
  })
})

describe('reorderRowsBySlot — preserveMinor option (D1)', () => {
  it('dragged row with a minor (5.1) keeps its .1 when landing on an integer slot', () => {
    /* Original: A=3, B=5, C=5.1, D=7. Drag C from pos 2 to pos 0.
     * New order: C, A, B, D.
     *   slot 0 (orig major 3): C had .1 → 3 + .1 = 3.1
     *   slot 1 (orig major 5): A had no minor → just 5
     *   slot 2 (orig major 5.1 → major 5): B had no minor → 5
     *     (creates duplicate with A — flagged via warning A) */
    const original: NumRow[] = [
      { uuid: 'A', number: 3 },
      { uuid: 'B', number: 5 },
      { uuid: 'C', number: 5.1 },
      { uuid: 'D', number: 7 },
    ]
    const next: NumRow[] = [
      { uuid: 'C', number: 5.1 },
      { uuid: 'A', number: 3 },
      { uuid: 'B', number: 5 },
      { uuid: 'D', number: 7 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit, {
      preserveMinor: true,
    })
    expect(commit).toHaveBeenCalledWith('C', 'number', 3.1)
    expect(commit).toHaveBeenCalledWith('A', 'number', 5)
    expect(commit).toHaveBeenCalledWith('B', 'number', 5)
  })

  it('integer row dragged onto a minor slot strips the minor', () => {
    /* Original: A=3, B=5, C=5.1. Drag A to between C and end.
     * New order: B, C, A.
     *   slot 0 (orig major 5): B unchanged
     *   slot 1 (orig major 5.1 → major 5): C had .1 → 5.1
     *   slot 2 (orig major 5.1 wait... let me recompute):
     *   Actually slots are [3, 5, 5.1]:
     *   slot 0 (was A): B ← orig major 3, B had no minor → 3
     *   slot 1 (was B): C ← orig major 5, C had .1 → 5.1
     *   slot 2 (was C): A ← orig major 5 (from 5.1), A had no minor → 5
     *     (duplicate with the 5 slot 1 emitted, but A landed on slot 2 not 1)
     */
    const original: NumRow[] = [
      { uuid: 'A', number: 3 },
      { uuid: 'B', number: 5 },
      { uuid: 'C', number: 5.1 },
    ]
    const next: NumRow[] = [
      { uuid: 'B', number: 5 },
      { uuid: 'C', number: 5.1 },
      { uuid: 'A', number: 3 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit, {
      preserveMinor: true,
    })
    expect(commit).toHaveBeenCalledWith('B', 'number', 3)
    expect(commit).toHaveBeenCalledWith('C', 'number', 5.1)
    expect(commit).toHaveBeenCalledWith('A', 'number', 5)
  })

  it('default (no options) keeps slot value as-is (back-compat with existing callers)', () => {
    const original: NumRow[] = [
      { uuid: 'A', number: 3 },
      { uuid: 'C', number: 5.1 },
    ]
    const next: NumRow[] = [
      { uuid: 'C', number: 5.1 },
      { uuid: 'A', number: 3 },
    ]
    const commit = vi.fn()
    reorderRowsBySlot(original, next, 'number', commit)
    expect(commit).toHaveBeenCalledWith('C', 'number', 3)
    expect(commit).toHaveBeenCalledWith('A', 'number', 5.1)
  })
})

describe('reorderRowsBySlot — getValue option (unsaved-edit overlay)', () => {
  /* Callers with a dirty-value overlay (the manage drawer) display
   * overlay values while the row objects keep their server values.
   * Slot numbers must come from what the user SEES, or the second
   * consecutive unsaved move renumbers from stale data. */
  it('takes slot values from getValue, not the raw rows', () => {
    /* Server numbers 10/20/30 for a/b/c; a previous unsaved move
     * left the overlay at b=10, c=20, a=30 — display order b,c,a.
     * The user now drags a back to the top: new order a,b,c. With
     * display values [10,20,30] every row returns to its server
     * number; raw rows would yield slots [20,30,10] and scramble. */
    const display: NumRow[] = [
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
      { uuid: 'a', number: 10 },
    ]
    const next: NumRow[] = [
      { uuid: 'a', number: 10 },
      { uuid: 'b', number: 20 },
      { uuid: 'c', number: 30 },
    ]
    const overlay = new Map<string, number>([
      ['b', 10],
      ['c', 20],
      ['a', 30],
    ])
    const commit = vi.fn()
    reorderRowsBySlot(display, next, 'number', commit, {
      getValue: (r) => overlay.get(r.uuid as string) ?? r.number,
    })
    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('a', 'number', 10)
    expect(commit).toHaveBeenCalledWith('b', 'number', 20)
    expect(commit).toHaveBeenCalledWith('c', 'number', 30)
  })

  it('preserveMinor reads the row minor from getValue too', () => {
    /* Row C's server number is 5.1 but an unsaved edit holds 7.2 —
     * moving it onto an integer slot must carry the DISPLAYED .2,
     * not the stale server .1. */
    const display: NumRow[] = [
      { uuid: 'A', number: 3 },
      { uuid: 'C', number: 5.1 },
    ]
    const next: NumRow[] = [
      { uuid: 'C', number: 5.1 },
      { uuid: 'A', number: 3 },
    ]
    const overlay = new Map<string, number>([['C', 7.2]])
    const commit = vi.fn()
    reorderRowsBySlot(display, next, 'number', commit, {
      preserveMinor: true,
      getValue: (r) => overlay.get(r.uuid as string) ?? r.number,
    })
    /* Slot 0's display value is 3 (major 3); C keeps its displayed
     * minor .2 → 3.2. A lands on C's displayed slot (7.2 → major
     * 7) and, having no minor, takes the bare major 7. */
    expect(commit).toHaveBeenCalledWith('C', 'number', 3.2)
    expect(commit).toHaveBeenCalledWith('A', 'number', 7)
  })
})

describe('renumberAsIntegers (R1: "Renumber as integers" — flatten)', () => {
  it('assigns sequential integers starting at startFrom in display order', () => {
    const rows: NumRow[] = [
      { uuid: 'a', number: 0 },
      { uuid: 'b', number: 100 },
      { uuid: 'c', number: 5.1 },
    ]
    const commit = vi.fn()
    renumberAsIntegers(rows, 'number', 1, commit)
    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('a', 'number', 1)
    expect(commit).toHaveBeenCalledWith('b', 'number', 2)
    expect(commit).toHaveBeenCalledWith('c', 'number', 3)
  })

  it('honours a non-1 startFrom (e.g. continuation of a manual block)', () => {
    const rows: NumRow[] = [
      { uuid: 'a', number: 0 },
      { uuid: 'b', number: 0 },
    ]
    const commit = vi.fn()
    renumberAsIntegers(rows, 'number', 100, commit)
    expect(commit).toHaveBeenCalledWith('a', 'number', 100)
    expect(commit).toHaveBeenCalledWith('b', 'number', 101)
  })

  it('skips uuidless rows defensively', () => {
    const rows: SlotReorderRow[] = [
      { number: 0 } /* no uuid */,
      { uuid: 'b', number: 0 },
    ]
    const commit = vi.fn()
    renumberAsIntegers(rows, 'number', 1, commit)
    expect(commit).toHaveBeenCalledTimes(1)
    expect(commit).toHaveBeenCalledWith('b', 'number', 1)
  })
})

describe('renumberPreservingMinors (R1: "Renumber preserving sub-channels")', () => {
  it('contiguous-major run forms a single cluster with shared new major', () => {
    /* [3, 5, 5.1, 7, 10, 10.1, 10.2] → [1, 2, 2.1, 3, 4, 4.1, 4.2] */
    const rows: NumRow[] = [
      { uuid: 'a', number: 3 },
      { uuid: 'b', number: 5 },
      { uuid: 'c', number: 5.1 },
      { uuid: 'd', number: 7 },
      { uuid: 'e', number: 10 },
      { uuid: 'f', number: 10.1 },
      { uuid: 'g', number: 10.2 },
    ]
    const commit = vi.fn()
    renumberPreservingMinors(rows, 'number', 1, commit)
    expect(commit).toHaveBeenCalledWith('a', 'number', 1)
    expect(commit).toHaveBeenCalledWith('b', 'number', 2)
    expect(commit).toHaveBeenCalledWith('c', 'number', 2.1)
    expect(commit).toHaveBeenCalledWith('d', 'number', 3)
    expect(commit).toHaveBeenCalledWith('e', 'number', 4)
    expect(commit).toHaveBeenCalledWith('f', 'number', 4.1)
    expect(commit).toHaveBeenCalledWith('g', 'number', 4.2)
  })

  it('unnumbered rows (major == 0) each get a fresh new major (no merging)', () => {
    /* Three back-to-back 0s are three separate channels, NOT one
     * cluster — they all want unique numbers after renumbering. */
    const rows: NumRow[] = [
      { uuid: 'a', number: 0 },
      { uuid: 'b', number: 0 },
      { uuid: 'c', number: 0 },
      { uuid: 'd', number: 5 },
    ]
    const commit = vi.fn()
    renumberPreservingMinors(rows, 'number', 1, commit)
    expect(commit).toHaveBeenCalledWith('a', 'number', 1)
    expect(commit).toHaveBeenCalledWith('b', 'number', 2)
    expect(commit).toHaveBeenCalledWith('c', 'number', 3)
    expect(commit).toHaveBeenCalledWith('d', 'number', 4)
  })

  it('preserves the minor exactly (.1 stays .1 even on a new major)', () => {
    const rows: NumRow[] = [
      { uuid: 'a', number: 100.25 },
    ]
    const commit = vi.fn()
    renumberPreservingMinors(rows, 'number', 7, commit)
    expect(commit).toHaveBeenCalledWith('a', 'number', 7.25)
  })

  it('honours startFrom (continuation block)', () => {
    const rows: NumRow[] = [
      { uuid: 'a', number: 5 },
      { uuid: 'b', number: 5.1 },
    ]
    const commit = vi.fn()
    renumberPreservingMinors(rows, 'number', 50, commit)
    expect(commit).toHaveBeenCalledWith('a', 'number', 50)
    expect(commit).toHaveBeenCalledWith('b', 'number', 50.1)
  })
})

describe('channel-number helpers (extract / combine)', () => {
  it('extractChannelMajor handles integer, dotted, null, undefined, 0, ""', () => {
    expect(extractChannelMajor(5)).toBe(5)
    expect(extractChannelMajor(5.1)).toBe(5)
    expect(extractChannelMajor('5.25')).toBe(5)
    expect(extractChannelMajor(0)).toBe(0)
    expect(extractChannelMajor(null)).toBe(0)
    expect(extractChannelMajor(undefined)).toBe(0)
    expect(extractChannelMajor('')).toBe(0)
    expect(extractChannelMajor('garbage')).toBe(0)
  })

  it('extractChannelMinorString returns leading-dot string or empty', () => {
    expect(extractChannelMinorString(5)).toBe('')
    expect(extractChannelMinorString(5.1)).toBe('.1')
    expect(extractChannelMinorString('5.25')).toBe('.25')
    expect(extractChannelMinorString(0)).toBe('')
    expect(extractChannelMinorString(null)).toBe('')
  })

  it('combineChannelNumber recomposes without float drift', () => {
    /* 7 + 0.1 in float = 7.1 (happens to work), but 7 + 0.2 +
     * 0.1 = 7.299999…. The string-based combiner sidesteps it. */
    expect(combineChannelNumber(7, '.1')).toBe(7.1)
    expect(combineChannelNumber(7, '.25')).toBe(7.25)
    expect(combineChannelNumber(7, '')).toBe(7)
    expect(combineChannelNumber(0, '')).toBe(0)
  })
})
