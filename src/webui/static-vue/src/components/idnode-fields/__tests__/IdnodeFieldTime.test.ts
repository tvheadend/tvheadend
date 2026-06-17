// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldTime — compact-mode rendering. Pins that compact
 * mode emits only the input (plus the "minutes" suffix for
 * duration props) without the `.ifld` wrapper or `.ifld__label`.
 * Also covers the dvr_show_seconds gate on second-precision.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldTime from '../IdnodeFieldTime.vue'
import { useAccessStore } from '@/stores/access'
import type { IdnodeProp } from '@/types/idnode'

const TIME_PROP: IdnodeProp = {
  id: 'start',
  type: 'time',
  caption: 'Start',
}

const DURATION_PROP: IdnodeProp = {
  id: 'pre_padding',
  type: 'time',
  caption: 'Pre padding',
  duration: true,
}

const DATE_PROP: IdnodeProp = {
  id: 'born',
  type: 'time',
  caption: 'Born',
  date: true,
}

/* Specific epoch chosen so seconds field is non-zero in local
 * time. 2024-01-15 14:30:45 UTC → still has :45 in the user's
 * local seconds regardless of timezone (seconds field doesn't
 * shift with timezone offsets, which are always whole minutes
 * for the common zones; round-half timezones like
 * Australia/Adelaide are 30-min offsets, still preserving the
 * :45). */
const EPOCH_WITH_SECONDS = 1705329045

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldTime — compact mode', () => {
  it('renders only the datetime input when compact', () => {
    const w = mount(IdnodeFieldTime, {
      props: { prop: TIME_PROP, modelValue: 1700000000, compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    const input = w.find('input.ifld__input')
    expect(input.exists()).toBe(true)
    expect(input.attributes('type')).toBe('datetime-local')
  })

  it('keeps the "minutes" suffix in compact mode for duration props', () => {
    const w = mount(IdnodeFieldTime, {
      props: { prop: DURATION_PROP, modelValue: 600, compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__suffix').exists()).toBe(true)
    expect(w.find('.ifld__suffix').text()).toBe('minutes')
  })

  it('keeps the .ifld wrapper + label by default', () => {
    const w = mount(IdnodeFieldTime, {
      props: { prop: TIME_PROP, modelValue: 1700000000 },
    })
    expect(w.find('.ifld').exists()).toBe(true)
    expect(w.find('.ifld__label').exists()).toBe(true)
  })
})

describe('IdnodeFieldTime — dvr_show_seconds gate', () => {
  /* Reads `access.dvr_show_seconds` and applies it to datetime
   * fields only. Date-only and duration fields are unaffected
   * (no seconds to expose). Mirrors Classic
   * `static/app/idnode.js:789`. */
  it('omits step + seconds when the flag is off (the default)', () => {
    const w = mount(IdnodeFieldTime, {
      props: { prop: TIME_PROP, modelValue: EPOCH_WITH_SECONDS },
    })
    const input = w.find<HTMLInputElement>('input.ifld__input')
    expect(input.attributes('step')).toBeUndefined()
    /* datetime-local format YYYY-MM-DDTHH:mm (no :ss). */
    expect(input.element.value).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}$/)
  })

  it('adds step="1" + seconds in the value when the flag is on', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, dvr_show_seconds: 1 }
    const w = mount(IdnodeFieldTime, {
      props: { prop: TIME_PROP, modelValue: EPOCH_WITH_SECONDS },
    })
    const input = w.find<HTMLInputElement>('input.ifld__input')
    expect(input.attributes('step')).toBe('1')
    /* datetime-local format YYYY-MM-DDTHH:mm:ss with the
     * persisted :45 visible. */
    expect(input.element.value).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:45$/)
  })

  it('leaves duration fields untouched even when the flag is on', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, dvr_show_seconds: 1 }
    const w = mount(IdnodeFieldTime, {
      props: { prop: DURATION_PROP, modelValue: 600 },
    })
    const input = w.find('input.ifld__input')
    expect(input.attributes('step')).toBeUndefined()
    expect(input.attributes('type')).toBe('number')
  })

  it('leaves date-only fields untouched even when the flag is on', () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, dvr_show_seconds: 1 }
    const w = mount(IdnodeFieldTime, {
      props: { prop: DATE_PROP, modelValue: EPOCH_WITH_SECONDS },
    })
    const input = w.find('input.ifld__input')
    expect(input.attributes('step')).toBeUndefined()
    expect(input.attributes('type')).toBe('date')
  })
})
