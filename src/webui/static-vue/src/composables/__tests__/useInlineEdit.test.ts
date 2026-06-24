// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useInlineEdit — per-row, per-cell dirty store + batch save.
 *
 * Pins the public API:
 *  - dirty tracking (add / read / revert / count)
 *  - canEdit gating via beforeEdit predicate
 *  - save: POST shape (Classic-compatible), success-clears,
 *    failure-preserves, in-flight guard, stale-uuid filter
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { ref } from 'vue'
import type { BaseRow } from '@/types/grid'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

const errorDialogShow = vi.fn()
vi.mock('@/composables/useErrorDialog', () => ({
  useErrorDialog: () => ({ show: errorDialogShow }),
}))

beforeEach(() => {
  apiMock.mockReset()
  errorDialogShow.mockReset()
})

afterEach(() => {
  vi.restoreAllMocks()
})

interface Row extends BaseRow {
  uuid: string
  comment?: string
  pri?: number
  enabled?: boolean
  status?: string
}

function rows(...uuids: string[]): Row[] {
  return uuids.map((uuid) => ({ uuid }))
}

describe('useInlineEdit — dirty tracking', () => {
  it('starts empty: hasDirty=false, dirtyRowCount=0', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a', 'b'))
    const e = useInlineEdit<Row>({ entries })
    expect(e.hasDirty.value).toBe(false)
    expect(e.dirtyRowCount.value).toBe(0)
    expect(e.isCellDirty('a', 'comment')).toBe(false)
  })

  it('commitCell makes the cell dirty and exposes the new value', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'updated')
    expect(e.hasDirty.value).toBe(true)
    expect(e.isCellDirty('a', 'comment')).toBe(true)
    expect(e.cellValue('a', 'comment', 'original')).toBe('updated')
    expect(e.dirtyRowCount.value).toBe(1)
  })

  it('cellValue returns the fallback for non-dirty cells', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    expect(e.cellValue('a', 'comment', 'original')).toBe('original')
  })

  it('committing the same cell twice keeps only the latest value', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'first')
    e.commitCell('a', 'comment', 'second')
    expect(e.cellValue('a', 'comment', 'orig')).toBe('second')
    expect(e.dirtyRowCount.value).toBe(1)
  })

  it('tracks multiple rows + multiple fields independently', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a', 'b', 'c'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'A-comment')
    e.commitCell('b', 'pri', 5)
    e.commitCell('b', 'enabled', true)
    expect(e.dirtyRowCount.value).toBe(2)
    expect(e.isCellDirty('a', 'comment')).toBe(true)
    expect(e.isCellDirty('a', 'pri')).toBe(false)
    expect(e.isCellDirty('b', 'pri')).toBe(true)
    expect(e.isCellDirty('b', 'enabled')).toBe(true)
    expect(e.isCellDirty('c', 'comment')).toBe(false)
  })

  it('revertAll clears the entire dirty store', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a', 'b'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'x')
    e.commitCell('b', 'pri', 7)
    e.revertAll()
    expect(e.hasDirty.value).toBe(false)
    expect(e.dirtyRowCount.value).toBe(0)
    expect(e.isCellDirty('a', 'comment')).toBe(false)
  })
})

