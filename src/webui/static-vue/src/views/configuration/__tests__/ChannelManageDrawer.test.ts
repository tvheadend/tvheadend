// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ChannelManageDrawer tests — verifies the slimmed-down column set
 * (Enabled / Name / Number / Tag), the open/close emit, and the
 * bulk-action handlers (Add Tag / Remove Tag / Enable / Disable)
 * that paint per-row through the host IdnodeGrid's inline-edit
 * commit handle.
 *
 * The IdnodeGrid itself is stubbed (its own tests cover its
 * behaviour); the drawer here is glue + bulk-action builders.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, ref } from 'vue'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import type { Option } from '@/components/idnode-fields/deferredEnum'

vi.mock('@/composables/useI18n', () => {
  const tFn = (s: string, ...args: Array<string | number>) =>
    s.replace(/\{(\d+)\}/g, (_m, i) => String(args[Number(i)] ?? _m))
  return {
    /* `t` is also imported as a free function by some shared
     * components (SettingsPopover etc.) — mock both surfaces. */
    t: tFn,
    useI18n: () => ({ t: tFn }),
  }
})

/* The drawer's discard-unsaved-changes guard uses
 * `useConfirmDialog` which under the hood calls PrimeVue's
 * `useConfirm` — that needs the ConfirmationService plugin
 * registered at app boot. Tests don't bootstrap the app, so
 * mock the composable surface. The `confirmAskMock` ref lets
 * tests flip the user's pretend choice between "Discard"
 * (true) and "Keep editing" (false). */
const confirmAskMock = vi.fn(async () => true)
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: confirmAskMock }),
}))

/* Channel-tag list — used by the Add Tag picker children + tag
 * label resolution. Wrapped in an arrow so vi.mock's hoisting
 * doesn't trip a TDZ error on TAGS. */
const fetchEnumMock = vi.fn<(d: unknown) => Promise<Option[]>>()
vi.mock('@/components/idnode-fields/deferredEnum', async (orig) => {
  const actual = await orig<typeof import('@/components/idnode-fields/deferredEnum')>()
  return {
    ...actual,
    fetchDeferredEnum: (d: unknown) => fetchEnumMock(d),
  }
})

const TAGS: Option[] = [
  { key: 'tag-hd', val: 'HD' },
  { key: 'tag-music', val: 'Music' },
  { key: 'tag-news', val: 'News' },
  { key: 'tag-sports', val: 'Sports' },
]

/* Stub PrimeVue's Drawer — just renders the default slot so tests
 * can find the grid + ActionMenu inside without depending on
 * PrimeVue's overlay teleport mechanics. */
vi.mock('primevue/drawer', () => ({
  default: defineComponent({
    name: 'Drawer',
    props: ['visible'],
    emits: ['update:visible'],
    setup(_, { slots }) {
      return () =>
        h(
          'div',
          { class: 'drawer-stub', 'data-visible': String(_.visible) },
          [slots.header?.(), slots.default?.()],
        )
    },
  }),
}))

/* Stub IdnodeGrid — capture its props + slot scope + the
 * commitCell spy. The Drawer's gridRef.value?.inlineEdit?.commitCell
 * chain pulls commitCell off the exposed object; we capture it
 * here so tests can assert against the spy directly. */
const gridProps = ref<Record<string, unknown> | null>(null)
const editingSlotRef = ref<((p: { selection: unknown[] }) => unknown) | null>(
  null,
)
const commitCellMock = vi.fn()
const storeUpdateMock = vi.fn()

/* The stub exposes a fake `effectiveEntries` reading from this
 * ref so per-test fixtures can drive the Renumber actions
 * against a known visible-rows set. */
const stubbedVisibleRows = ref<Array<{ uuid: string; number?: number | string }>>(
  [],
)
/* Stubs for the new IdnodeGrid surface the drawer header's
 * SettingsPopover reads (`isAtDefaults`) and writes
 * (`resetGridPrefs`). The ref-of-boolean shape mirrors the
 * exposed Vue computed; tests flip it to assert the popover's
 * disabled state. */
