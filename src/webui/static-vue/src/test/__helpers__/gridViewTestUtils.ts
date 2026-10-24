// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- stand-ins for IdnodeGrid,
 * its toolbar menu, the editor drawer, the Service Mapper and the
 * components a grid view test does not look at. */

/*
 * Stubs for the tests that mount a grid view and check what it hands
 * to IdnodeGrid and its toolbar. vi.mock() has to stay in the test
 * file, where Vitest hoists it, so the test loads this module in a
 * hoisted block and returns its stubs from the mock factories:
 *
 *   const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
 *   vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
 *   vi.mock('vue-router', () => stubs.vueRouter)
 *   stubs.resetEachTest()
 *
 * Each stub is a module: `default` for a component, the hooks for
 * vue-router and the composables, `apiCall` for the API client. What
 * they see goes to `captured`, the toasts and API calls to spies.
 */
import { afterEach, beforeEach, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, type Component } from 'vue'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import type { ActionDef } from '@/types/action'
import type { ColumnDef } from '@/types/column'
import type { GlobalFilterSpec } from '@/types/grid'

export const captured = {
  /* The props the view handed to IdnodeGrid. */
  gridProps: null as Record<string, unknown> | null,
  /* The rows the toolbar slot gets as the grid selection. */
  selection: [] as Array<Record<string, unknown>>,
  /* The actions the view handed to ActionMenu. */
  actions: [] as ActionDef[],
  /* The rows IdnodeEditor was last rendered for. */
  editor: { uuid: null as unknown, uuids: null as unknown },
  /* The props ServiceMapperDialog was last rendered with. */
  mapper: {} as Record<string, unknown>,
  /* The route query the view reads. */
  query: {} as Record<string, string>,
}

/* The clearSelection the toolbar slot gets, to see the view call it. */
export const clearSelection = vi.fn()

function gridStub(withToolbar: boolean) {
  return {
    default: defineComponent({
      name: 'IdnodeGrid',
      inheritAttrs: false,
      setup(_, { attrs, slots }) {
        captured.gridProps = attrs as Record<string, unknown>
        const toolbar = () =>
          slots.toolbarActions?.({ selection: captured.selection, clearSelection })
        return () => h('div', { class: 'idnode-grid-stub' }, withToolbar ? toolbar() : undefined)
      },
    }),
  }
}

/* IdnodeGrid, recording its props. */
export const idnodeGrid = gridStub(false)

/* IdnodeGrid that also renders the toolbar slot for `captured.selection`. */
export const idnodeGridWithToolbar = gridStub(true)

/* ActionMenu, recording the actions. */
export const actionMenu = {
  default: defineComponent({
    name: 'ActionMenu',
    props: { actions: { type: Array, default: () => [] } },
    setup(props) {
      return () => {
        captured.actions = props.actions as ActionDef[]
        return null
      }
    },
  }),
}

/* IdnodeEditor, recording the rows it was opened for. */
export const idnodeEditor = {
  default: defineComponent({
    name: 'IdnodeEditor',
    props: { uuid: { type: String, default: null }, uuids: { type: Array, default: null } },
    setup(props) {
      return () => {
        captured.editor = { uuid: props.uuid, uuids: props.uuids }
        return null
      }
    },
  }),
}

/* ServiceMapperDialog, recording its props. */
export const serviceMapperDialog = {
  default: defineComponent({
    name: 'ServiceMapperDialog',
    props: {
      visible: { type: Boolean, default: false },
      preselect: { type: Object, default: null },
      mapAll: { type: Boolean, default: false },
    },
    setup(props) {
      return () => {
        captured.mapper = { ...props }
        return null
      }
    },
  }),
}

/* Any component the test does not look at. */
export const emptyComponent = {
  default: defineComponent({ name: 'EmptyStub', render: () => null }),
}

export const router = {
  push: vi.fn(() => Promise.resolve()),
  replace: vi.fn(() => Promise.resolve()),
}

export const vueRouter = {
  useRoute: () => ({ query: captured.query, hash: '', fullPath: '/' }),
  useRouter: () => router,
}

export const confirmDialog = {
  useConfirmDialog: () => ({ ask: vi.fn(() => Promise.resolve(true)) }),
}

/* The toasts the view shows. */
export const toast = { success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }

export const toastNotify = {
  useToastNotify: () => toast,
}

/* The API calls the view makes, answered with `{}` unless a test
 * says otherwise. */
export const apiMock = vi.fn()

export const apiClient = {
  apiCall: (...args: unknown[]) => apiMock(...args),
}

/* The columns the view handed to IdnodeGrid. */
export function gridColumns(): ColumnDef[] {
  return (captured.gridProps?.columns as ColumnDef[] | undefined) ?? []
}

export function gridColumn(field: string): ColumnDef | undefined {
  return gridColumns().find((c) => c.field === field)
}

