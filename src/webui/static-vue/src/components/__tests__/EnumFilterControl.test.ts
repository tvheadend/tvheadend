// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * EnumFilterControl — per-column funnel for enum-typed
 * columns. Tests cover the inline `Option[]` rendering path
 * (the synchronous one); the deferred-enum branch is covered
 * by `EnumNameCell`'s test suite + the integration tests of
 * any consumer that exercises a deferred enum. Mounting
 * PrimeVue's full Select dropdown overlay is brittle in
 * happy-dom, so the assertions stub the Select with a
 * lightweight passthrough that exposes the props + emits the
 * test cares about.
 */

import { describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import EnumFilterControl from '../EnumFilterControl.vue'
import type { Option } from '../idnode-fields/deferredEnum'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s }),
  t: (s: string) => s,
}))

/* Stub: render every Option as a `<button>` with a data
 * attribute the tests can target. `update:modelValue` fires on
 * button click; the "(any)" affordance is the stub's
 * `clear-button` which emits null. */
const SELECT_STUB = defineComponent({
  name: 'SelectStub',
  props: {
    modelValue: { type: [String, Number, null], default: null },
    options: { type: Array, default: () => [] },
    optionValue: { type: String, default: 'value' },
    optionLabel: { type: String, default: 'label' },
    optionGroupLabel: { type: String, default: undefined },
    optionGroupChildren: { type: String, default: undefined },
    placeholder: { type: String, default: '' },
    ariaLabel: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { emit }) {
    return () => {
      /* When optionGroupLabel + optionGroupChildren are
       * supplied, PrimeVue Select renders grouped: the
       * options array is `[{label, items: [...]}]`. The
       * stub mirrors that: emits one `.sel-stub__group`
       * per bucket with its label + the items underneath. */
      const isGrouped = !!props.optionGroupLabel && !!props.optionGroupChildren
      type Group = { label: string; items: { key: string | number; val: string }[] }
      type FlatOpt = { key: string | number; val: string }
      const rows: ReturnType<typeof h>[] = []
      if (isGrouped) {
        for (const g of props.options as Group[]) {
          rows.push(
            h('div', { class: 'sel-stub__group', 'data-group-label': g.label }, g.label),
          )
          for (const opt of g.items) {
            rows.push(
              h(
                'button',
                {
                  class: 'sel-stub__opt',
                  'data-key': String(opt.key),
                  'data-in-group': g.label,
                  onClick: () => emit('update:modelValue', opt.key),
                },
                opt.val,
              ),
            )
          }
        }
      } else {
        for (const opt of props.options as FlatOpt[]) {
          rows.push(
            h(
              'button',
              {
                class: 'sel-stub__opt',
                'data-key': String(opt.key),
                onClick: () => emit('update:modelValue', opt.key),
              },
              opt.val,
            ),
          )
        }
      }
      return h('div', { class: 'sel-stub', 'aria-label': props.ariaLabel }, [
        h('span', { class: 'sel-stub__placeholder' }, props.placeholder),
        h(
          'span',
          { class: 'sel-stub__value', 'data-value': String(props.modelValue ?? '') },
          String(props.modelValue ?? ''),
        ),
        ...rows,
        h(
          'button',
          {
            class: 'sel-stub__clear',
            onClick: () => emit('update:modelValue', null),
          },
          'Clear',
        ),
      ])
    }
  },
})

const PRI_OPTIONS: Option[] = [
  { key: 6, val: 'Default' },
  { key: 0, val: 'Important' },
  { key: 1, val: 'High' },
  { key: 2, val: 'Normal' },
  { key: 3, val: 'Low' },
  { key: 4, val: 'Unimportant' },
]

function mountWith(modelValue: number | string | null = null) {
  return mount(EnumFilterControl, {
    props: {
      modelValue,
      enumSource: PRI_OPTIONS,
      controlLabel: 'Filter Priority',
    },
    global: {
      stubs: { Select: SELECT_STUB },
    },
  })
}

describe('EnumFilterControl — inline Option[] source', () => {
  it('renders every option label from the inline source', () => {
    const w = mountWith()
    const buttons = w.findAll('.sel-stub__opt')
    expect(buttons).toHaveLength(6)
    expect(buttons.map((b) => b.text())).toEqual([
      'Default',
      'Important',
      'High',
      'Normal',
      'Low',
      'Unimportant',
    ])
  })

  it('renders the default placeholder when no override is supplied', () => {
    const w = mountWith()
    expect(w.find('.sel-stub__placeholder').text()).toBe('Any')
  })

  it('forwards the controlLabel prop to the inner Select as aria-label', () => {
    const w = mountWith()
    expect(w.find('.sel-stub').attributes('aria-label')).toBe('Filter Priority')
  })

  it('emits update:modelValue with the chosen option key on selection', async () => {
    const w = mountWith()
    /* Click "Important" — key 0. */
    await w.find('[data-key="0"]').trigger('click')
    const emitted = w.emitted('update:modelValue')
    expect(emitted).toBeDefined()
    expect(emitted![0]).toEqual([0])
  })

  it('emits update:modelValue with null when the user clears the selection', async () => {
    const w = mountWith(0)
    await w.find('.sel-stub__clear').trigger('click')
    const emitted = w.emitted('update:modelValue')
    expect(emitted).toBeDefined()
    expect(emitted![0]).toEqual([null])
  })

  it('reflects the current modelValue into the inner Select', () => {
    /* The stub stamps modelValue into a data-value attribute
     * so the assertion can read it back without mounting
     * PrimeVue's overlay. */
    const w = mountWith(1)
    expect(w.find('.sel-stub__value').attributes('data-value')).toBe('1')
  })
})

