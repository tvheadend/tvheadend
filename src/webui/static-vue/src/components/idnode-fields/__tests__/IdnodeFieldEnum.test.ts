/*
 * IdnodeFieldEnum — single-value enum dropdown rendering tests.
 *
 * Focus: the clear-to-null option for string-typed enums. For
 * `prop.type === 'str'` the dropdown renders an empty `(none)`
 * option as the first item; selecting it emits `null` so optional
 * UUID-reference fields (channel, tag, dvr_config, profile, …)
 * can be cleared from the editor — parity with ExtJS's combobox
 * `forceSelection: false` + clear-X behaviour. Numeric-typed
 * enums (PT_INT, etc.) skip the clear option because their value
 * sets are typically full-coverage and required.
 *
 * Other rendering behaviour (synthetic current-value option for
 * unknown keys, deferred-fetch in-flight rendering) is exercised
 * indirectly via `useEnumOptions` consumers and isn't repeated
 * here.
 */

import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import IdnodeFieldEnum from '../IdnodeFieldEnum.vue'
import type { IdnodeProp } from '@/types/idnode'
import { setActivePinia, createPinia } from 'pinia'

beforeEach(() => {
  setActivePinia(createPinia())
})

const STR_ENUM_PROP: IdnodeProp = {
  id: 'tag',
  caption: 'Channel tag',
  type: 'str',
  enum: [
    { key: 'uuid-1', val: 'Sport' },
    { key: 'uuid-2', val: 'News' },
  ],
}

const INT_ENUM_PROP: IdnodeProp = {
  id: 'enabled',
  caption: 'Enabled',
  type: 'int',
  enum: [
    { key: -1, val: 'Ignore' },
    { key: 0, val: 'Disable' },
    { key: 1, val: 'Enable' },
  ],
}

describe('IdnodeFieldEnum — clear-to-null option', () => {
  it('renders an empty "(none)" option for str-typed enums', () => {
    const w = mount(IdnodeFieldEnum, {
      props: { prop: STR_ENUM_PROP, modelValue: 'uuid-1' },
    })
    const opts = w.findAll('option')
    /* Empty option is first; existing inline options follow. */
    expect(opts[0].element.value).toBe('')
    expect(opts[0].text()).toBe('(none)')
    /* The two real options still render after it. */
    expect(opts).toHaveLength(3)
  })

  it('shows the empty option as the displayed selection when modelValue is null', () => {
    const w = mount(IdnodeFieldEnum, {
      props: { prop: STR_ENUM_PROP, modelValue: null },
    })
    /* `:value="modelValue ?? ''"` makes the select pick the
     * empty-string option when modelValue is null. */
    const select = w.find('select').element as HTMLSelectElement
    expect(select.value).toBe('')
  })

  it('selecting the empty option emits update:modelValue with null', async () => {
    const w = mount(IdnodeFieldEnum, {
      props: { prop: STR_ENUM_PROP, modelValue: 'uuid-1' },
    })
    /* User picks the "(none)" option. The handler maps `'' || null`
     * → null, so the parent's edit-state sees a clear. */
    await w.find('select').setValue('')
    const emitted = w.emitted('update:modelValue')
    expect(emitted).toBeTruthy()
    expect(emitted?.[0]).toEqual([null])
  })

  it('does NOT render the empty option for non-str enums (e.g. PT_INT tri-state)', () => {
    const w = mount(IdnodeFieldEnum, {
      props: { prop: INT_ENUM_PROP, modelValue: 1 },
    })
    const opts = w.findAll('option')
    /* All three real options render; no synthetic "(none)". */
    expect(opts).toHaveLength(3)
    for (const o of opts) {
      expect(o.text()).not.toBe('(none)')
    }
  })

  it('does NOT render a duplicate empty synthetic option when modelValue is ""', () => {
    /* A fresh entry's PT_STR field defaults to '' on the server
     * (e.g. mpegts_mux_sched.mux on Add). The `(none)` option is
     * the legitimate representation of empty; without an explicit
     * empty-string guard on the synthetic-current-value branch a
     * blank ghost row would appear right after `(none)` — once the
     * `(none)` row, once the synthetic "value not in options" row.
     * This test locks the contract: only `(none)` + the real
     * options render, no blank line in between. */
    const w = mount(IdnodeFieldEnum, {
      props: { prop: STR_ENUM_PROP, modelValue: '' },
    })
    const opts = w.findAll('option')
    expect(opts).toHaveLength(3) /* (none) + 2 real options */
    expect(opts[0].text()).toBe('(none)')
    expect(opts[1].text()).toBe('Sport')
    expect(opts[2].text()).toBe('News')
  })
})
