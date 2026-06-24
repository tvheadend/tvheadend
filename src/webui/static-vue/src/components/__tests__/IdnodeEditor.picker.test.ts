// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor picker-mode unit tests. Picker mode renders a
 * single-select table above the form; switching rows emits `pick`
 * (behind a discard-confirm when the form is dirty). Single-edit
 * and multi-edit coverage live in their own files.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodeEditor from '../IdnodeEditor.vue'
import {
  CHECKBOX_STUB,
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'
import type { PickerColumn, PickerRow } from '@/types/picker'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Controllable discard-confirm — default accepts; the "keep
 * editing" test flips it to reject. */
const askMock = vi.fn()
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: askMock }),
}))

setupApiMockReset(apiMock)
beforeEach(() => {
  askMock.mockReset()
  askMock.mockResolvedValue(true)
})

const COLUMNS: PickerColumn[] = [{ field: 'disp_title', label: 'Title' }]
const ROWS: PickerRow[] = [
  { uuid: 'uuid-a', disp_title: 'Show A' },
  { uuid: 'uuid-b', disp_title: 'Show B' },
]
const SAMPLE_PARAMS = [{ id: 'comment', type: 'str', caption: 'Comment', value: 'initial' }]

function mountPicker(
  overrides: Partial<{
    uuid: string | null
    pickerRows: PickerRow[] | null
    pickerColumns: PickerColumn[] | null
  }> = {},
) {
  return mount(IdnodeEditor, {
    props: {
      uuid: 'uuid-a',
      level: 'expert' as const,
      pickerRows: ROWS,
      pickerColumns: COLUMNS,
      ...overrides,
    },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub({ withHeader: true }), Checkbox: CHECKBOX_STUB },
    },
  })
}

describe('IdnodeEditor — picker mode', () => {
  it('renders the picker table above the form when pickerRows has 2+', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    expect(wrapper.find('.entity-picker').exists()).toBe(true)
    expect(wrapper.findAll('.entity-picker__row')).toHaveLength(2)
    expect(wrapper.find('.idnode-editor__form').exists()).toBe(true)
  })

  it('does not render the picker table when pickerRows is null', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker({ pickerRows: null, pickerColumns: null })
    await flushPromises()
    expect(wrapper.find('.entity-picker').exists()).toBe(false)
  })

  it('does not render the picker table for a single-entry list', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker({ pickerRows: [ROWS[0]] })
    await flushPromises()
    expect(wrapper.find('.entity-picker').exists()).toBe(false)
  })

  it('marks the row matching uuid as selected', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    const rows = wrapper.findAll('.entity-picker__row')
    expect(rows[0].classes()).toContain('entity-picker__row--selected')
    expect(rows[1].classes()).not.toContain('entity-picker__row--selected')
  })

  it('emits pick on a clean-form row switch with no confirm', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    await wrapper.findAll('.entity-picker__row')[1].trigger('click')
    await flushPromises()
    expect(askMock).not.toHaveBeenCalled()
    expect(wrapper.emitted('pick')).toEqual([['uuid-b']])
  })

  it('does not emit pick when clicking the already-selected row', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    await wrapper.findAll('.entity-picker__row')[0].trigger('click')
    await flushPromises()
    expect(wrapper.emitted('pick')).toBeUndefined()
  })

  it('confirms before a dirty-form row switch; emits pick on accept', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    await wrapper.find('input[type="text"]').setValue('changed')
    await flushPromises()
    await wrapper.findAll('.entity-picker__row')[1].trigger('click')
    await flushPromises()
    expect(askMock).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('pick')).toEqual([['uuid-b']])
  })

  it('keeps editing (no pick) when the dirty-switch confirm is rejected', async () => {
    askMock.mockResolvedValue(false)
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'uuid-a', params: SAMPLE_PARAMS }] })
    const wrapper = mountPicker()
    await flushPromises()
    await wrapper.find('input[type="text"]').setValue('changed')
    await flushPromises()
    await wrapper.findAll('.entity-picker__row')[1].trigger('click')
    await flushPromises()
    expect(askMock).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('pick')).toBeUndefined()
  })
})
