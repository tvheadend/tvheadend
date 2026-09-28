// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ConnectionsView — column-wiring tests. StatusGrid is auto-stubbed,
 * and the assertions read the `columns` prop handed to the grid.
 */

import { afterEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import ConnectionsView from '../ConnectionsView.vue'
import StatusGrid from '@/components/StatusGrid.vue'
import type { ColumnDef } from '@/types/column'

function gridColumns(): ColumnDef[] {
  const wrapper = mount(ConnectionsView, {
    global: { stubs: { StatusGrid: true } },
  })
  return wrapper.findComponent(StatusGrid).props('columns') as ColumnDef[]
}

afterEach(() => {
  delete (globalThis as { tvh_locale?: unknown }).tvh_locale
})

describe('ConnectionsView — columns', () => {
  it('lists the client data ports per protocol', () => {
    const ports = gridColumns().find((c) => c.field === 'peer_extra_ports')
    expect(ports?.format?.({ tcp: [5000, 5001], udp: [6000] }, {})).toBe(
      'TCP: 5000, 5001; UDP: 6000',
    )
  })

  it('reuses the Classic TCP and UDP msgids in the client data ports', () => {
    ;(globalThis as { tvh_locale?: unknown }).tvh_locale = { TCP: 'tcp-x', UDP: 'udp-x' }
    const ports = gridColumns().find((c) => c.field === 'peer_extra_ports')
    expect(ports?.format?.({ tcp: [5000], udp: [6000] }, {})).toBe('tcp-x: 5000; udp-x: 6000')
  })
})
