// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- inline stub components
 * for the grid, the toolbar and the editor; each is a throwaway
 * harness, not a real component. */

/*
 * Editor drawer title for the grid views that can both add and edit.
 * The first dynamic import of a view transforms its whole dependency
 * tree, hence the generous per-test timeout.
 *
 * For a selection of two or more rows, useEditorMode opens the editor
 * with `editingUuids` and leaves `editingUuid` null. A view that picks
 * its title with `editingUuid ? Edit : Add` then titled a multi-row
 * edit "Add ... (2 entries)". The title has to follow create mode
 * (`creatingBase`) instead.
 *
 * Each view is mounted with the grid, toolbar and editor stubbed: the
 * grid stub renders the toolbar slot with a two-row selection, the
 * ActionMenu stub captures the actions, and the IdnodeEditor stub
 * captures the title it is given.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, type Component } from 'vue'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import type { ActionDef } from '@/types/action'

const captured = vi.hoisted(() => ({
  actions: [] as unknown[],
  editorTitle: undefined as string | undefined,
}))

const SELECTION = [{ uuid: 'row-a' }, { uuid: 'row-b' }]

vi.mock('@/components/IdnodeGrid.vue', () => ({
  default: defineComponent({
    name: 'IdnodeGrid',
    inheritAttrs: false,
    setup(_, { slots }) {
      return () =>
        h(
          'div',
          { class: 'idnode-grid-stub' },
          slots.toolbarActions?.({ selection: SELECTION, clearSelection: () => undefined }),
        )
    },
  }),
}))
vi.mock('@/components/ActionMenu.vue', () => ({
  default: defineComponent({
    name: 'ActionMenu',
    props: { actions: { type: Array, default: () => [] } },
    setup(props) {
      return () => {
        captured.actions = props.actions
        return null
      }
    },
  }),
}))
vi.mock('@/components/IdnodeEditor.vue', () => ({
  default: defineComponent({
    name: 'IdnodeEditor',
    props: { title: { type: String, default: undefined } },
    setup(props) {
      return () => {
        captured.editorTitle = props.title
        return null
      }
    },
  }),
}))
const nullStub = (name: string) => ({
  default: defineComponent({ name, render: () => null }),
})
vi.mock('@/components/ServiceMapperDialog.vue', () => nullStub('ServiceMapperDialog'))
vi.mock('@/components/IdnodePickEntityDialog.vue', () => nullStub('IdnodePickEntityDialog'))
vi.mock('@/components/IdnodePickClassDialog.vue', () => nullStub('IdnodePickClassDialog'))
vi.mock('@/components/EpgRelatedDialog.vue', () => nullStub('EpgRelatedDialog'))
vi.mock('@/views/epg/EpgEventDrawer.vue', () => nullStub('EpgEventDrawer'))
vi.mock('@/views/configuration/ChannelManageDrawer.vue', () => nullStub('ChannelManageDrawer'))
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: {}, hash: '', fullPath: '/' }),
  useRouter: () => ({ push: vi.fn(() => Promise.resolve()), replace: vi.fn() }),
  onBeforeRouteLeave: vi.fn(),
}))
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(() => Promise.resolve(true)) }),
}))
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: vi.fn(), error: vi.fn(), info: vi.fn(), warn: vi.fn() }),
}))
vi.mock('@/api/client', () => ({
  apiCall: vi.fn(() => Promise.resolve({ entries: [] })),
}))

enableAutoUnmount(afterEach)

beforeEach(() => {
  setActivePinia(createPinia())
  captured.actions = []
  captured.editorTitle = undefined
})

/* EsfilterGridView serves every stream filter page and takes the page
 * as props. */
const ESFILTER_VIDEO = {
  apiBase: 'esfilter/video',
  entityClass: 'esfilter_video',
  storeKey: 'config-stream-esfilter-video',
  columns: [],
  entityLabel: 'Video Stream Filter',
}

/* The views, by path under src/views, with the title of their Edit
 * drawer. */
const CASES: Array<[string, string, Record<string, unknown>?]> = [
  ['configuration/ChannelsView', 'Edit Channel'],
  ['configuration/ChannelTagsView', 'Edit Channel Tag'],
  ['configuration/BouquetsView', 'Edit Bouquet'],
  ['configuration/EsfilterGridView', 'Edit Video Stream Filter', ESFILTER_VIDEO],
  ['configuration/ConfigUsersAccessEntriesView', 'Edit Access Entry'],
  ['configuration/ConfigUsersIpBlockingView', 'Edit IP Blocking Entry'],
  ['configuration/ConfigUsersPasswordsView', 'Edit Password'],
  ['configuration/DvbMuxSchedView', 'Edit Mux Scheduler'],
  ['configuration/DvbMuxesView', 'Edit Mux'],
  ['configuration/DvbNetworksView', 'Edit Network'],
  ['configuration/RatingLabelsView', 'Edit Rating Label'],
  ['dvr/AutorecsView', 'Edit Autorec'],
  ['dvr/TimersView', 'Edit Timer'],
  ['dvr/UpcomingView', 'Edit Recording'],
]

/* Lazy loaders, so a view is imported only when a case mounts it. */
const VIEWS = import.meta.glob<{ default: Component }>(['../configuration/*.vue', '../dvr/*.vue'])

async function mountView(path: string, props?: Record<string, unknown>): Promise<void> {
  const load = VIEWS[`../${path}.vue`]
  if (!load) throw new Error(`no view ${path}`)
  mount((await load()).default, { props })
  await nextTick()
}

function action(id: string): ActionDef {
  const found = (captured.actions as ActionDef[]).find((a) => a.id === id)
  if (!found?.onClick) throw new Error(`no ${id} action captured`)
  return found
}

describe('editor title for a multi-row Edit', () => {
  it.each(CASES)('%s titles it Edit, not Add', async (path, editTitle, props) => {
    await mountView(path, props)
    const edit = action('edit')
    expect(edit.disabled).toBeFalsy()
    await edit.onClick?.()
    await nextTick()
    expect(captured.editorTitle).toBe(editTitle)
  }, 30_000)
})

describe('editor title in create mode', () => {
  it.each([
    ['configuration/ChannelTagsView', 'Add Channel Tag'],
    ['configuration/RatingLabelsView', 'Add Rating Label'],
  ])('%s still titles a new entry Add', async (path, title) => {
    await mountView(path)
    await action('add').onClick?.()
    await nextTick()
    expect(captured.editorTitle).toBe(title)
  }, 30_000)
})
