// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- stand-ins for IdnodeGrid
 * and the components a grid view test does not look at. */

/*
 * Stubs for the tests that mount a grid view and check what it hands
 * to IdnodeGrid. vi.mock() has to stay in the test file, where Vitest
 * hoists it, so the test loads this module in a hoisted block and
 * returns its stubs from the mock factories:
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
import type { ColumnDef } from '@/types/column'

export const captured = {
  /* The props the view handed to IdnodeGrid. */
  gridProps: null as Record<string, unknown> | null,
}

/* IdnodeGrid, recording its props. */
export const idnodeGrid = {
  default: defineComponent({
    name: 'IdnodeGrid',
    inheritAttrs: false,
    setup(_, { attrs }) {
      captured.gridProps = attrs as Record<string, unknown>
      return () => h('div', { class: 'idnode-grid-stub' })
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

/* Unmount after each test and start the next one with a fresh Pinia
 * and nothing captured. Call it once at the top of the test file. */
export function resetEachTest(): void {
  enableAutoUnmount(afterEach)
  beforeEach(() => {
    setActivePinia(createPinia())
    captured.gridProps = null
  })
}
