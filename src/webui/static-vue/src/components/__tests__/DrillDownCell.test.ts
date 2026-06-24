// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DrillDownCell — wrapper cell with text + chevron-right
 * drill-down icon. Tests cover:
 *   - Renders the value text in all cases (icon is the only
 *     part that's conditional).
 *   - Chevron hidden when targetUuidField is unset on the col.
 *   - Chevron hidden when the row lacks the named UUID field.
 *   - Chevron hidden when the access flag check fails.
 *   - Chevron shown when both UUID is present AND access flag
 *     is set.
 *   - Click stopPropagation so the row's click handler stays
 *     cold.
 *   - Click calls useEntityEditor().open() with the row's UUID.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import DrillDownCell from '../DrillDownCell.vue'
import { useAccessStore } from '@/stores/access'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'

const openMock = vi.fn()
const closeMock = vi.fn()

vi.mock('@/composables/useEntityEditor', () => ({
  useEntityEditor: () => ({
    editingUuid: { value: null },
    isOpen: { value: false },
    open: openMock,
    close: closeMock,
  }),
}))

function makeCol(extra: Partial<ColumnDef> = {}): ColumnDef {
  return {
    field: 'channel',
    label: 'Channel',
    ...extra,
  }
}

beforeEach(() => {
  setActivePinia(createPinia())
  openMock.mockReset()
  closeMock.mockReset()
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('DrillDownCell', () => {
  it('renders the value text', () => {
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One' } as BaseRow,
        col: makeCol(),
      },
    })
    expect(wrapper.text()).toContain('Channel One')
  })

  it('hides the chevron when col.targetUuidField is unset', () => {
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol(),
      },
    })
    expect(wrapper.find('button').exists()).toBe(false)
  })

  it('hides the chevron when the row lacks the UUID field', () => {
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One' } as BaseRow,
        col: makeCol({ targetUuidField: 'channelUuid' }),
      },
    })
    expect(wrapper.find('button').exists()).toBe(false)
  })

  it('hides the chevron when the access flag is required but not satisfied', () => {
    /* Default access store state has data = null, so no flag is set. */
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({
          targetUuidField: 'channelUuid',
          targetAccessKey: 'admin',
        }),
      },
    })
    expect(wrapper.find('button').exists()).toBe(false)
  })

  it('shows the chevron when targetUuid is present and no access flag is required', () => {
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({ targetUuidField: 'channelUuid' }),
      },
    })
    expect(wrapper.find('button').exists()).toBe(true)
  })

  it('shows the chevron when targetUuid is present and access flag is satisfied', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true }
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({
          targetUuidField: 'channelUuid',
          targetAccessKey: 'admin',
        }),
      },
    })
    expect(wrapper.find('button').exists()).toBe(true)
  })

  it('hides the chevron when access flag exists but is false for this user', () => {
    const access = useAccessStore()
    access.data = { admin: false, dvr: true }
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({
          targetUuidField: 'channelUuid',
          targetAccessKey: 'admin',
        }),
      },
    })
    expect(wrapper.find('button').exists()).toBe(false)
  })

  it('click calls useEntityEditor().open() with the row UUID', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true }
    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({
          targetUuidField: 'channelUuid',
          targetAccessKey: 'admin',
        }),
      },
    })
    await wrapper.find('button').trigger('click')
    expect(openMock).toHaveBeenCalledTimes(1)
    expect(openMock).toHaveBeenCalledWith('abc-uuid')
  })

  it('click stopPropagation prevents the row-click handler from firing', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true }
    const parent = document.createElement('div')
    const onParentClick = vi.fn()
    parent.addEventListener('click', onParentClick)
    document.body.appendChild(parent)

    const wrapper = mount(DrillDownCell, {
      props: {
        value: 'Channel One',
        row: { channel: 'Channel One', channelUuid: 'abc-uuid' } as BaseRow,
        col: makeCol({
          targetUuidField: 'channelUuid',
          targetAccessKey: 'admin',
        }),
      },
      attachTo: parent,
    })
    await wrapper.find('button').trigger('click')
    expect(openMock).toHaveBeenCalledTimes(1)
    expect(onParentClick).not.toHaveBeenCalled()

    wrapper.unmount()
    parent.removeEventListener('click', onParentClick)
    parent.remove()
  })

  it('falls back to row[col.field] when the explicit value prop is empty', () => {
    const wrapper = mount(DrillDownCell, {
      props: {
        row: { channel: 'Fallback Channel' } as BaseRow,
        col: makeCol(),
      },
    })
    expect(wrapper.text()).toContain('Fallback Channel')
  })
})
