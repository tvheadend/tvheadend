/*
 * DVR Failed toolbar gating tests. Validates that buildFailedActions
 * produces correct disabled states across the meaningful selection
 * shapes, and — critically — verifies the difference from Finished:
 * Re-record and Move-to-finished are gated on selection count alone,
 * NOT on first-row filesize. Only Download is file-gated.
 */
import { describe, it, expect, vi } from 'vitest'
import { buildFailedActions, type FailedActionDeps } from '../failedActions'
import type { BaseRow } from '@/types/grid'

function deps(over: Partial<FailedActionDeps> = {}): FailedActionDeps {
  return {
    selection: [],
    clearSelection: vi.fn(),
    deleting: false,
    rerecording: false,
    moving: false,
    onEdit: vi.fn(),
    onDelete: vi.fn(),
    onDownload: vi.fn(),
    onRerecord: vi.fn(),
    onMoveToFinished: vi.fn(),
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

function findById(actions: ReturnType<typeof buildFailedActions>, id: string) {
  const a = actions.find((x) => x.id === id)
  if (!a) throw new Error(`action ${id} missing`)
  return a
}

describe('buildFailedActions', () => {
  it('produces five actions in expected toolbar order', () => {
    const actions = buildFailedActions(deps())
    expect(actions.map((a) => a.id)).toEqual([
      'edit',
      'delete',
      'rerecord',
      'move',
      'download',
    ])
  })

  /* === Empty selection === */
  describe('with empty selection', () => {
    it('disables every action', () => {
      const actions = buildFailedActions(deps({ selection: [] }))
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be disabled`).toBe(true)
      }
    })
  })

  /* === Single selection, with file === */
  describe('with one row that has a file', () => {
    it('enables every action', () => {
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITH_FILE] })
      )
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be enabled`).toBeFalsy()
      }
    })
  })

  /* === Single selection, without file ===
   * THIS IS THE KEY DIVERGENCE FROM FINISHED.
   * Re-record and Move-to-finished must be enabled even when the row
   * has no file. Only Download should be disabled. */
  describe('with one row that has no file', () => {
    it('enables Edit, Delete, Re-record, Move; disables only Download', () => {
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITHOUT_FILE] })
      )
      expect(findById(actions, 'edit').disabled).toBeFalsy()
      expect(findById(actions, 'delete').disabled).toBeFalsy()
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
      expect(findById(actions, 'move').disabled).toBeFalsy()
      expect(findById(actions, 'download').disabled).toBe(true)
    })
  })

  /* === Multi-selection === */
  describe('with multiple rows', () => {
    it('disables Edit (single-select rule); enables Delete/Re-record/Move regardless of files', () => {
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITHOUT_FILE, ROW_WITHOUT_FILE] })
      )
      expect(findById(actions, 'edit').disabled).toBe(true)
      expect(findById(actions, 'delete').disabled).toBeFalsy()
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
      expect(findById(actions, 'move').disabled).toBeFalsy()
      /* Download still file-gated (first row has no file) */
      expect(findById(actions, 'download').disabled).toBe(true)
    })

    it('Download disabled on multi-select even when first row has a file', () => {
      /* Download's handler only consumes selected[0]; allowing
       * multi-select would silently drop every row past the first.
       * The legacy ExtJS UI accepts multi-select on Download and
       * has the same drop-the-rest bug — see BACKLOG. */
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITH_FILE, ROW_WITHOUT_FILE] })
      )
      expect(findById(actions, 'download').disabled).toBe(true)
    })

    it('Download enabled when exactly one row is selected and has a file', () => {
      const actions = buildFailedActions(deps({ selection: [ROW_WITH_FILE] }))
      expect(findById(actions, 'download').disabled).toBeFalsy()
    })
  })

  /* === In-flight overrides === */
  describe('in-flight overrides', () => {
    it('disables Delete while deleting=true even with valid selection', () => {
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITH_FILE], deleting: true })
      )
      expect(findById(actions, 'delete').disabled).toBe(true)
      expect(findById(actions, 'delete').label).toBe('Deleting…')
      /* Other actions unaffected. */
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
    })

    it('disables Re-record while rerecording=true', () => {
      const actions = buildFailedActions(
        deps({ selection: [ROW_WITH_FILE], rerecording: true })
      )
      expect(findById(actions, 'rerecord').disabled).toBe(true)
      expect(findById(actions, 'rerecord').label).toBe('Toggling…')
    })

    it('disables Move while moving=true', () => {
      const actions = buildFailedActions(
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
      const actions = buildFailedActions(deps({ selection: sel, onEdit }))
      findById(actions, 'edit').onClick()
      expect(onEdit).toHaveBeenCalledWith(sel)
    })

    it('Delete click calls onDelete with selection + clearSelection', () => {
      const onDelete = vi.fn()
      const clearSelection = vi.fn()
      const sel = [ROW_WITH_FILE]
      const actions = buildFailedActions(
        deps({ selection: sel, clearSelection, onDelete })
      )
      findById(actions, 'delete').onClick()
      expect(onDelete).toHaveBeenCalledWith(sel, clearSelection)
    })
  })
})
