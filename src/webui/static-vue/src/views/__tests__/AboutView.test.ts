// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * AboutView — the serverinfo-driven capabilities list, the credits
 * and the theme-dependent TMDb / TheTVDB marks.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia } from 'pinia'

const apiCallMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

import AboutView from '../AboutView.vue'

async function mountWith(info: Record<string, unknown>) {
  apiCallMock.mockResolvedValue(info)
  const wrapper = mount(AboutView, {
    global: {
      plugins: [createPinia()],
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
