// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ServiceMapperDialog mount tests. Mocks api/client to avoid
 * touching the real API. Drives the dialog via the `visible`
 * prop (the parent grid view's binding).
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import ServiceMapperDialog from '../ServiceMapperDialog.vue'
import {
  DIALOG_PASSTHROUGH_STUB,
  TOOLTIP_DIRECTIVE_STUB,
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

function mountDialog(
  propOverrides: Partial<{
    visible: boolean
    preselect: Record<string, unknown> | null
  }> = {},
) {
  return mount(ServiceMapperDialog, {
    props: {
      visible: false,
      ...propOverrides,
    },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Dialog: DIALOG_PASSTHROUGH_STUB },
    },
  })
}

describe('ServiceMapperDialog', () => {
  it('does not render the form when visible=false', () => {
    const wrapper = mountDialog({ visible: false })
    /* The Dialog stub renders nothing when visible=false; the
     * IdnodeConfigForm shouldn't be in the DOM. */
    expect(wrapper.find('.idnode-config-form').exists()).toBe(false)
  })

  it('renders IdnodeConfigForm against service/mapper/load when visible=true', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [
            { id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true },
          ],
        },
      ],
    })

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    expect(apiMock).toHaveBeenCalledWith('service/mapper/load', { meta: 1 })
    expect(wrapper.find('.idnode-config-form').exists()).toBe(true)
  })

  it('forwards preselect to IdnodeConfigForm', async () => {
    /* Preselect overrides the loaded value → form is dirty →
     * Save (Map services) button is enabled even with default
     * options. Same dirty-check we already cover in the form's
     * own tests; here we're just pinning that the prop wires
     * through. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [
            {
              id: 'services',
              type: 'str',
              caption: 'Services',
              value: [],
              list: true,
              enum: [],
            },
          ],
        },
      ],
    })

    const wrapper = mountDialog({
      visible: true,
      preselect: { services: ['uuid-a', 'uuid-b'] },
    })
    await flushPromises()

    /* The form rendered, and the save button should be enabled
     * (alwaysDirty=true is set unconditionally in the dialog). */
    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.exists()).toBe(true)
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('Save button stays enabled even without preselect (alwaysDirty)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [
            { id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true },
          ],
        },
      ],
    })

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    /* No preselect, no manual edits — Save would normally be
     * disabled. alwaysDirty=true (set inside the dialog
     * unconditionally) keeps it enabled so users can re-run
     * the last-saved config without touching anything. */
    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('emits started + closes (update:visible=false) after Map services succeeds', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [{ id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true }],
        },
      ],
    })

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    /* Mock the save round-trip + the post-save reload load(). */
    apiMock.mockResolvedValueOnce({}) /* save */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [{ id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true }],
        },
      ],
    }) /* reload */

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('started')).toBeTruthy()
    expect(wrapper.emitted('update:visible')).toBeTruthy()
    /* Last update:visible event should be `false` (close). */
    const closeCalls = wrapper.emitted('update:visible') as unknown[][]
    expect(closeCalls[closeCalls.length - 1][0]).toBe(false)
  })

  it('does not emit started when the save POST rejects', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [{ id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true }],
        },
      ],
    })

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    apiMock.mockRejectedValueOnce(new Error('server error'))

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('started')).toBeFalsy()
  })
})