describe('useInlineEdit — smart dirty (toggle back to original clears dirty)', () => {
  it('does not mark dirty when committing the original value', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'orig')
    expect(e.isCellDirty('a', 'comment')).toBe(false)
    expect(e.hasDirty.value).toBe(false)
  })

  it('clears dirty when an existing dirty cell is reverted to its original value', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    expect(e.isCellDirty('a', 'comment')).toBe(true)
    e.commitCell('a', 'comment', 'orig')
    expect(e.isCellDirty('a', 'comment')).toBe(false)
    expect(e.hasDirty.value).toBe(false)
    expect(e.dirtyRowCount.value).toBe(0)
  })

  it('coerces 0/1 ↔ true/false on bool fields when comparing', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    /* Wire format for PT_BOOL is 0/1; the cell editor commits
     * actual booleans. Toggling a checkbox twice should land
     * back at the original 0 (or 1) and clear dirty. */
    const entries = ref<Row[]>([{ uuid: 'a', enabled: false }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'enabled', true)
    expect(e.isCellDirty('a', 'enabled')).toBe(true)
    e.commitCell('a', 'enabled', false)
    expect(e.isCellDirty('a', 'enabled')).toBe(false)
  })

  it('treats null / undefined / empty-string as equivalent originals', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a' }])
    const e = useInlineEdit<Row>({ entries })
    /* Original is undefined; user types nothing → empty
     * string committed. Should NOT mark dirty. */
    e.commitCell('a', 'comment', '')
    expect(e.isCellDirty('a', 'comment')).toBe(false)
  })

  it('only the matching field clears; other dirty fields on the same row survive', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig', pri: 5 }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    e.commitCell('a', 'pri', 9)
    expect(e.dirtyRowCount.value).toBe(1)
    e.commitCell('a', 'comment', 'orig')
    expect(e.isCellDirty('a', 'comment')).toBe(false)
    expect(e.isCellDirty('a', 'pri')).toBe(true)
    /* Row still counts as dirty because pri is still dirty. */
    expect(e.dirtyRowCount.value).toBe(1)
  })

  it('array fields clear dirty when round-trip edits restore the original contents', async () => {
    /* Round-trip from the chip cell: user adds a tag (creates a
     * fresh array), then removes the same tag (another fresh
     * array). The resulting array is a different reference but
     * positionally identical to the server value. The Save
     * button should revert to disabled. */
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'a', comment: '', tags: ['t1'] },
    ])
    const e = useInlineEdit<Row>({ entries })
    /* Step 1: add 't2'. */
    e.commitCell('a', 'tags', ['t1', 't2'])
    expect(e.isCellDirty('a', 'tags')).toBe(true)
    expect(e.hasDirty.value).toBe(true)
    /* Step 2: remove 't2' — back to ['t1'] (new array, same
     * contents). Smart-clear should kick in. */
    e.commitCell('a', 'tags', ['t1'])
    expect(e.isCellDirty('a', 'tags')).toBe(false)
    expect(e.hasDirty.value).toBe(false)
  })

  it('array fields STAY dirty when contents truly differ from the original', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'a', comment: '', tags: ['t1', 't2'] },
    ])
    const e = useInlineEdit<Row>({ entries })
    /* Add a new tag — contents really did change. */
    e.commitCell('a', 'tags', ['t1', 't2', 't3'])
    expect(e.isCellDirty('a', 'tags')).toBe(true)
    /* Different length → not equal. */
    e.commitCell('a', 'tags', ['t1', 't2', 't4'])
    expect(e.isCellDirty('a', 'tags')).toBe(true)
  })

  it('array fields treat reorderings as no-op (islist values are sets)', async () => {
    /* Channels' `tags` column is a membership set — the server
     * stores attach-order, which varies per channel. Editing the
     * set such that the final membership matches the original
     * (regardless of order) should clear dirty. */
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'a', comment: '', tags: ['t1', 't2'] },
    ])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'tags', ['t2', 't1'])
    expect(e.isCellDirty('a', 'tags')).toBe(false)
    expect(e.hasDirty.value).toBe(false)
  })

  it('numeric server value cleared by an equivalent numeric-string commit (IntSplit editor round-trip)', async () => {
    /* IdnodeFieldIntSplit (channel number's major.minor editor)
     * emits the sanitised STRING value because partial states
     * like "5." can't round-trip through Number. The server
     * stores number as an actual Number. Without numeric ↔
     * numeric-string equivalence in valuesMatch, "2 → 200 →
     * 2" would leave the cell stuck dirty against the server's
     * `2`. The smart-clear MUST treat them as equal. */
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'r2', number: 2 },
    ])
    const e = useInlineEdit<Row>({ entries })
    /* Editor emits string "200" mid-edit. */
    e.commitCell('r2', 'number', '200')
    expect(e.isCellDirty('r2', 'number')).toBe(true)
    /* User backspaces and types "2" — editor emits string "2".
     * Server has number 2. Smart-clear must fire. */
    e.commitCell('r2', 'number', '2')
    expect(e.isCellDirty('r2', 'number')).toBe(false)
    expect(e.hasDirty.value).toBe(false)
  })

  it('skipSmartClear holds the dirty entry even when a keystroke transiently matches the server value', async () => {
    /* User is editing a number cell currently at dirty 200,
     * server value 2. They backspace toward 210: 200 → 20 →
     * 2 (transient match!) → 21 → 210. The per-keystroke
     * commit with skipSmartClear must keep the dirty entry
     * present through the "2" step so the active-edit pin
     * stays anchored and the row doesn't visually drop the
     * dirty marker mid-edit. */
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'r', number: 2 },
    ])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('r', 'number', 200, { skipSmartClear: true })
    expect(e.isCellDirty('r', 'number')).toBe(true)
    e.commitCell('r', 'number', '20', { skipSmartClear: true })
    expect(e.isCellDirty('r', 'number')).toBe(true)
    /* The transient match — must NOT clear. */
    e.commitCell('r', 'number', '2', { skipSmartClear: true })
    expect(e.isCellDirty('r', 'number')).toBe(true)
    e.commitCell('r', 'number', '21', { skipSmartClear: true })
    e.commitCell('r', 'number', '210', { skipSmartClear: true })
    expect(e.isCellDirty('r', 'number')).toBe(true)
    /* No evaluateSmartClear call — final value differs from
     * the server, so dirty stays. */
  })

  it('evaluateSmartClear fires the deferred check and clears when final value matches the original', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'r', number: 2 },
    ])
    const e = useInlineEdit<Row>({ entries })
    /* User changed it, then changed back — all per-keystroke
     * commits skip the smart-clear. */
    e.commitCell('r', 'number', 200, { skipSmartClear: true })
    e.commitCell('r', 'number', '20', { skipSmartClear: true })
    e.commitCell('r', 'number', '2', { skipSmartClear: true })
    expect(e.isCellDirty('r', 'number')).toBe(true)
    /* cell-edit-complete fires → IdnodeGrid calls
     * evaluateSmartClear → dirty clears because "2" matches
     * server 2. */
    e.evaluateSmartClear('r', 'number')
    expect(e.isCellDirty('r', 'number')).toBe(false)
  })

  it('evaluateSmartClear is a no-op when the final value differs from the original', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'r', number: 2 },
    ])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('r', 'number', 200, { skipSmartClear: true })
    e.commitCell('r', 'number', '210', { skipSmartClear: true })
    e.evaluateSmartClear('r', 'number')
    expect(e.isCellDirty('r', 'number')).toBe(true)
  })

  it('numeric-string ↔ number equivalence covers dotted minors (5.1 ≡ "5.1")', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'a', number: 5.1 },
    ])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'number', '7')
    expect(e.isCellDirty('a', 'number')).toBe(true)
    /* Back to the dotted original — should clear. */
    e.commitCell('a', 'number', '5.1')
    expect(e.isCellDirty('a', 'number')).toBe(false)
  })
})

