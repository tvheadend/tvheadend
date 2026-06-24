// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * SearchInput — wrapper for `<input type="search">` with an
 * inline clear-`×` button. Tests cover the v-model contract,
 * the conditional clear-button visibility, and the
 * inheritAttrs passthrough.
 */

import { describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import SearchInput from '../SearchInput.vue'

describe('SearchInput', () => {
  it('renders a native search input with the modelValue', () => {
    const w = mount(SearchInput, { props: { modelValue: 'hello' } })
    const input = w.find('input')
    expect(input.attributes('type')).toBe('search')
    expect((input.element as HTMLInputElement).value).toBe('hello')
  })

  it('emits update:modelValue on input', async () => {
    const w = mount(SearchInput, { props: { modelValue: '' } })
    await w.find('input').setValue('typed')
    const events = w.emitted('update:modelValue')
    expect(events).toBeTruthy()
    expect(events?.[0]).toEqual(['typed'])
  })

  it('hides the clear button when modelValue is empty', () => {
    const w = mount(SearchInput, { props: { modelValue: '' } })
    expect(w.find('.search-input__clear').exists()).toBe(false)
  })

  it('shows the clear button when modelValue has text', () => {
    const w = mount(SearchInput, { props: { modelValue: 'x' } })
    expect(w.find('.search-input__clear').exists()).toBe(true)
  })

  it('emits an empty update:modelValue when the clear button is clicked', async () => {
    const w = mount(SearchInput, { props: { modelValue: 'something' } })
    await w.find('.search-input__clear').trigger('click')
    const events = w.emitted('update:modelValue')
    expect(events).toBeTruthy()
    expect(events?.[0]).toEqual([''])
  })

  it('lands consumer attrs (class, data-test) on the wrapper span', () => {
    /* Vue's scoped CSS only applies to elements rendered by
     * the component that owns the styles. Consumer classes
     * passed through to a CHILD component's inner element
     * would be silently ignored by the consumer's scoped CSS.
     * SearchInput lands attrs on the root `<span>` (the
     * element this component renders within the consumer's
     * scope) so consumer-side rules cascade naturally; the
     * inner input fills its parent's width. */
    const w = mount(SearchInput, {
      props: { modelValue: '' },
      attrs: { class: 'consumer-class', 'data-test': 'foo' },
    })
    const root = w.find('.search-input')
    expect(root.classes()).toContain('consumer-class')
    expect(root.attributes('data-test')).toBe('foo')
  })

  it('passes placeholder prop to the inner input', () => {
    const w = mount(SearchInput, {
      props: { modelValue: '', placeholder: 'Search…' },
    })
    expect(w.find('input').attributes('placeholder')).toBe('Search…')
  })

  it('aria-label defaults to placeholder when not explicitly set', () => {
    const w = mount(SearchInput, {
      props: { modelValue: '', placeholder: 'Find rows' },
    })
    expect(w.find('input').attributes('aria-label')).toBe('Find rows')
  })

  it('aria-label overrides placeholder when supplied', () => {
    const w = mount(SearchInput, {
      props: {
        modelValue: '',
        placeholder: 'Search…',
        ariaLabel: 'Filter the visible rows',
      },
    })
    expect(w.find('input').attributes('aria-label')).toBe('Filter the visible rows')
  })
})
