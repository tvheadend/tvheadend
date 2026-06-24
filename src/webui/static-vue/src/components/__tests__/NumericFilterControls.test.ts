// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * NumericFilterControls unit tests.
 *
 * Covers: form renders the right input count per operator; each
 * control change emits the correct model upward; eq→between
 * transition seeds value2 from the existing value.
 *
 * PrimeVue's Apply / Clear are not under test here — they live
 * in the surrounding filter menu popover, not in this component.
 * Wire-format translation lives in IdnodeGrid's onFilter and is
 * covered by IdnodeGrid.test.ts.
 */
import { describe, expect, it, beforeEach } from 'vitest'
import { mount } from '@vue/test-utils'
import NumericFilterControls, {
  type NumericFilterModel,
} from '../NumericFilterControls.vue'

function mountControls(modelValue: NumericFilterModel | null = null) {
  return mount(NumericFilterControls, {
    props: { modelValue },
    attachTo: document.body,
  })
}

beforeEach(() => {
  document.body.innerHTML = ''
})

describe('NumericFilterControls', () => {
  describe('renders correct inputs per operator', () => {
    it('shows one Value input for op:eq (default null model)', () => {
      const wrapper = mountControls(null)
      const labels = wrapper.findAll('.num-filter__label').map((l) => l.text())
      expect(labels).toEqual(['Operator', 'Value'])
      expect(wrapper.findAll('.num-filter__input')).toHaveLength(1)
    })

    it('shows one Value input for op:lt', () => {
      const wrapper = mountControls({ op: 'lt', value: 5, value2: null })
      const labels = wrapper.findAll('.num-filter__label').map((l) => l.text())
      expect(labels).toEqual(['Operator', 'Value'])
    })

    it('shows Min + Max inputs for op:between', () => {
      const wrapper = mountControls({ op: 'between', value: 5, value2: 10 })
      const labels = wrapper.findAll('.num-filter__label').map((l) => l.text())
      expect(labels).toEqual(['Operator', 'Min', 'Max'])
      const inputs = wrapper.findAll('.num-filter__input')
      expect(inputs).toHaveLength(2)
      expect((inputs[0].element as HTMLInputElement).value).toBe('5')
      expect((inputs[1].element as HTMLInputElement).value).toBe('10')
    })

    it('reflects the current model in the operator select', () => {
      const wrapper = mountControls({ op: 'gt', value: 7, value2: null })
      const sel = wrapper.find('.num-filter__select').element as HTMLSelectElement
      expect(sel.value).toBe('gt')
    })
  })

  describe('emits on change', () => {
    it('operator change emits a model with the new op', async () => {
      const wrapper = mountControls({ op: 'eq', value: 5, value2: null })
      await wrapper.find('.num-filter__select').setValue('gt')
      const emits = wrapper.emitted('update:modelValue')
      expect(emits).toBeTruthy()
      expect(emits![0][0]).toEqual({ op: 'gt', value: 5, value2: null })
    })

    it('value change emits a model with the new value', async () => {
      const wrapper = mountControls({ op: 'eq', value: 5, value2: null })
      await wrapper.find('.num-filter__input').setValue('42')
      const emits = wrapper.emitted('update:modelValue')
      expect(emits![0][0]).toEqual({ op: 'eq', value: 42, value2: null })
    })

    it('empty value emits a model with value:null (clear via Apply)', async () => {
      const wrapper = mountControls({ op: 'eq', value: 5, value2: null })
      await wrapper.find('.num-filter__input').setValue('')
      const emits = wrapper.emitted('update:modelValue')
      expect(emits![0][0]).toEqual({ op: 'eq', value: null, value2: null })
    })

    it('switching to between seeds value2 from the existing value', async () => {
      const wrapper = mountControls({ op: 'eq', value: 7, value2: null })
      await wrapper.find('.num-filter__select').setValue('between')
      const emits = wrapper.emitted('update:modelValue')
      expect(emits![0][0]).toEqual({ op: 'between', value: 7, value2: 7 })
    })

    it('switching from between to eq drops value2', async () => {
      const wrapper = mountControls({ op: 'between', value: 5, value2: 10 })
      await wrapper.find('.num-filter__select').setValue('eq')
      const emits = wrapper.emitted('update:modelValue')
      expect(emits![0][0]).toEqual({ op: 'eq', value: 5, value2: null })
    })

    it('between Min/Max inputs emit independently', async () => {
      const wrapper = mountControls({ op: 'between', value: 5, value2: 10 })
      const inputs = wrapper.findAll('.num-filter__input')
      await inputs[0].setValue('3')
      let emits = wrapper.emitted('update:modelValue')
      expect(emits![0][0]).toEqual({ op: 'between', value: 3, value2: 10 })
      await inputs[1].setValue('15')
      emits = wrapper.emitted('update:modelValue')
      expect(emits![1][0]).toEqual({ op: 'between', value: 5, value2: 15 })
    })
  })

  it('treats a null modelValue as a fresh eq form', () => {
    const wrapper = mountControls(null)
    const sel = wrapper.find('.num-filter__select').element as HTMLSelectElement
    expect(sel.value).toBe('eq')
    const input = wrapper.find('.num-filter__input').element as HTMLInputElement
    expect(input.value).toBe('')
  })
})
