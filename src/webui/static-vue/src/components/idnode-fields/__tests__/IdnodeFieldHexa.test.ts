// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldHexa — numeric prop with the `hexa` modifier.
 *
 * Pins display formatting (0xUPPER), tolerant input parsing
 * (accepts 0x / 0X / bare hex), commit shape (numeric on the
 * wire), readonly behaviour, and the blur-snap-back pattern
 * that recovers from invalid in-progress input.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldHexa from '../IdnodeFieldHexa.vue'
import type { IdnodeProp } from '@/types/idnode'

const HEXA_PROP: IdnodeProp = {
  id: 'caid',
  type: 'u16',
  caption: 'CAID',
  hexa: true,
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldHexa — display', () => {
  it('formats 1280 as 0x500', () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 1280 },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('0x500')
  })

  it('uppercases the hex digits', () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 0xabcd },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('0xABCD')
  })

  it('renders empty for null model', () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: null },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('')
  })
})

describe('IdnodeFieldHexa — input parsing', () => {
  it('accepts 0x500 and emits 1280', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 0 },
    })
    await w.find('input').setValue('0x500')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([1280])
  })

  it('accepts uppercase 0X500', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 0 },
    })
    await w.find('input').setValue('0X500')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([1280])
  })

  it('accepts bare 500 (no 0x prefix) as hex', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 0 },
    })
    await w.find('input').setValue('500')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([1280])
  })

  it('emits null for empty input', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 1280 },
    })
    await w.find('input').setValue('')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([null])
  })

  it('does NOT emit on transient-invalid input (lone 0x while typing)', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 1280 },
    })
    await w.find('input').setValue('0x')
    expect(w.emitted('update:modelValue')).toBeUndefined()
  })
})

describe('IdnodeFieldHexa — blur-snap-back', () => {
  it('snaps back to the last valid model value on blur if input is invalid', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 1280 },
    })
    /* Type something invalid that doesn't parse, then blur. The
     * display should revert to the canonical form of the prior
     * valid model. */
    await w.find('input').setValue('not-hex')
    await w.find('input').trigger('blur')
    expect((w.find('input').element as HTMLInputElement).value).toBe('0x500')
  })

  it('canonicalises bare hex to 0x form on blur', async () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 0 },
    })
    await w.find('input').setValue('abc')
    await w.find('input').trigger('blur')
    expect((w.find('input').element as HTMLInputElement).value).toBe('0xABC')
  })
})

describe('IdnodeFieldHexa — readonly', () => {
  it('disables the input when prop.rdonly is true', () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: { ...HEXA_PROP, rdonly: true }, modelValue: 1280 },
    })
    expect((w.find('input').element as HTMLInputElement).disabled).toBe(true)
  })
})

describe('IdnodeFieldHexa — compact mode', () => {
  it('omits the .ifld wrapper and label in compact mode', () => {
    const w = mount(IdnodeFieldHexa, {
      props: { prop: HEXA_PROP, modelValue: 1280, compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    const input = w.find('input.ifld__input--compact')
    expect(input.exists()).toBe(true)
    expect((input.element as HTMLInputElement).value).toBe('0x500')
  })
})
