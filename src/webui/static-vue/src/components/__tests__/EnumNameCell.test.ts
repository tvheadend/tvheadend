// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EnumNameCell unit tests. Covers both shapes the cell handles:
 *   - scalar value (single enum key)
 *   - array value (multiple enum keys)
 * The component auto-detects via Array.isArray, so the same set
 * of cases applies to both — fetcher mock + lifecycle expectations
 * are identical.
 *
 * `fetchDeferredEnum` is the module-level cached fetcher; we mock
 * it so the test doesn't make a real network call and so we can
 * control resolution timing via a deferred promise.
 */

import { describe, expect, it, vi, beforeEach } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import EnumNameCell from '../EnumNameCell.vue'
import { useAccessStore } from '@/stores/access'
import type { ColumnDef } from '@/types/column'
import type { Option } from '../idnode-fields/deferredEnum'

vi.mock('../idnode-fields/deferredEnum', async (importOriginal) => {
  const actual = await importOriginal<typeof import('../idnode-fields/deferredEnum')>()
  return {
    ...actual,
    fetchDeferredEnum: vi.fn<(d: unknown) => Promise<Option[]>>(),
  }
})

/* EnumNameCell pulls in useEntityEditor for its drill-down chevron;
 * the real composable lazily wires vue-router watchers on first
 * call, so swap it for spies. The chevron-suite tests below inspect
 * the mock calls; the non-chevron tests don't trigger the click but
 * still mount the cell, so the mock must always be safe to call. */
const openMock = vi.fn()
const openListMock = vi.fn()
const closeMock = vi.fn()

vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({
    editingUuid: { value: null },
    isOpen: { value: false },
    open: openMock,
    openList: openListMock,
    close: closeMock,
  }),
}))

import { fetchDeferredEnum } from '../idnode-fields/deferredEnum'

const mockedFetch = vi.mocked(fetchDeferredEnum)

const COL_SCALAR: ColumnDef = {
  field: 'channel',
  enumSource: { type: 'api', uri: 'channel/list' },
}

const COL_ARRAY: ColumnDef = {
  field: 'profile',
  enumSource: { type: 'api', uri: 'profile/list' },
}

const FIXTURE_SCALAR: Option[] = [
  { key: 'uuid-1', val: 'BBC One HD' },
  { key: 'uuid-2', val: 'ITV1' },
]

const FIXTURE_ARRAY: Option[] = [
  { key: 'uuid-1', val: 'Pass' },
  { key: 'uuid-2', val: 'MKV' },
  { key: 'uuid-3', val: 'Audio Only' },
]

beforeEach(() => {
  /* EnumNameCell now consults useAccessStore() via
   * resolveChannelListDescriptor for chname_num / chname_src
   * param injection on `channel/list` sources — Pinia must be
   * active. Default access is empty (no flags set), so the
   * helper passes descriptors through unchanged unless a test
   * opts in. */
  setActivePinia(createPinia())
  mockedFetch.mockReset()
  openMock.mockReset()
  openListMock.mockReset()
  closeMock.mockReset()
})

describe('EnumNameCell — scalar value', () => {
  it('renders the resolved name once the fetch settles', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-1', col: COL_SCALAR },
    })
    /* In-flight: should show the raw key as a fallback so the cell
     * isn't visibly empty during the fetch window. */
    expect(wrapper.text()).toBe('uuid-1')
    await flushPromises()
    expect(wrapper.text()).toBe('BBC One HD')
  })

  it('renders an em-dash for an unknown key after the fetch settled', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-missing', col: COL_SCALAR },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('renders an em-dash for null / empty values without fetching at all', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: null, col: COL_SCALAR },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('falls back gracefully when the deferred fetch rejects', async () => {
    mockedFetch.mockRejectedValueOnce(new Error('network down'))
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-1', col: COL_SCALAR },
    })
    expect(wrapper.text()).toBe('uuid-1')
    await flushPromises()
    /* After rejection, options resolves to empty list → unknown key
     * → em-dash. The raw key during the fetch window is honest, the
     * em-dash post-failure signals "no resolution available". */
    expect(wrapper.text()).toBe('—')
  })
})

