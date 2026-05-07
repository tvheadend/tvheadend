/*
 * dvrToolbarHelpers unit tests — covers the regression surface the
 * inline patterns relied on across UpcomingView / AutorecsView /
 * TimersView. Helper functions are pure (or pure-reactive); tested
 * directly without mounting any Vue component.
 */
import { ref } from 'vue'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'
import {
  adminAwareEditList,
  buildAddEditDeleteActions,
  buildEditDeleteRerecordActions,
} from '../dvrToolbarHelpers'
import { useAccessStore } from '@/stores/access'

beforeEach(() => {
  setActivePinia(createPinia())
})

/* Test fixtures hoisted to module scope so the per-test setup
 * doesn't recreate them. */

function makeRemove(inflight = false) {
  return {
    inflight: ref(inflight),
    run: vi.fn(async () => {}),
  }
}

/*
 * Two reusable adminAwareEditList option presets: a verbose one
 * mirroring real DVR call-sites for the admin/non-admin shape
 * assertions, and a short single-letter one for the structural
 * tests that don't care about the segment content.
 *
 * Hoisted as consts (rather than re-declared per test) so the same
 * 4-property literal isn't repeated across the suite.
 */
const VERBOSE_OPTS = {
  head: 'enabled',
  base: 'name,title,channel',
  adminExtra: 'owner,creator',
  tail: 'retention,removal',
}

const SHORT_OPTS = { head: 'h', base: 'b', adminExtra: 'a', tail: 't' }

function baseDeps(overrides: Record<string, unknown> = {}) {
  return {
    selection: [] as { uuid: string }[],
    clearSelection: () => {},
    deleting: false,
    rerecording: false,
    onEdit: () => {},
    onDelete: () => {},
    onRerecord: () => {},
    ...overrides,
  }
}

/* ---------------------------------------------------------------- */
/*   adminAwareEditList                                             */
/* ---------------------------------------------------------------- */

describe('adminAwareEditList', () => {
  it('joins head, base, tail (no admin extra) when access is non-admin', () => {
    /* Default Pinia state: data is null → access.data?.admin is undefined → falsy. */
    const list = adminAwareEditList(VERBOSE_OPTS)
    expect(list.value).toBe('enabled,name,title,channel,retention,removal')
  })

  it('inserts admin extras between base and tail when access.admin is true', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }
    const list = adminAwareEditList(VERBOSE_OPTS)
    expect(list.value).toBe('enabled,name,title,channel,owner,creator,retention,removal')
  })

  it('reactively recomputes when admin flag flips', () => {
    const access = useAccessStore()
    access.data = { admin: false, dvr: true, uilevel: 'expert' }
    const list = adminAwareEditList(SHORT_OPTS)
    expect(list.value).toBe('h,b,t')
    access.data = { admin: true, dvr: true, uilevel: 'expert' }
    expect(list.value).toBe('h,b,a,t')
  })

  it('falls back to non-admin shape when access.data is null (pre-Comet)', () => {
    /* Pre-accessUpdate window: data ref is null. The optional chain in
     * the helper returns undefined → falsy. */
    const list = adminAwareEditList(SHORT_OPTS)
    expect(list.value).toBe('h,b,t')
  })

  it('handles empty admin extras when admin flag is on', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }
    /* Edge case: a class with no admin-only fields — adminExtra is ''.
     * Resulting comma-comma is acceptable; matches the inline pattern's
     * behaviour pre-extraction (prop_serialize tolerates empty
     * comma-separated entries). */
    const list = adminAwareEditList({ ...SHORT_OPTS, adminExtra: '' })
    expect(list.value).toBe('h,b,,t')
  })
})

/* ---------------------------------------------------------------- */
/*   buildAddEditDeleteActions                                      */
/* ---------------------------------------------------------------- */

