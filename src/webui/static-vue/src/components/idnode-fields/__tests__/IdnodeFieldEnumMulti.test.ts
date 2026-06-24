// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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

/* PrimeVue MultiSelect needs the PrimeVue plugin's app-level
 * config; mounting it bare crashes inside its own setup. Stub
 * with a passthrough that we can grep for in the assertions —
 * we're testing the dispatch decision (dropdown vs inline), not
 * MultiSelect's own internals (PrimeVue's tests cover that). */
const MULTISELECT_STUB = {
  template: '<div class="ms-stub" />',
  props: ['modelValue', 'options', 'optionLabel', 'optionValue'],
}

describe('IdnodeFieldEnumMulti — render mode', () => {
  it('defaults to MultiSelect dropdown when populated and inline is unset', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: POPULATED_PROP, modelValue: [] },
      global: { stubs: { MultiSelect: MULTISELECT_STUB } },
    })
    expect(w.find('.ms-stub').exists()).toBe(true)
    expect(w.findAll('input[type="checkbox"]')).toHaveLength(0)
    expect(w.find('.ifld__empty').exists()).toBe(false)
  })

  it('renders inline checkboxes when inline=true and options are populated', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: POPULATED_PROP, modelValue: [], inline: true },
      global: { stubs: { MultiSelect: MULTISELECT_STUB } },
    })
    expect(w.findAll('input[type="checkbox"]')).toHaveLength(2)
    expect(w.find('.ms-stub').exists()).toBe(false)
    expect(w.find('.ifld__empty').exists()).toBe(false)
  })

  it('renders the empty placeholder when options array is empty (inline)', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: EMPTY_PROP, modelValue: [], inline: true },
    })
    expect(w.findAll('input[type="checkbox"]')).toHaveLength(0)
    const empty = w.find('.ifld__empty')
    expect(empty.exists()).toBe(true)
    expect(empty.text()).toBe('(no options available)')
  })

  it('renders the empty placeholder when prop.enum is undefined (default dropdown mode)', () => {
    /* Same UX as the empty-array case — useEnumOptions returns []
     * for both. The deferred-fetch initial state (before the API
     * lands) also flows through this path. The empty-state guard
     * fires regardless of the chosen render mode. */
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: MISSING_ENUM_PROP, modelValue: [] },
    })
    const empty = w.find('.ifld__empty')
    expect(empty.exists()).toBe(true)
    expect(empty.text()).toBe('(no options available)')
  })
})

/* Stub that DOES render the #value slot so we can inspect what
 * the parent emits into PrimeVue's collapsed-trigger surface.
 * The earlier MULTISELECT_STUB suppressed slots entirely. */
const MULTISELECT_VALUE_SLOT_STUB = {
  template: '<div class="ms-stub"><slot name="value" /></div>',
  props: ['modelValue', 'options', 'optionLabel', 'optionValue'],
}

const SORTABLE_PROP: IdnodeProp = {
  id: 'tags',
  caption: 'Tags',
  type: 'str',
  list: true,
  enum: [
    { key: 'uuid-1', val: 'Apples' },
    { key: 'uuid-2', val: 'Bananas' },
    { key: 'uuid-3', val: 'Cherries' },
  ],
}

describe('IdnodeFieldEnumMulti — collapsed-trigger display', () => {
  it('renders nothing in the value slot when modelValue is empty (PrimeVue placeholder takes over)', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: SORTABLE_PROP, modelValue: [] },
      global: { stubs: { MultiSelect: MULTISELECT_VALUE_SLOT_STUB } },
    })
    expect(w.find('.ifld__multiselect-value').exists()).toBe(false)
  })

  it('renders a bare label when exactly one item is selected', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: SORTABLE_PROP, modelValue: ['uuid-2'] },
      global: { stubs: { MultiSelect: MULTISELECT_VALUE_SLOT_STUB } },
    })
    const span = w.find('.ifld__multiselect-value')
    expect(span.exists()).toBe(true)
    expect(span.text()).toBe('Bananas')
    expect(span.attributes('title')).toBeUndefined()
  })

  it('compresses 2+ selections to "First, +N more" with full list on the title tooltip', () => {
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: SORTABLE_PROP, modelValue: ['uuid-1', 'uuid-2', 'uuid-3'] },
      global: { stubs: { MultiSelect: MULTISELECT_VALUE_SLOT_STUB } },
    })
    const span = w.find('.ifld__multiselect-value')
    expect(span.text()).toBe('Apples, +2 more')
    expect(span.attributes('title')).toBe('Apples, Bananas, Cherries')
  })

  it('sorts selected items by their position in the canonical options list', () => {
    /* Wire input reverse-ordered; canonical-sorted output puts
     * Apples first regardless of insertion order. */
    const w = mount(IdnodeFieldEnumMulti, {
      props: { prop: SORTABLE_PROP, modelValue: ['uuid-3', 'uuid-2', 'uuid-1'] },
      global: { stubs: { MultiSelect: MULTISELECT_VALUE_SLOT_STUB } },
    })
    const span = w.find('.ifld__multiselect-value')
    expect(span.text()).toBe('Apples, +2 more')
    expect(span.attributes('title')).toBe('Apples, Bananas, Cherries')
  })
})