/* A global filter the view handed to IdnodeGrid. */
export function gridFilter(key: string): GlobalFilterSpec | undefined {
  const filters = captured.gridProps?.filters as GlobalFilterSpec[] | undefined
  return filters?.find((f) => f.key === key)
}

/* A toolbar action the view handed to ActionMenu. */
export function toolbarAction(id: string): ActionDef {
  const found = captured.actions.find((a) => a.id === id)
  if (!found) throw new Error(`no ${id} action captured`)
  return found
}

/* Unmount after each test and start the next one with a fresh Pinia,
 * nothing captured, no toasts and no API calls. Call it once at the
 * top of the test file. */
export function resetEachTest(): void {
  enableAutoUnmount(afterEach)
  beforeEach(() => {
    setActivePinia(createPinia())
    captured.gridProps = null
    captured.selection = []
    captured.actions = []
    captured.editor = { uuid: null, uuids: null }
    captured.mapper = {}
    captured.query = {}
    router.push.mockClear()
    router.replace.mockClear()
    clearSelection.mockClear()
    for (const fn of Object.values(toast)) fn.mockReset()
    apiMock.mockReset()
    apiMock.mockResolvedValue({})
  })
}

/* Mount `view` with rows of these uuids selected in the grid. */
export function mountSelected(view: Component, uuids: string[] = []) {
  captured.selection = uuids.map((uuid) => ({ uuid }))
  return mount(view)
}

/* Edit is disabled with nothing selected, opens the plain editor for
 * one row and the multi-edit drawer for several. `tooltips` are the
 * texts for one and for several rows. The test file mocks IdnodeGrid
 * with the toolbar, ActionMenu and IdnodeEditor with these stubs. */
export function runEditActionTests(view: Component, tooltips: [string, string]): void {
  it('is disabled with nothing selected', () => {
    mountSelected(view)
    expect(toolbarAction('edit').disabled).toBe(true)
  })

  it('edits several rows at once', async () => {
    mountSelected(view, ['r1', 'r2', 'r3'])
    const edit = toolbarAction('edit')
    expect(edit.disabled).toBe(false)
    expect(edit.tooltip).toBe(tooltips[1])
    await edit.onClick?.()
    await nextTick()
    expect(captured.editor.uuids).toEqual(['r1', 'r2', 'r3'])
  })

  it('keeps the singular tooltip and the plain editor for one row', async () => {
    mountSelected(view, ['r1'])
    const edit = toolbarAction('edit')
    expect(edit.tooltip).toBe(tooltips[0])
    await edit.onClick?.()
    await nextTick()
    expect(captured.editor.uuid).toBe('r1')
  })
}

interface ForceScanCase {
  view: Component
  /* The toolbar action and the endpoint it posts to. */
  actionId: string
  endpoint: string
  /* Success toasts for one and for two rows. */
  toasts: [string, string]
  /* Error toast text before the server's message. */
  failPrefix: string
}

/* Force Scan posts the selected rows, confirms the started scan in a
 * toast and clears the selection, reports a failure in an error toast
 * and keeps its label while the request is in flight. The test file
 * mocks IdnodeGrid with the toolbar, ActionMenu, useToastNotify and
 * the API client with these stubs. */
export function runForceScanTests(c: ForceScanCase): void {
  async function scan(uuids: string[]) {
    mountSelected(c.view, uuids)
    await toolbarAction(c.actionId).onClick?.()
    await flushPromises()
  }

  it('confirms the started scan with a toast and clears the selection', async () => {
    await scan(['r1', 'r2'])
    expect(apiMock).toHaveBeenCalledWith(c.endpoint, { uuid: JSON.stringify(['r1', 'r2']) })
    expect(toast.success).toHaveBeenCalledWith(c.toasts[1])
    expect(clearSelection).toHaveBeenCalledOnce()
  })

  it('uses the singular text for one row', async () => {
    await scan(['r1'])
    expect(toast.success).toHaveBeenCalledWith(c.toasts[0])
  })

  it('reports a failed request in an error toast', async () => {
    apiMock.mockRejectedValue(new Error('boom'))
    await scan(['r1'])
    expect(toast.success).not.toHaveBeenCalled()
    expect(toast.error).toHaveBeenCalledWith(`${c.failPrefix}: boom`)
  })

  it('keeps the Force Scan label while the request is in flight', async () => {
    let resolve: (v: unknown) => void = () => {}
    apiMock.mockImplementation(() => new Promise((r) => (resolve = r)))
    mountSelected(c.view, ['r1'])
    const pending = toolbarAction(c.actionId).onClick?.()
    await flushPromises()
    expect(toolbarAction(c.actionId).label).toBe('Force Scan')
    expect(toolbarAction(c.actionId).disabled).toBe(true)
    resolve({})
    await pending
  })
}