const stubbedIsAtDefaults = ref<boolean>(true)
const resetGridPrefsMock = vi.fn()

/* Production IdnodeGrid exposes `inlineEdit.dirtyMap` as a
 * `Ref<Map<uuid, Map<field, value>>>` — nested refs in
 * `defineExpose` are NOT auto-unwrapped per Vue 3's docs, so
 * the drawer reads `.value.size`. Mirror the shape here. Tests
 * flip the inner Map to drive the dirty branch of the
 * attempt-close discard guard. */
const stubbedDirtyMap = { value: new Map<string, Map<string, unknown>>() }
vi.mock('@/components/IdnodeGrid.vue', () => ({
  default: defineComponent({
    name: 'IdnodeGrid',
    props: {
      endpoint: { type: String, default: '' },
      columns: { type: Array, default: () => [] },
      storeKey: { type: String, default: '' },
      defaultSort: { type: Object, default: () => ({}) },
      extraParams: { type: Object, default: () => ({}) },
      virtualScrollerOptions: { type: Object, default: () => ({}) },
      countLabel: { type: String, default: '' },
      editMode: { type: String, default: undefined },
      alwaysEdit: { type: Boolean, default: false },
      reorderableRows: { type: Boolean, default: false },
      reorderField: { type: String, default: 'number' },
      entityClass: { type: String, default: '' },
      columnActions: { type: Object, default: () => ({}) },
      clientFilter: { type: Function, default: undefined },
      reorderPreserveMinor: { type: Boolean, default: false },
    },
    setup(props, { slots, expose }) {
      gridProps.value = props as Record<string, unknown>
      editingSlotRef.value = (slots.editingActions ?? null) as
        | ((p: { selection: unknown[] }) => unknown)
        | null
      expose({
        inlineEdit: {
          commitCell: commitCellMock,
          /* `dirtyMap` is a Ref<Map> in production (nested refs
           * aren't auto-unwrapped); see the const above. */
          dirtyMap: stubbedDirtyMap,
        },
        store: { update: storeUpdateMock },
        /* Stub the displayed-rows accessor so the Renumber
         * actions can iterate over a predictable test set.
         * Plain array (post-auto-unwrap shape). */
        effectiveEntries: stubbedVisibleRows.value,
        /* Surface the new View options popover reads. The ref
         * here so a test can flip the value mid-mount and assert
         * the popover's disabled state. */
        isAtDefaults: stubbedIsAtDefaults,
        resetGridPrefs: resetGridPrefsMock,
      })
      /* Render the editingActions slot inline so tests can find
       * controls the drawer places inside it (Include disabled
       * checkbox, ActionMenu). Passes an empty selection — the
       * bulk-action tests reach the slot via editingSlotRef and
       * supply their own selections. */
      return () =>
        h('div', { class: 'grid-stub' }, [
          slots.editingActions?.({ selection: [] }),
        ])
    },
  }),
}))

import ChannelManageDrawer from '../ChannelManageDrawer.vue'

let wrapper: VueWrapper

beforeEach(() => {
  gridProps.value = null
  editingSlotRef.value = null
  fetchEnumMock.mockReset()
  fetchEnumMock.mockResolvedValue(TAGS)
  commitCellMock.mockReset()
  storeUpdateMock.mockReset()
  resetGridPrefsMock.mockReset()
  stubbedVisibleRows.value = []
  stubbedIsAtDefaults.value = true
  stubbedDirtyMap.value.clear()
  confirmAskMock.mockReset()
  confirmAskMock.mockResolvedValue(true)
})

afterEach(() => {
  wrapper?.unmount()
})

function mountDrawer() {
  wrapper = mount(ChannelManageDrawer, {
    props: { visible: true },
    attachTo: document.body,
  })
}

