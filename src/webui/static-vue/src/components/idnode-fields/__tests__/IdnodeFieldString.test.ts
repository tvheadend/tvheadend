// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldString — compact-mode rendering.
 *
 * The default (non-compact) mode is exercised indirectly by the
 * IdnodeEditor / IdnodeConfigForm tests; this file pins that the
 * compact-mode path emits ONLY the bare control (no `.ifld`
 * wrapper, no label) so an inline-edit cell can mount the
 * component without surrounding form chrome.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldString from '../IdnodeFieldString.vue'
import type { IdnodeProp } from '@/types/idnode'

const STR_PROP: IdnodeProp = {
  id: 'comment',
  type: 'str',
  caption: 'Comment',
}

const STR_MULTI_PROP: IdnodeProp = {
  id: 'desc',
  type: 'str',
  caption: 'Description',
  multiline: true,
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldString — compact mode', () => {
  it('renders a textarea (no .ifld wrapper or label) when compact', () => {
    /* Compact-mode plain str now uses a single <textarea> styled
     * like a single-line input by default (white-space:nowrap +
     * fixed height). Clicking the expand button toggles the same
     * textarea to a multi-line state — no element swap. */
    const w = mount(IdnodeFieldString, {
      props: { prop: STR_PROP, modelValue: 'hi', compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    expect(w.find('textarea.ifld__input').exists()).toBe(true)
    expect(
      (w.find('textarea.ifld__input').element as HTMLTextAreaElement).value,
    ).toBe('hi')
  })

  it('renders a textarea (no wrapper / label) for multiline strings in compact mode', () => {
    const w = mount(IdnodeFieldString, {
      props: { prop: STR_MULTI_PROP, modelValue: 'multi', compact: true },
    })
    expect(w.find('.ifld').exists()).toBe(false)
    expect(w.find('.ifld__label').exists()).toBe(false)
    expect(w.find('textarea.ifld__input').exists()).toBe(true)
  })

  it('keeps the .ifld wrapper + label by default (non-compact)', () => {
    const w = mount(IdnodeFieldString, {
      props: { prop: STR_PROP, modelValue: 'hi' },
    })
    expect(w.find('.ifld').exists()).toBe(true)
    expect(w.find('.ifld__label').exists()).toBe(true)
    expect(w.find('input.ifld__input').exists()).toBe(true)
  })
})
