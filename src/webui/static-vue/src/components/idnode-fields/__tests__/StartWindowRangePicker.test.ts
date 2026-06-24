// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * StartWindowRangePicker — paired-field renderer for the autorec
 * (start, start_window) time-of-day window.
 *
 * Mounts with passthrough stubs for PrimeVue's <Slider> and
 * <Checkbox>: happy-dom's lack of pointer-event geometry makes the
 * real Slider's thumb drag unreliable in unit tests, and the
 * checkbox's binary v-model is the only behaviour we need from it
 * either way. The stubs expose the props the picker drives and
 * emit `update:modelValue` from a synthetic click / call —
 * sufficient to assert every code path through the script.
 *
 * Covered:
 *   - Load states map correctly to anyTime / slider thumbs / inputs
 *   - Half-Any normalisation (one field -1, the other valid → paired Any)
 *   - Toggle Any-on emits null for BOTH fields
 *   - Toggle Any-off emits the remembered (or default) thumb positions
 *   - Slider drag emits both updated positions, snapped to 10-min
 *   - Typed input parses, snaps, emits; invalid input reverts
 *   - Wrap-around state surfaces +1d affordance + "crosses midnight"
 *   - Zero-length window helper text
 *   - Disabled prop propagates
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { defineComponent, h, nextTick } from 'vue'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import StartWindowRangePicker from '../StartWindowRangePicker.vue'
import type { IdnodeProp } from '@/types/idnode'

const START_PROP: IdnodeProp = {
  id: 'start',
  type: 'str',
  caption: 'Start after',
  description: 'An event whose start time falls inside the window matches.',
  enum: [],
}
const STOP_PROP: IdnodeProp = {
  id: 'start_window',
  type: 'str',
  caption: 'Start before',
  enum: [],
}

const SLIDER_STUB = defineComponent({
  name: 'SliderStub',
  props: {
    modelValue: { type: Array, default: () => [0, 0] },
    min: { type: Number, default: 0 },
    max: { type: Number, default: 1440 },
    step: { type: Number, default: 1 },
    range: { type: Boolean, default: false },
    disabled: { type: Boolean, default: false },
  },
  emits: ['update:modelValue'],
  setup(props, { emit }) {
    return () =>
      h('div', {
        class: 'slider-stub',
        'data-model': JSON.stringify(props.modelValue),
        'data-min': String(props.min),
        'data-max': String(props.max),
        'data-step': String(props.step),
        'data-range': String(props.range),
        'data-disabled': String(props.disabled),
        onClick(ev: MouseEvent) {
          /* Tests dispatch synthetic clicks carrying the new pair
           * in a `data-emit` attribute on the firing element. */
          const target = ev.currentTarget as HTMLElement
          const raw = target.dataset.emit
          if (raw) emit('update:modelValue', JSON.parse(raw))
        },
      })
  },
})

const CHECKBOX_STUB = defineComponent({
  name: 'CheckboxStub',
  props: {
    modelValue: { type: Boolean, default: false },
    binary: { type: Boolean, default: false },
    disabled: { type: Boolean, default: false },
    inputId: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { emit }) {
    return () =>
      h('input', {
        type: 'checkbox',
        class: 'cb-stub',
        'data-disabled': String(props.disabled),
        checked: props.modelValue,
        onChange(ev: Event) {
          emit('update:modelValue', (ev.target as HTMLInputElement).checked)
        },
      })
  },
})