describe('ChannelManageDrawer — IdnodeGrid wiring', () => {
  it('passes the slimmed-down column set (Enabled / Name / Number / Tags) to IdnodeGrid', async () => {
    mountDrawer()
    await flushPromises()
    expect(gridProps.value).not.toBeNull()
    const fields = (gridProps.value!.columns as Array<{ field: string }>).map(
      (c) => c.field,
    )
    expect(fields).toEqual(['enabled', 'name', 'number', 'tags'])
  })

  it('configures the grid for the reorganise workflow (alwaysEdit + reorderableRows + fixed sort)', async () => {
    mountDrawer()
    await flushPromises()
    expect(gridProps.value).toMatchObject({
      endpoint: 'channel/grid',
      storeKey: 'config-channel-manage',
      editMode: 'cell',
      alwaysEdit: true,
      reorderableRows: true,
      reorderField: 'number',
      defaultSort: { key: 'number', dir: 'ASC' },
    })
  })

  it('hides every per-column kebab menu via columnActions = {} (drawer\'s fixed-layout intent)', async () => {
    mountDrawer()
    await flushPromises()
    /* `{}` reads as "no per-column actions supported" → DataGrid
     * stops rendering the kebab entirely. The drawer's
     * fixed-sort + no-filter + fixed-cols design means sort /
     * filter / hide / reset-width would all be dead controls. */
    expect(gridProps.value!.columnActions).toEqual({})
  })

  it('passes a clientFilter predicate that respects the dirty enabled value', async () => {
    /* Server-side enabled filter is gone — replaced by a
     * client-side predicate so dirty toggles (chip-cell-style
     * commits, bulk Enable/Disable) take effect on visibility
     * immediately, without waiting for Save + reload. */
    mountDrawer()
    await flushPromises()
    const filter = gridProps.value!.clientFilter as (
      row: { uuid: string; enabled?: unknown },
      dirty: ReadonlyMap<string, unknown> | undefined,
    ) => boolean
    expect(typeof filter).toBe('function')
    /* Server-enabled, no dirty → visible (includeDisabled off). */
    expect(filter({ uuid: 'a', enabled: true }, undefined)).toBe(true)
    /* Server-disabled, no dirty → hidden. */
    expect(filter({ uuid: 'a', enabled: false }, undefined)).toBe(false)
    /* Server-enabled, dirty to disabled → hidden (effective false). */
    const dirtyDisabled = new Map<string, unknown>([['enabled', false]])
    expect(filter({ uuid: 'a', enabled: true }, dirtyDisabled)).toBe(false)
    /* Server-disabled, dirty to enabled → visible (effective true). */
    const dirtyEnabled = new Map<string, unknown>([['enabled', true]])
    expect(filter({ uuid: 'a', enabled: false }, dirtyEnabled)).toBe(true)
  })

  it('Include disabled checkbox shows every row regardless of effective enabled', async () => {
    mountDrawer()
    await flushPromises()
    const checkbox = wrapper.find('input[type="checkbox"]')
    expect(checkbox.exists()).toBe(true)
    await checkbox.setValue(true)
    await flushPromises()
    const filter = gridProps.value!.clientFilter as (
      row: { uuid: string; enabled?: unknown },
      dirty: ReadonlyMap<string, unknown> | undefined,
    ) => boolean
    expect(filter({ uuid: 'a', enabled: true }, undefined)).toBe(true)
    expect(filter({ uuid: 'a', enabled: false }, undefined)).toBe(true)
    /* Dirty values irrelevant once includeDisabled is on. */
    const dirtyDisabled = new Map<string, unknown>([['enabled', false]])
    expect(filter({ uuid: 'a', enabled: true }, dirtyDisabled)).toBe(true)
  })

  it('clean drawer: Drawer close dispatches update:visible(false) with no confirm', async () => {
    mountDrawer()
    await flushPromises()
    const drawer = wrapper.find('.drawer-stub')
    expect(drawer.exists()).toBe(true)
    /* dirtyMap is empty (beforeEach clear) → attemptClose
     * short-circuits past the confirm and emits close. */
    await wrapper
      .findComponent({ name: 'Drawer' })
      .vm.$emit('update:visible', false)
    await flushPromises()
    expect(confirmAskMock).not.toHaveBeenCalled()
    expect(wrapper.emitted('update:visible')).toBeDefined()
    expect(wrapper.emitted('update:visible')![0]).toEqual([false])
  })

  it('dirty drawer: close prompts confirm; on Discard → emits update:visible(false)', async () => {
    /* Mark a row dirty so attemptClose hits the confirm branch. */
    stubbedDirtyMap.value.set('ch1', new Map([['enabled', false]]))
    confirmAskMock.mockResolvedValueOnce(true) // user picked "Discard"
    mountDrawer()
    await flushPromises()
    await wrapper
      .findComponent({ name: 'Drawer' })
      .vm.$emit('update:visible', false)
    await flushPromises()
    expect(confirmAskMock).toHaveBeenCalledTimes(1)
    /* Message + severity reflect IdnodeEditor's discard-prompt
     * shape — destructive action with explicit Discard / Keep
     * editing labels. */
    expect(confirmAskMock).toHaveBeenCalledWith(
      'Discard unsaved channel changes?',
      expect.objectContaining({
        severity: 'danger',
        acceptLabel: 'Discard',
        rejectLabel: 'Keep editing',
      }),
    )
    expect(wrapper.emitted('update:visible')).toBeDefined()
    expect(wrapper.emitted('update:visible')![0]).toEqual([false])
  })

  it('dirty drawer: close prompts confirm; on Keep editing → does NOT emit close', async () => {
    stubbedDirtyMap.value.set('ch1', new Map([['enabled', false]]))
    confirmAskMock.mockResolvedValueOnce(false) // user picked "Keep editing"
    mountDrawer()
    await flushPromises()
    await wrapper
      .findComponent({ name: 'Drawer' })
      .vm.$emit('update:visible', false)
    await flushPromises()
    expect(confirmAskMock).toHaveBeenCalledTimes(1)
    /* Drawer stays open: no close emit, parent doesn't flip
     * visible, computed proxy keeps PrimeVue's visible=true. */
    expect(wrapper.emitted('update:visible')).toBeUndefined()
  })

  it('opts into drag-reorder minor preservation (reorderPreserveMinor=true)', async () => {
    mountDrawer()
    await flushPromises()
    expect(gridProps.value!.reorderPreserveMinor).toBe(true)
  })
})

