// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it, vi } from 'vitest'
import { createMemoryHistory, createRouter, type RouteRecordRaw } from 'vue-router'
import { buildRouteCommands } from '../commandRegistry'

/* Build a minimal router stand-in for the registry. Production
 * uses the full `src/router/index.ts`, but for unit testing we
 * pin the shape to a small fixture so the assertions don't drift
 * when the real router grows. */
function makeTestRouter(routes: RouteRecordRaw[]) {
  return createRouter({
    history: createMemoryHistory('/'),
    routes,
  })
}

const noop = { render: () => null }

describe('buildRouteCommands', () => {
  it('produces a command for every leaf route with meta.title', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      { path: '/b', name: 'b', component: noop, meta: { title: 'Beta' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.map((c) => c.label)).toEqual(['Alpha', 'Beta'])
  })

  it('skips routes without meta.title', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      { path: '/anon', name: 'anon', component: noop },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.map((c) => c.id)).toEqual(['nav:a'])
  })

  it('skips wizard routes (meta.isWizard)', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      {
        path: '/wizard/hello',
        name: 'wizard-hello',
        component: noop,
        meta: { title: 'Setup Wizard', isWizard: true },
      },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.map((c) => c.id)).toEqual(['nav:a'])
  })

  it("skips the 'home' root placeholder", () => {
    const router = makeTestRouter([
      { path: '/', name: 'home', component: noop, meta: { title: 'Home' } },
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.map((c) => c.id)).toEqual(['nav:a'])
  })

  it('skips routes carrying a static redirect (empty-path children)', () => {
    const router = makeTestRouter([
      {
        path: '/parent',
        component: noop,
        meta: { title: 'Parent' },
        children: [
          { path: '', name: 'parent', redirect: { name: 'parent-leaf' } },
          { path: 'leaf', name: 'parent-leaf', component: noop, meta: { title: 'Leaf' } },
        ],
      },
    ])
    const commands = buildRouteCommands(router)
    /* Parent has meta.title but vue-router records the parent as
     * `name`-less when its empty-path child carries the parent
     * name via redirect. Verify only the leaf shows up. */
    expect(commands.map((c) => c.label)).toContain('Leaf')
    expect(commands.find((c) => c.id === 'nav:parent')).toBeUndefined()
  })

  it('skips _dev_* routes (dev-only auto-imports)', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      { path: '/_dev/foo', name: '_dev_foo', component: noop, meta: { title: '[dev] foo' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.map((c) => c.id)).toEqual(['nav:a'])
  })

  it('inherits meta.permission as the command requires gate', () => {
    const router = makeTestRouter([
      {
        path: '/admin',
        name: 'admin-only',
        component: noop,
        meta: { title: 'Admin Only', permission: 'admin' },
      },
      {
        path: '/dvr',
        name: 'dvr-only',
        component: noop,
        meta: { title: 'DVR Only', permission: 'dvr' },
      },
      { path: '/pub', name: 'pub', component: noop, meta: { title: 'Public' } },
    ])
    const commands = buildRouteCommands(router)
    const byId = Object.fromEntries(commands.map((c) => [c.id, c]))
    expect(byId['nav:admin-only'].requires).toBe('admin')
    expect(byId['nav:dvr-only'].requires).toBe('dvr')
    expect(byId['nav:pub'].requires).toBeUndefined()
  })

  it("builds the breadcrumb description from ancestor route titles ('Parent / Child')", () => {
    const router = makeTestRouter([
      {
        path: '/cfg',
        component: noop,
        meta: { title: 'Configuration' },
        children: [
          {
            path: 'dvb',
            component: noop,
            meta: { title: 'DVB Inputs' },
            children: [
              {
                path: 'networks',
                name: 'cfg-dvb-networks',
                component: noop,
                meta: { title: 'Networks' },
              },
            ],
          },
        ],
      },
    ])
    const commands = buildRouteCommands(router)
    const networks = commands.find((c) => c.id === 'nav:cfg-dvb-networks')
    expect(networks?.label).toBe('Networks')
    expect(networks?.description).toBe('Configuration / DVB Inputs')
  })

  it('does not set a description when there are no ancestors', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands[0].description).toBeUndefined()
  })

  it('command.action pushes to the route by name', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      { path: '/b', name: 'b', component: noop, meta: { title: 'Beta' } },
    ])
    const pushSpy = vi.spyOn(router, 'push').mockResolvedValue()
    const commands = buildRouteCommands(router)
    const alpha = commands.find((c) => c.id === 'nav:a')!
    alpha.action()
    expect(pushSpy).toHaveBeenCalledWith({ name: 'a' })
  })

  it('every command has the Navigation section and a stable nav:<name> id', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
      { path: '/b', name: 'b', component: noop, meta: { title: 'Beta' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands.every((c) => c.section === 'Navigation')).toBe(true)
    expect(commands.every((c) => c.id.startsWith('nav:'))).toBe(true)
  })

  it('attaches an icon to every command', () => {
    const router = makeTestRouter([
      { path: '/a', name: 'a', component: noop, meta: { title: 'Alpha' } },
    ])
    const commands = buildRouteCommands(router)
    expect(commands[0].icon).toBeDefined()
  })
})