function mountPicker(values: {
  start?: unknown
  start_window?: unknown
  disabled?: boolean
}) {
  return mount(StartWindowRangePicker, {
    props: {
      groupProps: { start: START_PROP, start_window: STOP_PROP },
      groupValues: {
        start: values.start,
        start_window: values.start_window,
      },
      disabled: values.disabled ?? false,
    },
    global: {
      stubs: { Slider: SLIDER_STUB, Checkbox: CHECKBOX_STUB },
      directives: { tooltip: () => undefined },
    },
  })
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('StartWindowRangePicker — load states', () => {
  it('both Any (null/null) → "Any time" checked, slider hidden', () => {
    const w = mountPicker({ start: null, start_window: null })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(true)
    expect(w.find('.start-window-picker__body').exists()).toBe(false)
  })

  it('half-Any (start null, stop set) → "Any time" checked (display-only normalisation)', () => {
    const w = mountPicker({ start: null, start_window: '22:00' })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(true)
    expect(w.find('.start-window-picker__body').exists()).toBe(false)
  })

  it('half-Any (start set, stop null) → "Any time" checked (display-only, mirror)', () => {
    const w = mountPicker({ start: '20:00', start_window: null })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(true)
  })

  it('translated "Any" string (non-digit first char) treated as Any', () => {
    /* Server emits the translated word "Any" via tvh_gettext_lang;
     * coercion rule mirrors `dvr_autorec.c:732` (first char non-digit → -1). */
    const w = mountPicker({ start: 'Beliebig', start_window: '22:00' })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(true)
  })

  it('both valid → slider visible at parsed positions, duration "2 h 0 min"', () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(false)
    const model = JSON.parse(w.find('.slider-stub').attributes('data-model') ?? '')
    expect(model).toEqual([1200, 1320])
    expect(w.find('.start-window-picker__duration').text()).toBe('2 h 0 min')
  })

  it('wrap-around (22:00 → 02:00) → +1d suffix + "crosses midnight" caption', () => {
    const w = mountPicker({ start: '22:00', start_window: '02:00' })
    expect(w.find('.start-window-picker__next-day').exists()).toBe(true)
    expect(w.find('.start-window-picker__duration').text()).toBe('4 h 0 min · crosses midnight')
  })

  it('zero-length window (start == stop) → "matches only events starting at exactly HH:MM"', () => {
    const w = mountPicker({ start: '20:00', start_window: '20:00' })
    expect(w.find('.start-window-picker__duration').text()).toBe(
      '0 min · matches only events starting at exactly 20:00',
    )
  })

  it('snaps loaded value to slider step (21:33 → 21:30 on display, but raw kept)', () => {
    /* Picker preserves the loaded value on display via formatTime
     * after parsing. Off-grid values render at their parsed time;
     * only outgoing edits snap to 10-min boundaries. */
    const w = mountPicker({ start: '21:33', start_window: '22:00' })
    const startInput = w.findAll('input[type="text"]')[0]
    expect((startInput.element as HTMLInputElement).value).toBe('21:33')
  })
})

describe('StartWindowRangePicker — toggle Any', () => {
  it('Any → on emits null for BOTH fields', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const cb = w.find('input.cb-stub')
    ;(cb.element as HTMLInputElement).checked = true
    await cb.trigger('change')
    const emits = w.emitted('update')!
    /* Two emits, one per field, both null. */
    expect(emits).toEqual(
      expect.arrayContaining([
        ['start', null],
        ['start_window', null],
      ]),
    )
  })

  it('Any → off emits remembered start + stop (the loaded values)', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    /* Step 1 — toggle ON: remembers (1200, 1320). */
    const cb = w.find('input.cb-stub')
    ;(cb.element as HTMLInputElement).checked = true
    await cb.trigger('change')
    w.emitted('update')!.length = 0
    /* Step 2 — toggle OFF: emits the remembered pair. */
    ;(cb.element as HTMLInputElement).checked = false
    await cb.trigger('change')
    expect(w.emitted('update')).toEqual([
      ['start', '20:00'],
      ['start_window', '22:00'],
    ])
  })

  it('Any → off from a fresh both-Any state emits defaults (20:00, 22:00)', async () => {
    const w = mountPicker({ start: null, start_window: null })
    const cb = w.find('input.cb-stub')
    ;(cb.element as HTMLInputElement).checked = false
    await cb.trigger('change')
    expect(w.emitted('update')).toEqual([
      ['start', '20:00'],
      ['start_window', '22:00'],
    ])
  })
})

