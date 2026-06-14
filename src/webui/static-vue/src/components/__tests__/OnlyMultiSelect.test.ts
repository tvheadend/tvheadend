// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * OnlyMultiSelect — wrapper around PrimeVue MultiSelect that
 * adds a per-row "Only" link, an optional leading icon, and a
 * "First label, +N more" trigger compression. The tests stub
 * out PrimeVue's MultiSelect with a passthrough that forwards
 * the `option` + `value` slots into the rendered output, so
 * the assertions can target the slot templates without booting
 * PrimeVue's interactive dropdown machinery.
 */

import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import OnlyMultiSelect from '../OnlyMultiSelect.vue'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s }),
  t: (s: string) => s,
}))

/* Stub: render every option through the `option` slot at
 * mount time so the template's icon + label + Only button are
 * exercised. The value slot is invoked with the current
 * modelValue + placeholder so the trigger compression can be
 * asserted alongside. The stub also passes through the
 * `update:modelValue` so emit-passthrough tests still work. */
const MULTISELECT_STUB = defineComponent({
  name: 'MultiSelect',
  props: {
    modelValue: { type: Array, default: () => [] },
    options: { type: Array, default: () => [] },
    placeholder: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { slots }) {
    return () =>
      h('div', { class: 'ms-stub' }, [
        h(
          'div',
          { class: 'ms-stub__value' },
          slots.value?.({ value: props.modelValue, placeholder: props.placeholder }),
        ),
        h(
          'ul',
          { class: 'ms-stub__list' },
          (props.options as { value: unknown; label: string }[]).map((opt, index) =>
            h(
              'li',
              { class: 'ms-stub__row', 'data-value': String(opt.value) },
              slots.option?.({ option: opt, selected: false, index }),
            ),
          ),
        ),
      ])
  },
})

function mountWith(
  props: {
    modelValue: (string | number)[]
    options: ReadonlyArray<{ value: string | number; label: string; icon?: string }>
    placeholder?: string
    ariaLabel?: string
    onlyLabel?: string
  },
) {
  return mount(OnlyMultiSelect, {
    props: {
      modelValue: props.modelValue,
      options: props.options,
      placeholder: props.placeholder,
      ariaLabel: props.ariaLabel ?? 'Label',
      onlyLabel: props.onlyLabel,
    },
    global: {
      stubs: { MultiSelect: MULTISELECT_STUB },
    },
  })
}

describe('OnlyMultiSelect — option template', () => {
  it('renders a label for every option', () => {
    const w = mountWith({
      modelValue: [],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
      ],
    })
    expect(w.text()).toContain('Alpha')
    expect(w.text()).toContain('Beta')
  })

  it('renders an icon when the option carries one', () => {
    const w = mountWith({
      modelValue: [],
      options: [{ value: 'a', label: 'Alpha', icon: 'https://example/icon.png' }],
    })
    const icon = w.find('.only-multiselect__icon')
    expect(icon.exists()).toBe(true)
    expect(icon.attributes('src')).toBe('https://example/icon.png')
  })

  it('skips the icon when the option has no icon URL', () => {
    const w = mountWith({
      modelValue: [],
      options: [{ value: 'a', label: 'Alpha' }],
    })
    expect(w.find('.only-multiselect__icon').exists()).toBe(false)
  })

  it('renders an "Only" button per row by default', () => {
    const w = mountWith({
      modelValue: [],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
      ],
    })
    const buttons = w.findAll('.only-multiselect__only')
    expect(buttons).toHaveLength(2)
    expect(buttons[0].text()).toBe('Only')
  })

  it('respects the onlyLabel prop override', () => {
    const w = mountWith({
      modelValue: [],
      options: [{ value: 'a', label: 'Alpha' }],
      onlyLabel: 'Just this',
    })
    expect(w.find('.only-multiselect__only').text()).toBe('Just this')
  })
})

describe('OnlyMultiSelect — Only click', () => {
  it('emits `only` with the row value when the link is clicked', async () => {
    const w = mountWith({
      modelValue: ['a'],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
      ],
    })
    const buttons = w.findAll('.only-multiselect__only')
    await buttons[1].trigger('click')
    const emitted = w.emitted('only')
    expect(emitted).toBeDefined()
    expect(emitted![0]).toEqual(['b'])
  })

  it('does NOT also fire update:modelValue from the Only click', async () => {
    /* @click.stop on the Only button keeps the wrapping row's
     * built-in selection-toggle from running. The wrapper's
     * own emit chain skips update:modelValue too — parent
     * decides whether "Only" means setSelection([value]) by
     * listening for the `only` event explicitly. */
    const w = mountWith({
      modelValue: ['a'],
      options: [{ value: 'a', label: 'Alpha' }],
    })
    await w.find('.only-multiselect__only').trigger('click')
    expect(w.emitted('update:modelValue')).toBeUndefined()
  })
})