describe('EnumNameCell — array value', () => {
  it('renders an em-dash for an empty array without fetching', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: [], col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('renders an em-dash for null / non-array values', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: null, col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('shows first key + "+N more" during the in-flight fetch window, tooltip carries the full raw list', async () => {
    /* Fetch never resolves — keep the component in the in-flight
     * state to verify the raw-keys fallback. Multi-element
     * arrays compress to "first, +N more" so a narrow column
     * doesn't ellipsise the visible content; the full list
     * lives on the `title` tooltip. */
    mockedFetch.mockReturnValueOnce(new Promise(() => {}))
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('uuid-1, +1 more')
    expect(wrapper.find('.enum-name-cell__text').attributes('title')).toBe('uuid-1, uuid-2')
  })

  it('renders first label + "+N more" once the fetch settles, tooltip carries the full joined list', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Pass, +1 more')
    expect(wrapper.find('.enum-name-cell__text').attributes('title')).toBe('Pass, MKV')
  })

  it('preserves cardinality count when one key is missing post-fetch; stale refs sink to the end in canonical sort', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-missing', 'uuid-3'], col: COL_ARRAY },
    })
    await flushPromises()
    /* Display compresses to "first, +2 more" — the count
     * communicates the remainder; the tooltip shows the full
     * list re-ordered to match the server-canonical sort of the
     * options list (resolved labels by their options index
     * ascending; unknown / stale references sink to the end). */
    expect(wrapper.text()).toBe('Pass, +2 more')
    expect(wrapper.find('.enum-name-cell__text').attributes('title')).toBe('Pass, Audio Only, —')
  })

  it('sorts row items by their position in the server-canonical options list', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    /* Wire input is in reverse order; server-canonical
     * sort (per FIXTURE_ARRAY: uuid-1 → Pass, uuid-2 → MKV,
     * uuid-3 → Audio Only) should put Pass first regardless of
     * insertion order. */
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-3', 'uuid-2', 'uuid-1'], col: COL_ARRAY },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Pass, +2 more')
    expect(wrapper.find('.enum-name-cell__text').attributes('title')).toBe('Pass, MKV, Audio Only')
  })

  it('renders a single resolved label without "+N more" when the array has one element', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1'], col: COL_ARRAY },
    })
    await flushPromises()
    /* Single-element arrays read as a plain label — no count
     * suffix, no tooltip. */
    expect(wrapper.text()).toBe('Pass')
    expect(wrapper.find('.enum-name-cell__text').attributes('title')).toBeUndefined()
  })

  it('collapses a forest of em-dashes when every key is missing', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-x', 'uuid-y'], col: COL_ARRAY },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('falls back to a single em-dash when the deferred fetch rejects', async () => {
    mockedFetch.mockRejectedValueOnce(new Error('network down'))
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    /* In-flight: first raw key + "+1 more". */
    expect(wrapper.text()).toBe('uuid-1, +1 more')
    await flushPromises()
    /* After rejection, options resolves to empty list → every key
     * is unknown → forest-collapse rule kicks in. */
    expect(wrapper.text()).toBe('—')
  })
})

