// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * PlayCell — per-row Play-icon cell. Tests cover URL build,
 * optional title decoration, disabled-state click suppression,
 * and stopPropagation so the row-click handler doesn't fire.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import PlayCell from '../PlayCell.vue'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'

const openMock = vi.fn()
vi.stubGlobal('open', openMock)

function makeCol(extra: Partial<ColumnDef> = {}): ColumnDef {
  return {
    field: '_play',
    label: '',
    width: 50,
    sortable: false,
    ...extra,
  }
}

afterEach(() => {
  openMock.mockReset()
})

describe('PlayCell', () => {
  it('opens /play/ticket/<playPath>/<uuid> in a new tab on click', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'abc-123' } as BaseRow,
        col: makeCol({ playPath: 'stream/channel' }),
      },
    })
    await wrapper.find('button').trigger('click')
    expect(openMock).toHaveBeenCalledWith(
      '/play/ticket/stream/channel/abc-123',
      '_blank',
      'noopener,noreferrer',
    )
  })

  it('appends the URL-encoded ?title= when col.playTitle returns a value', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'mux-1', name: 'Mux A', network: 'Net B' } as BaseRow,
        col: makeCol({
          playPath: 'stream/mux',
          playTitle: (r) => `${r.name} / ${r.network}`,
        }),
      },
    })
    await wrapper.find('button').trigger('click')
    expect(openMock.mock.calls[0][0]).toBe(
      '/play/ticket/stream/mux/mux-1?title=Mux%20A%20%2F%20Net%20B',
    )
  })

  it('skips the ?title= query when col.playTitle returns an empty string', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'svc-1' } as BaseRow,
        col: makeCol({
          playPath: 'stream/service',
          playTitle: () => '',
        }),
      },
    })
    await wrapper.find('button').trigger('click')
    expect(openMock.mock.calls[0][0]).toBe('/play/ticket/stream/service/svc-1')
  })

  it('renders disabled when col.playEnabled returns false; click does nothing', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'dvr-1', filesize: 0 } as BaseRow,
        col: makeCol({
          playPath: 'dvrfile',
          playEnabled: (r) => typeof r.filesize === 'number' && r.filesize > 0,
        }),
      },
    })
    expect(wrapper.find('button').attributes('disabled')).toBeDefined()
    expect(wrapper.find('button').classes()).toContain('play-cell--disabled')
    await wrapper.find('button').trigger('click')
    expect(openMock).not.toHaveBeenCalled()
  })

  it('renders disabled when col.playPath is missing', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'x' } as BaseRow,
        col: makeCol({}),
      },
    })
    expect(wrapper.find('button').attributes('disabled')).toBeDefined()
    await wrapper.find('button').trigger('click')
    expect(openMock).not.toHaveBeenCalled()
  })

  it('renders disabled when row.uuid is missing or non-string', async () => {
    const wrapper = mount(PlayCell, {
      props: {
        row: {} as BaseRow,
        col: makeCol({ playPath: 'stream/channel' }),
      },
    })
    expect(wrapper.find('button').attributes('disabled')).toBeDefined()
    await wrapper.find('button').trigger('click')
    expect(openMock).not.toHaveBeenCalled()
  })

  it('stops click propagation so the row-click handler does not fire', async () => {
    /* Mount PlayCell with an attachTo container that has a click
     * listener on the parent DOM node — bypass Vue's wrapper and
     * verify event.stopPropagation() at the browser level. */
    const parent = document.createElement('div')
    const onParentClick = vi.fn()
    parent.addEventListener('click', onParentClick)
    document.body.appendChild(parent)
    const wrapper = mount(PlayCell, {
      props: {
        row: { uuid: 'abc' } as BaseRow,
        col: makeCol({ playPath: 'stream/channel' }),
      },
      attachTo: parent,
    })
    await wrapper.find('button').trigger('click')
    expect(openMock).toHaveBeenCalledTimes(1)
    /* Without stopPropagation in PlayCell, the bubbling click would
     * fire onParentClick. With it, the parent listener stays cold. */
    expect(onParentClick).not.toHaveBeenCalled()
    wrapper.unmount()
    parent.removeEventListener('click', onParentClick)
    parent.remove()
  })
})
