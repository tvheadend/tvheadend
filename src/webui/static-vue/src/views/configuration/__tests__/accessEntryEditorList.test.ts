// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Access Entries: the editor's field list follows Classic's
 * `acleditor.js` list2, including the per-user Default tab
 * (`default_tab`), which the server applies at login
 * (src/webui/comet.c). The editor loads only the listed props, so a
 * field missing here cannot be set at all.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import ConfigUsersAccessEntriesView from '../ConfigUsersAccessEntriesView.vue'

vi.mock('@/composables/useI18n', () => ({
  t: (s: string) => s,
  useI18n: () => ({ t: (s: string) => s }),
}))

const editorModeOpts: Array<{ editList: { value: string }; createList: string }> = []

vi.mock('@/composables/useEditorMode', async () => {
  const { ref } = await import('vue')
  return {
    useEditorMode: (opts: { editList: { value: string }; createList: string }) => {
      editorModeOpts.push(opts)
      return {
        editingUuid: ref(null),
        editingUuids: ref(null),
        creatingBase: ref(null),
        gridRef: ref(null),
        editorLevel: ref(undefined),
        editorList: ref(opts.editList.value),
        openEditor: vi.fn(),
        openCreate: vi.fn(),
        closeEditor: vi.fn(),
        flipToEdit: vi.fn(),
      }
    },
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

describe('Access Entries editor field list', () => {
  it('includes default_tab after langui, in Classic order, for edit and create', () => {
    mount(ConfigUsersAccessEntriesView, {
      global: { stubs: { IdnodeGrid: true, IdnodeEditor: true, ActionMenu: true } },
    })
    const opts = editorModeOpts[editorModeOpts.length - 1]!
    for (const list of [opts.editList.value, opts.createList]) {
      const ids = list.split(',')
      expect(ids).toContain('default_tab')
      expect(ids.indexOf('default_tab')).toBe(ids.indexOf('langui') + 1)
      expect(ids.indexOf('uilevel')).toBe(ids.indexOf('default_tab') + 1)
    }
  })
})