describe('ChannelManageDrawer — Renumber actions (R1)', () => {
  type ActionShape = {
    id: string
    label: string
    children?: Array<{ id: string; label: string; onClick?: () => void }>
  }

  function findRenumberActions(): ActionShape[] {
    const node = editingSlotRef.value?.({ selection: [] })
    const all: ActionShape[][] = []
    function walk(n: unknown): void {
      if (!n) return
      if (Array.isArray(n)) {
        for (const c of n) walk(c)
        return
      }
      if (typeof n !== 'object') return
      const v = n as { props?: { actions?: unknown[] }; children?: unknown }
      if (Array.isArray(v.props?.actions)) {
        all.push(v.props.actions as ActionShape[])
      }
      walk(v.children)
    }
    walk(node)
    const renumber = all.find((arr) =>
      arr.some((a) => a.id === 'manage-renumber'),
    )
    return renumber ?? []
  }

  it('exposes two child actions: as integers + preserving sub-channels', async () => {
    mountDrawer()
    await flushPromises()
    const actions = findRenumberActions()
    const renumber = actions.find((a) => a.id === 'manage-renumber')
    expect(renumber).toBeDefined()
    const childIds = renumber!.children!.map((c) => c.id)
    expect(childIds).toEqual([
      'manage-renumber-integers',
      'manage-renumber-preserve',
    ])
  })

  it('"as integers" commits 1, 2, 3, … in display order, flattening any minors', async () => {
    stubbedVisibleRows.value = [
      { uuid: 'a', number: 0 },
      { uuid: 'b', number: 5 },
      { uuid: 'c', number: 5.1 },
    ]
    mountDrawer()
    await flushPromises()
    const actions = findRenumberActions()
    actions
      .find((a) => a.id === 'manage-renumber')!
      .children!.find((c) => c.id === 'manage-renumber-integers')!
      .onClick!()
    expect(commitCellMock).toHaveBeenCalledTimes(3)
    expect(commitCellMock).toHaveBeenCalledWith('a', 'number', 1)
    expect(commitCellMock).toHaveBeenCalledWith('b', 'number', 2)
    expect(commitCellMock).toHaveBeenCalledWith('c', 'number', 3)
  })

  it('"preserving sub-channels" keeps minors and clusters contiguous-major runs', async () => {
    stubbedVisibleRows.value = [
      { uuid: 'a', number: 3 },
      { uuid: 'b', number: 5 },
      { uuid: 'c', number: 5.1 },
      { uuid: 'd', number: 7 },
    ]
    mountDrawer()
    await flushPromises()
    const actions = findRenumberActions()
    actions
      .find((a) => a.id === 'manage-renumber')!
      .children!.find((c) => c.id === 'manage-renumber-preserve')!
      .onClick!()
    expect(commitCellMock).toHaveBeenCalledTimes(4)
    expect(commitCellMock).toHaveBeenCalledWith('a', 'number', 1)
    expect(commitCellMock).toHaveBeenCalledWith('b', 'number', 2)
    expect(commitCellMock).toHaveBeenCalledWith('c', 'number', 2.1)
    expect(commitCellMock).toHaveBeenCalledWith('d', 'number', 3)
  })
})

