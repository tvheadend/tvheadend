// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * SubscriptionsView — column-wiring tests. StatusGrid and
 * BandwidthChartView are auto-stubbed (their own behaviour is
 * covered elsewhere); the assertions read the `columns` prop
 * handed to the grid.
 *
 * Coverage:
 *   - The Start column delegates to the shared fmtDate (custom
 *     date mask + phone smart-relative form), matching
 *     ConnectionsView's Started column.
 *   - The Id column keeps the ExtJS zero-padded uppercase hex look.
 */

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import SubscriptionsView from '../SubscriptionsView.vue'
import StatusGrid from '@/components/StatusGrid.vue'
import { fmtDate } from '@/utils/formatTime'
import type { ColumnDef } from '@/types/column'

function gridColumns(): ColumnDef[] {
  const wrapper = mount(SubscriptionsView, {
    global: {
      stubs: {
        StatusGrid: true,
        BandwidthChartView: true,
      },
    },
  })
  return wrapper.findComponent(StatusGrid).props('columns') as ColumnDef[]
}

describe('SubscriptionsView — column formatting', () => {
  it('formats the Start column with the shared fmtDate helper', () => {
    const start = gridColumns().find((c) => c.field === 'start')
    expect(start).toBeDefined()
    expect(start?.format).toBe(fmtDate)
  })

  it('renders the Id column as zero-padded uppercase hex', () => {
    const id = gridColumns().find((c) => c.field === 'id')
    expect(id?.format?.(0x1a2b, {})).toBe('00001A2B')
  })

  it('converts Input/Output from bytes/s to kb/s (kilobits), matching Classic', () => {
    const cols = gridColumns()
    const input = cols.find((c) => c.field === 'in')
    const output = cols.find((c) => c.field === 'out')
    /* 1,500,000 bytes/s = 12 Mbit/s -> 12000 kb/s (bytes*8/1000),
     * NOT 1465 (the old bytes/1024). */
    expect(input?.format?.(1_500_000, {})).toBe('12000')
    expect(output?.format?.(1_500_000, {})).toBe('12000')
    expect(input?.format?.(0, {})).toBe('0')
    expect(input?.format?.('n/a', {})).toBe('')
    /* malformed data must not render as "NaN" / "Infinity". */
    expect(input?.format?.(NaN, {})).toBe('')
    expect(input?.format?.(Infinity, {})).toBe('')
  })
})
