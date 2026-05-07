/*
 * IdnodeFieldEnumMulti — empty-state rendering tests.
 *
 * Without an explicit empty-state, a deferred enum that resolves
 * to zero options (e.g. `epggrab` on a fresh server with no
 * harvested grabber-channel records) renders only the field
 * label + an empty `<div>` of checkboxes — looks like a broken
 * widget. Tests pin the placeholder so any future refactor that
 * skips the guard surfaces immediately.
 */

import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import IdnodeFieldEnumMulti from '../IdnodeFieldEnumMulti.vue'
import type { IdnodeProp } from '@/types/idnode'
import { setActivePinia, createPinia } from 'pinia'

beforeEach(() => {
  setActivePinia(createPinia())
})

const POPULATED_PROP: IdnodeProp = {
  id: 'tags',
  caption: 'Tags',
  type: 'str',
  list: true,
  enum: [
    { key: 'uuid-1', val: 'Sport' },
    { key: 'uuid-2', val: 'News' },
  ],
}

const EMPTY_PROP: IdnodeProp = {
  id: 'tags',
  caption: 'Tags',
  type: 'str',
  list: true,
  enum: [],
}

const MISSING_ENUM_PROP: IdnodeProp = {
  id: 'tags',
  caption: 'Tags',
  type: 'str',
  list: true,
}

describe('IdnodeFieldEnumMulti — empty-state placeholder', () => {
  it('renders inline checkboxes when options are populated', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: POPULATED_PROP, modelValue: [] },
    })
    expect(w.findAll('input[type="checkbox"]')).toHaveLength(2)
    expect(w.find('.ifld__empty').exists()).toBe(false)
  })

  it('renders the empty placeholder when options array is empty', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: EMPTY_PROP, modelValue: [] },
    })
    expect(w.findAll('input[type="checkbox"]')).toHaveLength(0)
    const empty = w.find('.ifld__empty')
    expect(empty.exists()).toBe(true)
    expect(empty.text()).toBe('(no options available)')
  })

  it('renders the empty placeholder when prop.enum is undefined', () => {
    /* Same UX as the empty-array case — useEnumOptions returns []
     * for both. The deferred-fetch initial state (before the API
     * lands) also flows through this path. */
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: MISSING_ENUM_PROP, modelValue: [] },
    })
    const empty = w.find('.ifld__empty')
    expect(empty.exists()).toBe(true)
    expect(empty.text()).toBe('(no options available)')
  })
})