describe('ChannelManageDrawer — bulk actions', () => {
  type ActionShape = {
    id: string
    label: string
    disabled?: boolean
    children?: Array<{ id: string; label: string; onClick?: () => void }>
    onClick?: () => void
  }

  /* Collect EVERY ActionMenu's `actions` array in the
   * editingActions slot — there are now two (Renumber + bulk).
   * Return the array that contains the requested id. */
  function findActionsArrayContaining(
    selection: unknown[],
    needleId: string,
  ): ActionShape[] | null {
    const node = editingSlotRef.value?.({ selection })
    const results: ActionShape[][] = []
    function walk(n: unknown): void {
      if (!n) return
      if (Array.isArray(n)) {
        for (const c of n) walk(c)
        return
      }
      if (typeof n !== 'object') return
      const v = n as {
        props?: { actions?: unknown[] } | null
        children?: unknown
      }
      if (Array.isArray(v.props?.actions)) {
        results.push(v.props.actions as ActionShape[])
      }
      walk(v.children)
    }
    walk(node)
    return results.find((arr) => arr.some((a) => a.id === needleId)) ?? null
  }

  function actionsForSelection(selection: unknown[]) {
    /* Default helper returns the bulk-actions array (the one
     * containing `manage-add-tag`); existing tests target it. */
    return findActionsArrayContaining(selection, 'manage-add-tag')
  }

  it('Add Tag children list every available tag', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [
      { uuid: 'ch1', tags: [] },
      { uuid: 'ch2', tags: [] },
    ]
    const actions = actionsForSelection(sel)!
    const addAction = actions.find((a) => a.id === 'manage-add-tag')!
    expect(addAction.children?.map((c) => c.label)).toEqual([
      'HD',
      'Music',
      'News',
      'Sports',
    ])
    expect(addAction.disabled).toBe(false)
  })

  it('Add Tag picks an HD tag → commits per row, preserving each row\'s existing tags', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [
      { uuid: 'ch1', tags: ['tag-news'] },
      { uuid: 'ch2', tags: ['tag-sports'] },
      { uuid: 'ch3', tags: [] },
    ]
    const actions = actionsForSelection(sel)!
    const addHd = actions
      .find((a) => a.id === 'manage-add-tag')!
      .children!.find((c) => c.label === 'HD')!
    addHd.onClick!()

    const commit = commitCellMock
    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('ch1', 'tags', ['tag-news', 'tag-hd'])
    expect(commit).toHaveBeenCalledWith('ch2', 'tags', ['tag-sports', 'tag-hd'])
    expect(commit).toHaveBeenCalledWith('ch3', 'tags', ['tag-hd'])
  })

  it('Add Tag skips a row that already has the tag (idempotent)', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [
      { uuid: 'ch1', tags: ['tag-hd', 'tag-news'] },
      { uuid: 'ch2', tags: [] },
    ]
    const actions = actionsForSelection(sel)!
    actions
      .find((a) => a.id === 'manage-add-tag')!
      .children!.find((c) => c.label === 'HD')!
      .onClick!()

    const commit = commitCellMock
    /* Only ch2 gets a commit — ch1 already has HD. */
    expect(commit).toHaveBeenCalledTimes(1)
    expect(commit).toHaveBeenCalledWith('ch2', 'tags', ['tag-hd'])
  })

  it('Remove Tag children show only tags present on the selection', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [
      { uuid: 'ch1', tags: ['tag-hd', 'tag-news'] },
      { uuid: 'ch2', tags: ['tag-hd'] },
    ]
    const actions = actionsForSelection(sel)!
    const removeAction = actions.find((a) => a.id === 'manage-remove-tag')!
    expect(
      removeAction.children!.map((c) => c.label).sort((a, b) => a.localeCompare(b)),
    ).toEqual(['HD', 'News'])
  })

  it('Remove Tag commits per row, only for rows that have the tag', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [
      { uuid: 'ch1', tags: ['tag-hd', 'tag-news'] },
      { uuid: 'ch2', tags: ['tag-hd'] },
      { uuid: 'ch3', tags: ['tag-news'] },
    ]
    const actions = actionsForSelection(sel)!
    actions
      .find((a) => a.id === 'manage-remove-tag')!
      .children!.find((c) => c.label === 'HD')!
      .onClick!()

    const commit = commitCellMock
    expect(commit).toHaveBeenCalledTimes(2)
    expect(commit).toHaveBeenCalledWith('ch1', 'tags', ['tag-news'])
    expect(commit).toHaveBeenCalledWith('ch2', 'tags', [])
  })

  it('Enable commits enabled=true for every selected row', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [{ uuid: 'ch1' }, { uuid: 'ch2' }, { uuid: 'ch3' }]
    const actions = actionsForSelection(sel)!
    actions.find((a) => a.id === 'manage-enable')!.onClick!()

    const commit = commitCellMock
    expect(commit).toHaveBeenCalledTimes(3)
    expect(commit).toHaveBeenCalledWith('ch1', 'enabled', true)
    expect(commit).toHaveBeenCalledWith('ch2', 'enabled', true)
    expect(commit).toHaveBeenCalledWith('ch3', 'enabled', true)
  })

  it('Disable commits enabled=false for every selected row', async () => {
    mountDrawer()
    await flushPromises()
    const sel = [{ uuid: 'ch1' }, { uuid: 'ch2' }]
    const actions = actionsForSelection(sel)!
    actions.find((a) => a.id === 'manage-disable')!.onClick!()

    const commit = commitCellMock
    expect(commit).toHaveBeenCalledTimes(2)
    expect(commit).toHaveBeenCalledWith('ch1', 'enabled', false)
    expect(commit).toHaveBeenCalledWith('ch2', 'enabled', false)
  })

  it('all bulk actions are disabled on empty selection', async () => {
    mountDrawer()
    await flushPromises()
    const actions = actionsForSelection([])!
    expect(actions.find((a) => a.id === 'manage-add-tag')!.disabled).toBe(true)
    expect(actions.find((a) => a.id === 'manage-remove-tag')!.disabled).toBe(true)
    expect(actions.find((a) => a.id === 'manage-enable')!.disabled).toBe(true)
    expect(actions.find((a) => a.id === 'manage-disable')!.disabled).toBe(true)
  })
})

