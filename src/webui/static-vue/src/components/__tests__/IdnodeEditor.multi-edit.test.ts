// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor multi-edit mode unit tests. Mocks api/client to
 * control load/save and exercises the apply-checkbox-per-field
 * UI that's gated by the new `uuids` prop. Single-edit
 * regression coverage stays in IdnodeEditor.test.ts.
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodeEditor from '../IdnodeEditor.vue'
import {
  CHECKBOX_STUB,
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Same confirm-dialog stub as IdnodeEditor.test.ts — we don't
 * test the discard-confirmation path here. */
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))

setupApiMockReset(apiMock)

function mountEditor(
  propOverrides: Partial<{
    uuid: string | null
    uuids: string[] | null
    createBase: string | null
    level: 'basic' | 'advanced' | 'expert'
    title: string
    list: string
  }> = {},
) {
  return mount(IdnodeEditor, {
    props: {
      uuid: null,
      level: 'expert',
      ...propOverrides,
    },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub({ withHeader: true }), Checkbox: CHECKBOX_STUB },
    },
  })
}

/* Mount with multi-edit props + a queued idnode/load response.
 * The editor loads from the FIRST uuid only as the template;
 * subsequent uuids share the same field set on save. */
async function mountMultiEdit(
  uuids: string[],
  params: Array<Record<string, unknown>>,
) {
  apiMock.mockResolvedValueOnce({ entries: [{ uuid: uuids[0], params }] })
  const wrapper = mountEditor({ uuids })
  await flushPromises()
  return wrapper
}

const SAMPLE_PARAMS = [
  { id: 'enabled', type: 'bool', caption: 'Enabled', value: true },
  { id: 'comment', type: 'str', caption: 'Comment', value: 'initial' },
  { id: 'pri', type: 'u32', caption: 'Priority', value: 5 },
  { id: 'computed', type: 'str', caption: 'Computed', value: 'x', rdonly: true },
]

