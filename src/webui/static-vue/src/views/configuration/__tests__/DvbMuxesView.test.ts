// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbMuxesView — the column set the view hands to IdnodeGrid. The
 * grid loads the abstract `mpegts_mux` class, which has no caption
 * for the tuning fields of the mux subclasses, so the view has to
 * label them itself.
 */
import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import DvbMuxesView from '../DvbMuxesView.vue'
import { formatMuxFrequency } from '../muxFrequency'
import type { BaseRow } from '@/types/grid'

const stubs = await vi.hoisted(() => import('@/test/__helpers__/gridViewTestUtils'))
vi.mock('@/components/IdnodeGrid.vue', () => stubs.idnodeGrid)
vi.mock('@/components/IdnodeEditor.vue', () => stubs.emptyComponent)
vi.mock('@/components/IdnodePickEntityDialog.vue', () => stubs.emptyComponent)
vi.mock('vue-router', () => stubs.vueRouter)
vi.mock('@/composables/useConfirmDialog', () => stubs.confirmDialog)
vi.mock('@/composables/useToastNotify', () => stubs.toastNotify)

stubs.resetEachTest()

const column = stubs.gridColumn

describe('DvbMuxesView tuning columns', () => {
  it('labels the subclass-only tuning columns instead of showing raw ids', () => {
    mount(DvbMuxesView)
    for (const field of ['frequency', 'symbolrate', 'constellation', 'modulation', 'polarisation']) {
      const col = column(field)
      expect(col, field).toBeDefined()
      expect(col?.label, field).toBeTruthy()
      expect(col?.label, field).not.toBe(field)
    }
  })

  it('shows constellation, the modulation field of cable and terrestrial muxes', () => {
    mount(DvbMuxesView)
    expect(column('constellation')?.label).toBe('Constellation')
  })

  it('formats frequency with the unit of the mux delivery system', () => {
    mount(DvbMuxesView)
    const fmt = column('frequency')?.format
    expect(fmt).toBeDefined()
    const cable = { uuid: 'c', delsys: 'DVB-C', constellation: 'QAM/64' } as BaseRow
    expect(fmt?.(658000000, cable)).toBe('658000000 Hz')
  })

  it('makes the tuning columns wide enough for their full header', () => {
    /* Measured in Chrome at 1440 px: the header cell needs about
     * 46 px beside the label for padding, the sort arrow and the
     * column menu. "Symbol rate (Sym/s)" is 129 px wide,
     * "Constellation" 84 px and "Polarization" 75 px. */
    mount(DvbMuxesView)
    expect(column('symbolrate')?.width).toBeGreaterThanOrEqual(180)
    expect(column('constellation')?.width).toBeGreaterThanOrEqual(150)
    expect(column('polarisation')?.width).toBeGreaterThanOrEqual(130)
  })
})

describe('formatMuxFrequency', () => {
  it('uses Hz for cable, terrestrial and ATSC muxes', () => {
    expect(formatMuxFrequency(658000000, { uuid: 'a', delsys: 'DVB-C' } as BaseRow)).toBe('658000000 Hz')
    expect(formatMuxFrequency(474000000, { uuid: 'b', delsys: 'DVB-T2' } as BaseRow)).toBe('474000000 Hz')
    expect(formatMuxFrequency(57000000, { uuid: 'c', delsys: 'ATSC-T' } as BaseRow)).toBe('57000000 Hz')
  })

  it('uses kHz for DVB-S and ISDB-S muxes', () => {
    expect(formatMuxFrequency(11778000, { uuid: 'a', delsys: 'DVB-S2', polarisation: 'V' } as BaseRow)).toBe(
      '11778000 kHz',
    )
    /* A DVB-S mux is recognised by its polarisation field alone. */
    expect(formatMuxFrequency(12188000, { uuid: 'b', polarisation: 'H' } as BaseRow)).toBe('12188000 kHz')
    expect(formatMuxFrequency(1049480, { uuid: 'c', delsys: 'ISDB-S' } as BaseRow)).toBe('1049480 kHz')
  })

  it('leaves rows without a frequency empty (IPTV muxes)', () => {
    expect(formatMuxFrequency(undefined, { uuid: 'a' } as BaseRow)).toBe('')
    expect(formatMuxFrequency(null, { uuid: 'b' } as BaseRow)).toBe('')
  })
})
