// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- stand-ins for IdnodeGrid,
 * its toolbar menu, the Service Mapper and the components a grid view
 * test does not look at. */

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
 * vue-router and the composables. What they see goes to `captured`.
 */
import { afterEach, beforeEach, vi } from 'vitest'
import { defineComponent, h } from 'vue'
import { enableAutoUnmount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import type { ActionDef } from '@/types/action'
import type { ColumnDef } from '@/types/column'

export const captured = {
  /* The props the view handed to IdnodeGrid. */
  gridProps: null as Record<string, unknown> | null,
  /* The rows the toolbar slot gets as the grid selection. */
  selection: [] as Array<Record<string, unknown>>,
  /* The actions the view handed to ActionMenu. */
  actions: [] as ActionDef[],
  /* The props ServiceMapperDialog was last rendered with. */
  mapper: {} as Record<string, unknown>,
}

function gridStub(withToolbar: boolean) {
  return {
    default: defineComponent({
      name: 'IdnodeGrid',
      inheritAttrs: false,
      setup(_, { attrs, slots }) {
        captured.gridProps = attrs as Record<string, unknown>
        const toolbar = () =>
          slots.toolbarActions?.({ selection: captured.selection, clearSelection: () => undefined })
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

export const vueRouter = {
  useRoute: () => ({ query: {}, hash: '', fullPath: '/' }),
  useRouter: () => ({ push: vi.fn(() => Promise.resolve()), replace: vi.fn() }),
}

export const confirmDialog = {
  useConfirmDialog: () => ({ ask: vi.fn(() => Promise.resolve(true)) }),
}

export const toastNotify = {
  useToastNotify: () => ({ success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }),
}

/* The columns the view handed to IdnodeGrid. */
export function gridColumns(): ColumnDef[] {
  return (captured.gridProps?.columns as ColumnDef[] | undefined) ?? []
}

export function gridColumn(field: string): ColumnDef | undefined {
  return gridColumns().find((c) => c.field === field)
}

/* A toolbar action the view handed to ActionMenu. */
export function toolbarAction(id: string): ActionDef {
  const found = captured.actions.find((a) => a.id === id)
  if (!found) throw new Error(`no ${id} action captured`)
  return found
}

/* Unmount after each test and start the next one with a fresh Pinia
 * and nothing captured. Call it once at the top of the test file. */
export function resetEachTest(): void {
  enableAutoUnmount(afterEach)
  beforeEach(() => {
    setActivePinia(createPinia())
    captured.gridProps = null
    captured.selection = []
    captured.actions = []
    captured.mapper = {}
  })
}