describe('ChannelManageDrawer — View options popover', () => {
  it('renders a "View options" trigger in the drawer header', () => {
    mountDrawer()
    const trigger = wrapper.find(
      '.channel-manage-drawer__view-options .settings-popover__btn',
    )
    expect(trigger.exists()).toBe(true)
    expect(trigger.attributes('aria-label')).toBe('View options')
  })

  it('Reset button is disabled while the grid layout matches defaults', async () => {
    stubbedIsAtDefaults.value = true
    mountDrawer()
    await wrapper
      .find('.channel-manage-drawer__view-options .settings-popover__btn')
      .trigger('click')
    const resetBtn = wrapper.find(
      '.channel-manage-drawer__view-options .settings-popover__reset',
    )
    expect(resetBtn.exists()).toBe(true)
    expect((resetBtn.element as HTMLButtonElement).disabled).toBe(true)
  })

  it('Reset button is enabled once the grid layout deviates from defaults', async () => {
    stubbedIsAtDefaults.value = false
    mountDrawer()
    await wrapper
      .find('.channel-manage-drawer__view-options .settings-popover__btn')
      .trigger('click')
    const resetBtn = wrapper.find(
      '.channel-manage-drawer__view-options .settings-popover__reset',
    )
    expect((resetBtn.element as HTMLButtonElement).disabled).toBe(false)
  })

  it('clicking Reset calls IdnodeGrid.resetGridPrefs AND clears the persisted drawer width', async () => {
    stubbedIsAtDefaults.value = false
    /* Seed a non-default persisted drawer width — assert Reset
     * clears the localStorage entry alongside the grid prefs. */
    localStorage.setItem('channel-manage-drawer:width', '900')
    mountDrawer()
    const trigger = wrapper.find(
      '.channel-manage-drawer__view-options .settings-popover__btn',
    )
    await trigger.trigger('click')
    await wrapper
      .find('.channel-manage-drawer__view-options .settings-popover__reset')
      .trigger('click')
    expect(resetGridPrefsMock).toHaveBeenCalledTimes(1)
    expect(localStorage.getItem('channel-manage-drawer:width')).toBeNull()
    /* Popover closed afterwards — panel no longer in the DOM. */
    expect(
      wrapper
        .find('.channel-manage-drawer__view-options .settings-popover__panel')
        .exists(),
    ).toBe(false)
  })

  it('Reset stays enabled when only the drawer width deviates (grid layout at defaults)', async () => {
    stubbedIsAtDefaults.value = true
    localStorage.setItem('channel-manage-drawer:width', '900')
    mountDrawer()
    await wrapper
      .find('.channel-manage-drawer__view-options .settings-popover__btn')
      .trigger('click')
    const resetBtn = wrapper.find(
      '.channel-manage-drawer__view-options .settings-popover__reset',
    )
    expect((resetBtn.element as HTMLButtonElement).disabled).toBe(false)
    localStorage.removeItem('channel-manage-drawer:width')
  })

  it('double-clicking the resize handle resets just the drawer width (grid prefs untouched)', async () => {
    stubbedIsAtDefaults.value = true
    localStorage.setItem('channel-manage-drawer:width', '900')
    mountDrawer()
    const handle = wrapper.find('.channel-manage-drawer__resize-handle')
    expect(handle.exists()).toBe(true)
    await handle.trigger('dblclick')
    expect(localStorage.getItem('channel-manage-drawer:width')).toBeNull()
    /* Power-user shortcut: grid prefs NOT touched on dblclick. */
    expect(resetGridPrefsMock).not.toHaveBeenCalled()
  })

  it('omits the divider above Reset when no slot content is passed (single-action panel)', async () => {
    mountDrawer()
    await wrapper
      .find('.channel-manage-drawer__view-options .settings-popover__btn')
      .trigger('click')
    /* Drawer's popover ships no default-slot content — the
     * SettingsPopover divider above the Reset footer should be
     * suppressed so the panel doesn't show a stray top-edge line. */
    expect(
      wrapper
        .find('.channel-manage-drawer__view-options .settings-popover__divider')
        .exists(),
    ).toBe(false)
  })
})
