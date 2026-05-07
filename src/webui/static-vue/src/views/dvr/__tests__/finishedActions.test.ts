/*
 * DVR Finished toolbar gating tests. Validates that buildFinishedActions
 * produces correct disabled states across the four meaningful selection
 * shapes:
 *
 *   - empty   → only Edit's "exactly 1" rule and the bulk actions'
 *               "≥1" rule are exercised (everything disabled).
 *   - one     → with file: Edit + all bulk actions enabled. without
 *               file: only Edit enabled.
 *   - multi   → Edit disabled (single-select rule). Bulk actions
 *               follow the first-row file gate.
 *   - in-flight → action's own "loading" flag overrides; the in-flight
 *               action stays disabled even with a valid selection.
 */
import { describe, it, expect, vi } from 'vitest'
import { buildFinishedActions, type FinishedActionDeps } from '../finishedActions'
import type { BaseRow } from '@/types/grid'

function deps(over: Partial<FinishedActionDeps> = {}): FinishedActionDeps {
  return {
    selection: [],
    clearSelection: vi.fn(),
    removing: false,
    rerecording: false,
    moving: false,
    onEdit: vi.fn(),
    onRemove: vi.fn(),
    onDownload: vi.fn(),
    onRerecord: vi.fn(),
    onMoveToFailed: vi.fn(),
    ...over,
  }
}

const ROW_WITH_FILE: BaseRow = {
  uuid: 'a',
  filesize: 1024,
} as BaseRow

const ROW_WITHOUT_FILE: BaseRow = {
  uuid: 'b',
  filesize: 0,
} as BaseRow

function findById(actions: ReturnType<typeof buildFinishedActions>, id: string) {
  const a = actions.find((x) => x.id === id)
  if (!a) throw new Error(`action ${id} missing`)
  return a
}

describe('buildFinishedActions', () => {
  it('produces five actions in ExtJS toolbar order', () => {
    const actions = buildFinishedActions(deps())
    expect(actions.map((a) => a.id)).toEqual([
      'edit',
      'remove',
      'download',
      'rerecord',
      'move',
    ])
  })

  /* === Empty selection === */
  describe('with empty selection', () => {
    it('disables every action', () => {
      const actions = buildFinishedActions(deps({ selection: [] }))
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be disabled`).toBe(true)
      }
    })
  })

  /* === Single selection, with file === */
  describe('with one row that has a file', () => {
    it('enables every action', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITH_FILE] })
      )
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be enabled`).toBeFalsy()
      }
    })
  })

  /* === Single selection, without file === */
  describe('with one row that has no file', () => {
    it('enables Edit only; disables Remove/Download/Re-record/Move', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITHOUT_FILE] })
      )
      expect(findById(actions, 'edit').disabled).toBeFalsy()
      expect(findById(actions, 'remove').disabled).toBe(true)
      expect(findById(actions, 'download').disabled).toBe(true)
      expect(findById(actions, 'rerecord').disabled).toBe(true)
      expect(findById(actions, 'move').disabled).toBe(true)
    })
  })

  /* === Multi-selection === */
  describe('with multiple rows', () => {
    it('disables Edit (single-select rule) and Download (single-row only); bulk actions follow first-row gate', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITH_FILE, ROW_WITHOUT_FILE] })
      )
      expect(findById(actions, 'edit').disabled).toBe(true)
      /* Download is single-row even though file-gate passes. The
       * handler only consumes selected[0]; allowing multi-select
       * here would silently drop the rest. */
      expect(findById(actions, 'download').disabled).toBe(true)
      /* First row has file → bulk actions enabled. */
      expect(findById(actions, 'remove').disabled).toBeFalsy()
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
      expect(findById(actions, 'move').disabled).toBeFalsy()
    })

    it('disables every bulk action when the first row has no file', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITHOUT_FILE, ROW_WITH_FILE] })
      )
      expect(findById(actions, 'remove').disabled).toBe(true)
      expect(findById(actions, 'download').disabled).toBe(true)
      expect(findById(actions, 'rerecord').disabled).toBe(true)
      expect(findById(actions, 'move').disabled).toBe(true)
    })
  })

  /* === In-flight overrides === */
  describe('in-flight overrides', () => {
    it('disables Remove while removing=true even with valid selection', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITH_FILE], removing: true })
      )
      expect(findById(actions, 'remove').disabled).toBe(true)
      expect(findById(actions, 'remove').label).toBe('Removing…')
      /* Other actions unaffected. */
      expect(findById(actions, 'download').disabled).toBeFalsy()
    })

    it('disables Re-record while rerecording=true', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITH_FILE], rerecording: true })
      )
      expect(findById(actions, 'rerecord').disabled).toBe(true)
      expect(findById(actions, 'rerecord').label).toBe('Toggling…')
    })

    it('disables Move while moving=true', () => {
      const actions = buildFinishedActions(
        deps({ selection: [ROW_WITH_FILE], moving: true })
      )
      expect(findById(actions, 'move').disabled).toBe(true)
      expect(findById(actions, 'move').label).toBe('Moving…')
    })
  })

  /* === Wiring sanity === */
  describe('callback wiring', () => {
    it('Edit click calls onEdit with the current selection', () => {
      const onEdit = vi.fn()
      const sel = [ROW_WITH_FILE]
      const actions = buildFinishedActions(deps({ selection: sel, onEdit }))
      findById(actions, 'edit').onClick()
      expect(onEdit).toHaveBeenCalledWith(sel)
    })

    it('Remove click calls onRemove with selection + clearSelection', () => {
      const onRemove = vi.fn()
      const clearSelection = vi.fn()
      const sel = [ROW_WITH_FILE]
      const actions = buildFinishedActions(
        deps({ selection: sel, clearSelection, onRemove })
      )
      findById(actions, 'remove').onClick()
      expect(onRemove).toHaveBeenCalledWith(sel, clearSelection)
    })
  })
})
