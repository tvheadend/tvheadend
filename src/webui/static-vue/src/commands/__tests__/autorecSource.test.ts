// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const apiMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiMock }))

import {
  __resetAutorecSourceForTests,
  ensureAutorecsLoaded,
  getAutorecCommands,
  type AutorecSourceDeps,
} from '../autorecSource'

const fakeEntityEditor = { open: vi.fn() }
const fakeRouter = { push: vi.fn().mockResolvedValue(undefined) }

function makeAccess(canDvr = true) {
  return { has: (k: string) => (k === 'dvr' ? canDvr : false) }
}

const fakeToast = {
  success: vi.fn(),
  error: vi.fn(),
  info: vi.fn(),
  warn: vi.fn(),
}

const fakeConfirm = {
  ask: vi.fn().mockResolvedValue(true),
}

const deps: AutorecSourceDeps = {
  entityEditor: fakeEntityEditor as unknown as AutorecSourceDeps['entityEditor'],
  router: fakeRouter as unknown as AutorecSourceDeps['router'],
  access: makeAccess() as unknown as AutorecSourceDeps['access'],
  toast: fakeToast,
  confirm: fakeConfirm,
}

describe('autorecSource', () => {
  beforeEach(() => {
    apiMock.mockReset()
    fakeEntityEditor.open.mockReset()
    fakeRouter.push.mockReset()
    fakeToast.success.mockReset()
    fakeToast.error.mockReset()
    fakeConfirm.ask.mockReset()
    fakeConfirm.ask.mockResolvedValue(true)
    __resetAutorecSourceForTests()
  })

  afterEach(() => {
    __resetAutorecSourceForTests()
  })

  it('fetches dvr/autorec/grid on first ensureAutorecsLoaded call', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    await ensureAutorecsLoaded(deps)
    expect(apiMock).toHaveBeenCalledWith(
      'dvr/autorec/grid',
      expect.objectContaining({ sort: 'name', dir: 'ASC' }),
    )
  })

  it('builds one Command per entry under the Autorecs section', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'a-1', name: 'Doc Martin Weekday', title: 'Doc Martin', enabled: true },
        { uuid: 'a-2', name: 'Sports News', title: 'News', enabled: true },
      ],
    })
    await ensureAutorecsLoaded(deps)
    const cmds = getAutorecCommands().value
    expect(cmds.length).toBe(2)
    expect(cmds[0].section).toBe('Autorecs')
    expect(cmds[0].id).toBe('autorec:a-1')
    expect(cmds[0].label).toBe('Doc Martin Weekday')
    expect(cmds[0].actionLabel).toBe('Edit')
  })

  it('falls back to `title` when `name` is empty', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a-1', name: '', title: 'Doc Martin', enabled: true }],
    })
    await ensureAutorecsLoaded(deps)
    expect(getAutorecCommands().value[0].label).toBe('Doc Martin')
  })

  it('falls back to "(Unnamed autorec)" when both name and title are missing', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a-1', enabled: true }],
    })
    await ensureAutorecsLoaded(deps)
    expect(getAutorecCommands().value[0].label).toBe('(Unnamed autorec)')
  })

  it('description includes the title matcher when distinct from the name', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'a-1',
          name: 'Weekday Evenings',
          title: 'Doc Martin',
          enabled: true,
          channelname: 'ITV',
        },
      ],
    })
    await ensureAutorecsLoaded(deps)
    const desc = getAutorecCommands().value[0].description ?? ''
    expect(desc).toContain('Doc Martin')
    expect(desc).toContain('ITV')
  })

  it('description carries a "disabled" badge when the autorec is off', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a-1', name: 'Foo', enabled: false }],
    })
    await ensureAutorecsLoaded(deps)
    expect(getAutorecCommands().value[0].description).toContain('disabled')
  })

  it('does NOT add a "disabled" badge when the autorec is enabled', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
    })
    await ensureAutorecsLoaded(deps)
    expect(getAutorecCommands().value[0].description).not.toContain('disabled')
  })

  it('primary action opens the entity editor with the autorec uuid', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
    })
    await ensureAutorecsLoaded(deps)
    getAutorecCommands().value[0].action()
    expect(fakeEntityEditor.open).toHaveBeenCalledWith('a-1')
  })

  describe('Toggle secondary action', () => {
    it('shows "Disable" when the autorec is currently enabled', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      expect(getAutorecCommands().value[0].secondaryAction?.label).toBe('Disable')
    })

    it('shows "Enable" when the autorec is currently disabled', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: false }],
      })
      await ensureAutorecsLoaded(deps)
      expect(getAutorecCommands().value[0].secondaryAction?.label).toBe('Enable')
    })

    it('toggle handler POSTs idnode/save with enabled flipped', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      apiMock.mockResolvedValueOnce({})
      await cmd.secondaryAction!.handler()
      expect(apiMock).toHaveBeenCalledWith('idnode/save', {
        node: JSON.stringify({ uuid: 'a-1', enabled: 0 }),
      })
      expect(fakeToast.success).toHaveBeenCalledWith(expect.stringContaining('Foo'))
    })

    it('toggle handler surfaces error toast on api failure', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      apiMock.mockRejectedValueOnce(new Error('storage busy'))
      await cmd.secondaryAction!.handler()
      expect(fakeToast.error).toHaveBeenCalledWith(
        'storage busy',
        expect.objectContaining({ summary: 'Could not disable autorec' }),
      )
    })
  })

  describe('Delete tertiary action', () => {
    it('Delete handler asks for confirmation first (danger severity)', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      apiMock.mockResolvedValueOnce({})
      await cmd.tertiaryAction!.handler()
      expect(fakeConfirm.ask).toHaveBeenCalledWith(
        expect.stringContaining('Foo'),
        expect.objectContaining({ severity: 'danger' }),
      )
    })

    it('posts idnode/delete with json-encoded uuid array on confirm', async () => {
      fakeConfirm.ask.mockResolvedValueOnce(true)
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      apiMock.mockResolvedValueOnce({})
      await cmd.tertiaryAction!.handler()
      expect(apiMock).toHaveBeenCalledWith('idnode/delete', {
        uuid: JSON.stringify(['a-1']),
      })
      expect(fakeToast.success).toHaveBeenCalledWith(expect.stringContaining('Foo'))
    })

    it('aborts cleanly when user cancels the confirm', async () => {
      fakeConfirm.ask.mockResolvedValueOnce(false)
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      await cmd.tertiaryAction!.handler()
      expect(apiMock).not.toHaveBeenCalled()
      expect(fakeToast.success).not.toHaveBeenCalled()
      expect(fakeToast.error).not.toHaveBeenCalled()
    })

    it('surfaces error toast on api failure (post-confirm)', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      const cmd = getAutorecCommands().value[0]
      apiMock.mockReset()
      apiMock.mockRejectedValueOnce(new Error('locked'))
      await cmd.tertiaryAction!.handler()
      expect(fakeToast.error).toHaveBeenCalledWith(
        'locked',
        expect.objectContaining({ summary: 'Could not delete autorec' }),
      )
    })
  })

  describe('caching', () => {
    it('does not refetch within the 60s cache window', async () => {
      apiMock.mockResolvedValue({
        entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }],
      })
      await ensureAutorecsLoaded(deps)
      await ensureAutorecsLoaded(deps)
      expect(apiMock).toHaveBeenCalledTimes(1)
    })

    it('coalesces concurrent in-flight calls into a single fetch', async () => {
      let resolveFetch: (v: { entries: unknown[] }) => void = () => {}
      apiMock.mockReturnValueOnce(
        new Promise((resolve) => {
          resolveFetch = resolve
        }),
      )
      const p1 = ensureAutorecsLoaded(deps)
      const p2 = ensureAutorecsLoaded(deps)
      resolveFetch({ entries: [{ uuid: 'a-1', name: 'Foo', enabled: true }] })
      await Promise.all([p1, p2])
      expect(apiMock).toHaveBeenCalledTimes(1)
    })

    it('tolerates empty entries / missing entries field', async () => {
      apiMock.mockResolvedValueOnce({})
      await ensureAutorecsLoaded(deps)
      expect(getAutorecCommands().value).toEqual([])
    })
  })
})
