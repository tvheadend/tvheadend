// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- The test needs a
 * route leaf, a layout with a RouterView and a host that calls the
 * composable, three tiny components that belong with the test. */

/*
 * usePageTitle — the document title names the page with its
 * sections, most specific first, and the server it comes from.
 */
import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { defineComponent, h, nextTick } from 'vue'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import { createMemoryHistory, createRouter, RouterView, type Router } from 'vue-router'
import { usePageTitle } from '../usePageTitle'
import { useAccessStore } from '@/stores/access'

const Empty = defineComponent({ render: () => h('div') })
const Layout = defineComponent({ render: () => h(RouterView) })

function makeRouter(): Router {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/', component: Empty },
      { path: '/about', component: Empty, meta: { title: 'About' } },
      {
        path: '/configuration',
        component: Layout,
        meta: { title: 'Configuration' },
        children: [
          {
            path: 'general',
            component: Layout,
            meta: { title: 'General' },
            children: [{ path: 'base', component: Empty, meta: { title: 'Base' } }],
          },
          {
            path: 'debugging',
            component: Layout,
            meta: { title: 'Debugging' },
            children: [{ path: 'config', component: Empty, meta: { title: 'Configuration' } }],
          },
        ],
      },
      {
        path: '/wizard',
        component: Layout,
        meta: { title: 'Setup Wizard' },
        children: [{ path: 'hello', component: Empty, meta: { title: 'Setup Wizard' } }],
      },
    ],
  })
}

const Host = defineComponent({
  setup() {
    usePageTitle()
    return () => h(RouterView)
  },
})

async function mountAt(path: string) {
  const router = makeRouter()
  await router.push(path)
  await router.isReady()
  const pinia = createPinia()
  setActivePinia(pinia)
  const wrapper = mount(Host, { global: { plugins: [router, pinia] } })
  await flushPromises()
  return { wrapper, router, access: useAccessStore() }
}

describe('usePageTitle', () => {
  beforeEach(() => {
    document.title = ''
  })
  afterEach(() => {
    delete (globalThis as { tvh_locale?: Record<string, string> }).tvh_locale
  })

  it('lists the page and its sections, most specific first', async () => {
    await mountAt('/configuration/general/base')
    expect(document.title).toBe('Base · General · Configuration — Tvheadend')
  })

  it('tells Debugging > Configuration apart from the Configuration section', async () => {
    await mountAt('/configuration/debugging/config')
    expect(document.title).toBe('Configuration · Debugging · Configuration — Tvheadend')
  })

  it('shows a title repeated by the parent once', async () => {
    await mountAt('/wizard/hello')
    expect(document.title).toBe('Setup Wizard — Tvheadend')
  })

  it('uses the server name from the access data', async () => {
    const { access } = await mountAt('/about')
    expect(document.title).toBe('About — Tvheadend')
    access.data = { admin: false, dvr: false, server_name: 'Omnia' }
    await nextTick()
    expect(document.title).toBe('About — Omnia')
  })

  it('falls back to Tvheadend for an empty server name', async () => {
    const { access } = await mountAt('/about')
    access.data = { admin: false, dvr: false, server_name: '  ' }
    await nextTick()
    expect(document.title).toBe('About — Tvheadend')
  })

  it('shows only the server name on a route without a title', async () => {
    const { access } = await mountAt('/')
    access.data = { admin: false, dvr: false, server_name: 'Omnia' }
    await nextTick()
    expect(document.title).toBe('Omnia')
  })

  it('follows navigation', async () => {
    const { router } = await mountAt('/about')
    await router.push('/configuration/general/base')
    await flushPromises()
    expect(document.title).toBe('Base · General · Configuration — Tvheadend')
  })

  it('translates each part through the catalogue', async () => {
    ;(globalThis as { tvh_locale?: Record<string, string> }).tvh_locale = {
      Base: 'Základ',
      General: 'Obecné',
      Configuration: 'Nastavení',
    }
    await mountAt('/configuration/general/base')
    expect(document.title).toBe('Základ · Obecné · Nastavení — Tvheadend')
  })
})
