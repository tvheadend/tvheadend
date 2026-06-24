// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EntityPickerTable tests — the compact single-select table that
 * tops the multi-entry picker drawer.
 */
import { afterEach, describe, expect, it } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import EntityPickerTable from '../EntityPickerTable.vue'
import type { PickerColumn, PickerRow } from '@/types/picker'

const columns: PickerColumn[] = [
  { field: 'title', label: 'Title' },
  { field: 'when', label: 'When', format: (v) => `@${String(v)}` },
]
const rows: PickerRow[] = [
  { uuid: 'u1', title: 'Alpha', when: 10 },
  { uuid: 'u2', title: 'Beta', when: 20 },
]

enableAutoUnmount(afterEach)

describe('EntityPickerTable', () => {
  it('renders a header per column and a row per entry', () => {
    const w = mount(EntityPickerTable, { props: { rows, columns, selected: null } })
    expect(w.findAll('.entity-picker__th').map((n) => n.text())).toEqual(['Title', 'When'])
    expect(w.findAll('.entity-picker__row')).toHaveLength(2)
  })

  it('formats cell values with the column format fn, raw otherwise', () => {
    const w = mount(EntityPickerTable, { props: { rows, columns, selected: null } })
    const cells = w.findAll('.entity-picker__row')[0].findAll('.entity-picker__td')
    expect(cells[0].text()).toBe('Alpha')
    expect(cells[1].text()).toBe('@10')
  })

  it('renders an empty cell for a missing field with no format', () => {
    const w = mount(EntityPickerTable, {
      props: { rows: [{ uuid: 'x' }], columns, selected: null },
    })
    expect(w.findAll('.entity-picker__td')[0].text()).toBe('')
  })

  it('marks the row matching `selected`', () => {
    const w = mount(EntityPickerTable, { props: { rows, columns, selected: 'u2' } })
    const r = w.findAll('.entity-picker__row')
    expect(r[0].classes()).not.toContain('entity-picker__row--selected')
    expect(r[1].classes()).toContain('entity-picker__row--selected')
    expect(r[1].attributes('aria-selected')).toBe('true')
  })

  it('emits select with the row uuid on click', async () => {
    const w = mount(EntityPickerTable, { props: { rows, columns, selected: null } })
    await w.findAll('.entity-picker__row')[1].trigger('click')
    expect(w.emitted('select')).toEqual([['u2']])
  })

  it('emits select on Enter and Space', async () => {
    const w = mount(EntityPickerTable, { props: { rows, columns, selected: null } })
    await w.findAll('.entity-picker__row')[0].trigger('keydown', { key: 'Enter' })
    await w.findAll('.entity-picker__row')[0].trigger('keydown', { key: ' ' })
    expect(w.emitted('select')).toEqual([['u1'], ['u1']])
  })
})
