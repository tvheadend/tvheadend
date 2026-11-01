// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrFieldDefs — pins withServerPriEnum: the Priority filter options
 * come from the class metadata, which the server localizes, instead
 * of the inline English fallback list.
 *
 * Also pins the title group of the shared group-by options of the
 * dvr_entry list views (Upcoming / Finished / Failed / Removed):
 * Classic groups recordings by title (dvr.js groupRenderer on
 * disp_title), so a series can be folded into one cluster. The
 * cluster header strips Kodi label codes only when the access flag
 * asks for it, like the title cells.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useAccessStore } from '@/stores/access'
import { DVR_FIELDS, DVR_GROUPABLE_FIELDS, withServerPriEnum } from '../dvrFieldDefs'
import type { ColumnDef } from '@/types/column'
import type { IdnodeClassMeta } from '@/types/idnode'

beforeEach(() => {
  setActivePinia(createPinia())
})

/* The dvrentry class as a Czech session would receive it. */
const CS_PRI = [
  { key: 6, val: 'Výchozí' },
  { key: 0, val: 'Důležitá' },
  { key: 1, val: 'Vysoká' },
  { key: 2, val: 'Normální' },
  { key: 3, val: 'Nízká' },
  { key: 4, val: 'Nedůležitá' },
]
const CS_META = {
  props: [
    { id: 'disp_title', caption: 'Název', type: 'str' },
    { id: 'pri', caption: 'Priorita', type: 'int', enum: CS_PRI },
  ],
} as unknown as IdnodeClassMeta

const COLS: ColumnDef[] = [
  { field: 'disp_title', ...DVR_FIELDS.disp_title },
  { field: 'pri', ...DVR_FIELDS.pri },
]

describe('withServerPriEnum', () => {
  it('takes the priority filter options from the class metadata', () => {
    const out = withServerPriEnum(COLS, CS_META)
    expect(out.find((c) => c.field === 'pri')?.enumSource).toEqual(CS_PRI)
  })

  it('leaves the other columns alone', () => {
    const out = withServerPriEnum(COLS, CS_META)
    expect(out[0]).toBe(COLS[0])
  })

  it('keeps the inline options until the metadata has loaded', () => {
    expect(withServerPriEnum(COLS, undefined)).toBe(COLS)
    expect(withServerPriEnum(COLS, null)).toBe(COLS)
    const noEnum = { props: [{ id: 'pri', type: 'int' }] } as unknown as IdnodeClassMeta
    expect(withServerPriEnum(COLS, noEnum)).toBe(COLS)
  })
})

function titleGroup() {
  return DVR_GROUPABLE_FIELDS.find((g) => g.field === 'disp_title')
}

describe('DVR_GROUPABLE_FIELDS — title group', () => {
  it('offers grouping by title', () => {
    const g = titleGroup()
    expect(g).toBeDefined()
    expect(g?.label).toBe('Title')
    /* A projector would re-key the rows; the raw title is the key. */
    expect(g?.groupKey).toBeUndefined()
  })

  it('keeps the other group options', () => {
    expect(DVR_GROUPABLE_FIELDS.map((g) => g.field)).toEqual([
      'disp_title',
      'channel',
      'config_name',
      'start',
    ])
  })

  it('shows the raw title when label formatting is off', () => {
    useAccessStore().data = { admin: true, dvr: true }
    expect(titleGroup()?.headerLabel?.({ disp_title: '[B]News[/B]' })).toBe('[B]News[/B]')
  })

  it('strips Kodi label codes when label formatting is on', () => {
    useAccessStore().data = { admin: true, dvr: true, label_formatting: 1 }
    expect(titleGroup()?.headerLabel?.({ disp_title: '[B]News[/B]' })).toBe('News')
  })

  it('renders an empty header for a row without a title', () => {
    expect(titleGroup()?.headerLabel?.({})).toBe('')
  })
})
