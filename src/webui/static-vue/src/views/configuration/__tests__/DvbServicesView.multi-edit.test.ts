// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DvbServicesView — editing several services at once, end to end.
 * The real view, grid, stores, router and editor drawer, with only
 * the API and the Comet client mocked.
 *
 * Two services are selected and edited together, and one of them
 * is deleted elsewhere while the drawer is open. The drawer keeps
 * the rows it was opened for and posts both uuids. The server's
 * idnode/save skips a uuid it no longer knows and succeeds when it
 * saved at least one entry (api_idnode_save), so the drawer closes
 * and the grid takes the saved row from the change notification.
 * When the save fails, the drawer stays open with the edit and the
 * error is shown.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h } from 'vue'
import { enableAutoUnmount, flushPromises, mount } from '@vue/test-utils'
import { createPinia } from 'pinia'
import { createMemoryHistory, createRouter, RouterView } from 'vue-router'
import PrimeVue from 'primevue/config'
import ConfirmationService from 'primevue/confirmationservice'
import ToastService from 'primevue/toastservice'
import Tooltip from 'primevue/tooltip'
import DvbServicesView from '../DvbServicesView.vue'
import ErrorDialog from '@/components/ErrorDialog.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import { ApiError } from '@/api/errors'
import { useAccessStore } from '@/stores/access'
import { fireComet, resetCometMock } from '@/test/__helpers__/cometClientMock'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))
vi.mock('@/api/comet', () => import('@/test/__helpers__/cometClientMock'))

const ROWS = [
  { uuid: 's1', svcname: 'One', enabled: true, priority: 0, channel: [] },
  { uuid: 's2', svcname: 'Two', enabled: true, priority: 0, channel: [] },
]
const PARAMS = [
  { id: 'svcname', type: 'str', caption: 'Service name', value: 'One', rdonly: true },
  { id: 'priority', type: 'int', caption: 'Priority', value: 0 },
]

let saveResult: Promise<unknown>

function api(endpoint: string, params: Record<string, unknown>): Promise<unknown> {
  if (endpoint === 'idnode/save') return saveResult
  if (endpoint === 'idnode/class') return Promise.resolve({ props: PARAMS })
  if (endpoint === 'mpegts/service/grid') return Promise.resolve({ entries: ROWS, total: 2 })
  if (endpoint === 'idnode/load' && params.grid) return Promise.resolve({ entries: [ROWS[0]] })
  if (endpoint === 'idnode/load') {
    const entry = { uuid: params.uuid, class: 'mpegts_service', event: 'service', params: PARAMS }
    return Promise.resolve({ entries: [entry] })
  }
  return Promise.resolve({ entries: [] })
}

enableAutoUnmount(afterEach)

beforeEach(() => {
  vi.useFakeTimers({ toFake: ['setTimeout', 'clearTimeout'] })
  resetCometMock()
  apiMock.mockReset()
  apiMock.mockImplementation(api)
  saveResult = Promise.resolve({})
})

afterEach(() => {
  vi.useRealTimers()
})

async function mountView() {
  const pinia = createPinia()
  const router = createRouter({
    history: createMemoryHistory(),
    routes: [{ path: '/configuration/dvb/services', component: DvbServicesView }],
  })
  await router.push('/configuration/dvb/services')
  const App = defineComponent({ render: () => [h(RouterView), h(ErrorDialog)] })
  const wrapper = mount(App, {
    attachTo: document.body,
    global: {
      plugins: [pinia, router, PrimeVue, ConfirmationService, ToastService],
      directives: { tooltip: Tooltip },
    },
  })
  useAccessStore(pinia).data = { admin: true, dvr: true, uilevel: 'expert' }
  await flushPromises()
  return wrapper
}

type Wrapper = Awaited<ReturnType<typeof mountView>>

/* Select both services, open Edit, set Priority to 5, then let the
 * server delete s2 while the drawer is open. */
async function editBothThenLoseOne(wrapper: Wrapper) {
  const grid = wrapper.findComponent(IdnodeGrid)
  for (const row of grid.vm.store.entries) grid.vm.toggleSelect(row)
  await flushPromises()
  const edit = wrapper.findAll('.action-menu__row button').find((b) => b.text() === 'Edit')
  await edit!.trigger('click')
  await flushPromises()

  const priority = document.querySelector<HTMLInputElement>('.idnode-editor #priority')!
  priority.value = '5'
  priority.dispatchEvent(new Event('input'))

  fireComet('service', { delete: ['s2'] })
  await vi.advanceTimersByTimeAsync(500)
  await flushPromises()
  expect(grid.vm.store.entries.map((r: { uuid?: string }) => r.uuid)).toEqual(['s1'])
  return grid
}

async function clickSave() {
  document.querySelector<HTMLButtonElement>('.idnode-editor__btn--save')!.click()
  await flushPromises()
}

function saveCalls() {
  return apiMock.mock.calls.filter(([endpoint]) => endpoint === 'idnode/save')
}

/* Mounting the real grid, drawer and router can take longer than
 * Vitest's default 5 s per test on a busy machine. */
describe('DvbServicesView — editing several services', { timeout: 30_000 }, () => {
  it('posts both uuids, closes the drawer and takes the saved row from Comet', async () => {
    const wrapper = await mountView()
    await editBothThenLoseOne(wrapper)
    await clickSave()

    expect(saveCalls()).toHaveLength(1)
    expect(JSON.parse(saveCalls()[0][1].node)).toEqual({ uuid: ['s1', 's2'], priority: 5 })
    expect(wrapper.findComponent(IdnodeEditor).props('uuids')).toBeNull()
    expect(document.querySelector('.idnode-editor')).toBeNull()

    apiMock.mockClear()
    fireComet('service', { change: ['s1'] })
    await vi.advanceTimersByTimeAsync(500)
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('idnode/load', { uuid: ['s1'], grid: 1 })
  })

  it('keeps the drawer and the edit open and shows the error when the save fails', async () => {
    saveResult = Promise.reject(new ApiError(400, 'Bad Request'))
    saveResult.catch(() => {})
    const wrapper = await mountView()
    await editBothThenLoseOne(wrapper)
    await clickSave()

    expect(saveCalls()).toHaveLength(1)
    expect(wrapper.findComponent(IdnodeEditor).props('uuids')).toEqual(['s1', 's2'])
    expect(document.querySelector<HTMLInputElement>('.idnode-editor #priority')!.value).toBe('5')
    const dialog = document.querySelector('.error-dialog')
    expect(dialog?.textContent).toContain('Bulk save failed')
    expect(dialog?.textContent).toContain('The server rejected the change')
  })
})