describe('StartWindowRangePicker — slider drag', () => {
  it('emits both thumb positions on slider update, snapped to 10-min', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const slider = w.find('.slider-stub')
    /* Synthesise a slider update to (1290, 1380) — already on 10-min grid. */
    ;(slider.element as HTMLElement).dataset.emit = JSON.stringify([1290, 1380])
    await slider.trigger('click')
    const emits = w.emitted('update')!
    expect(emits).toEqual(
      expect.arrayContaining([
        ['start', '21:30'],
        ['start_window', '23:00'],
      ]),
    )
  })

  it('emits only the changed side when one thumb stays put', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const slider = w.find('.slider-stub')
    /* start unchanged; stop moves 22:00 → 23:00. */
    ;(slider.element as HTMLElement).dataset.emit = JSON.stringify([1200, 1380])
    await slider.trigger('click')
    const emits = w.emitted('update')!
    expect(emits.map((e) => (e as unknown[])[0])).toEqual(['start_window'])
    expect(emits[0]).toEqual(['start_window', '23:00'])
  })
})

describe('StartWindowRangePicker — typed input', () => {
  it('valid "HH:MM" + blur emits the parsed time', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const startInput = w.findAll('input[type="text"]')[0]
    await startInput.setValue('21:30')
    await startInput.trigger('blur')
    expect(w.emitted('update')).toEqual([['start', '21:30']])
  })

  it('off-grid input snaps to nearest 10-min boundary', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const startInput = w.findAll('input[type="text"]')[0]
    await startInput.setValue('21:33')
    await startInput.trigger('blur')
    /* 21:33 → 21:30 (round-to-nearest). Both the emit + the
     * displayed text reflect the snap. */
    expect(w.emitted('update')).toEqual([['start', '21:30']])
    expect((startInput.element as HTMLInputElement).value).toBe('21:30')
  })

  it('invalid input reverts display, emits nothing', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const startInput = w.findAll('input[type="text"]')[0]
    await startInput.setValue('garbage')
    await startInput.trigger('blur')
    expect(w.emitted('update')).toBeUndefined()
    expect((startInput.element as HTMLInputElement).value).toBe('20:00')
  })

  it('no-op input (same as current) emits nothing', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    const startInput = w.findAll('input[type="text"]')[0]
    await startInput.setValue('20:00')
    await startInput.trigger('blur')
    expect(w.emitted('update')).toBeUndefined()
  })
})

describe('StartWindowRangePicker — disabled', () => {
  it('disabled propagates to slider + checkbox + inputs', () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00', disabled: true })
    expect(w.find('.slider-stub').attributes('data-disabled')).toBe('true')
    expect(w.find('input.cb-stub').attributes('data-disabled')).toBe('true')
    for (const inp of w.findAll('input[type="text"]')) {
      expect((inp.element as HTMLInputElement).disabled).toBe(true)
    }
  })

  it('readonly prop on either field also disables the picker', () => {
    const RO_START: IdnodeProp = { ...START_PROP, rdonly: true }
    const w = mount(StartWindowRangePicker, {
      props: {
        groupProps: { start: RO_START, start_window: STOP_PROP },
        groupValues: { start: '20:00', start_window: '22:00' },
        disabled: false,
      },
      global: {
        stubs: { Slider: SLIDER_STUB, Checkbox: CHECKBOX_STUB },
        directives: { tooltip: () => undefined },
      },
    })
    expect(w.find('.slider-stub').attributes('data-disabled')).toBe('true')
  })
})

describe('StartWindowRangePicker — parent value change', () => {
  it('re-evaluates anyTime when parent updates groupValues to new state', async () => {
    const w = mountPicker({ start: '20:00', start_window: '22:00' })
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(false)
    await w.setProps({
      groupProps: { start: START_PROP, start_window: STOP_PROP },
      groupValues: { start: null, start_window: null },
    })
    await nextTick()
    expect((w.find('input.cb-stub').element as HTMLInputElement).checked).toBe(true)
  })
})