describe('buildAddEditDeleteActions', () => {
  it('returns three actions in order: Add, Edit, Delete', () => {
    const actions = buildAddEditDeleteActions({
      selection: [],
      clearSelection: () => {},
      remove: makeRemove(),
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: 'Add a new entry',
    })
    expect(actions).toHaveLength(3)
    expect(actions.map((a) => a.id)).toEqual(['add', 'edit', 'delete'])
  })

  it('Add tooltip mirrors the per-view string', () => {
    const actions = buildAddEditDeleteActions({
      selection: [],
      clearSelection: () => {},
      remove: makeRemove(),
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: 'Add a new autorec entry',
    })
    expect(actions[0].tooltip).toBe('Add a new autorec entry')
  })

  it('Edit is disabled with no selection or multi-selection (single-row only)', () => {
    const r = makeRemove()
    expect(
      buildAddEditDeleteActions({
        selection: [],
        clearSelection: () => {},
        remove: r,
        onAdd: () => {},
        onEdit: () => {},
        addTooltip: '',
      })[1].disabled
    ).toBe(true)
    expect(
      buildAddEditDeleteActions({
        selection: [{ uuid: 'a' }, { uuid: 'b' }],
        clearSelection: () => {},
        remove: r,
        onAdd: () => {},
        onEdit: () => {},
        addTooltip: '',
      })[1].disabled
    ).toBe(true)
  })

  it('Edit is enabled with exactly one selected row', () => {
    const actions = buildAddEditDeleteActions({
      selection: [{ uuid: 'a' }],
      clearSelection: () => {},
      remove: makeRemove(),
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: '',
    })
    expect(actions[1].disabled).toBe(false)
  })

  it('Delete is disabled with no selection or while remove.inflight is true', () => {
    expect(
      buildAddEditDeleteActions({
        selection: [],
        clearSelection: () => {},
        remove: makeRemove(false),
        onAdd: () => {},
        onEdit: () => {},
        addTooltip: '',
      })[2].disabled
    ).toBe(true)
    expect(
      buildAddEditDeleteActions({
        selection: [{ uuid: 'a' }],
        clearSelection: () => {},
        remove: makeRemove(true),
        onAdd: () => {},
        onEdit: () => {},
        addTooltip: '',
      })[2].disabled
    ).toBe(true)
  })

  it('Delete label flips to "Deleting…" while remove.inflight is true', () => {
    const idle = buildAddEditDeleteActions({
      selection: [{ uuid: 'a' }],
      clearSelection: () => {},
      remove: makeRemove(false),
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: '',
    })
    const busy = buildAddEditDeleteActions({
      selection: [{ uuid: 'a' }],
      clearSelection: () => {},
      remove: makeRemove(true),
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: '',
    })
    expect(idle[2].label).toBe('Delete')
    expect(busy[2].label).toBe('Deleting…')
  })

  it('Add onClick invokes onAdd; Edit invokes onEdit with current selection', () => {
    const onAdd = vi.fn()
    const onEdit = vi.fn()
    const sel = [{ uuid: 'a' }]
    const actions = buildAddEditDeleteActions({
      selection: sel,
      clearSelection: () => {},
      remove: makeRemove(),
      onAdd,
      onEdit,
      addTooltip: '',
    })
    actions[0].onClick?.()
    actions[1].onClick?.()
    expect(onAdd).toHaveBeenCalledOnce()
    expect(onEdit).toHaveBeenCalledWith(sel)
  })

  it('Delete onClick calls remove.run with selection + clearSelection', () => {
    const remove = makeRemove()
    const clearSelection = vi.fn()
    const sel = [{ uuid: 'a' }, { uuid: 'b' }]
    const actions = buildAddEditDeleteActions({
      selection: sel,
      clearSelection,
      remove,
      onAdd: () => {},
      onEdit: () => {},
      addTooltip: '',
    })
    actions[2].onClick?.()
    expect(remove.run).toHaveBeenCalledWith(sel, clearSelection)
  })
})

/* ---------------------------------------------------------------- */
/*   buildEditDeleteRerecordActions                                 */
/* ---------------------------------------------------------------- */

describe('buildEditDeleteRerecordActions', () => {
  it('returns three actions in order: edit, delete, rerecord', () => {
    const actions = buildEditDeleteRerecordActions(baseDeps())
    expect(actions).toHaveLength(3)
    expect(actions.map((a) => a.id)).toEqual(['edit', 'delete', 'rerecord'])
  })

  it('Edit gates on single-row selection', () => {
    expect(buildEditDeleteRerecordActions(baseDeps())[0].disabled).toBe(true)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }] }))[0].disabled
    ).toBe(false)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }, { uuid: 'b' }] }))[0]
        .disabled
    ).toBe(true)
  })

  it('Delete disabled with empty selection or while deleting; label flips to "Deleting…"', () => {
    expect(buildEditDeleteRerecordActions(baseDeps())[1].disabled).toBe(true)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }], deleting: true }))[1]
        .disabled
    ).toBe(true)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }], deleting: true }))[1]
        .label
    ).toBe('Deleting…')
    expect(buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }] }))[1].label).toBe(
      'Delete'
    )
  })

  it('Re-record disabled with empty selection or in flight; label flips to "Toggling…"', () => {
    expect(buildEditDeleteRerecordActions(baseDeps())[2].disabled).toBe(true)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }], rerecording: true }))[2]
        .disabled
    ).toBe(true)
    expect(
      buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }], rerecording: true }))[2]
        .label
    ).toBe('Toggling…')
    expect(buildEditDeleteRerecordActions(baseDeps({ selection: [{ uuid: 'a' }] }))[2].label).toBe(
      'Re-record'
    )
  })

  it('click handlers wire selection + clearSelection through correctly', () => {
    const onEdit = vi.fn()
    const onDelete = vi.fn()
    const onRerecord = vi.fn()
    const clearSelection = vi.fn()
    const sel = [{ uuid: 'a' }]
    const actions = buildEditDeleteRerecordActions({
      selection: sel,
      clearSelection,
      deleting: false,
      rerecording: false,
      onEdit,
      onDelete,
      onRerecord,
    })
    actions[0].onClick?.()
    actions[1].onClick?.()
    actions[2].onClick?.()
    expect(onEdit).toHaveBeenCalledWith(sel)
    expect(onDelete).toHaveBeenCalledWith(sel, clearSelection)
    expect(onRerecord).toHaveBeenCalledWith(sel, clearSelection)
  })
})
