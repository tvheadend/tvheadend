// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ServiceMapperDialog mount tests. Mocks api/client to avoid
 * touching the real API. Drives the dialog via the `visible`
 * prop (the parent grid view's binding).
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { nextTick } from 'vue'
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

/* The mapper form as `service/mapper/load` returns it: the server
 * never reads back the `services` list (`service_mapper_services_get`
 * returns NULL), so it always loads empty. */
function mapperLoad(services: string[] = []) {
  return {
    entries: [
      {
        params: [
          {
            id: 'services',
            type: 'str',
            caption: 'Services',
            value: services,
            list: true,
            enum: [],
          },
          { id: 'encrypted', type: 'bool', caption: 'Map encrypted', value: true },
        ],
      },
    ],
  }
}

/* Answer by endpoint so the order of the list and load fetches does
 * not matter. */
function routeApi(routes: Record<string, unknown>) {
  apiMock.mockImplementation((endpoint: string) =>
    endpoint in routes ? Promise.resolve(routes[endpoint]) : Promise.resolve({}),
  )
}

function mountDialog(
  propOverrides: Partial<{
    visible: boolean
    preselect: Record<string, unknown> | null
    mapAll: boolean
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

  it('keeps Map disabled while no service is picked', async () => {
    /* The server starts a mapping job only for a non-empty
     * services list (`service_mapper_conf_class_save`), so Map
     * with nothing picked would report a job that never runs. */
    apiMock.mockResolvedValueOnce(mapperLoad())

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeDefined()
  })

  it('enables Map once the user picks a service', async () => {
    apiMock.mockResolvedValueOnce(mapperLoad())

    const wrapper = mountDialog({ visible: true })
    await flushPromises()

    const form = wrapper.findComponent({ name: 'IdnodeConfigForm' })
    ;(form.vm as unknown as { currentValues: Record<string, unknown> }).currentValues.services = [
      'uuid-a',
    ]
    await nextTick()

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('emits started + closes (update:visible=false) after Map services succeeds', async () => {
    apiMock.mockResolvedValueOnce(mapperLoad())

    const wrapper = mountDialog({ visible: true, preselect: { services: ['uuid-a'] } })
    await flushPromises()

    /* Mock the save round-trip + the post-save reload load(). */
    apiMock.mockResolvedValueOnce({}) /* save */
    apiMock.mockResolvedValueOnce(mapperLoad()) /* reload */

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('started')).toBeTruthy()
    expect(wrapper.emitted('update:visible')).toBeTruthy()
    /* Last update:visible event should be `false` (close). */
    const closeCalls = wrapper.emitted('update:visible') as unknown[][]
    expect(closeCalls[closeCalls.length - 1][0]).toBe(false)
  })

  it('does not emit started when the save POST rejects', async () => {
    apiMock.mockResolvedValueOnce(mapperLoad())

    const wrapper = mountDialog({ visible: true, preselect: { services: ['uuid-a'] } })
    await flushPromises()

    apiMock.mockRejectedValueOnce(new Error('server error'))

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('started')).toBeFalsy()
  })

  describe('mapAll', () => {
    const SERVICE_LIST = {
      entries: [
        { key: 'svc-1', val: 'Net/Mux/One' },
        { key: 'svc-2', val: 'Net/Mux/Two' },
      ],
    }

    it('preselects every service from service/list', async () => {
      routeApi({ 'service/list': SERVICE_LIST, 'service/mapper/load': mapperLoad() })

      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()

      expect(apiMock).toHaveBeenCalledWith('service/list', { enum: 1 })
      const form = wrapper.findComponent({ name: 'IdnodeConfigForm' })
      const vals = (form.vm as unknown as { currentValues: Record<string, unknown> }).currentValues
      expect(vals.services).toEqual(['svc-1', 'svc-2'])
      expect(wrapper.find('.idnode-config-form__btn--save').attributes('disabled')).toBeUndefined()
    })

    it('submits every service when Map is pressed', async () => {
      routeApi({ 'service/list': SERVICE_LIST, 'service/mapper/load': mapperLoad() })

      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()
      await wrapper.find('.idnode-config-form__btn--save').trigger('click')
      await flushPromises()

      const save = apiMock.mock.calls.find(([endpoint]) => endpoint === 'service/mapper/save')
      expect(save).toBeDefined()
      const node = JSON.parse((save![1] as { node: string }).node)
      expect(node.services).toEqual(['svc-1', 'svc-2'])
      expect(wrapper.emitted('started')).toBeTruthy()
    })

    it('keeps Map disabled when there are no services at all', async () => {
      routeApi({ 'service/list': { entries: [] }, 'service/mapper/load': mapperLoad() })

      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()

      expect(wrapper.find('.idnode-config-form__btn--save').attributes('disabled')).toBeDefined()
    })

    it('still opens the form when the service list cannot be fetched', async () => {
      apiMock.mockImplementation((endpoint: string) =>
        endpoint === 'service/list'
          ? Promise.reject(new Error('boom'))
          : Promise.resolve(mapperLoad()),
      )

      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()

      expect(wrapper.find('.idnode-config-form').exists()).toBe(true)
      expect(wrapper.find('.service-mapper-dialog__error').text()).toContain('boom')
    })

    /* A service/list answer that arrives after the dialog was
     * closed belongs to an earlier opening and must not touch the
     * current one. */
    function deferredLists() {
      const pending: Array<(v: unknown) => void> = []
      apiMock.mockImplementation((endpoint: string) =>
        endpoint === 'service/list'
          ? new Promise((resolve) => pending.push(resolve))
          : Promise.resolve(mapperLoad()),
      )
      return pending
    }

    it('shows the form at once when reopened without mapAll during a late list', async () => {
      const pending = deferredLists()
      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()
      expect(wrapper.find('.service-mapper-dialog__loading').exists()).toBe(true)

      await wrapper.setProps({ visible: false })
      await wrapper.setProps({ visible: true, mapAll: false })
      await flushPromises()

      expect(wrapper.find('.service-mapper-dialog__loading').exists()).toBe(false)
      expect(wrapper.find('.idnode-config-form').exists()).toBe(true)

      pending[0](SERVICE_LIST)
      await flushPromises()
      const form = wrapper.findComponent({ name: 'IdnodeConfigForm' })
      const vals = (form.vm as unknown as { currentValues: Record<string, unknown> }).currentValues
      expect(vals.services).toEqual([])
    })

    it('waits for the latest list when reopened with mapAll', async () => {
      const pending = deferredLists()
      const wrapper = mountDialog({ visible: true, mapAll: true })
      await flushPromises()
      await wrapper.setProps({ visible: false })
      await wrapper.setProps({ visible: true })
      await flushPromises()
      expect(pending).toHaveLength(2)

      pending[0]({ entries: [{ key: 'svc-old', val: 'Old' }] })
      await flushPromises()
      expect(wrapper.find('.service-mapper-dialog__loading').exists()).toBe(true)
      expect(wrapper.find('.idnode-config-form').exists()).toBe(false)

      pending[1](SERVICE_LIST)
      await flushPromises()
      const form = wrapper.findComponent({ name: 'IdnodeConfigForm' })
      const vals = (form.vm as unknown as { currentValues: Record<string, unknown> }).currentValues
      expect(vals.services).toEqual(['svc-1', 'svc-2'])
    })
  })
})
