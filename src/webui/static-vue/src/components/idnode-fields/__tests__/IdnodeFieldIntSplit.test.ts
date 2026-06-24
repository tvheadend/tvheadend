// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldIntSplit — numeric prop with the `intsplit` modifier.
 *
 * Pins display tolerance for both wire shapes (string "100.1"
 * when fractional, number 100 when whole), keystroke masking
 * (digits + at most one dot), commit shape (always emits string
 * for round-trip stability), readonly, and the trailing-dot
 * blur fix-up.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldIntSplit from '../IdnodeFieldIntSplit.vue'
import type { IdnodeProp } from '@/types/idnode'

const ISPLIT_PROP: IdnodeProp = {
  id: 'number',
  type: 's64',
  caption: 'Number',
  intsplit: true,
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldIntSplit — display', () => {
  it('renders a fractional string as-is', () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: '100.1' },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('100.1')
  })

  it('renders a whole number as a plain integer string', () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: 100 },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('100')
  })

  it('renders empty for null model', () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: null },
    })
    expect((w.find('input').element as HTMLInputElement).value).toBe('')
  })
})

describe('IdnodeFieldIntSplit — input', () => {
  it('accepts a fractional number and emits the string form', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: 100 },
    })
    await w.find('input').setValue('100.5')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['100.5'])
  })

  it('accepts a plain whole number and emits string form', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: null },
    })
    await w.find('input').setValue('200')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['200'])
  })

  it('emits null for empty input', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: 100 },
    })
    await w.find('input').setValue('')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([null])
  })

  it('does NOT emit on transient trailing-dot input', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: 100 },
    })
    await w.find('input').setValue('100.')
    expect(w.emitted('update:modelValue')).toBeUndefined()
  })

  it('strips non-digit / non-dot characters from input', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: null },
    })
    const input = w.find('input').element as HTMLInputElement
    /* Simulate a paste of "1abc.5xyz" — mask should keep only "1.5". */
    input.value = '1abc.5xyz'
    await w.find('input').trigger('input')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['1.5'])
    /* And the visible value reflects the cleaned form. */
    expect(input.value).toBe('1.5')
  })

  it('rejects a second dot — keeps only the first', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: null },
    })
    const input = w.find('input').element as HTMLInputElement
    input.value = '1.2.3'
    await w.find('input').trigger('input')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['1.23'])
    expect(input.value).toBe('1.23')
  })
})

describe('IdnodeFieldIntSplit — blur', () => {
  it('trims a trailing dot on blur', async () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: null },
    })
    await w.find('input').setValue('100.')
    await w.find('input').trigger('blur')
    /* The blur handler trims the trailing dot AND emits the
     * cleaned value. The input value also updates. */
    const emits = w.emitted('update:modelValue')
    expect(emits?.[emits.length - 1]).toEqual(['100'])
    expect((w.find('input').element as HTMLInputElement).value).toBe('100')
  })
})

describe('IdnodeFieldIntSplit — readonly', () => {
  it('disables the input when prop.rdonly is true', () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: { ...ISPLIT_PROP, rdonly: true }, modelValue: '100.1' },
    })
    expect((w.find('input').element as HTMLInputElement).disabled).toBe(true)
  })
})

describe('IdnodeFieldIntSplit — compact mode', () => {
  it('omits the .ifld wrapper and label in compact mode', () => {
    const w = mount(IdnodeFieldIntSplit, {
      props: { prop: ISPLIT_PROP, modelValue: '100.1', compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    const input = w.find('input.ifld__input--compact')
    expect(input.exists()).toBe(true)
    expect((input.element as HTMLInputElement).value).toBe('100.1')
  })
})
