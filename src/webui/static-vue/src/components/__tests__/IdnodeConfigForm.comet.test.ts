// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeConfigForm — live refresh from Comet.
 *
 * The server notifies a singleton config class (config, imagecache,
 * satip_server, tvhlog_conf, …) with `{ reload: 1 }` under the
 * class's `event`, and a multi-instance entry with
 * `{ change: [uuid] }`. The form refetches quietly, takes the new
 * values on fields the user has not touched and keeps the user's
 * edits, so the next Save no longer writes stale values back.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import IdnodeConfigForm from '../IdnodeConfigForm.vue'
import { useAccessStore } from '@/stores/access'
import { setupApiMockReset } from './__helpers__/idnodeEditorTestUtils'
import {
  cometListenerCount,
  fireComet as fire,
  resetCometMock,
} from '@/test/__helpers__/cometClientMock'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

vi.mock('vue-router', () => ({
  useRoute: () => ({ hash: '' }),
}))

vi.mock('@/api/comet', () => import('@/test/__helpers__/cometClientMock'))

setupApiMockReset(apiMock)

beforeEach(() => {
  resetCometMock()
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

function params(name: string, comment: string) {
  return [
    { id: 'name', type: 'str', caption: 'Name', value: name },
    { id: 'comment', type: 'str', caption: 'Comment', value: comment },
  ]
}

async function mountSingleton() {
  const access = useAccessStore()
  access.data = { admin: true, dvr: true, uilevel: 'basic' }
  apiMock.mockResolvedValueOnce({
    entries: [{ class: 'config', event: 'config', params: params('a', 'x') }],
  })
  const wrapper = mount(IdnodeConfigForm, {
    props: { loadEndpoint: 'config/load', saveEndpoint: 'config/save' },
  })
  await flushPromises()
  return wrapper
}

function inputs(wrapper: Awaited<ReturnType<typeof mountSingleton>>) {
  const [name, comment] = wrapper.findAll('input[type="text"]')
  return { name: name!, comment: comment! }
}

async function reloadWith(name: string, comment: string) {
  apiMock.mockResolvedValueOnce({
    entries: [{ class: 'config', event: 'config', params: params(name, comment) }],
  })
  fire('config', { reload: 1 })
  await vi.advanceTimersByTimeAsync(300)
  await flushPromises()
}

describe('IdnodeConfigForm — Comet refresh (singleton)', () => {
  it('subscribes to the class event and takes new values on untouched fields', async () => {
    const wrapper = await mountSingleton()
    expect(cometListenerCount('config')).toBe(1)

    await reloadWith('b', 'y')

    expect(apiMock).toHaveBeenLastCalledWith('config/load', {})
    const { name, comment } = inputs(wrapper)
    expect((name.element as HTMLInputElement).value).toBe('b')
    expect((comment.element as HTMLInputElement).value).toBe('y')
    /* Still clean: nothing to save or undo. */
    expect(wrapper.find('.idnode-config-form__btn--save').attributes('disabled')).toBeDefined()
    expect(wrapper.find('.idnode-config-form__changed-elsewhere').exists()).toBe(false)
  })

  it('refreshes without swapping the form for the loading state', async () => {
    const wrapper = await mountSingleton()
    let resolve: (v: unknown) => void = () => {}
    apiMock.mockReturnValueOnce(new Promise((r) => (resolve = r)))
    fire('config', { reload: 1 })
    await vi.advanceTimersByTimeAsync(300)

    expect(wrapper.text()).not.toContain('Loading configuration')
    expect(wrapper.find('input[type="text"]').exists()).toBe(true)

    resolve({ entries: [{ params: params('b', 'x') }] })
    await flushPromises()
    expect((inputs(wrapper).name.element as HTMLInputElement).value).toBe('b')
  })

  it('keeps the user edit, names the field changed elsewhere, and Undo gives the new value', async () => {
    const wrapper = await mountSingleton()
    await inputs(wrapper).name.setValue('mine')

    await reloadWith('theirs', 'y')

    const { name, comment } = inputs(wrapper)
    expect((name.element as HTMLInputElement).value).toBe('mine')
    expect((comment.element as HTMLInputElement).value).toBe('y')
    const banner = wrapper.find('.idnode-config-form__changed-elsewhere')
    expect(banner.exists()).toBe(true)
    expect(banner.text()).toContain('Name')
    expect(banner.text()).not.toContain('Comment')

    const undo = wrapper.findAll('.idnode-config-form__btn').find((b) => b.text() === 'Undo')
    await undo!.trigger('click')
    expect((inputs(wrapper).name.element as HTMLInputElement).value).toBe('theirs')
    expect(wrapper.find('.idnode-config-form__changed-elsewhere').exists()).toBe(false)
  })

  it('drops the notice once the user sets the field to the new saved value', async () => {
    const wrapper = await mountSingleton()
    await inputs(wrapper).name.setValue('mine')
    await reloadWith('theirs', 'x')
    expect(wrapper.find('.idnode-config-form__changed-elsewhere').exists()).toBe(true)

    await inputs(wrapper).name.setValue('theirs')

    expect(wrapper.find('.idnode-config-form__changed-elsewhere').exists()).toBe(false)
  })

  it('does not flag an edited field the server left unchanged', async () => {
    const wrapper = await mountSingleton()
    await inputs(wrapper).name.setValue('mine')

    await reloadWith('a', 'y')

    expect((inputs(wrapper).name.element as HTMLInputElement).value).toBe('mine')
    expect(wrapper.find('.idnode-config-form__changed-elsewhere').exists()).toBe(false)
  })

  it('saves the refreshed values of untouched fields, not the stale ones', async () => {
    const wrapper = await mountSingleton()
    await inputs(wrapper).name.setValue('mine')
    await reloadWith('a', 'y')

    apiMock.mockResolvedValueOnce({}) /* save */
    apiMock.mockResolvedValueOnce({ entries: [{ params: params('mine', 'y') }] })
    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(apiMock).toHaveBeenCalledWith('config/save', {
      node: JSON.stringify({ name: 'mine', comment: 'y' }),
    })
  })

  it('ignores notifications without reload and unsubscribes on unmount', async () => {
    const wrapper = await mountSingleton()
    const calls = apiMock.mock.calls.length
    fire('config', { change: ['whatever'] })
    await vi.advanceTimersByTimeAsync(300)
    expect(apiMock.mock.calls.length).toBe(calls)

    wrapper.unmount()
    expect(cometListenerCount('config')).toBe(0)
  })
})

describe('IdnodeConfigForm — Comet refresh (uuid mode)', () => {
  it('refetches on a change for its own uuid only', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'u1', class: 'dvrconfig', event: 'dvrconfig', params: params('a', 'x') }],
    })
    const wrapper = mount(IdnodeConfigForm, { props: { uuid: 'u1' } })
    await flushPromises()
    const calls = apiMock.mock.calls.length

    fire('dvrconfig', { change: ['u2'] })
    await vi.advanceTimersByTimeAsync(300)
    expect(apiMock.mock.calls.length).toBe(calls)

    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'u1', params: params('b', 'x') }],
    })
    fire('dvrconfig', { change: ['u1'] })
    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()

    expect(apiMock).toHaveBeenLastCalledWith('idnode/load', { uuid: 'u1' })
    expect((wrapper.find('input[type="text"]').element as HTMLInputElement).value).toBe('b')
  })
})
