// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor — fieldGroups hook tests.
 *
 * Asserts that when a `fieldGroups` entry is passed:
 *   - The combined component renders ONCE at the first key's
 *     section position.
 *   - The non-first keys of the group are suppressed (no row, no
 *     per-field renderer, no error slot).
 *   - The combined component receives both group props + values
 *     keyed by field id.
 *   - Its `update(id, value)` emit threads through the same
 *     `onFieldChange` path so currentValues mutates per-field.
 *
 * Negative control: `fieldGroups = []` (default) leaves the
 * per-field render path entirely intact.
 */
import { defineComponent, h, type Component } from 'vue'
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodeEditor from '../IdnodeEditor.vue'
import {
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))

setupApiMockReset(apiMock)

/* Passthrough capture stub for the combined renderer — exposes the
 * received props on data-* attributes so the tests can assert what
 * IdnodeEditor passed in, and emits `update(id, value)` via a
 * synthetic click. */
const CAPTURE_STUB: Component = defineComponent({
  name: 'CaptureStub',
  props: {
    groupProps: { type: Object, default: () => ({}) },
    groupValues: { type: Object, default: () => ({}) },
    disabled: { type: Boolean, default: false },
  },
  emits: ['update'],
  setup(props, { emit }) {
    return () =>
      h('div', {
        class: 'capture-stub',
        'data-keys': Object.keys(props.groupProps).sort((a, b) => a.localeCompare(b)).join(','),
        'data-values': JSON.stringify(props.groupValues),
        'data-disabled': String(props.disabled),
        onClick(ev: MouseEvent) {
          /* Tests post a `data-emit` JSON tuple on the firing
           * element to control what gets emitted. */
          const target = ev.currentTarget as HTMLElement
          const raw = target.dataset.emit
          if (raw) {
            const [id, value] = JSON.parse(raw) as [string, unknown]
            emit('update', id, value)
          }
        },
      })
  },
})

function mountEditorWithGroups(
  params: Array<Record<string, unknown>>,
  fieldGroups: Array<{ keys: readonly string[]; component: Component }>,
) {
  apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params }] })
  return mount(IdnodeEditor, {
    props: { uuid: 'x', level: 'expert', fieldGroups },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

describe('IdnodeEditor — fieldGroups', () => {
  it('renders the combined component ONCE at the first key and suppresses non-first keys', async () => {
    const wrapper = mountEditorWithGroups(
      [
        { id: 'start', type: 'str', caption: 'Start after', value: '20:00' },
        { id: 'start_window', type: 'str', caption: 'Start before', value: '22:00' },
        { id: 'comment', type: 'str', caption: 'Comment', value: '' },
      ],
      [{ keys: ['start', 'start_window'], component: CAPTURE_STUB }],
    )
    await flushPromises()

    /* Exactly one combined render. */
    const captures = wrapper.findAll('.capture-stub')
    expect(captures).toHaveLength(1)

    /* The combined render carries both keys + both values. */
    expect(captures[0].attributes('data-keys')).toBe('start,start_window')
    const vals = JSON.parse(captures[0].attributes('data-values') ?? '')
    expect(vals).toEqual({ start: '20:00', start_window: '22:00' })

    /* No per-field row was rendered for start_window — confirmed by
     * the absence of its caption in any non-capture row. The Comment
     * field still renders normally (negative control). */
    const html = wrapper.html()
    expect(html).toContain('Comment')
    /* Both captions belong to the suppressed pair; neither should
     * appear as a standalone row — they're routed into the group. */
    expect(html).not.toContain('Start after')
    expect(html).not.toContain('Start before')
  })

  it('threads the combined component\'s `update(id, value)` emit through onFieldChange', async () => {
    const wrapper = mountEditorWithGroups(
      [
        { id: 'start', type: 'str', value: '20:00' },
        { id: 'start_window', type: 'str', value: '22:00' },
      ],
      [{ keys: ['start', 'start_window'], component: CAPTURE_STUB }],
    )
    await flushPromises()

    /* Fire the stub's synthetic emit for start → '21:00'. */
    const capture = wrapper.find('.capture-stub')
    ;(capture.element as HTMLElement).dataset.emit = JSON.stringify(['start', '21:00'])
    await capture.trigger('click')

    /* The combined renderer re-renders with the new value reflected
     * in groupValues — confirms currentValues mutated. */
    const updated = wrapper.find('.capture-stub')
    const vals = JSON.parse(updated.attributes('data-values') ?? '')
    expect(vals.start).toBe('21:00')
    expect(vals.start_window).toBe('22:00')
  })

  it('fieldGroups = [] (default) leaves per-field renders unchanged', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [
            { id: 'start', type: 'str', caption: 'Start after', value: '20:00' },
            { id: 'start_window', type: 'str', caption: 'Start before', value: '22:00' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeEditor, {
      props: { uuid: 'x', level: 'expert' },
      global: {
        directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
        stubs: { Drawer: makeDrawerStub() },
      },
    })
    await flushPromises()

    /* No capture stub rendered — picker isn't passed in. */
    expect(wrapper.find('.capture-stub').exists()).toBe(false)
    /* Both captions appear (per-field path). */
    const html = wrapper.html()
    expect(html).toContain('Start after')
    expect(html).toContain('Start before')
  })
})
