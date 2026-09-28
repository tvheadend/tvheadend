// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrFieldDefs — pins withServerPriEnum: the Priority filter options
 * come from the class metadata, which the server localizes, instead
 * of the inline English fallback list.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { DVR_FIELDS, withServerPriEnum } from '../dvrFieldDefs'
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
