// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * NavRail grouping tests (ADR 0016 / 0017). Verifies the rail renders
 * Home as a leading ungrouped item, clusters the rest into "TV" and
 * "Server" sections, drops a section's header when every item in it is
 * permission-hidden, and renders About as an ungrouped trailing item
 * outside any group.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { createRouter, createMemoryHistory } from 'vue-router'
import NavRail from '../NavRail.vue'

/* useI18n → identity, so section headers assert as their English keys. */
vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: (s: string) => s }),
}))

vi.mock('@/composables/useRailPreference', () => ({
  useRailPreference: () => ({ toggle: vi.fn() }),
}))

/* Access store stub — `has(perm)` drives per-item visibility,
 * `loaded` gates the rail rendering and the login/logout split,
 * `data.admin` distinguishes anonymous-anonymous from `--noacl`
 * mode (both have empty `username`). Reset per test from
 * `beforeEach` so individual cases can flip without bleeding. */
const accessStub = {
  loaded: true,
  data: { username: '', admin: true } as { username?: string; admin?: boolean },
  has: vi.fn(() => true),
}
vi.mock('@/stores/access', () => ({
  useAccessStore: () => accessStub,
}))

/* Comet client stub — only `reset()` is exercised by the login
 * click path; the rest is unused in these tests. Reset is set
 * per-test in beforeEach so each `expect(cometResetSpy)` is
 * isolated. */
const cometResetSpy = vi.fn()
vi.mock('@/api/comet', () => ({
  cometClient: { reset: () => cometResetSpy() },
}))

/* Throwaway component for each route — NavRail only needs the route
 * to resolve (path + meta.permission), never to render it. */
const RouteStub = { template: '<div />' }

function makeRouter() {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/home', name: 'dashboard', component: RouteStub },
      { path: '/epg', name: 'epg', component: RouteStub },
      { path: '/dvr', name: 'dvr', component: RouteStub, meta: { permission: 'dvr' } },
      {
        path: '/configuration',
        name: 'configuration',
        component: RouteStub,
        meta: { permission: 'admin' },
      },
      {
        path: '/status',
        name: 'status',
        component: RouteStub,
        meta: { permission: 'admin' },
      },
      { path: '/about', name: 'about', component: RouteStub },
    ],
  })
}

async function mountRail() {
  const router = makeRouter()
  router.push('/epg')
  await router.isReady()
  return mount(NavRail, {
    props: { open: true },
    global: {
      plugins: [router],
      stubs: { RailInfoArea: true },
    },
  })
}

describe('NavRail — section grouping', () => {
  it('groups items into TV and Server sections for an admin', async () => {
    accessStub.has.mockImplementation(() => true)
    const wrapper = await mountRail()

    const headers = wrapper.findAll('.nav-rail__section-header').map((h) => h.text())
    expect(headers).toEqual(['TV', 'Server'])

    const groups = wrapper.findAll('[role="group"]')
    expect(groups).toHaveLength(2)
    expect(groups[0].attributes('aria-label')).toBe('TV')
    expect(groups[1].attributes('aria-label')).toBe('Server')

    /* TV holds EPG + DVR; Server holds Configuration + Status. */
    expect(groups[0].findAll('.nav-item')).toHaveLength(2)
    expect(groups[1].findAll('.nav-item')).toHaveLength(2)
  })

  it('renders Home as the first item, ungrouped', async () => {
    accessStub.has.mockImplementation(() => true)
    const wrapper = await mountRail()

    const links = wrapper.findAll('a.nav-item')
    expect(links[0].attributes('href')).toContain('/home')

    /* Home sits in a header-less wrapper, not a labelled group. */
    for (const group of wrapper.findAll('[role="group"]')) {
      expect(group.text()).not.toContain('Home')
    }
  })

  it('shows Home for a non-admin and drops the Server header', async () => {
    /* No admin, no dvr → Configuration, Status and DVR all hidden. */
    accessStub.has.mockImplementation(() => false)
    const wrapper = await mountRail()

    const headers = wrapper.findAll('.nav-rail__section-header').map((h) => h.text())
    expect(headers).toEqual(['TV'])
    expect(wrapper.text()).not.toContain('Server')

    /* Home has no permission gate — still the first item. */
    const links = wrapper.findAll('a.nav-item')
    expect(links[0].attributes('href')).toContain('/home')
  })

  it('renders About as an ungrouped item outside any section group', async () => {
    accessStub.has.mockImplementation(() => true)
    const wrapper = await mountRail()

    const aboutLink = wrapper
      .findAll('a.nav-item')
      .find((a) => a.attributes('href')?.includes('/about'))
    expect(aboutLink).toBeTruthy()

    /* About sits in a wrapper that is not a labelled group. */
    for (const group of wrapper.findAll('[role="group"]')) {
      expect(group.text()).not.toContain('About')
    }
  })

  it('renders Classic UI as a plain external anchor to /extjs.html, not a router link', async () => {
    accessStub.has.mockImplementation(() => true)
    const wrapper = await mountRail()

    const classic = wrapper
      .findAll('a.nav-item')
      .find((a) => a.text().includes('Classic UI'))
    expect(classic).toBeTruthy()
    /* Full-page navigation out of the SPA — the href is the bare
     * extjs path, not a router-resolved in-app route. */
    expect(classic!.attributes('href')).toBe('/extjs.html')
    /* A RouterLink in this router would carry no active-class binding
     * for an unmatched path, but more importantly it must sit OUTSIDE
     * any labelled section group (trailing ungrouped block). */
    for (const group of wrapper.findAll('[role="group"]')) {
      expect(group.text()).not.toContain('Classic UI')
    }
  })
})

