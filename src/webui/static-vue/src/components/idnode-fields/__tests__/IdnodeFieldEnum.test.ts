// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldEnum — single-value enum dropdown rendering tests.
 *
 * After the swap to PrimeVue `<Select>`, mounting the real Select
 * + its body-teleported overlay is brittle in happy-dom (positioning
 * + click-outside machinery don't fully simulate). The component
 * stubs `Select` with a lightweight passthrough that exposes the
 * `options` / `filter` / `disabled` props verbatim and emits
 * `update:modelValue` on a per-option button click. The stub is the
 * same shape as `EnumFilterControl.test.ts`'s SELECT_STUB — single
 * convention across the codebase.
 *
 * Edge cases covered (must survive the swap):
 *   - `(none)` clear-to-null option for str-typed non-mandatory props
 *   - `(none)` suppressed for numeric enums and for `mandatory: true`
 *   - Synthetic "current value" option for orphaned / pre-deferred-
 *     fetch references; excluded for empty string (no ghost row)
 *   - `update:modelValue` emission preserves native-`<select>` shape:
 *     string-coerced for option picks; `null` for the clear option
 *   - Filter affordance gated on option count (≥10)
 */

import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import IdnodeFieldEnum from '../IdnodeFieldEnum.vue'
import type { IdnodeProp } from '@/types/idnode'
import { setActivePinia, createPinia } from 'pinia'

beforeEach(() => {
  setActivePinia(createPinia())
})

/* Passthrough Select stub — exposes the props the tests assert on
 * + emits `update:modelValue` from a per-option button click. The
 * `(none)` clear option, the real options and the synthetic
 * current-value entry are all just rows in the `options` prop;
 * the stub renders them uniformly so a test can target by data-key. */
const SELECT_STUB = defineComponent({
  name: 'SelectStub',
  props: {
    modelValue: { type: [String, Number, null], default: null },
    options: { type: Array, default: () => [] },
    optionValue: { type: String, default: 'value' },
    optionLabel: { type: String, default: 'label' },
    filter: { type: Boolean, default: false },
    disabled: { type: Boolean, default: false },
    placeholder: { type: String, default: '' },
    ariaLabel: { type: String, default: '' },
    inputId: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { emit }) {
    return () => {
      type Opt = { key: string | number; val: string }
      const rows = (props.options as Opt[]).map((opt) =>
        h(
          'button',
          {
            class: 'sel-stub__opt',
            'data-key': String(opt.key),
            type: 'button',
            onClick: () => emit('update:modelValue', opt.key),
          },
          opt.val,
        ),
      )
      return h(
        'div',
        {
          class: 'sel-stub',
          'data-filter': String(props.filter),
          'data-disabled': String(props.disabled),
          'data-value': String(props.modelValue ?? ''),
          'aria-label': props.ariaLabel,
        },
        rows,
      )
    }
  },
})

function mountField(p: Record<string, unknown>) {
  return mount(IdnodeFieldEnum, {
    props: p as never,
    global: {
      stubs: { Select: SELECT_STUB },
      directives: {
        tooltip: { mounted: () => undefined, updated: () => undefined, unmounted: () => undefined },
      },
    },
  })
}

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

const NUMERIC_PRI_PROP: IdnodeProp = {
  /* Real-world int-keyed enum (mirrors dvr_entry's `pri`). The
   * native-`<select>` always emitted string-coerced values because
   * HTML option values are strings; preserving that shape after the
   * Select swap is what guards parents from a wire-format shift. */
  id: 'pri',
  caption: 'Priority',
  type: 'int',
  enum: [
    { key: 0, val: 'Important' },
    { key: 1, val: 'High' },
    { key: 2, val: 'Normal' },
    { key: 3, val: 'Low' },
    { key: 4, val: 'Unimportant' },
  ],
}

/* 10-entry inline enum — at the filter threshold. */
function makeTenOptionProp(): IdnodeProp {
  return {
    id: 'genre',
    type: 'str',
    /* type:'str' + no mandatory → also gets the synthetic (none),
     * pushing the combined-options count to 11. Above threshold. */
    enum: Array.from({ length: 10 }, (_, i) => ({
      key: `k${i}`,
      val: `Option ${i}`,
    })),
  }
}

describe('IdnodeFieldEnum — clear-to-null option', () => {
  it('renders an empty "(none)" option for str-typed enums', () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1' })
    const opts = w.findAll('.sel-stub__opt')
    expect(opts[0].attributes('data-key')).toBe('')
    expect(opts[0].text()).toBe('(none)')
    expect(opts).toHaveLength(3) /* (none) + 2 real options */
  })

  it('shows the empty value as the displayed selection when modelValue is null', () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: null })
    /* `:model-value="modelValue ?? ''"` makes the Select bind to ''
     * when modelValue is null — the (none) option then renders as
     * selected. */
    expect(w.find('.sel-stub').attributes('data-value')).toBe('')
  })

  it('selecting the empty option emits update:modelValue with null', async () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1' })
    /* Click the "(none)" row — the stub emits the option's key (''),
     * IdnodeFieldEnum's onChange handler maps '' → null. */
    await w.find('.sel-stub__opt[data-key=""]').trigger('click')
    const emitted = w.emitted('update:modelValue')
    expect(emitted).toBeTruthy()
    expect(emitted?.[0]).toEqual([null])
  })

  it('does NOT render the empty option for non-str enums (e.g. PT_INT tri-state)', () => {
    const w = mountField({ prop: INT_ENUM_PROP, modelValue: 1 })
    const opts = w.findAll('.sel-stub__opt')
    expect(opts).toHaveLength(3) /* all three real options, no (none) */
    for (const o of opts) {
      expect(o.text()).not.toBe('(none)')
    }
  })

  it('does NOT render the empty option when prop.mandatory is true', () => {
    /* Config singletons (language_ui / theme_ui) always carry a
     * runtime value and have no clearable equivalent in Classic.
     * IdnodeConfigForm synthesises `mandatory: true` for them via
     * its `mandatoryFields` allowlist; this test locks the
     * IdnodeFieldEnum side of the contract so it stays in place
     * once the future PO_MANDATORY flag replaces the manual list. */
    const w = mountField({
      prop: { ...STR_ENUM_PROP, mandatory: true },
      modelValue: 'uuid-1',
    })
    const opts = w.findAll('.sel-stub__opt')
    expect(opts).toHaveLength(2) /* just the two real options, no (none) */
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
     * `(none)` row, once the synthetic "value not in options" row. */
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: '' })
    const opts = w.findAll('.sel-stub__opt')
    expect(opts).toHaveLength(3) /* (none) + 2 real options, no ghost */
    expect(opts[0].text()).toBe('(none)')
    expect(opts[1].text()).toBe('Sport')
    expect(opts[2].text()).toBe('News')
  })
})

