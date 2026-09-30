// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * The Users views declare their `password` column masked
 * (`format: fmtPassword`), so the grid never shows the stored value,
 * whether or not the class metadata that flags it has loaded. The
 * grid is stubbed and only the columns it receives are checked;
 * IdnodeGrid's own tests cover how it renders a masked column.
 */

import { describe, expect, it, vi } from 'vitest'
import type { Component } from 'vue'
import { mount } from '@vue/test-utils'
import type { ColumnDef } from '@/types/column'
import { fmtPassword } from '@/utils/formatPassword'
import ConfigUsersPasswordsView from '../ConfigUsersPasswordsView.vue'
import ConfigUsersAccessEntriesView from '../ConfigUsersAccessEntriesView.vue'

vi.mock('@/composables/useI18n', () => ({
  t: (s: string) => s,
  useI18n: () => ({ t: (s: string) => s }),
}))

vi.mock('@/composables/useEditorMode', async () => {
  const { ref } = await import('vue')
  return {
    useEditorMode: () => ({
      editingUuid: ref(null),
      editingUuids: ref(null),
      creatingBase: ref(null),
      gridRef: ref(null),
      editorLevel: ref('basic'),
      editorList: ref(''),
      openEditor: vi.fn(),
      openCreate: vi.fn(),
      closeEditor: vi.fn(),
      flipToEdit: vi.fn(),
    }),
  }
})

vi.mock('@/composables/useBulkAction', async () => {
  const { ref } = await import('vue')
  return { useBulkAction: () => ({ inflight: ref(false), run: vi.fn() }) }
})

vi.mock('@/composables/useIdnodeMove', async () => {
  const { ref } = await import('vue')
  return {
    useIdnodeMove: () => ({
      moveInflight: ref(false),
      moveSelected: vi.fn(),
      canMove: () => false,
    }),
  }
})

function passwordColumn(view: Component): ColumnDef {
  const wrapper = mount(view, {
    global: { stubs: { IdnodeGrid: true, IdnodeEditor: true, ActionMenu: true } },
  })
  const columns = wrapper.findComponent({ name: 'IdnodeGrid' }).props('columns') as ColumnDef[]
  const col = columns.find((c) => c.field === 'password')
  expect(col).toBeDefined()
  return col as ColumnDef
}

describe('Users views mask the password column statically', () => {
  it.each([
    ['Passwords', ConfigUsersPasswordsView],
    ['Access Entries', ConfigUsersAccessEntriesView],
  ])('%s: password column is masked and not inline-editable', (_name, view) => {
    const col = passwordColumn(view)
    expect(col.format).toBe(fmtPassword)
    expect(col.cellComponent).toBeUndefined()
    expect(col.editable).toBeFalsy()
    expect(col.format?.('hunter2', { uuid: 'a' })).toBe('********')
    expect(col.format?.('', { uuid: 'a' })).toBe('')
  })
})
