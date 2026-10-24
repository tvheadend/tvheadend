// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * AboutView — the serverinfo-driven capabilities list, the credits
 * and the theme-dependent TMDb / TheTVDB marks, and the build
 * information: the build date from serverinfo and the admin-only
 * build details, which load from serverinfo/build when the section
 * is first opened.
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

describe('AboutView capabilities, credits and logos', () => {
  beforeEach(() => {
    apiCallMock.mockReset()
    delete document.documentElement.dataset.theme
  })
  afterEach(() => {
    delete document.documentElement.dataset.theme
    vi.unstubAllGlobals()
  })

  it('lists capabilities with readable labels and the raw id as title', async () => {
    const wrapper = await mountWith({
      capabilities: [
        'tvadapters',
        'satip_client',
        'satip_server',
        'caclient_advanced',
        'new_thing',
      ],
    })
    const list = wrapper.find('ul.about__caps')
    expect(list.exists()).toBe(true)
    expect(list.attributes('role')).toBe('list')
    const items = list.findAll('li')
    expect(items.map((li) => li.text())).toEqual([
      'new_thing',
      'SAT>IP Client',
      'SAT>IP Server',
      'TV adapters',
    ])
    expect(items.map((li) => li.attributes('title'))).toEqual([
      'new_thing',
      'satip_client',
      'satip_server',
      'tvadapters',
    ])
  })

  it('has no Capabilities row when only the UI setting is listed', async () => {
    const wrapper = await mountWith({ capabilities: ['caclient_advanced'] })
    expect(wrapper.findAll('dt').map((d) => d.text())).not.toContain('Capabilities')
  })

  it('credits the help page icons and not the classic UI stack', async () => {
    const wrapper = await mountWith({})
    const text = wrapper.text()
    expect(text).toContain('Help page icons from')
    expect(wrapper.find('a[href="https://www.famfamfam.com/lab/icons/silk/"]').text()).toBe(
      'FamFamFam Silk'
    )
    expect(text).not.toContain('ExtJS')
    expect(text).not.toContain('Noto')
  })

  function logoSources(wrapper: Awaited<ReturnType<typeof mountWith>>): string[] {
    return wrapper.findAll('img.about__inline-logo').map((img) => img.attributes('src') ?? '')
  }

  it('uses the dark TMDb and TheTVDB marks on the light palette', async () => {
    document.documentElement.dataset.theme = 'light'
    const wrapper = await mountWith({})
    const [tmdb, tvdb] = logoSources(wrapper)
    expect(tmdb).toMatch(/static\/img\/tmdb\.png$/)
    expect(tvdb).toMatch(/static\/img\/tvdb\.png$/)
  })

  it.each(['dark', 'access'])('uses the white marks on the %s palette', async (theme) => {
    document.documentElement.dataset.theme = theme
    const wrapper = await mountWith({})
    const [tmdb, tvdb] = logoSources(wrapper)
    expect(tmdb).toMatch(/static\/img\/tmdb_white\.png$/)
    expect(tvdb).toMatch(/static\/img\/tvdb_white\.png$/)
  })

  it('follows a theme switch while the page is open', async () => {
    /* happy-dom keeps a MutationObserver callback only through a
     * WeakRef, so a garbage collection at the wrong moment loses the
     * mutation and the test fails at random. Stub the observer and
     * run the page's callback by hand. */
    let onMutation: (() => void) | undefined
    const observe = vi.fn()
    const disconnect = vi.fn()
    vi.stubGlobal(
      'MutationObserver',
      class {
        observe = observe
        disconnect = disconnect
        constructor(callback: () => void) {
          onMutation = callback
        }
      }
    )
    document.documentElement.dataset.theme = 'light'
    const wrapper = await mountWith({})
    expect(observe).toHaveBeenCalledWith(
      document.documentElement,
      expect.objectContaining({ attributeFilter: ['data-theme'] })
    )
    document.documentElement.dataset.theme = 'dark'
    onMutation?.()
    await wrapper.vm.$nextTick()
    expect(logoSources(wrapper)[0]).toMatch(/tmdb_white\.png$/)
  })
})

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