describe('IdnodeFieldEnum — synthetic current-value option', () => {
  it('prepends an entry for an orphaned reference (value not in options)', () => {
    /* Value matches no known key — e.g. a channel UUID that's been
     * deleted but a DVR entry still references it; or a deferred
     * fetch that hasn't landed yet. The synthetic entry keeps the
     * value visible instead of letting the Select pick the wrong
     * default. */
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'unknown-uuid' })
    const opts = w.findAll('.sel-stub__opt')
    /* (none) + 2 real options + 1 synthetic = 4 */
    expect(opts).toHaveLength(4)
    /* The synthetic entry is appended after the real options;
     * its key matches the raw value, its label is the same
     * string (no resolved display name available). */
    const last = opts[opts.length - 1]
    expect(last.attributes('data-key')).toBe('unknown-uuid')
    expect(last.text()).toBe('unknown-uuid')
  })

  it('does NOT add a synthetic entry when the value is in the options array', () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1' })
    const opts = w.findAll('.sel-stub__opt')
    expect(opts).toHaveLength(3) /* (none) + 2 real, no synthetic */
  })

  it('keeps a numeric off-list value as a raw number in the synthetic key', () => {
    /* PrimeVue Select matches optionValue by strict equality. A
     * numeric wire value (e.g. dvrentry's `pri`: 50) that misses the
     * options list must produce a synthetic option whose key === the
     * model value; a stringified "50" would never match and the
     * trigger would render the placeholder instead. */
    const w = mountField({ prop: NUMERIC_PRI_PROP, modelValue: 50 })
    const options = w.findComponent({ name: 'SelectStub' }).props('options') as Array<{
      key: string | number
      val: string
    }>
    const synthetic = options.find((o) => o.val === '50')
    expect(synthetic).toBeDefined()
    expect(synthetic?.key).toBe(50) /* strict number, not "50" */
  })
})

describe('IdnodeFieldEnum — compact mode', () => {
  it('renders only the Select when compact (no .ifld wrapper or label)', () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1', compact: true })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    expect(w.find('.sel-stub').exists()).toBe(true)
    /* Same option set as the non-compact variant — clear-to-null,
     * real options, no synthetic for a known value. */
    expect(w.findAll('.sel-stub__opt')).toHaveLength(3)
  })
})

describe('IdnodeFieldEnum — filter gating', () => {
  it('enables Select filter when combined-options count is ≥ 10', () => {
    /* 10 inline + (none) = 11 — at the threshold. */
    const w = mountField({ prop: makeTenOptionProp(), modelValue: null })
    expect(w.find('.sel-stub').attributes('data-filter')).toBe('true')
  })

  it('disables Select filter when combined-options count is below 10', () => {
    /* 2 inline + (none) = 3 — below the threshold. */
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1' })
    expect(w.find('.sel-stub').attributes('data-filter')).toBe('false')
  })
})

describe('IdnodeFieldEnum — value emission semantics', () => {
  it('emits the string-coerced key on a regular option pick', async () => {
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: null })
    await w.find('.sel-stub__opt[data-key="uuid-1"]').trigger('click')
    const emitted = w.emitted('update:modelValue')
    expect(emitted?.[0]).toEqual(['uuid-1'])
  })

  it('emits the raw numeric key on emit (round-trip with int-keyed options)', async () => {
    /* PrimeVue Select uses strict equality on optionValue when
     * driving its trigger display. If onChange string-coerced the
     * emitted value, the parent would store "3" but the option's
     * key is 3 (number, from `dvr_entry_class_duration_list`'s
     * htsmsg_add_u32 + similar `pri` int-keyed lists); on re-render
     * the Select can't find a match and the trigger renders blank
     * — the original symptom that prompted this contract.
     *
     * The emit-type signature already allows both string and number,
     * so the value flows through unchanged. Numeric-keyed enums
     * (dvr_entry's `pri`, `start_extra`, `stop_extra`) are the
     * canonical case this guards. */
    const w = mountField({ prop: NUMERIC_PRI_PROP, modelValue: 2 })
    await w.find('.sel-stub__opt[data-key="3"]').trigger('click')
    const emitted = w.emitted('update:modelValue')
    expect(emitted?.[0]).toEqual([3]) /* raw number, not stringified */
  })

  it('emits null when the chosen value is empty / null / undefined', async () => {
    /* The (none) row's key is ''; the onChange handler maps ''/null/
     * undefined → null so the parent's clear-to-null path fires. */
    const w = mountField({ prop: STR_ENUM_PROP, modelValue: 'uuid-1' })
    await w.find('.sel-stub__opt[data-key=""]').trigger('click')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([null])
  })
})
