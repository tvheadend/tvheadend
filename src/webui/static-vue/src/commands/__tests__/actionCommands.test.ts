// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for the static-action command builder. The handlers
 * themselves are mocked here — we assert the command shape
 * (id, section, label, requires) and that each command.action()
 * invokes the right handler with the right dependencies. The
 * handlers' own behaviour (apiCall plumbing, toast wording) is
 * covered by HomeView.test.ts, which calls them through the
 * same surface.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const handlerSpies = vi.hoisted(() => ({
  scanAllNetworks: vi.fn(),
  refreshEpg: vi.fn(),
  startSetupWizard: vi.fn(),
  openLogout: vi.fn(),
  rerunInternalEpg: vi.fn(),
  triggerOtaEpg: vi.fn(),
  cleanImageCache: vi.fn(),
  refetchImages: vi.fn(),
  discoverSatipServers: vi.fn(),
  openChannelsReorganize: vi.fn(),
  openChannelsMapper: vi.fn(),
  removeUnseenServices: vi.fn(),
}))

vi.mock('../actionHandlers', () => handlerSpies)

import { buildActionCommands } from '../commandRegistry'

const fakeToast = {} as never
const fakeWizard = {} as never
const fakeRouter = {} as never
const fakeConfirm = {} as never

/* Access store stub — only `data.username` is consulted by
 * buildActionCommands (the Logout gate). The store's other fields
 * aren't touched in this file. Pass `null` to simulate no session
 * (`--noacl` / anonymous) — using `undefined` would collide with
 * the default-parameter machinery. */
function makeAccess(username: string | null = 'tvh-admin') {
  return { data: username === null ? null : { username } } as never
}

const deps = {
  toast: fakeToast,
  wizard: fakeWizard,
  router: fakeRouter,
  access: makeAccess(),
  confirm: fakeConfirm,
}

