// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor `preSave` hook — the optional gate on single-entry
 * edit saves (first consumer: the autorec view's voluntary match-all
 * warning).
 *
 * Covered:
 *   - the hook receives the dirty diff (uuid excluded) and the uuid
 *   - resolving true proceeds to idnode/save
 *   - resolving false aborts the save silently (no POST, no close,
 *     form stays open and dirty)
 *   - create mode does not consult the hook
 */
import { describe, expect, it, vi } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import IdnodeEditor from '../IdnodeEditor.vue'
import {
  makeDrawerStub,
  TOOLTIP_DIRECTIVE_STUB,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...a: unknown[]) => apiMock(...a),
}))

vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))

setupApiMockReset(apiMock)

const PARAMS = [
  { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
  { id: 'pri', type: 'int', caption: 'Priority', value: 1 },
]

function mountWithPreSave(
  preSave: (dirty: Record<string, unknown>, uuid: string) => Promise<boolean>,
  props: { uuid: string | null; createBase?: string | null } = { uuid: 'x' },
) {
  return mount(IdnodeEditor, {
    props: { level: 'expert' as const, preSave, ...props },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

async function editAndSave(wrapper: ReturnType<typeof mountWithPreSave>) {
  const vm = wrapper.vm as unknown as {
    currentValues: Record<string, unknown>
  }
  vm.currentValues.comment = 'updated'
  await flushPromises()
  await wrapper.find('.idnode-editor__btn--save').trigger('click')
  await flushPromises()
}

describe('IdnodeEditor preSave', () => {
  it('receives the dirty diff and proceeds on true', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params: PARAMS }] })
    apiMock.mockResolvedValueOnce({}) /* save response */
    const preSave = vi.fn(async () => true)
    const wrapper = mountWithPreSave(preSave)
    await flushPromises()
    await editAndSave(wrapper)

    expect(preSave).toHaveBeenCalledTimes(1)
    const [dirty, uuid] = preSave.mock.calls[0] as unknown as [
      Record<string, unknown>, string]
    expect(dirty).toEqual({ comment: 'updated' })
    expect(uuid).toBe('x')

    const saveCall = apiMock.mock.calls[1]!
    expect(saveCall[0]).toBe('idnode/save')
    expect(wrapper.emitted('saved')).toBeTruthy()
  })

  it('aborts silently on false: no POST, drawer stays open', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params: PARAMS }] })
    const preSave = vi.fn(async () => false)
    const wrapper = mountWithPreSave(preSave)
    await flushPromises()
    await editAndSave(wrapper)

    expect(preSave).toHaveBeenCalledTimes(1)
    /* only the load call went out */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('saved')).toBeFalsy()
    expect(wrapper.emitted('close')).toBeFalsy()
  })

  it('is not consulted in create mode', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrautorec', props: PARAMS })
    apiMock.mockResolvedValueOnce({ uuid: 'new-uuid' })
    const preSave = vi.fn(async () => false)
    const wrapper = mountWithPreSave(preSave, {
      uuid: null,
      createBase: 'dvr/autorec',
    })
    await flushPromises()
    await editAndSave(wrapper)

    expect(preSave).not.toHaveBeenCalled()
    const createCall = apiMock.mock.calls[1]!
    expect(createCall[0]).toBe('dvr/autorec/create')
  })
})
