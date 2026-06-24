// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * settingsSource tests — verifies the singleton-config field
 * indexer that powers Settings results in the Cmd-K palette.
 * Covers the admin gate, the parallel fetch of all six config
 * pages, the 60 s cache, the noui/phidden field filter, the
 * stable id shape, and the navigation hash payload.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const apiMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiMock }))

vi.mock('@/composables/useI18n', () => ({
  t: (s: string) => s,
}))

import {
  __resetSettingsSourceForTests,
  ensureSettingsLoaded,
  getSettingsCommands,
  type SettingsSourceDeps,
} from '../settingsSource'

const fakeRouter = { push: vi.fn().mockResolvedValue(undefined) }

function makeAccess(canAdmin = true) {
  return { has: (k: string) => (k === 'admin' ? canAdmin : false) }
}

const baseDeps: SettingsSourceDeps = {
  router: fakeRouter as unknown as SettingsSourceDeps['router'],
  access: makeAccess() as unknown as SettingsSourceDeps['access'],
}

/* The six load endpoints the source fetches in parallel. Tests
 * use this list to assert all-six coverage without hard-coding it
 * across each spec. */
const LOAD_ENDPOINTS = [
  'config/load',
  'imagecache/config/load',
  'satips/config/load',
  'timeshift/config/load',
  'tvhlog/config/load',
  'epggrab/config/load',
]

/* Build a minimal load-response with the given props. Server's
 * real shape is `{ entries: [{ params, meta: { groups } }] }`. */
function fakeLoadResponse(
  params: Array<{ id: string; caption?: string; description?: string; advanced?: boolean; expert?: boolean; noui?: boolean; phidden?: boolean; group?: number }>,
  groups: Array<{ number: number; name: string }> = [],
) {
  return { entries: [{ params, meta: { groups } }] }
}

/* Dispatch mock based on endpoint string. Tests pin a specific
 * endpoint's response and let everything else return an empty
 * default. */
function pinApi(map: Record<string, ReturnType<typeof fakeLoadResponse> | Error>) {
  apiMock.mockImplementation((endpoint: string) => {
    const pick = map[endpoint]
    if (pick instanceof Error) return Promise.reject(pick)
    return Promise.resolve(pick ?? fakeLoadResponse([]))
  })
}

