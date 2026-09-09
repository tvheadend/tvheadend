// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor create-path gates:
 *
 * `preCreate` — the optional gate on create saves, sibling of
 * `preSave` (first consumer: the smart autorec view's born-smart
 * guarantee, refusing a rule without an expression).
 *   - the hook receives the filled-in conf (empty fields omitted)
 *   - resolving true proceeds to the create POST
 *   - resolving false aborts silently (no POST, drawer stays open)
 *   - edit mode does not consult the hook
 *
 * Confirm-on-create counts — when the server declines with
 * `force_required` and carries `matched`/`scanned` (the match-all
 * safeguard), the confirm dialog formats the same counted message
 * the edit-path preSave warning uses; without the counts it falls
 * back to the server's error string. Accepting retries with force.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
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

const { askMock } = vi.hoisted(() => ({
  askMock: vi.fn(async () => true),
}))
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: askMock }),
}))

setupApiMockReset(apiMock)
beforeEach(() => {
  askMock.mockClear()
  askMock.mockImplementation(async () => true)
})

const PARAMS = [
  { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
  { id: 'pri', type: 'int', caption: 'Priority', value: 1 },
]

function mountEditor(props: {
  uuid: string | null
  createBase?: string | null
  preCreate?: (conf: Record<string, unknown>) => Promise<boolean>
}) {
  return mount(IdnodeEditor, {
    props: { level: 'expert' as const, ...props },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

async function editAndSave(wrapper: ReturnType<typeof mountEditor>) {
  const vm = wrapper.vm as unknown as {
    currentValues: Record<string, unknown>
  }
  vm.currentValues.comment = 'updated'
  await flushPromises()
  await wrapper.find('.idnode-editor__btn--save').trigger('click')
  await flushPromises()
}

describe('IdnodeEditor preCreate', () => {
  it('receives the conf and proceeds on true', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrautorec', props: PARAMS })
    apiMock.mockResolvedValueOnce({ uuid: 'new-uuid' })
    const preCreate = vi.fn(async () => true)
    const wrapper = mountEditor({
      uuid: null,
      createBase: 'dvr/autorec',
      preCreate,
    })
    await flushPromises()
    await editAndSave(wrapper)

    expect(preCreate).toHaveBeenCalledTimes(1)
    const [conf] = preCreate.mock.calls[0] as unknown as [
      Record<string, unknown>]
    expect(conf).toMatchObject({ comment: 'updated' })

    const createCall = apiMock.mock.calls[1]!
    expect(createCall[0]).toBe('dvr/autorec/create')
  })

  it('aborts silently on false: no POST, drawer stays open', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrautorec', props: PARAMS })
    const preCreate = vi.fn(async () => false)
    const wrapper = mountEditor({
      uuid: null,
      createBase: 'dvr/autorec',
      preCreate,
    })
    await flushPromises()
    await editAndSave(wrapper)

    expect(preCreate).toHaveBeenCalledTimes(1)
    /* only the metadata load went out */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('created')).toBeFalsy()
    expect(wrapper.emitted('close')).toBeFalsy()
  })

  it('is not consulted in edit mode', async () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params: PARAMS }] })
    apiMock.mockResolvedValueOnce({}) /* save response */
    const preCreate = vi.fn(async () => false)
    const wrapper = mountEditor({ uuid: 'x', preCreate })
    await flushPromises()
    await editAndSave(wrapper)

    expect(preCreate).not.toHaveBeenCalled()
    const saveCall = apiMock.mock.calls[1]!
    expect(saveCall[0]).toBe('idnode/save')
  })
})

describe('IdnodeEditor confirm-on-create counts', () => {
  it('formats matched/scanned into the confirm and retries with force', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrautorec', props: PARAMS })
    apiMock.mockResolvedValueOnce({
      force_required: 1,
      error: 'expression matches most of the EPG; resubmit with force to save anyway',
      matched: 9000,
      scanned: 10000,
    })
    apiMock.mockResolvedValueOnce({ uuid: 'new-uuid' })
    const wrapper = mountEditor({ uuid: null, createBase: 'dvr/autorec' })
    await flushPromises()
    await editAndSave(wrapper)

    expect(askMock).toHaveBeenCalledTimes(1)
    const [message] = askMock.mock.calls[0] as unknown as [string]
    expect(message).toContain('9000 of 10000')
    expect(message).not.toContain('{0}')

    /* accepted → the retry carries force */
    const retryCall = apiMock.mock.calls[2]!
    expect(retryCall[0]).toBe('dvr/autorec/create')
    expect(retryCall[1]).toMatchObject({ force: 1 })
  })

  it('falls back to the server error string without counts', async () => {
    apiMock.mockResolvedValueOnce({ class: 'dvrautorec', props: PARAMS })
    apiMock.mockResolvedValueOnce({
      force_required: 1,
      error: 'declined for reasons of its own',
    })
    askMock.mockImplementationOnce(async () => false)
    const wrapper = mountEditor({ uuid: null, createBase: 'dvr/autorec' })
    await flushPromises()
    await editAndSave(wrapper)

    expect(askMock).toHaveBeenCalledTimes(1)
    const [message] = askMock.mock.calls[0] as unknown as [string]
    expect(message).toBe('declined for reasons of its own')
    /* declined → no retry */
    expect(apiMock).toHaveBeenCalledTimes(2)
    expect(wrapper.emitted('created')).toBeFalsy()
  })
})