describe('OnlyMultiSelect — value slot compression', () => {
  it('shows the placeholder when no values are selected', () => {
    const w = mountWith({
      modelValue: [],
      options: [{ value: 'a', label: 'Alpha' }],
      placeholder: 'Any',
    })
    expect(w.find('.ms-stub__value').text()).toBe('Any')
  })

  it('shows just the single label when one value is selected', () => {
    const w = mountWith({
      modelValue: ['a'],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
      ],
    })
    expect(w.find('.ms-stub__value').text()).toBe('Alpha')
  })

  it('shows "First, +N more" when 2+ values are selected (but not all)', () => {
    /* Selection is 3-of-4 → "+N more" branch (the all-
     * selected branch only fires when length === options
     * length, see the test below). Browsers collapse
     * whitespace visually so the precise spacing between
     * the leading label, the comma, and the "+N more"
     * suffix doesn't matter in render — assert the
     * substrings rather than tying to whitespace layout. */
    const w = mountWith({
      modelValue: ['a', 'b', 'c'],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
        { value: 'c', label: 'Gamma' },
        { value: 'd', label: 'Delta' },
      ],
    })
    const txt = w.find('.ms-stub__value').text()
    expect(txt).toContain('Alpha')
    expect(txt).toContain('+2 more')
    expect(txt).not.toContain('Beta') /* Compressed away */
  })

  it('shows "All" when every option is selected (no-narrowing shorthand)', () => {
    /* The 2-of-3 case shows the leading label + "+N more";
     * once every option is in the selection, there's no
     * meaningful narrowing to summarise — the trigger
     * collapses to a single "All" label. */
    const w = mountWith({
      modelValue: ['a', 'b', 'c'],
      options: [
        { value: 'a', label: 'Alpha' },
        { value: 'b', label: 'Beta' },
        { value: 'c', label: 'Gamma' },
      ],
    })
    expect(w.find('.ms-stub__value').text()).toBe('All')
  })

  it('does NOT show "All" when only one option exists and it is selected', () => {
    /* Edge case: a single-option list where the user has
     * picked the one option. Showing "All" here would be
     * literally accurate but visually weird — just render
     * the label. The threshold for "All" is "more than one
     * option, every option selected." */
    const w = mountWith({
      modelValue: ['a'],
      options: [{ value: 'a', label: 'Alpha' }],
    })
    /* One option = the 1-selected branch (not 2+), so the
     * label renders directly. This test pins that — adding
     * "All" handling for the 1/1 case would silently change
     * single-option pickers across the app. */
    expect(w.find('.ms-stub__value').text()).toBe('Alpha')
  })

  it('does NOT show "All" when the option list is empty', () => {
    /* Pure defensiveness: an empty options list means
     * nothing to select. Trigger should fall through to the
     * placeholder (0 selected ↔ 0 options is the no-options
     * empty state). */
    const w = mountWith({
      modelValue: [],
      options: [],
      placeholder: 'Any',
    })
    expect(w.find('.ms-stub__value').text()).toBe('Any')
  })

  it('falls back to String(value) when no matching option label exists', () => {
    /* Defensive: if the model carries a value that's not in
     * the options array (stale persisted state, racy
     * options-loading), render the raw value rather than
     * an empty cell. */
    const w = mountWith({
      modelValue: ['z'],
      options: [{ value: 'a', label: 'Alpha' }],
    })
    expect(w.find('.ms-stub__value').text()).toBe('z')
  })
})

describe('OnlyMultiSelect — numeric values', () => {
  /* The wrapper is generic across string | number value types.
   * Genre uses numeric major-group codes; tags use UUID
   * strings. Verifying numeric values flow through identically
   * regression-protects the genre consumer. */

  it('handles numeric option values + emits them on Only click', async () => {
    const w = mount(OnlyMultiSelect, {
      props: {
        modelValue: [] as number[],
        options: [
          { value: 16, label: 'Movie' },
          { value: 64, label: 'Sports' },
        ],
        ariaLabel: 'Genre',
      },
      global: { stubs: { MultiSelect: MULTISELECT_STUB } },
    })
    expect(w.text()).toContain('Movie')
    expect(w.text()).toContain('Sports')
    await w.findAll('.only-multiselect__only')[0].trigger('click')
    expect(w.emitted('only')![0]).toEqual([16])
  })
})
