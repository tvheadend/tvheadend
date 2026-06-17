// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EditableTagChipCell unit tests — covers both render paths
 * (delegated read-only via EnumNameCell when not managing, chip
 * painter when managing), the chip remove + picker add flows, the
 * outside-click / Escape dismissal, and the no-available-tags
 * disabled state.
 */

import {
  afterEach,
  beforeEach,
  describe,
  expect,
  it,
  vi,
} from 'vitest'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import { ref } from 'vue'
import EditableTagChipCell from '../EditableTagChipCell.vue'
import type { ColumnDef } from '@/types/column'
import type { Option } from '../idnode-fields/deferredEnum'

vi.mock('../idnode-fields/deferredEnum', async (importOriginal) => {
  const actual = await importOriginal<typeof import('../idnode-fields/deferredEnum')>()
  return {
    ...actual,
    fetchDeferredEnum: vi.fn<(d: unknown) => Promise<Option[]>>(),
  }
})

/* EnumNameCell (which EditableTagChipCell delegates to in read-only
 * mode) pulls in useEntityEditor; the composable lazily wires
 * vue-router watchers on first call. Mock to a no-op handle —
 * delegated tests don't exercise the chevron-drilldown path. */
vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({
    editingUuid: { value: null },
    isOpen: { value: false },
    open: vi.fn(),
    openList: vi.fn(),
    close: vi.fn(),
  }),
}))

import { fetchDeferredEnum } from '../idnode-fields/deferredEnum'
const mockedFetch = vi.mocked(fetchDeferredEnum)

const COL: ColumnDef = {
  field: 'tags',
  enumSource: { type: 'api', uri: 'channeltag/list', params: { all: 1 } },
}

const TAGS: Option[] = [
  { key: 'tag-news', val: 'News' },
  { key: 'tag-sports', val: 'Sports' },
  { key: 'tag-hd', val: 'HD' },
  { key: 'tag-music', val: 'Music' },
]

const commitMock = vi.fn()

let lastWrapper: VueWrapper | null = null

function mountCell(opts: {
  value: unknown
  managing?: boolean
  rowUuid?: string
}): VueWrapper {
  const provides: Record<string, unknown> = {
    idnodeGridActivelyEditing: ref(opts.managing ?? false),
    idnodeGridInlineCommit: commitMock,
  }
  lastWrapper = mount(EditableTagChipCell, {
    props: {
      value: opts.value,
      row: opts.rowUuid ? { uuid: opts.rowUuid } : undefined,
      col: COL,
    },
    global: { provide: provides },
    attachTo: document.body,
  })
  return lastWrapper
}

beforeEach(() => {
  setActivePinia(createPinia())
  mockedFetch.mockReset()
  mockedFetch.mockResolvedValue(TAGS)
  commitMock.mockReset()
})

afterEach(() => {
  vi.restoreAllMocks()
  /* Unmount the cell so its document-level click/keydown listeners
   * (registered in onMounted) come off — otherwise the next test
   * still has the previous cell's listener firing on document
   * clicks and racing with the new cell's listener. */
  lastWrapper?.unmount()
  lastWrapper = null
  /* Belt + braces: clear any leftover teleported picker DOM. */
  document.body.querySelectorAll('.tag-chip-cell__picker').forEach((el) => {
    el.remove()
  })
})