describe('useInlineEdit — server-pending conflict marker', () => {
  /* Mirrors the drawer's smart-merge: when the user has a dirty
   * cell AND a comet refresh delivers a new server value for the
   * same field, the cell should pulse-mark instead of silently
   * overwriting OR being ignored. The composable doesn't drive
   * the refresh itself — IdnodeGrid hands the row's CURRENT
   * value to `isCellServerPending` and we compare it against the
   * baseline captured at FIRST dirty. */

  it('clean cells are never server-pending', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    expect(e.isCellServerPending('a', 'comment', 'orig')).toBe(false)
    /* Even if the server value somehow differs from what the
     * cell currently shows, a clean cell can't conflict — there
     * are no local edits to preserve. */
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(false)
  })

  it('dirty cell is NOT server-pending while the row value matches the baseline', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    /* Baseline captured at first dirty = 'orig'. Row's current
     * server value is still 'orig' (no refresh yet). No conflict. */
    expect(e.isCellServerPending('a', 'comment', 'orig')).toBe(false)
  })

  it('dirty cell becomes server-pending when the row value diverges from the captured baseline', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    /* Comet refresh just landed: row.comment is now 'server-new'.
     * The user's local edit ('edited') is still in the dirty
     * store, but the server moved out from under them. */
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(true)
  })

  it('baseline tracks the value at FIRST dirty — subsequent keystrokes do not advance it', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edit-1')
    e.commitCell('a', 'comment', 'edit-2')
    /* Baseline must still be 'orig' — keystrokes update the
     * dirty value but never the conflict reference point. So a
     * server change to 'server-new' is still a conflict. */
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(true)
    /* And if the row's current value is back to the baseline,
     * no conflict (the server hasn't moved). */
    expect(e.isCellServerPending('a', 'comment', 'orig')).toBe(false)
  })

  it('reverting the dirty cell to the row\'s value clears the conflict baseline', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const row = { uuid: 'a', comment: 'orig' }
    const entries = ref<Row[]>([row])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    /* Simulate a comet refresh: row.comment becomes 'server-new'
     * AND the entries ref is updated (the grid does this). */
    row.comment = 'server-new'
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(true)
    /* User clicks Undo on the cell or types the new server value:
     * commitCell with the matching value clears dirty + baseline. */
    e.commitCell('a', 'comment', 'server-new')
    expect(e.isCellDirty('a', 'comment')).toBe(false)
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(false)
    /* Re-dirty after the clear should re-capture the baseline at
     * the NEW server value, not the stale original. */
    e.commitCell('a', 'comment', 'edited-again')
    expect(e.isCellServerPending('a', 'comment', 'server-new')).toBe(false)
    expect(e.isCellServerPending('a', 'comment', 'server-newer')).toBe(true)
  })

  it('save success clears the conflict baseline along with the dirty store', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', comment: 'orig' }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edited')
    apiMock.mockResolvedValueOnce({})
    await e.save()
    expect(e.isCellDirty('a', 'comment')).toBe(false)
    /* Baseline must be cleared too — otherwise re-dirtying the
     * same cell after save would compare against a stale
     * pre-save baseline. */
    e.commitCell('a', 'comment', 'edited-again')
    expect(e.isCellServerPending('a', 'comment', 'orig')).toBe(false)
  })

  it('revertAll clears every conflict baseline', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([
      { uuid: 'a', comment: 'orig-a' },
      { uuid: 'b', pri: 1 },
    ])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'edit-a')
    e.commitCell('b', 'pri', 7)
    e.revertAll()
    /* After revertAll, re-dirty + new server value should NOT
     * be flagged against any pre-revert baseline. */
    e.commitCell('a', 'comment', 'edit-a-again')
    expect(e.isCellServerPending('a', 'comment', 'orig-a')).toBe(false)
  })

  it('coerces 0/1 ↔ true/false in the conflict comparison too', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    /* Baseline captured as 0 (PT_BOOL wire format), row value
     * arrives as `false` after a refresh. Same equivalence
     * class as the dirty comparison — must NOT flag conflict. */
    const entries = ref<Row[]>([{ uuid: 'a', enabled: false }])
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'enabled', true)
    expect(e.isCellServerPending('a', 'enabled', 0)).toBe(false)
    /* Whereas `1` after the user dirtied a baseline of `false`
     * IS a conflict. */
    expect(e.isCellServerPending('a', 'enabled', 1)).toBe(true)
  })
})

