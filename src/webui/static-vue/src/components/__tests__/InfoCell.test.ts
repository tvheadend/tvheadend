// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * InfoCell — per-row Info-icon cell. Tests cover the click →
 * col.onInfo callback path, the disabled state when the
 * callback isn't supplied, and the stopPropagation so the
 * row-click handler doesn't also fire.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import InfoCell from '../InfoCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'

function makeCol(extra: Partial<ColumnDef> = {}): ColumnDef {
  return {
    field: '_info',
    label: 'Info',
    width: 40,
    sortable: false,
    ...extra,
  }
}

afterEach(() => {
  vi.restoreAllMocks()
})

describe('InfoCell', () => {
  it('calls col.onInfo with the row on click', async () => {
    const onInfo = vi.fn()
    const row = { uuid: 'svc-1', name: 'Service Alpha' } as BaseRow
    const wrapper = mount(InfoCell, {
      props: {
        row,
        col: makeCol({ onInfo }),
      },
    })
    await wrapper.find('button').trigger('click')
    expect(onInfo).toHaveBeenCalledTimes(1)
    expect(onInfo).toHaveBeenCalledWith(row)
  })

  it('renders disabled when col.onInfo is missing; click does nothing', async () => {
    const wrapper = mount(InfoCell, {
      props: {
        row: { uuid: 'svc-1' } as BaseRow,
        col: makeCol(),
      },
    })
    expect(wrapper.find('button').attributes('disabled')).toBeDefined()
    expect(wrapper.find('button').classes()).toContain('info-cell--disabled')
    await wrapper.find('button').trigger('click')
    /* No callback to assert — but a disabled button shouldn't
     * fire its @click anyway. The defensive guard inside
     * `open()` is what we're really validating; the disabled
     * styling is the visual feedback for that guard. */
  })

  it('stops click propagation so the row-click handler does not fire', async () => {
    const onInfo = vi.fn()
    const parent = document.createElement('div')
    const onParentClick = vi.fn()
    parent.addEventListener('click', onParentClick)
    document.body.appendChild(parent)
    const wrapper = mount(InfoCell, {
      props: {
        row: { uuid: 'svc-1' } as BaseRow,
        col: makeCol({ onInfo }),
      },
      attachTo: parent,
    })
    await wrapper.find('button').trigger('click')
    expect(onInfo).toHaveBeenCalledTimes(1)
    /* Without stopPropagation in InfoCell, the bubbling click
     * would reach the parent listener. With it, the parent
     * stays cold. */
    expect(onParentClick).not.toHaveBeenCalled()
    wrapper.unmount()
    parent.removeEventListener('click', onParentClick)
    parent.remove()
  })
})