describe('EditableTagChipCell — delegated read-only render', () => {
  it('renders the EnumNameCell delegate when not managing', async () => {
    const wrapper = mountCell({
      value: ['tag-news', 'tag-hd'],
      managing: false,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    /* EnumNameCell renders its own .enum-name-cell shell — assert
     * that's present (not the chip-painter classes). */
    expect(wrapper.find('.enum-name-cell').exists()).toBe(true)
    expect(wrapper.find('.tag-chip-cell').exists()).toBe(false)
  })
})

describe('EditableTagChipCell — chip painter (managing)', () => {
  it('renders one chip per UUID with the resolved label, sorted alphabetically', async () => {
    /* Server emits tags in attach-order — varies per channel.
     * Cell sorts by resolved label so every row's chip strip
     * reads in the same alphabetical order ("HD, News" not
     * "News, HD" on one row and "HD, News" on another). */
    const wrapper = mountCell({
      value: ['tag-news', 'tag-hd'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()

    const chips = wrapper.findAll('.tag-chip-cell__chip')
    expect(chips).toHaveLength(2)
    expect(chips[0].find('.tag-chip-cell__label').text()).toBe('HD')
    expect(chips[1].find('.tag-chip-cell__label').text()).toBe('News')
  })

  it('uses the raw UUID as label while the deferred fetch is in flight', () => {
    /* Fetch never resolves — keep options null. */
    mockedFetch.mockReturnValueOnce(new Promise(() => {}))
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    expect(wrapper.find('.tag-chip-cell__label').text()).toBe('tag-news')
  })

  it('clicking a chip\'s × button commits new tags without that uuid', async () => {
    const wrapper = mountCell({
      value: ['tag-news', 'tag-hd'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()

    /* Chips are sorted alphabetically by label — index 0 is HD,
     * index 1 is News. Removing the first chip should commit
     * the array minus HD = ['tag-news']. */
    const chips = wrapper.findAll('.tag-chip-cell__chip')
    await chips[0].find('.tag-chip-cell__remove').trigger('click')

    expect(commitMock).toHaveBeenCalledTimes(1)
    expect(commitMock).toHaveBeenCalledWith('ch-1', 'tags', ['tag-news'])
  })

  it('removing the only tag commits an empty array (not undefined)', async () => {
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    await wrapper.find('.tag-chip-cell__remove').trigger('click')
    expect(commitMock).toHaveBeenCalledWith('ch-1', 'tags', [])
  })

  it('clicking + opens the picker with only unused tags, sorted by label', async () => {
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()

    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(false)
    await wrapper.find('.tag-chip-cell__add').trigger('click')

    const items = Array.from(
      document.body.querySelectorAll<HTMLElement>('.tag-chip-cell__picker-item'),
    )
    expect(items.map((i) => i.textContent?.trim() ?? '')).toEqual([
      'HD',
      'Music',
      'Sports',
    ])
  })

  it('picking a tag commits new tags with the addition and closes the picker', async () => {
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    await wrapper.find('.tag-chip-cell__add').trigger('click')

    /* HD is the first item alphabetically. */
    const pickerItems = Array.from(
      document.body.querySelectorAll<HTMLElement>('.tag-chip-cell__picker-item'),
    )
    pickerItems[0].dispatchEvent(
      new MouseEvent('click', { bubbles: true, cancelable: true }),
    )
    await wrapper.vm.$nextTick()
    await flushPromises()

    expect(commitMock).toHaveBeenCalledTimes(1)
    expect(commitMock).toHaveBeenCalledWith('ch-1', 'tags', [
      'tag-news',
      'tag-hd',
    ])
    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(false)
  })

  it('add button is disabled when every tag is already on the row', async () => {
    const wrapper = mountCell({
      value: ['tag-news', 'tag-sports', 'tag-hd', 'tag-music'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    expect(
      wrapper.find('.tag-chip-cell__add').attributes('disabled'),
    ).toBeDefined()
  })

  it('outside-click closes the picker', async () => {
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    await wrapper.find('.tag-chip-cell__add').trigger('click')
    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(true)

    /* Click on an element outside the cell. */
    const outside = document.createElement('div')
    document.body.appendChild(outside)
    outside.dispatchEvent(
      new MouseEvent('click', { bubbles: true, cancelable: true }),
    )
    await flushPromises()

    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(false)
    outside.remove()
  })

  it('Escape closes the picker', async () => {
    const wrapper = mountCell({
      value: ['tag-news'],
      managing: true,
      rowUuid: 'ch-1',
    })
    await flushPromises()
    await wrapper.find('.tag-chip-cell__add').trigger('click')
    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(true)

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }))
    await flushPromises()

    expect(!!document.body.querySelector('.tag-chip-cell__picker')).toBe(false)
  })

  it('does not commit when the commit handle is not provided (defensive)', async () => {
    /* Mount without the inject — simulates a cell mounted outside an
     * inline-edit-enabled grid. */
    const wrapper = mount(EditableTagChipCell, {
      props: {
        value: ['tag-news'],
        row: { uuid: 'ch-1' },
        col: COL,
      },
      global: {
        provide: { idnodeGridActivelyEditing: ref(true) },
      },
    })
    await flushPromises()
    await wrapper.find('.tag-chip-cell__remove').trigger('click')
    expect(commitMock).not.toHaveBeenCalled()
  })
})
