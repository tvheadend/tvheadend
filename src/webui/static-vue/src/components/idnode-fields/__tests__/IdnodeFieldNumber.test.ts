// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldNumber — compact-mode rendering. Pins that compact
 * mode emits only the `<input type="number">` without the `.ifld`
 * wrapper or `.ifld__label`.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldNumber from '../IdnodeFieldNumber.vue'
import type { IdnodeProp } from '@/types/idnode'

const NUM_PROP: IdnodeProp = {
  id: 'pri',
  type: 'u32',
  caption: 'Priority',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldNumber — compact mode', () => {
  it('renders only the number input when compact', () => {
    const w = mount(IdnodeFieldNumber, {
      props: { prop: NUM_PROP, modelValue: 5, compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    const input = w.find('input.ifld__input')
    expect(input.exists()).toBe(true)
    expect(input.attributes('type')).toBe('number')
    expect(input.attributes('value')).toBe('5')
  })

  it('keeps the .ifld wrapper + label by default', () => {
    const w = mount(IdnodeFieldNumber, {
      props: { prop: NUM_PROP, modelValue: 5 },
    })
    expect(w.find('.ifld').exists()).toBe(true)
    expect(w.find('.ifld__label').exists()).toBe(true)
  })
})