describe('IdnodeEditor — multi-edit', () => {
  it('loads idnode/load using the FIRST uuid only (template fetch)', async () => {
    await mountMultiEdit(['uuid-a', 'uuid-b', 'uuid-c'], SAMPLE_PARAMS)

    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock).toHaveBeenCalledWith('idnode/load', {
      uuid: 'uuid-a',
      meta: 1,
    })
  })

  it('appends "(N entries)" to the title in multi-edit mode', async () => {
    const wrapper = await mountMultiEdit(['a', 'b', 'c'], SAMPLE_PARAMS)
    expect(wrapper.html()).toContain('(3 entries)')
  })

  it('respects a caller-supplied title prefix', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'a', params: SAMPLE_PARAMS }] })
    const wrapper = mountEditor({
      uuids: ['a', 'b'],
      title: 'Edit Recording',
    })
    await flushPromises()

    expect(wrapper.html()).toContain('Edit Recording (2 entries)')
  })

  it('renders the multi-edit subtitle banner', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    expect(wrapper.find('.idnode-editor__multi-banner').exists()).toBe(true)
    expect(wrapper.html()).toContain('Editing 2 entries')
  })

  it('renders an apply-checkbox in the left column for editable fields', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    /* Three editable fields → three apply-checkboxes. The
     * read-only `computed` field gets a spacer instead. */
    const checks = wrapper.findAll('.ifld-row__apply-check')
    expect(checks.length).toBe(3)

    const spacers = wrapper.findAll('.ifld-row__apply-spacer')
    expect(spacers.length).toBe(1)
  })

  it('marks the row as multi (CSS hook) so the grid layout kicks in', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    const rows = wrapper.findAll('.ifld-row--multi')
    expect(rows.length).toBeGreaterThan(0)
  })

  it('Save button is disabled when no apply-checkboxes are ticked', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    const saveBtn = wrapper.find('.idnode-editor__btn--save')
    expect(saveBtn.attributes('disabled')).toBeDefined()
  })

  it('Save button enables when at least one checkbox is ticked', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    const checks = wrapper.findAll('.ifld-row__apply-check')
    /* Tick the first apply-checkbox (enabled field). */
    await checks[0].find('input').setValue(true)
    await flushPromises()

    const saveBtn = wrapper.find('.idnode-editor__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('auto-checks the apply-checkbox when the user edits a field', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)
    /* Find the comment text input and change its value. The
     * row's apply-checkbox should auto-flip to checked. */
    const inputs = wrapper.findAll('input[type="text"]')
    /* First text-typed input is `comment` (the only str field
     * in the fixture). */
    expect(inputs.length).toBeGreaterThan(0)
    await inputs[0].setValue('new comment')
    await flushPromises()

    /* The apply-checkbox in the same row should now be checked.
     * Find the comment row by its caption + read its checkbox. */
    const checks = wrapper.findAll('.ifld-row__apply-check')
    /* In the fixture order — enabled, comment, pri, computed —
     * the second checkbox covers comment (since rdonly computed
     * has a spacer instead). */
    const commentCheck = checks[1].find('input').element as HTMLInputElement
    expect(commentCheck.checked).toBe(true)
  })

  it('Save POSTs idnode/save with Case-2 payload (uuid array + ticked fields)', async () => {
    const wrapper = await mountMultiEdit(
      ['uuid-a', 'uuid-b', 'uuid-c'],
      SAMPLE_PARAMS,
    )
    /* Tick the comment checkbox + change comment to 'updated'. */
    const checks = wrapper.findAll('.ifld-row__apply-check')
    /* In fixture order: enabled (idx 0), comment (idx 1), pri (idx 2). */
    const inputs = wrapper.findAll('input[type="text"]')
    await inputs[0].setValue('updated')
    await flushPromises()

    /* Confirm comment's apply-checkbox auto-checked. */
    expect((checks[1].find('input').element as HTMLInputElement).checked).toBe(true)

    /* Mock the save response. */
    apiMock.mockResolvedValueOnce({})

    const saveBtn = wrapper.find('.idnode-editor__btn--save')
    await saveBtn.trigger('click')
    await flushPromises()

    /* apiMock calls: [0] = load, [1] = save. */
    expect(apiMock.mock.calls.length).toBeGreaterThanOrEqual(2)
    const [endpoint, body] = apiMock.mock.calls[1]
    expect(endpoint).toBe('idnode/save')
    const node = JSON.parse((body as { node: string }).node)
    expect(node.uuid).toEqual(['uuid-a', 'uuid-b', 'uuid-c'])
    expect(node.comment).toBe('updated')
    /* Untouched fields shouldn't appear in the payload. */
    expect('enabled' in node).toBe(false)
    expect('pri' in node).toBe(false)
  })

  it('does NOT render multi-edit chrome when uuids has fewer than 2 entries', async () => {
    /* Single-uuid arrays shouldn't reach the editor in practice
     * (useEditorMode normalises to `uuid`), but if one does, we
     * fall back to single-edit shape — no checkboxes, no banner. */
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'only', params: SAMPLE_PARAMS }] })
    const wrapper = mountEditor({ uuid: 'only', uuids: ['only'] })
    await flushPromises()

    expect(wrapper.find('.idnode-editor__multi-banner').exists()).toBe(false)
    expect(wrapper.findAll('.ifld-row__apply-check').length).toBe(0)
    expect(wrapper.findAll('.ifld-row--multi').length).toBe(0)
  })

  /* Regression: with a limited editList that omits some of the
   * class's CLASS_RULES.required fields (e.g. DVR Finished's
   * `disp_title,…,comment` editList against dvrentry's `required:
   * ['start', 'stop', 'channel']`), applyClassRules must not flag
   * the absent fields as empty-but-required — Save would lock
   * permanently because the user can't see or edit those fields
   * in this view. The editor scopes class-rule errors to loaded
   * fieldProps only. */
  it('limited editList: applyClassRules ignores required fields not loaded into the editor', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'fin-a',
          class: 'dvrentry',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: '' },
            { id: 'disp_title', type: 'str', caption: 'Title', value: 'Show A' },
          ],
        },
      ],
    })
    const wrapper = mountEditor({ uuids: ['fin-a', 'fin-b'] })
    await flushPromises()

    /* Type into comment. */
    const inputs = wrapper.findAll('input[type="text"]')
    expect(inputs.length).toBeGreaterThan(0)
    await inputs[0].setValue('new comment')
    await flushPromises()

    /* Save enables: comment is dirty + auto-ticked + hasErrors is
     * false (start/stop/channel absent from fieldProps so their
     * required-but-empty status is correctly ignored). */
    const saveBtn = wrapper.find('.idnode-editor__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('explicitly unticking an auto-checked checkbox excludes that field from the save payload', async () => {
    const wrapper = await mountMultiEdit(['a', 'b'], SAMPLE_PARAMS)

    /* Edit the comment field — auto-check fires. */
    const inputs = wrapper.findAll('input[type="text"]')
    await inputs[0].setValue('updated')
    await flushPromises()

    /* User now explicitly unticks the comment apply-checkbox. */
    const checks = wrapper.findAll('.ifld-row__apply-check')
    await checks[1].find('input').setValue(false)
    await flushPromises()

    /* Tick a different field to keep Save enabled (we're testing
     * that the unticked field is excluded, not that Save is
     * gated). */
    await checks[0].find('input').setValue(true)
    await flushPromises()

    apiMock.mockResolvedValueOnce({})
    const saveBtn = wrapper.find('.idnode-editor__btn--save')
    await saveBtn.trigger('click')
    await flushPromises()

    const [, body] = apiMock.mock.calls[1]
    const node = JSON.parse((body as { node: string }).node)
    /* Comment was unticked → not in the payload despite being
     * value-changed. */
    expect('comment' in node).toBe(false)
    /* Enabled was ticked → in the payload (with its current
     * value). */
    expect('enabled' in node).toBe(true)
  })
})
