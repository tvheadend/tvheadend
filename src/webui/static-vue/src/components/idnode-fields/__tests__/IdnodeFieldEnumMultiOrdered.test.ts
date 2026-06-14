// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldEnumMultiOrdered tests focused on the `noReorder`
 * variant introduced for the Config → Debugging subsystem
 * pickers. The ordered base flow (chevrons reorder, Selected
 * pane preserves insertion order) is exercised in the live
 * EPG → Default Languages page; this file pins the noReorder
 * differences so a future refactor that drops them surfaces
 * the regression immediately.
 */

import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldEnumMultiOrdered from '../IdnodeFieldEnumMultiOrdered.vue'
import type { IdnodeProp } from '@/types/idnode'

beforeEach(() => {
  setActivePinia(createPinia())
})

const PROP: IdnodeProp = {
  id: 'subs',
  caption: 'Subsystems',
  type: 'str',
  list: true,
  enum: [
    { key: 'zeta', val: 'Zeta' },
    { key: 'alpha', val: 'Alpha' },
    { key: 'gamma', val: 'Gamma' },
  ],
}

describe('IdnodeFieldEnumMultiOrdered — noReorder', () => {
  it('renders up/down chevrons by default (ordered mode)', () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: { prop: PROP, modelValue: ['alpha'] },
    })
    expect(w.find('button[aria-label="Move Up"]').exists()).toBe(true)
    expect(w.find('button[aria-label="Move Down"]').exists()).toBe(true)
  })

  it('hides up/down chevrons when noReorder is true', () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: { prop: PROP, modelValue: ['alpha'], noReorder: true },
    })
    expect(w.find('button[aria-label="Move Up"]').exists()).toBe(false)
    expect(w.find('button[aria-label="Move Down"]').exists()).toBe(false)
    /* Add / Remove keep working — they're the include/exclude
     * affordance the picker is built around. */
    expect(w.find('button[aria-label="Add to Selected"]').exists()).toBe(true)
    expect(w.find('button[aria-label="Remove from Selected"]').exists()).toBe(true)
  })

  it('preserves insertion order in Selected pane by default', () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: { prop: PROP, modelValue: ['zeta', 'alpha', 'gamma'] },
    })
    const selectedList = w.findAll('ul.ifld__transfer-list')[1]
    const items = selectedList.findAll('li.ifld__transfer-item').map((li) => li.text())
    expect(items).toEqual(['Zeta', 'Alpha', 'Gamma'])
  })

  it('alphabetises the Selected pane when noReorder is true', () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: {
        prop: PROP,
        modelValue: ['zeta', 'alpha', 'gamma'],
        noReorder: true,
      },
    })
    const selectedList = w.findAll('ul.ifld__transfer-list')[1]
    const items = selectedList.findAll('li.ifld__transfer-item').map((li) => li.text())
    expect(items).toEqual(['Alpha', 'Gamma', 'Zeta'])
  })

  it('add still works in noReorder mode', async () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: { prop: PROP, modelValue: ['alpha'], noReorder: true },
    })
    /* Highlight an option in the Available pane (zeta), then
     * click the + button. The emit should carry the new
     * modelValue with zeta appended. */
    const availableList = w.findAll('ul.ifld__transfer-list')[0]
    const zetaItem = availableList
      .findAll('li.ifld__transfer-item')
      .find((li) => li.text() === 'Zeta')
    expect(zetaItem).toBeDefined()
    await zetaItem!.trigger('click')
    await w.find('button[aria-label="Add to Selected"]').trigger('click')

    const emits = w.emitted('update:modelValue') as unknown[][]
    expect(emits).toBeDefined()
    expect(emits[emits.length - 1][0]).toEqual(['alpha', 'zeta'])
  })

  it('remove still works in noReorder mode', async () => {
    const w = mount(IdnodeFieldEnumMultiOrdered, {
      props: {
        prop: PROP,
        modelValue: ['alpha', 'gamma'],
        noReorder: true,
      },
    })
    const selectedList = w.findAll('ul.ifld__transfer-list')[1]
    const gammaItem = selectedList
      .findAll('li.ifld__transfer-item')
      .find((li) => li.text() === 'Gamma')
    expect(gammaItem).toBeDefined()
    await gammaItem!.trigger('click')
    await w.find('button[aria-label="Remove from Selected"]').trigger('click')

    const emits = w.emitted('update:modelValue') as unknown[][]
    expect(emits).toBeDefined()
    expect(emits[emits.length - 1][0]).toEqual(['alpha'])
  })
})