describe('EnumNameCell — inline enum source', () => {
  /* Inline static enum: option list is provided directly on the
   * column descriptor (no `fetchDeferredEnum` round-trip). Used
   * for small fixed enums like the mux `enabled` tri-state
   * (Ignore / Disable / Enable). The cell should resolve labels
   * synchronously without any in-flight phase. */

  const TRI_STATE: Option[] = [
    { key: -1, val: 'Ignore' },
    { key: 0, val: 'Disable' },
    { key: 1, val: 'Enable' },
  ]
  const COL_TRI: ColumnDef = { field: 'enabled', enumSource: TRI_STATE }

  /* Note on `await flushPromises()`: even though the inline-enum
   * path sets `options.value` synchronously in `onMounted`, Vue's
   * computed-driven re-render is queued to a microtask. The
   * deferred-fetch tests above don't need a flush BEFORE asserting
   * the raw-key fallback because that's the initial render state;
   * inline tests assert the POST-mount label, which requires the
   * reactive flush. */

  it('resolves a scalar value without calling fetchDeferredEnum', async () => {
    const wrapper = mount(EnumNameCell, {
      props: { value: 1, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Enable')
    expect(mockedFetch).not.toHaveBeenCalled()
  })

  it('resolves tri-state PT_INT enum (value=-1 → "Ignore", not "Enable")', async () => {
    /* BooleanCell would render the same green tick for value=1
     * (Enable) and value=-1 (Ignore) because both are truthy.
     * EnumNameCell + an inline enum surfaces the correct label
     * for each. */
    const wrapper = mount(EnumNameCell, {
      props: { value: -1, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Ignore')
  })

  it('renders an em-dash for an unknown key against an inline enum', async () => {
    const wrapper = mount(EnumNameCell, {
      props: { value: 99, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })
})

describe('EnumNameCell — drill-down chevron', () => {
  /* Mirrors DrillDownCell's chevron contract: column opts in via
   * `targetUuidField` (+ optional `targetAccessKey`), the cell reads
   * `row[targetUuidField]` and renders a chevron-right button when
   * the lookup yields one or more UUIDs. Click routes through
   * `useEntityEditor.openList(rows, columns, title?)` — collapses
   * to `open(uuid)` for a single ref, drives the picker drawer for
   * 2+. The lookup field can be the column's own `field` (the
   * islist case here: `services` is both the value and the UUID
   * field) or a sibling (e.g. EPG event rows carry `channelUuid`
   * alongside the rendered `channelName`). */

  const COL_REF: ColumnDef = {
    field: 'services',
    label: 'Services',
    enumSource: { type: 'api', uri: 'service/list' },
    targetUuidField: 'services',
    targetAccessKey: 'admin',
    pickerTitle: 'Mapped services',
  }

  const COL_REF_NO_TITLE: ColumnDef = {
    field: 'services',
    label: 'Services',
    enumSource: { type: 'api', uri: 'service/list' },
    targetUuidField: 'services',
    targetAccessKey: 'admin',
  }

  const COL_NO_DRILL: ColumnDef = {
    field: 'services',
    label: 'Services',
    enumSource: { type: 'api', uri: 'service/list' },
  }

  const SVC_OPTIONS: Option[] = [
    { key: 'svc-1', val: 'Service A' },
    { key: 'svc-2', val: 'Service B' },
    { key: 'svc-3', val: 'Service C' },
  ]

  it('shows the chevron for a 2+ element array when the column opts in and access is granted', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_REF,
      },
    })
    await flushPromises()
    expect(wrapper.find('.enum-name-cell__drill').exists()).toBe(true)
  })

  it('hides the chevron when the column does not opt in (no targetUuidField)', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_NO_DRILL,
      },
    })
    await flushPromises()
    expect(wrapper.find('.enum-name-cell__drill').exists()).toBe(false)
  })

  it('hides the chevron when the access flag is set but false for this user', async () => {
    useAccessStore().data = { admin: false, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_REF,
      },
    })
    await flushPromises()
    expect(wrapper.find('.enum-name-cell__drill').exists()).toBe(false)
  })

  it('hides the chevron for an empty array', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: [],
        row: { services: [] },
        col: COL_REF,
      },
    })
    await flushPromises()
    expect(wrapper.find('.enum-name-cell__drill').exists()).toBe(false)
  })

  it('chevron click on a 2+ array calls openList with one row per uuid + resolved name + pickerTitle', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_REF,
      },
    })
    await flushPromises()
    await wrapper.find('.enum-name-cell__drill').trigger('click')

    expect(openListMock).toHaveBeenCalledTimes(1)
    const [rows, columns, title] = openListMock.mock.calls[0]
    expect(rows).toEqual([
      { uuid: 'svc-1', name: 'Service A' },
      { uuid: 'svc-2', name: 'Service B' },
    ])
    /* Single-column compact table — only the resolved name is
     * shown in the picker; clicking a row drops into that
     * entity's editor below where every property is visible. */
    expect(columns).toEqual([{ field: 'name', label: 'Name' }])
    expect(title).toBe('Mapped services')
    /* openList collapses single-row lists to plain open(); the
     * 2+-row branch must not invoke open() directly. */
    expect(openMock).not.toHaveBeenCalled()
  })

  it('chevron click falls back to col.label as the picker title when pickerTitle is unset', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_REF_NO_TITLE,
      },
    })
    await flushPromises()
    await wrapper.find('.enum-name-cell__drill').trigger('click')

    const [, , title] = openListMock.mock.calls[0]
    expect(title).toBe('Services')
  })

  it('chevron click on a 1-element array still routes through openList (collapses to single-entity drawer)', async () => {
    /* Same code path for both shapes: openList itself short-circuits
     * to open(uuid) for a one-row list. The test asserts the route
     * — openList is called with a single-row list, the component
     * does not invoke open() directly. */
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1'],
        row: { services: ['svc-1'] },
        col: COL_REF,
      },
    })
    await flushPromises()
    await wrapper.find('.enum-name-cell__drill').trigger('click')

    expect(openListMock).toHaveBeenCalledTimes(1)
    const [rows] = openListMock.mock.calls[0]
    expect(rows).toEqual([{ uuid: 'svc-1', name: 'Service A' }])
    expect(openMock).not.toHaveBeenCalled()
  })

  it('chevron click on a scalar UUID routes through openList with a single-row list', async () => {
    /* DVR-style idnode-enum field: row[col.field] IS the UUID. */
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const wrapper = mount(EnumNameCell, {
      props: {
        value: 'svc-2',
        row: { services: 'svc-2' },
        col: COL_REF,
      },
    })
    await flushPromises()
    await wrapper.find('.enum-name-cell__drill').trigger('click')

    expect(openListMock).toHaveBeenCalledTimes(1)
    const [rows] = openListMock.mock.calls[0]
    expect(rows).toEqual([{ uuid: 'svc-2', name: 'Service B' }])
  })

  it('chevron click stops propagation so the parent row-click handler stays cold', async () => {
    useAccessStore().data = { admin: true, dvr: true }
    mockedFetch.mockResolvedValueOnce(SVC_OPTIONS)
    const parent = document.createElement('div')
    const onParentClick = vi.fn()
    parent.addEventListener('click', onParentClick)
    document.body.appendChild(parent)

    const wrapper = mount(EnumNameCell, {
      props: {
        value: ['svc-1', 'svc-2'],
        row: { services: ['svc-1', 'svc-2'] },
        col: COL_REF,
      },
      attachTo: parent,
    })
    await flushPromises()
    await wrapper.find('.enum-name-cell__drill').trigger('click')

    expect(openListMock).toHaveBeenCalledTimes(1)
    expect(onParentClick).not.toHaveBeenCalled()

    wrapper.unmount()
    parent.remove()
  })
})