describe('settingsSource', () => {
  beforeEach(() => {
    apiMock.mockReset()
    fakeRouter.push.mockReset()
    fakeRouter.push.mockResolvedValue(undefined)
    __resetSettingsSourceForTests()
  })

  afterEach(() => {
    __resetSettingsSourceForTests()
  })

  it('no-ops and returns no commands for a non-admin user', async () => {
    const noAdmin: SettingsSourceDeps = {
      ...baseDeps,
      access: makeAccess(false) as unknown as SettingsSourceDeps['access'],
    }
    pinApi({
      'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
    })
    await ensureSettingsLoaded(noAdmin)
    expect(apiMock).not.toHaveBeenCalled()
    expect(getSettingsCommands().value).toHaveLength(0)
  })

  it('fetches all six singleton-config endpoints in parallel', async () => {
    pinApi({})
    await ensureSettingsLoaded(baseDeps)
    const calledEndpoints = apiMock.mock.calls.map((c) => c[0])
    for (const ep of LOAD_ENDPOINTS) {
      expect(calledEndpoints).toContain(ep)
    }
  })

  it('passes `meta: 1` so the response carries metadata + values', async () => {
    pinApi({})
    await ensureSettingsLoaded(baseDeps)
    for (const call of apiMock.mock.calls) {
      expect(call[1]).toEqual({ meta: 1 })
    }
  })

  it('builds a command per editable field with a stable id', async () => {
    pinApi({
      'config/load': fakeLoadResponse([
        { id: 'hostname', caption: 'Server name', description: 'Hostname of the server' },
        { id: 'http_port', caption: 'HTTP port' },
      ]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmds = getSettingsCommands().value
    const ids = cmds.map((c) => c.id)
    expect(ids).toContain('settings:config-general-base.hostname')
    expect(ids).toContain('settings:config-general-base.http_port')
  })

  it('uses the prop caption as the visible label, falling back to id', async () => {
    pinApi({
      'config/load': fakeLoadResponse([
        { id: 'hostname', caption: 'Server name' },
        { id: 'mysteryField' /* no caption */ },
      ]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmds = getSettingsCommands().value
    const byId = Object.fromEntries(cmds.map((c) => [c.id, c]))
    expect(byId['settings:config-general-base.hostname'].label).toBe('Server name')
    expect(byId['settings:config-general-base.mysteryField'].label).toBe('mysteryField')
  })

  it('skips fields flagged `noui` or `phidden`', async () => {
    pinApi({
      'config/load': fakeLoadResponse([
        { id: 'visible', caption: 'Visible field' },
        { id: 'internal', caption: 'Internal only', noui: true },
        { id: 'hidden', caption: 'Hidden forever', phidden: true },
      ]),
    })
    await ensureSettingsLoaded(baseDeps)
    const ids = getSettingsCommands().value.map((c) => c.id)
    expect(ids).toContain('settings:config-general-base.visible')
    expect(ids).not.toContain('settings:config-general-base.internal')
    expect(ids).not.toContain('settings:config-general-base.hidden')
  })

  it('puts results in the Settings section with admin permission gate', async () => {
    pinApi({
      'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmd = getSettingsCommands().value.find(
      (c) => c.id === 'settings:config-general-base.hostname',
    )
    expect(cmd?.section).toBe('Settings')
    expect(cmd?.requires).toBe('admin')
  })

  it('description includes group name when the prop carries one', async () => {
    pinApi({
      'config/load': fakeLoadResponse(
        [{ id: 'foo', caption: 'Foo', group: 3 }],
        [{ number: 3, name: 'Networking' }],
      ),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmd = getSettingsCommands().value[0]
    expect(cmd.description).toBe('General — Base · Networking')
  })

  it('description omits group name when the prop has no group', async () => {
    pinApi({
      'config/load': fakeLoadResponse([{ id: 'foo', caption: 'Foo' }]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmd = getSettingsCommands().value[0]
    expect(cmd.description).toBe('General — Base')
  })

  it('keywords include the prop id + description for synonym matching', async () => {
    pinApi({
      'config/load': fakeLoadResponse([
        { id: 'log_path', caption: 'Log path', description: 'Where to write log files' },
      ]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmd = getSettingsCommands().value[0]
    expect(cmd.keywords).toContain('log_path')
    expect(cmd.keywords).toContain('Where to write log files')
  })

  it("action navigates to the field's page with `#field=<id>`", async () => {
    pinApi({
      'tvhlog/config/load': fakeLoadResponse([{ id: 'syslog', caption: 'syslog' }]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmd = getSettingsCommands().value.find(
      (c) => c.id === 'settings:config-debugging-config.syslog',
    )
    cmd?.action()
    expect(fakeRouter.push).toHaveBeenCalledWith({
      name: 'config-debugging-config',
      hash: '#field=syslog',
    })
  })

  it('caches results for 60 s — repeat call inside the window skips the fetch', async () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date(1_700_000_000_000))
    try {
      pinApi({
        'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
      })
      await ensureSettingsLoaded(baseDeps)
      expect(apiMock).toHaveBeenCalledTimes(6) /* one per page */
      apiMock.mockClear()
      vi.setSystemTime(new Date(1_700_000_000_000 + 30_000)) /* +30 s */
      await ensureSettingsLoaded(baseDeps)
      expect(apiMock).not.toHaveBeenCalled()
    } finally {
      vi.useRealTimers()
    }
  })

  it('re-fetches once the cache TTL has elapsed', async () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date(1_700_000_000_000))
    try {
      pinApi({
        'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
      })
      await ensureSettingsLoaded(baseDeps)
      apiMock.mockClear()
      vi.setSystemTime(new Date(1_700_000_000_000 + 61_000)) /* +61 s */
      await ensureSettingsLoaded(baseDeps)
      expect(apiMock).toHaveBeenCalledTimes(6)
    } finally {
      vi.useRealTimers()
    }
  })

  it('a single page erroring leaves the other five contributing', async () => {
    /* Simulates a build without one capability (e.g. satip_server
     * disabled). The page fetch rejects, the other five succeed,
     * and we still get their commands. */
    pinApi({
      'satips/config/load': new Error('module disabled'),
      'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
      'imagecache/config/load': fakeLoadResponse([{ id: 'enabled', caption: 'Enabled' }]),
    })
    await ensureSettingsLoaded(baseDeps)
    const cmds = getSettingsCommands().value
    expect(cmds.find((c) => c.id === 'settings:config-general-base.hostname')).toBeDefined()
    expect(cmds.find((c) => c.id === 'settings:config-general-image-cache.enabled')).toBeDefined()
    /* No satips entries — the load rejected. */
    expect(cmds.find((c) => c.id.startsWith('settings:config-general-satip-server.'))).toBeUndefined()
  })

  it('concurrent calls collapse onto a single in-flight fetch', async () => {
    pinApi({
      'config/load': fakeLoadResponse([{ id: 'hostname', caption: 'Server name' }]),
    })
    const a = ensureSettingsLoaded(baseDeps)
    const b = ensureSettingsLoaded(baseDeps)
    await Promise.all([a, b])
    /* Each endpoint fetched exactly once despite two callers. */
    expect(apiMock).toHaveBeenCalledTimes(6)
  })
})
