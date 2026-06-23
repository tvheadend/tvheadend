// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldBool — compact-mode rendering. Pins that compact
 * mode emits only the bare `<input type="checkbox">` without the
 * `.ifld` wrapper or `.ifld__label`.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldBool from '../IdnodeFieldBool.vue'
import type { IdnodeProp } from '@/types/idnode'

const BOOL_PROP: IdnodeProp = {
  id: 'enabled',
  type: 'bool',
  caption: 'Enabled',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldBool — compact mode', () => {
  it('renders only the checkbox when compact', () => {
    const w = mount(IdnodeFieldBool, {
      props: { prop: BOOL_PROP, modelValue: true, compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    const cb = w.find('input.ifld__check')
    expect(cb.exists()).toBe(true)
    expect(cb.attributes('type')).toBe('checkbox')
  })

  it('keeps the .ifld wrapper + label by default', () => {
    const w = mount(IdnodeFieldBool, {
      props: { prop: BOOL_PROP, modelValue: true },
    })
    expect(w.find('.ifld').exists()).toBe(true)
    expect(w.find('.ifld__label').exists()).toBe(true)
  })
})