describe('EnumFilterControl — grouped (tree) options', () => {
  /* EIT content_type shape: major-group codes (`0x10` =
   * "Movies/Drama", `0x20` = "News") share their option list
   * with subtype codes (`0x11` = "Detective", `0x12` =
   * "Adventure/Western"). The major-group entry itself
   * doubles as the group-header label by virtue of its `key`
   * matching the bucket key the `groupBy` hook returns. */
  const CONTENT_TYPE_FIXTURE: Option[] = [
    { key: 0x10, val: 'Movies/Drama' },
    { key: 0x11, val: 'Detective' },
    { key: 0x12, val: 'Adventure/Western' },
    { key: 0x20, val: 'News' },
    { key: 0x21, val: 'News magazine' },
  ]

  const groupByEitMajor = (opt: Option) => Number(opt.key) & 0xf0

  it('renders one group header per unique bucket', () => {
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: CONTENT_TYPE_FIXTURE,
        controlLabel: 'Content type',
        groupBy: groupByEitMajor,
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    const headers = w.findAll('.sel-stub__group')
    expect(headers).toHaveLength(2)
    expect(headers[0].text()).toBe('Movies/Drama')
    expect(headers[1].text()).toBe('News')
  })

  it('places each option in its derived bucket', () => {
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: CONTENT_TYPE_FIXTURE,
        controlLabel: 'Content type',
        groupBy: groupByEitMajor,
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    const buttons = w.findAll('.sel-stub__opt')
    const map: Record<string, string> = {}
    for (const b of buttons) {
      map[b.attributes('data-key')!] = b.attributes('data-in-group')!
    }
    expect(map).toEqual({
      '16': 'Movies/Drama',   // 0x10
      '17': 'Movies/Drama',   // 0x11 Detective
      '18': 'Movies/Drama',   // 0x12 Adventure/Western
      '32': 'News',           // 0x20
      '33': 'News',           // 0x21 News magazine
    })
  })

  it('falls back to the bucket key when no option owns the group key', () => {
    /* Defensive: a server response that returns only
     * subtypes (no major-group entry like 0x10). The bucket
     * key (16) becomes the header label string. */
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: [
          { key: 0x11, val: 'Detective' },
          { key: 0x12, val: 'Adventure/Western' },
        ] as Option[],
        controlLabel: 'Content type',
        groupBy: groupByEitMajor,
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    const header = w.find('.sel-stub__group')
    expect(header.text()).toBe('16')
  })

  it('routes groupBy-null options to a trailing "Other" bucket', () => {
    /* When the hook returns null for some options, they
     * collect into an "Other" bucket so they remain
     * selectable. */
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: [
          { key: 0x10, val: 'Movies/Drama' },
          { key: 0x11, val: 'Detective' },
          { key: 'special', val: 'Special entry' },
        ] as Option[],
        controlLabel: 'Content type',
        groupBy: (opt) =>
          typeof opt.key === 'number' ? opt.key & 0xf0 : null,
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    const headers = w.findAll('.sel-stub__group')
    expect(headers).toHaveLength(2)
    expect(headers[0].text()).toBe('Movies/Drama')
    expect(headers[1].text()).toBe('Other')
  })

  it('flat list rendering preserved when groupBy is omitted', () => {
    /* The flat-list path is byte-identical to before: when
     * `groupBy` is unset, the stub sees no
     * optionGroupLabel/optionGroupChildren props and emits
     * a flat list of options. No `.sel-stub__group` entries. */
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: CONTENT_TYPE_FIXTURE,
        controlLabel: 'Content type',
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    expect(w.findAll('.sel-stub__group')).toHaveLength(0)
    expect(w.findAll('.sel-stub__opt')).toHaveLength(5)
  })
})

describe('EnumFilterControl — empty-label sanitisation', () => {
  it('drops options whose `val` is empty (server-placeholder rows)', () => {
    /* `epg/content_type/list` returns a `{key: 0, val: ""}`
     * entry as its first item (server's
     * `_epg_genre_names[0][0]` placeholder). It renders as
     * a blank dropdown row — unselectable in practice and
     * visually confusing. The component drops it. */
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: [
          { key: 0, val: '' },
          { key: 0x10, val: 'Movies/Drama' },
          { key: 0x20, val: 'News' },
        ] as Option[],
        controlLabel: 'Content type',
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    const buttons = w.findAll('.sel-stub__opt')
    expect(buttons).toHaveLength(2)
    expect(buttons.map((b) => b.text())).toEqual(['Movies/Drama', 'News'])
  })

  it('keeps options with whitespace-only labels? no — strict non-empty check', () => {
    /* Belt-and-braces: an entry with `val: ""` is dropped.
     * Whitespace-only would also produce a visually blank
     * row, but we keep the predicate strict on length > 0
     * so locale-quirks like a leading space remain visible
     * (the user can still see and pick them). */
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: [
          { key: 1, val: ' ' },
          { key: 2, val: 'OK' },
        ] as Option[],
        controlLabel: 'X',
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    expect(w.findAll('.sel-stub__opt')).toHaveLength(2)
  })
})

describe('EnumFilterControl — empty source', () => {
  it('renders no option buttons when the inline list is empty', () => {
    const w = mount(EnumFilterControl, {
      props: {
        modelValue: null,
        enumSource: [] as Option[],
        controlLabel: 'Filter X',
      },
      global: { stubs: { Select: SELECT_STUB } },
    })
    expect(w.findAll('.sel-stub__opt')).toHaveLength(0)
    /* The "(any)" clear affordance still renders so users
     * have a way to dismiss the popover gracefully. */
    expect(w.find('.sel-stub__clear').exists()).toBe(true)
  })
})