/*
 * Login / logout affordance — regression coverage for the user
 * report where the Vue UI never offered a way to authenticate.
 * The Vue bootstrap doesn't fire an auth-gated API call, so the
 * browser never pops its native prompt; an explicit Login button
 * is the only path in for an anonymous user. The button must NOT
 * appear in `--noacl` mode (admin without a username) where there
 * is nothing to log in to, and must not flash during the pre-auth
 * window before the first `accessUpdate` arrives.
 */
describe('NavRail — login / logout affordance', () => {
  /* `globalThis.location.href = '...'` triggers navigation in jsdom
   * (which warns and bails) and `globalThis.location.reload()` isn't
   * implemented at all. Swap in a plain object for the duration
   * of each test so href writes and reload() calls are just
   * spies we can assert on. `fetch` is stubbed per test below;
   * default resolves with `ok: true` so the Login click path
   * exercises its reload branch. */
  const originalLocation = globalThis.location
  const originalFetch = globalThis.fetch
  let reloadSpy: ReturnType<typeof vi.fn>
  let fetchSpy: ReturnType<typeof vi.fn>
  beforeEach(() => {
    accessStub.has.mockImplementation(() => true)
    accessStub.loaded = true
    accessStub.data = { username: '', admin: false }
    reloadSpy = vi.fn()
    fetchSpy = vi.fn().mockResolvedValue({ ok: true, status: 200 })
    cometResetSpy.mockReset()
    Object.defineProperty(globalThis, 'location', {
      configurable: true,
      writable: true,
      value: { href: '', reload: reloadSpy },
    })
    globalThis.fetch = fetchSpy as unknown as typeof fetch
  })
  afterEach(() => {
    Object.defineProperty(globalThis, 'location', {
      configurable: true,
      writable: true,
      value: originalLocation,
    })
    globalThis.fetch = originalFetch
  })

  it('shows Login (and not Logout) for an anonymous, non-admin user', async () => {
    accessStub.data = { username: '', admin: false }
    const wrapper = await mountRail()

    expect(wrapper.find('.nav-rail__login').exists()).toBe(true)
    expect(wrapper.find('.nav-rail__logout-row .nav-rail__logout:not(.nav-rail__login)').exists())
      .toBe(false)
  })

  it('shows Logout (and not Login) for an authenticated user', async () => {
    accessStub.data = { username: 'alice', admin: false }
    const wrapper = await mountRail()

    expect(wrapper.find('.nav-rail__login').exists()).toBe(false)
    /* Logout button is the one WITHOUT the .nav-rail__login modifier. */
    const logoutBtn = wrapper.find('.nav-rail__logout:not(.nav-rail__login)')
    expect(logoutBtn.exists()).toBe(true)
    expect(logoutBtn.attributes('aria-label')).toBe('Logout')
  })

  it('renders neither button in --noacl mode (admin without a username)', async () => {
    accessStub.data = { username: '', admin: true }
    const wrapper = await mountRail()

    expect(wrapper.find('.nav-rail__login').exists()).toBe(false)
    expect(wrapper.find('.nav-rail__logout-row').exists()).toBe(false)
  })

  it('renders neither button in the pre-auth window (loaded=false)', async () => {
    accessStub.loaded = false
    accessStub.data = { username: '', admin: false }
    const wrapper = await mountRail()

    expect(wrapper.find('.nav-rail__login').exists()).toBe(false)
    expect(wrapper.find('.nav-rail__logout-row').exists()).toBe(false)
  })

  it('fetches /login and resets comet on success (Login click happy path)', async () => {
    accessStub.data = { username: '', admin: false }
    const wrapper = await mountRail()

    await wrapper.find('.nav-rail__login').trigger('click')
    /* Two ticks let the fetch promise resolve and the await
     * inside `onLoginClick` continue to the comet reset call. */
    await Promise.resolve()
    await Promise.resolve()

    expect(fetchSpy).toHaveBeenCalledWith(
      '/login',
      expect.objectContaining({
        method: 'GET',
        credentials: 'include',
        cache: 'no-store',
      }),
    )
    expect(cometResetSpy).toHaveBeenCalled()
    /* Critical: must NOT have reloaded the page or navigated. */
    expect(reloadSpy).not.toHaveBeenCalled()
    expect(globalThis.location.href).toBe('')
  })

  it('does not reset comet when /login returns 401 (user cancelled the prompt)', async () => {
    accessStub.data = { username: '', admin: false }
    fetchSpy.mockResolvedValue({ ok: false, status: 401 })
    const wrapper = await mountRail()

    await wrapper.find('.nav-rail__login').trigger('click')
    await Promise.resolve()
    await Promise.resolve()

    expect(fetchSpy).toHaveBeenCalled()
    expect(cometResetSpy).not.toHaveBeenCalled()
    expect(reloadSpy).not.toHaveBeenCalled()
    expect(globalThis.location.href).toBe('')
  })

  it('swallows fetch errors without resetting comet or navigating', async () => {
    accessStub.data = { username: '', admin: false }
    fetchSpy.mockRejectedValue(new TypeError('network down'))
    const wrapper = await mountRail()

    await wrapper.find('.nav-rail__login').trigger('click')
    await Promise.resolve()
    await Promise.resolve()

    expect(fetchSpy).toHaveBeenCalled()
    expect(cometResetSpy).not.toHaveBeenCalled()
    expect(reloadSpy).not.toHaveBeenCalled()
    expect(globalThis.location.href).toBe('')
  })

  it('navigates to /logout on Logout click (regression guard)', async () => {
    accessStub.data = { username: 'alice', admin: false }
    const wrapper = await mountRail()

    await wrapper.find('.nav-rail__logout:not(.nav-rail__login)').trigger('click')
    expect(globalThis.location.href).toBe('/logout')
  })
})