describe('useInlineEdit — canEdit gate', () => {
  it('returns true when no beforeEdit predicate is configured', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    expect(e.canEdit({ uuid: 'a' }, 'comment')).toBe(true)
  })

  it('delegates to beforeEdit and forwards row + field', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref<Row[]>([{ uuid: 'a', status: 'recording' }])
    const e = useInlineEdit<Row>({
      entries,
      beforeEdit: (row) =>
        row.status === 'recording'
          ? 'cannot edit a row currently recording'
          : true,
    })
    const allowed = e.canEdit({ uuid: 'b', status: 'idle' }, 'comment')
    expect(allowed).toBe(true)
    const blocked = e.canEdit({ uuid: 'a', status: 'recording' }, 'comment')
    expect(blocked).toBe('cannot edit a row currently recording')
  })
})

describe('useInlineEdit — save', () => {
  it('no-ops when nothing is dirty (no apiCall fired)', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    await e.save()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('POSTs the Classic per-row partial-object array shape', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a', 'b', 'c'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'A-comment')
    e.commitCell('b', 'pri', 5)
    e.commitCell('b', 'enabled', true)
    apiMock.mockResolvedValueOnce({})
    await e.save()
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('idnode/save')
    const body = apiMock.mock.calls[0][1] as { node: string }
    const payload = JSON.parse(body.node) as Array<Record<string, unknown>>
    /* Order of rows isn't strictly defined by Map iteration, but
     * is insertion-ordered in modern JS — assert by uuid. */
    const byUuid = new Map(payload.map((r) => [r.uuid as string, r]))
    expect(byUuid.get('a')).toEqual({ uuid: 'a', comment: 'A-comment' })
    expect(byUuid.get('b')).toEqual({ uuid: 'b', pri: 5, enabled: true })
    /* No clean rows in payload. */
    expect(byUuid.has('c')).toBe(false)
  })

  it('clears the dirty store on successful save', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'x')
    apiMock.mockResolvedValueOnce({})
    await e.save()
    expect(e.hasDirty.value).toBe(false)
    expect(e.dirtyRowCount.value).toBe(0)
  })

  it('preserves the dirty store + pops the error dialog when save fails', async () => {
    /* Save catches the rejection and surfaces it via the global
     * error dialog (the user signal), then swallows so the
     * Promise resolves cleanly. Dirty state stays intact so the
     * user can correct the offending field and retry. */
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'x')
    apiMock.mockRejectedValueOnce(new Error('server bad'))
    await expect(e.save()).resolves.toBeUndefined()
    expect(errorDialogShow).toHaveBeenCalledOnce()
    expect(errorDialogShow.mock.calls[0][0]).toMatchObject({
      message: 'server bad',
    })
    expect(e.hasDirty.value).toBe(true)
    expect(e.cellValue('a', 'comment', 'orig')).toBe('x')
    expect(e.inflight.value).toBe(false)
  })

  it('filters out dirty entries whose uuid is no longer in the entries list', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a', 'b'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'A-comment')
    e.commitCell('zombie', 'pri', 99)
    apiMock.mockResolvedValueOnce({})
    await e.save()
    const payload = JSON.parse(
      (apiMock.mock.calls[0][1] as { node: string }).node,
    ) as Array<Record<string, unknown>>
    expect(payload).toHaveLength(1)
    expect(payload[0]).toEqual({ uuid: 'a', comment: 'A-comment' })
  })

  it('clears the dirty store with no POST when ALL dirty rows are stale', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('zombie1', 'comment', 'x')
    e.commitCell('zombie2', 'pri', 1)
    await e.save()
    expect(apiMock).not.toHaveBeenCalled()
    expect(e.hasDirty.value).toBe(false)
  })

  it('honours a custom saveEndpoint', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({
      entries,
      saveEndpoint: 'custom/save',
    })
    e.commitCell('a', 'comment', 'x')
    apiMock.mockResolvedValueOnce({})
    await e.save()
    expect(apiMock.mock.calls[0][0]).toBe('custom/save')
  })

  it('inflight flips true during save and back to false on resolve', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'x')
    let resolveFn!: () => void
    apiMock.mockReturnValueOnce(
      new Promise<void>((res) => {
        resolveFn = res
      }),
    )
    const saving = e.save()
    expect(e.inflight.value).toBe(true)
    resolveFn()
    await saving
    expect(e.inflight.value).toBe(false)
  })

  it('blocks a second save while one is already in flight', async () => {
    const { useInlineEdit } = await import('../useInlineEdit')
    const entries = ref(rows('a'))
    const e = useInlineEdit<Row>({ entries })
    e.commitCell('a', 'comment', 'x')
    let resolveFn!: () => void
    apiMock.mockReturnValueOnce(
      new Promise<void>((res) => {
        resolveFn = res
      }),
    )
    const first = e.save()
    /* Second call should early-return without firing the API. */
    const second = e.save()
    expect(apiMock).toHaveBeenCalledTimes(1)
    resolveFn()
    await Promise.all([first, second])
  })
})
