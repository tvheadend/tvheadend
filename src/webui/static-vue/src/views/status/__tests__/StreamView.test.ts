// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * StreamView — column-wiring tests. StatusGrid and
 * BandwidthChartView are auto-stubbed, and the assertions read the
 * `columns` prop handed to the grid.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import StreamView from '../StreamView.vue'
import StatusGrid from '@/components/StatusGrid.vue'
import type { ColumnDef } from '@/types/column'

vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => false) }),
}))
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ error: vi.fn() }),
}))

function gridColumns(): ColumnDef[] {
  const wrapper = mount(StreamView, {
    global: {
      stubs: {
        StatusGrid: true,
        BandwidthChartView: true,
      },
      directives: { tooltip: () => undefined },
    },
  })
  return wrapper.findComponent(StatusGrid).props('columns') as ColumnDef[]
}

afterEach(() => {
  delete (globalThis as { tvh_locale?: unknown }).tvh_locale
})

describe('StreamView — PID list', () => {
  it('lists the PIDs in ascending order', () => {
    const pids = gridColumns().find((c) => c.field === 'pids')
    expect(pids?.format?.([256, 0, 17], {})).toBe('0, 17, 256')
  })

  it('reuses the Classic "all" msgid for a full-mux subscription', () => {
    ;(globalThis as { tvh_locale?: unknown }).tvh_locale = { all: 'vše' }
    const pids = gridColumns().find((c) => c.field === 'pids')
    expect(pids?.format?.([0, 65535], {})).toBe('vše')
  })
})
