// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- stub components for
 * the grid, its toolbar and the editor drawer, each a throwaway
 * harness, not a real component. */

/*
 * Harness for grid view tests: a view that hands IdnodeGrid its
 * toolbar actions and opens IdnodeEditor from them, tested without
 * the real grid and drawer.
 *
 *   - The IdnodeGrid stub renders the toolbar slot with
 *     `harness.selection`.
 *   - The ActionMenu stub records the actions built for it.
 *   - The IdnodeEditor stub records the rows it was opened for.
 *
 * `vi.mock` stays in the test file. Its factories take the stubs
 * from this module, loaded through `vi.hoisted` so it is the same
 * instance the test uses:
 *
 *   const h = await vi.hoisted(() => import('@/test/__helpers__/gridViewHarness'))
 *   vi.mock('@/components/IdnodeGrid.vue', () => h.stubs.grid)
 */
import { afterEach, beforeEach, expect, it } from 'vitest'
import { defineComponent, h, nextTick, type Component } from 'vue'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import ConfirmationService from 'primevue/confirmationservice'
import ToastService from 'primevue/toastservice'
import { createMemoryHistory, createRouter } from 'vue-router'
import type { ActionDef } from '@/types/action'

export const harness = {
  selection: [] as Array<Record<string, unknown>>,
  actions: [] as ActionDef[],
  editor: { uuid: null as unknown, uuids: null as unknown },
}

/* Stand-ins for the mocked modules, in the shape `vi.mock` takes. */
export const stubs = {
  grid: {
    default: defineComponent({
      name: 'IdnodeGrid',
      inheritAttrs: false,
      setup(_, { slots }) {
        return () =>
          h(
            'div',
            { class: 'idnode-grid-stub' },
            slots.toolbarActions?.({ selection: harness.selection, clearSelection: () => {} }),
          )
      },
    }),
  },
  actionMenu: {
    default: defineComponent({
      name: 'ActionMenu',
      props: { actions: { type: Array, default: () => [] } },
      setup(props) {
        return () => {
          harness.actions = props.actions as ActionDef[]
          return null
        }
      },
    }),
  },
  editor: {
    default: defineComponent({
      name: 'IdnodeEditor',
      props: { uuid: { type: String, default: null }, uuids: { type: Array, default: null } },
      setup(props) {
        return () => {
          harness.editor = { uuid: props.uuid, uuids: props.uuids }
          return null
        }
      },
    }),
  },
  /* A component the test does not look at. */
  blank: (name: string) => ({ default: defineComponent({ name, render: () => null }) }),
}

/* Per test: a fresh Pinia, an empty harness, and the mounted view
 * unmounted afterwards. Call once at the top level. */
export function setupGridViewHarness(): void {
  enableAutoUnmount(afterEach)
  beforeEach(() => {
    setActivePinia(createPinia())
    harness.selection = []
    harness.actions = []
    harness.editor = { uuid: null, uuids: null }
  })
}

/* The toolbar action with this id, as last rendered. */
export function action(id: string): ActionDef {
  const found = harness.actions.find((a) => a.id === id)
  if (!found) throw new Error(`no ${id} action captured`)
  return found
}

/* Mount `view` with rows of these uuids selected in the grid, with
 * the router and the confirm and toast services the app installs. */
export function mountSelected(view: Component, uuids: string[] = []) {
  harness.selection = uuids.map((uuid) => ({ uuid }))
  const router = createRouter({
    history: createMemoryHistory(),
    routes: [{ path: '/:path(.*)*', component: { render: () => null } }],
  })
  return mount(view, { global: { plugins: [router, ConfirmationService, ToastService] } })
}

/* Edit is disabled with nothing selected, opens the plain editor for
 * one row and the multi-edit drawer for several. `tooltips` are the
 * texts for one and for several rows. */
export function runEditActionTests(view: Component, tooltips: [string, string]): void {
  it('is disabled with nothing selected', () => {
    mountSelected(view)
    expect(action('edit').disabled).toBe(true)
  })

  it('edits several rows at once', async () => {
    mountSelected(view, ['r1', 'r2', 'r3'])
    const edit = action('edit')
    expect(edit.disabled).toBe(false)
    expect(edit.tooltip).toBe(tooltips[1])
    await edit.onClick?.()
    await nextTick()
    expect(harness.editor.uuids).toEqual(['r1', 'r2', 'r3'])
  })

  it('keeps the singular tooltip and the plain editor for one row', async () => {
    mountSelected(view, ['r1'])
    const edit = action('edit')
    expect(edit.tooltip).toBe(tooltips[0])
    await edit.onClick?.()
    await nextTick()
    expect(harness.editor.uuid).toBe('r1')
  })
}
