// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- inline stub components
 * for the grid and its heavy drawer/editor companions; each is a
 * throwaway harness, not a real component. */

/*
 * UpcomingView — pins the `duplicates: 0` grid parameter (issue
 * #2207). The server's grid_upcoming endpoint INCLUDES dedup-skipped
 * rerun entries by default; without the param they render as ordinary
 * "Scheduled for recording" rows although they will not record. The
 * ExtJS Upcoming grid has always passed duplicates=0 (dvr.js) — this
 * test keeps the Vue view from silently regressing to the default.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h } from 'vue'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import UpcomingView from '../UpcomingView.vue'

/* Capture the props UpcomingView hands to the grid; the grid itself
 * (store wiring, fetching, virtual scroller) is out of scope here. */
const gridProps = vi.hoisted(() => ({ current: null as Record<string, unknown> | null }))
vi.mock('@/components/IdnodeGrid.vue', () => ({
  default: defineComponent({
    name: 'IdnodeGrid',
    inheritAttrs: false,
    setup(_, { attrs }) {
      gridProps.current = attrs as Record<string, unknown>
      return () => h('div', { class: 'idnode-grid-stub' })
    },
  }),
}))
/* The drawer/editor/dialog companions drag Pinia-heavy dependency
 * trees — stub them out; this test only inspects the grid mount. */
vi.mock('@/views/epg/EpgEventDrawer.vue', () => ({
  default: defineComponent({ name: 'EpgEventDrawer', render: () => null }),
}))
vi.mock('@/components/IdnodeEditor.vue', () => ({
  default: defineComponent({ name: 'IdnodeEditor', render: () => null }),
}))
vi.mock('@/components/EpgRelatedDialog.vue', () => ({
  default: defineComponent({ name: 'EpgRelatedDialog', render: () => null }),
}))
/* useEditorMode syncs the edit target with the route — no real
 * router in the harness. */
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: {}, hash: '', fullPath: '/dvr/upcoming' }),
  useRouter: () => ({ push: vi.fn(), replace: vi.fn() }),
}))
/* PrimeVue's injection-based confirm/toast services aren't installed
 * in the harness — same mock idiom as useBulkAction.test.ts. */
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(() => Promise.resolve(true)) }),
}))
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: vi.fn(), error: vi.fn(), info: vi.fn() }),
}))

enableAutoUnmount(afterEach)

describe('UpcomingView — dedup-skipped entries', () => {
  it('requests the grid with duplicates: 0 (hide skipped reruns, ExtJS parity)', () => {
    setActivePinia(createPinia())
    mount(UpcomingView)
    expect(gridProps.current?.endpoint).toBe('dvr/entry/grid_upcoming')
    expect(gridProps.current?.['extra-params']).toEqual({ duplicates: 0 })
  })
})
