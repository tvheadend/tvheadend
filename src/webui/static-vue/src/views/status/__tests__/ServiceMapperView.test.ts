// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ServiceMapperView — status text. The store is real, with the API
 * and Comet mocked, so the tests cover what the page shows after
 * its initial service/mapper/status fetch.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import ServiceMapperView from '../ServiceMapperView.vue'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

vi.mock('@/api/comet', () => ({
  cometClient: { on: () => () => {} },
}))

vi.mock('@/composables/useHelp', async () => {
  const { ref } = await import('vue')
  return { useHelp: () => ({ isOpen: ref(false), toggle: vi.fn(async () => {}) }) }
})

function respond(status: Record<string, unknown>, serviceName?: string): void {
  apiMock.mockImplementation(async (path: string) => {
    if (path === 'service/mapper/status') return status
    if (path === 'idnode/load') return { entries: [{ uuid: status.active, text: serviceName }] }
    throw new Error(`unexpected ${path}`)
  })
}

async function mountView() {
  const w = mount(ServiceMapperView, {
    global: { directives: { tooltip: () => undefined } },
  })
  await flushPromises()
  return w
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
})

describe('ServiceMapperView', () => {
  it('spaces the idle summary as one sentence', async () => {
    respond({ total: 19, ok: 19, fail: 0, ignore: 0 })
    const w = await mountView()
    expect(w.find('.service-mapper__idle').text()).toBe('Idle. Last run mapped 19 of 19 services.')
  })

  it('names failed and ignored services in the idle summary', async () => {
    respond({ total: 19, ok: 16, fail: 2, ignore: 1 })
    const w = await mountView()
    expect(w.find('.service-mapper__idle').text()).toBe(
      'Idle. Last run mapped 16 of 19 services, 2 failed, 1 ignored.',
    )
  })

  it('shows the service name, not its uuid, when opened during a job', async () => {
    const uuid = '0123456789abcdef0123456789abcdef'
    respond({ total: 19, ok: 3, fail: 0, ignore: 0, active: uuid }, 'BBC One')
    const w = await mountView()
    const active = w.find('.service-mapper__active')
    expect(active.text()).toContain('BBC One')
    expect(active.text()).not.toContain(uuid)
  })
})
