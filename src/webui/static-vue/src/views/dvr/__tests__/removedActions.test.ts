// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DVR Removed toolbar gating tests. Smaller surface than Finished /
 * Failed: three actions, all count-only gating (no file-existence
 * predicate).
 */
import { describe, it, expect, vi } from 'vitest'
import { buildRemovedActions, type RemovedActionDeps } from '../removedActions'
import type { BaseRow } from '@/types/grid'

function deps(over: Partial<RemovedActionDeps> = {}): RemovedActionDeps {
  return {
    selection: [],
    clearSelection: vi.fn(),
    deleting: false,
    rerecording: false,
    onEdit: vi.fn(),
    onDelete: vi.fn(),
    onRerecord: vi.fn(),
    ...over,
  }
}

const ROW: BaseRow = { uuid: 'a' }

function findById(actions: ReturnType<typeof buildRemovedActions>, id: string) {
  const a = actions.find((x) => x.id === id)
  if (!a) throw new Error(`action ${id} missing`)
  return a
}

describe('buildRemovedActions', () => {
  it('produces three actions in expected toolbar order', () => {
    const actions = buildRemovedActions(deps())
    expect(actions.map((a) => a.id)).toEqual(['edit', 'delete', 'rerecord'])
  })

  describe('with empty selection', () => {
    it('disables every action', () => {
      const actions = buildRemovedActions(deps({ selection: [] }))
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be disabled`).toBe(true)
      }
    })
  })

  describe('with one row', () => {
    it('enables every action', () => {
      const actions = buildRemovedActions(deps({ selection: [ROW] }))
      for (const a of actions) {
        expect(a.disabled, `${a.id} should be enabled`).toBeFalsy()
      }
    })
  })

  describe('with multiple rows', () => {
    it('enables Edit (multi-edit allowed) plus Delete and Re-record', () => {
      /* Multi-edit shipped with the Edit gate lift in
       * `dvrToolbarHelpers.ts` — Edit is now enabled at any
       * non-empty selection. The editor itself dispatches
       * single-edit vs multi-edit based on selection
       * cardinality. */
      const actions = buildRemovedActions(
        deps({ selection: [ROW, ROW] })
      )
      expect(findById(actions, 'edit').disabled).toBeFalsy()
      expect(findById(actions, 'delete').disabled).toBeFalsy()
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
    })
  })

  describe('in-flight overrides', () => {
    it('disables Delete while deleting=true even with valid selection', () => {
      const actions = buildRemovedActions(
        deps({ selection: [ROW], deleting: true })
      )
      expect(findById(actions, 'delete').disabled).toBe(true)
      expect(findById(actions, 'delete').label).toBe('Deleting…')
      expect(findById(actions, 'rerecord').disabled).toBeFalsy()
    })

    it('disables Re-record while rerecording=true', () => {
      const actions = buildRemovedActions(
        deps({ selection: [ROW], rerecording: true })
      )
      expect(findById(actions, 'rerecord').disabled).toBe(true)
      expect(findById(actions, 'rerecord').label).toBe('Toggling…')
    })
  })

  describe('callback wiring', () => {
    it('Edit click calls onEdit with the current selection', () => {
      const onEdit = vi.fn()
      const sel = [ROW]
      const actions = buildRemovedActions(deps({ selection: sel, onEdit }))
      findById(actions, 'edit').onClick!()
      expect(onEdit).toHaveBeenCalledWith(sel)
    })
  })
})
