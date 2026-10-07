// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * AboutView — the server-driven parts: the build date from
 * serverinfo and the admin-only build details, which load from
 * serverinfo/build when the section is first opened.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import { useAccessStore } from '@/stores/access'

const apiCallMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

const copyTextMock = vi.fn<(s: string) => Promise<boolean>>(() => Promise.resolve(true))
vi.mock('@/composables/useClipboard', () => ({
  useClipboard: () => ({ copyText: copyTextMock }),
}))

const toastSuccess = vi.fn()
const toastError = vi.fn()
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({ success: toastSuccess, error: toastError, info: vi.fn() }),
}))

import AboutView from '../AboutView.vue'

const BUILD_CONFIG = 'Configure arguments:\n--disable-libav\n\nOptions:\n  libav no\n'
const SERVERINFO = {
  sw_version: '2026.09.24~547f8bfa',
  build_timestamp: '2026-09-24T20:08:31+0000',
}

/* serverinfo answers `info`, serverinfo/build answers `build`
 * (a rejection when it is an Error). */
async function mountWith(
  info: Record<string, unknown>,
  { admin = true, build = BUILD_CONFIG as string | Error } = {},
) {
  const pinia = createPinia()
  setActivePinia(pinia)
  useAccessStore().data = { admin, dvr: true, uilevel: 'basic' }
  apiCallMock.mockImplementation((endpoint: string) => {
    if (endpoint !== 'serverinfo/build') return Promise.resolve(info)
    return build instanceof Error ? Promise.reject(build) : Promise.resolve({ build_config: build })
  })
  const wrapper = mount(AboutView, {
    global: {
      /* fmtDate reads the date mask from the access store. */
      plugins: [pinia],
      stubs: {
        /* Plain button: the click listener falls through to it. */
        Button: { template: '<button type="button"><slot /></button>' },
      },
    },
  })
  await flushPromises()
  return wrapper
}

type Wrapper = Awaited<ReturnType<typeof mountWith>>

function detailRow(wrapper: Wrapper, label: string) {
  const dt = wrapper.findAll('dt').find((d) => d.text() === label)
  return dt ? dt.element.nextElementSibling : null
}

function buildRequests(): number {
  return apiCallMock.mock.calls.filter(([endpoint]) => endpoint === 'serverinfo/build').length
}

/* A click on the summary toggles the native disclosure. */
async function toggleBuildDetails(wrapper: Wrapper) {
  await wrapper.find('details.about__build summary').trigger('click')
  await flushPromises()
  return wrapper.find('details.about__build')
}

describe('AboutView build information', () => {
  beforeEach(() => {
    apiCallMock.mockReset()
    copyTextMock.mockClear()
    toastSuccess.mockClear()
    toastError.mockClear()
  })
  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('shows the build date under Version, with the raw value as its title', async () => {
    const wrapper = await mountWith(SERVERINFO)
    const dd = detailRow(wrapper, 'Built')
    expect(dd).not.toBeNull()
    expect(dd!.getAttribute('title')).toBe('2026-09-24T20:08:31+0000')
    /* Rendered through the shared formatter, not echoed raw. */
    expect(dd!.textContent?.trim()).not.toBe('2026-09-24T20:08:31+0000')
    expect(dd!.textContent).toContain('2026')
  })

  it('has no Built row when the server sends no build date', async () => {
    const wrapper = await mountWith({ sw_version: '2026.09.24~547f8bfa' })
    expect(detailRow(wrapper, 'Built')).toBeNull()
  })

  it('keeps the build details closed and unrequested until opened', async () => {
    const wrapper = await mountWith(SERVERINFO)
    const details = wrapper.find('details.about__build')
    expect(details.attributes('open')).toBeUndefined()
    expect(details.find('summary').text()).toBe('Build details')
    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock).toHaveBeenCalledWith('serverinfo')
    expect(buildRequests()).toBe(0)
  })

  it('loads the build details once, on the first open', async () => {
    const wrapper = await mountWith(SERVERINFO)
    const details = await toggleBuildDetails(wrapper)
    expect((details.element as HTMLDetailsElement).open).toBe(true)
    expect(buildRequests()).toBe(1)
    expect(details.find('pre').text()).toBe(BUILD_CONFIG.trim())

    await toggleBuildDetails(wrapper)
    await toggleBuildDetails(wrapper)
    expect(buildRequests()).toBe(1)
  })

  it('shows Loading while the request is in flight', async () => {
    const wrapper = await mountWith(SERVERINFO)
    apiCallMock.mockReturnValueOnce(new Promise(() => {}))
    const details = await toggleBuildDetails(wrapper)
    expect(details.text()).toContain('Loading…')
    expect(details.find('pre').exists()).toBe(false)
  })

  it('copies the loaded build details', async () => {
    const wrapper = await mountWith(SERVERINFO)
    const details = await toggleBuildDetails(wrapper)
    await details.find('button').trigger('click')
    await flushPromises()
    expect(copyTextMock).toHaveBeenCalledWith(BUILD_CONFIG.trim())
    expect(toastSuccess).toHaveBeenCalledTimes(1)
  })

  it('reports a failed copy', async () => {
    copyTextMock.mockResolvedValueOnce(false)
    const wrapper = await mountWith(SERVERINFO)
    const details = await toggleBuildDetails(wrapper)
    await details.find('button').trigger('click')
    await flushPromises()
    expect(toastError).toHaveBeenCalledTimes(1)
  })

  it('shows a failed load and tries again on the next open', async () => {
    const wrapper = await mountWith(SERVERINFO, { build: new Error('boom') })
    const details = await toggleBuildDetails(wrapper)
    expect(details.find('[role="alert"]').text()).toBe('Failed to load: boom')
    expect(details.find('button').exists()).toBe(false)

    await toggleBuildDetails(wrapper)
    await toggleBuildDetails(wrapper)
    expect(buildRequests()).toBe(2)
  })

  it('has no build details for a viewer who is not an admin', async () => {
    const wrapper = await mountWith(SERVERINFO, { admin: false })
    expect(wrapper.find('details.about__build').exists()).toBe(false)
    expect(buildRequests()).toBe(0)
  })
})