describe('buildActionCommands', () => {
  beforeEach(() => {
    for (const fn of Object.values(handlerSpies)) fn.mockReset()
  })

  afterEach(() => {
    for (const fn of Object.values(handlerSpies)) fn.mockReset()
  })

  it('returns commands for every static-action entry', () => {
    const commands = buildActionCommands(deps)
    const ids = commands.map((c) => c.id)
    expect(ids).toEqual([
      'action:scan-channels',
      'action:refresh-epg',
      'action:start-wizard',
      'action:epg-rerun-internal',
      'action:epg-trigger-ota',
      'action:imagecache-clean',
      'action:imagecache-refetch',
      'action:satip-discover',
      'action:channels-reorganize',
      'action:channels-map-services',
      'action:services-remove-unseen-pat',
      'action:services-remove-unseen-all',
      'action:logout',
    ])
  })

  it('every action command sits in the Actions section', () => {
    const commands = buildActionCommands(deps)
    expect(commands.every((c) => c.section === 'Actions')).toBe(true)
  })

  it('every action command has an icon and human-readable label', () => {
    const commands = buildActionCommands(deps)
    for (const c of commands) {
      expect(c.icon).toBeDefined()
      expect(c.label.length).toBeGreaterThan(0)
    }
  })

  it("admin-only actions carry requires: 'admin'", () => {
    const commands = buildActionCommands(deps)
    const byId = Object.fromEntries(commands.map((c) => [c.id, c]))
    expect(byId['action:scan-channels'].requires).toBe('admin')
    expect(byId['action:refresh-epg'].requires).toBe('admin')
    expect(byId['action:start-wizard'].requires).toBe('admin')
    expect(byId['action:epg-rerun-internal'].requires).toBe('admin')
    expect(byId['action:epg-trigger-ota'].requires).toBe('admin')
    expect(byId['action:imagecache-clean'].requires).toBe('admin')
    expect(byId['action:imagecache-refetch'].requires).toBe('admin')
    expect(byId['action:satip-discover'].requires).toBe('admin')
    expect(byId['action:channels-reorganize'].requires).toBe('admin')
    expect(byId['action:channels-map-services'].requires).toBe('admin')
    expect(byId['action:services-remove-unseen-pat'].requires).toBe('admin')
    expect(byId['action:services-remove-unseen-all'].requires).toBe('admin')
  })

  it("logout is NOT gated on a permission (always reachable when signed in)", () => {
    const commands = buildActionCommands(deps)
    const logout = commands.find((c) => c.id === 'action:logout')!
    expect(logout.requires).toBeUndefined()
  })

  it('logout is omitted entirely when there is no username (--noacl / anonymous)', () => {
    const noSession = { ...deps, access: makeAccess(null) }
    const commands = buildActionCommands(noSession)
    expect(commands.find((c) => c.id === 'action:logout')).toBeUndefined()
  })

  it('logout is omitted when access.data is present but username is empty', () => {
    const emptyUsername = { ...deps, access: makeAccess('') }
    const commands = buildActionCommands(emptyUsername)
    expect(commands.find((c) => c.id === 'action:logout')).toBeUndefined()
  })

  it('scan-channels.action invokes scanAllNetworks with the toast dependency', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:scan-channels')!.action()
    expect(handlerSpies.scanAllNetworks).toHaveBeenCalledWith(fakeToast)
  })

  it('refresh-epg.action invokes refreshEpg with the toast dependency', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:refresh-epg')!.action()
    expect(handlerSpies.refreshEpg).toHaveBeenCalledWith(fakeToast)
  })

  it('start-wizard.action invokes startSetupWizard with wizard + router + toast', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:start-wizard')!.action()
    expect(handlerSpies.startSetupWizard).toHaveBeenCalledWith(
      fakeWizard,
      fakeRouter,
      fakeToast,
    )
  })

  it('logout.action invokes openLogout', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:logout')!.action()
    expect(handlerSpies.openLogout).toHaveBeenCalled()
  })

  it('epg-rerun-internal.action invokes rerunInternalEpg with the toast', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:epg-rerun-internal')!.action()
    expect(handlerSpies.rerunInternalEpg).toHaveBeenCalledWith(fakeToast)
  })

  it('epg-trigger-ota.action invokes triggerOtaEpg with the toast', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:epg-trigger-ota')!.action()
    expect(handlerSpies.triggerOtaEpg).toHaveBeenCalledWith(fakeToast)
  })

  it('imagecache-clean.action invokes cleanImageCache with toast + confirm', () => {
    /* Threaded confirm dep — the destructive action shares its
     * confirm-then-clean flow with the Image Cache page button. */
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:imagecache-clean')!.action()
    expect(handlerSpies.cleanImageCache).toHaveBeenCalledWith({
      toast: fakeToast,
      confirm: fakeConfirm,
    })
  })

  it('imagecache-refetch.action invokes refetchImages with the toast', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:imagecache-refetch')!.action()
    expect(handlerSpies.refetchImages).toHaveBeenCalledWith(fakeToast)
  })

  it('satip-discover.action invokes discoverSatipServers with the toast', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:satip-discover')!.action()
    expect(handlerSpies.discoverSatipServers).toHaveBeenCalledWith(fakeToast)
  })

  it('channels-reorganize.action invokes openChannelsReorganize with the router', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:channels-reorganize')!.action()
    expect(handlerSpies.openChannelsReorganize).toHaveBeenCalledWith(fakeRouter)
  })

  it('channels-map-services.action invokes openChannelsMapper with the router', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:channels-map-services')!.action()
    expect(handlerSpies.openChannelsMapper).toHaveBeenCalledWith(fakeRouter)
  })

  it('services-remove-unseen-pat.action invokes removeUnseenServices with scope "pat"', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:services-remove-unseen-pat')!.action()
    expect(handlerSpies.removeUnseenServices).toHaveBeenCalledWith(
      { toast: fakeToast, confirm: fakeConfirm },
      'pat',
    )
  })

  it('services-remove-unseen-all.action invokes removeUnseenServices with scope "all"', () => {
    const commands = buildActionCommands(deps)
    commands.find((c) => c.id === 'action:services-remove-unseen-all')!.action()
    expect(handlerSpies.removeUnseenServices).toHaveBeenCalledWith(
      { toast: fakeToast, confirm: fakeConfirm },
      'all',
    )
  })

  it('each command exposes keywords for synonym matching', () => {
    const commands = buildActionCommands(deps)
    /* Empty arrays would technically pass — assert that at least
     * one keyword is present on each, since their value is
     * specifically to surface fuzzy matches the label alone won't. */
    for (const c of commands) {
      expect(c.keywords?.length ?? 0).toBeGreaterThan(0)
    }
  })
})
