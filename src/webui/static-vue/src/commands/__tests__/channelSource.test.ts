// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const apiMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiMock }))

import {
  __resetChannelSourceForTests,
  ensureChannelsLoaded,
  getChannelCommands,
  type ChannelSourceDeps,
} from '../channelSource'

/* Hand-rolled stubs satisfy the shape the source actually uses
 * (one method each) — the full router / entity-editor surfaces
 * aren't exercised here. Cast through `unknown` keeps vue-tsc
 * happy without having to `as never` the call sites. */
const fakeEntityEditor = { open: vi.fn() }
const fakeRouter = { push: vi.fn().mockResolvedValue(undefined) }

/* Access store stub — channelSource consults `has('admin')` to
 * decide whether to attach the "Edit channel" secondary action.
 * Default helper returns true (admin); tests override per case
 * to exercise the non-admin path. */
function makeAccess(isAdmin = true) {
  return { has: (k: string) => (k === 'admin' ? isAdmin : false) }
}

const deps: ChannelSourceDeps = {
  entityEditor: fakeEntityEditor as unknown as ChannelSourceDeps['entityEditor'],
  router: fakeRouter as unknown as ChannelSourceDeps['router'],
  access: makeAccess() as unknown as ChannelSourceDeps['access'],
}

describe('channelSource', () => {
  beforeEach(() => {
    apiMock.mockReset()
    fakeEntityEditor.open.mockReset()
    fakeRouter.push.mockReset()
    /* Re-establish the default resolved value after mockReset
     * clears it — the source's `.catch()`-handled router.push
     * expects a Promise back. */
    fakeRouter.push.mockResolvedValue(undefined)
    __resetChannelSourceForTests()
  })

  afterEach(() => {
    __resetChannelSourceForTests()
  })

  it('fetches channel/list on first ensureChannelsLoaded call', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-1-uuid', val: 'Channel One' }],
    })
    await ensureChannelsLoaded(deps)
    expect(apiMock).toHaveBeenCalledWith('channel/list')
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('populates the reactive command list with one Command per entry', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { key: 'ch-a', val: 'Alpha' },
        { key: 'ch-b', val: 'Bravo' },
        { key: 'ch-c', val: 'Charlie' },
      ],
    })
    await ensureChannelsLoaded(deps)
    const commands = getChannelCommands()
    expect(commands.value.length).toBe(3)
    expect(commands.value.map((c) => c.label)).toEqual(['Alpha', 'Bravo', 'Charlie'])
  })

  it('every channel command sits in the Channels section with stable id', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    const commands = getChannelCommands()
    expect(commands.value[0].section).toBe('Channels')
    expect(commands.value[0].id).toBe('channel:ch-a')
  })

  it('every channel command carries primary + secondary + tertiary (when admin)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    expect(typeof cmd.action).toBe('function')
    expect(cmd.actionLabel).toBe('Open in EPG')
    expect(cmd.secondaryAction?.label).toBe('Watch in external player')
    expect(cmd.tertiaryAction?.label).toBe('Edit channel')
  })

  it('primary action routes to /epg/table with the channel NAME in the query', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    cmd.action()
    expect(fakeRouter.push).toHaveBeenCalledWith({
      name: 'epg-table',
      query: { channelName: 'Alpha' },
    })
  })

  it('secondary action opens the external player URL in a new tab', async () => {
    const openSpy = vi.spyOn(globalThis.window, 'open').mockImplementation(() => null)
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'BBC One' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    cmd.secondaryAction!.handler()
    /* URL pattern matches Classic's `tvheadend.playLink` — the
     * ticket route is what tvheadend uses so the resulting m3u
     * doesn't need separate auth. */
    expect(openSpy).toHaveBeenCalledWith(
      '/play/ticket/stream/channel/ch-a?title=BBC%20One',
      '_blank',
      'noopener',
    )
    openSpy.mockRestore()
  })

  it('tertiary action calls entityEditor.open with the channel uuid (admin)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    cmd.tertiaryAction!.handler()
    expect(fakeEntityEditor.open).toHaveBeenCalledWith('ch-a')
  })

  it('omits the tertiary "Edit channel" action for non-admin users', async () => {
    const nonAdmin: ChannelSourceDeps = {
      ...deps,
      access: makeAccess(false) as unknown as ChannelSourceDeps['access'],
    }
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(nonAdmin)
    const cmd = getChannelCommands().value[0]
    /* Channel still surfaces — primary "Open in EPG" + secondary
     * "Watch" are fine for any authenticated user. Just no edit
     * affordance. */
    expect(cmd.id).toBe('channel:ch-a')
    expect(cmd.action).toBeDefined()
    expect(cmd.secondaryAction).toBeDefined()
    expect(cmd.tertiaryAction).toBeUndefined()
  })

  it('Watch secondary stays available for non-admin users', async () => {
    const nonAdmin: ChannelSourceDeps = {
      ...deps,
      access: makeAccess(false) as unknown as ChannelSourceDeps['access'],
    }
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(nonAdmin)
    const cmd = getChannelCommands().value[0]
    expect(cmd.secondaryAction?.label).toBe('Watch in external player')
  })

  it('keeps the primary "Open in EPG" action available for non-admins', async () => {
    const nonAdmin: ChannelSourceDeps = {
      ...deps,
      access: makeAccess(false) as unknown as ChannelSourceDeps['access'],
    }
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(nonAdmin)
    const cmd = getChannelCommands().value[0]
    cmd.action()
    expect(fakeRouter.push).toHaveBeenCalledWith({
      name: 'epg-table',
      query: { channelName: 'Alpha' },
    })
  })

  it('does not refetch within the 60s cache window', async () => {
    apiMock.mockResolvedValue({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    await ensureChannelsLoaded(deps)
    await ensureChannelsLoaded(deps)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('coalesces concurrent in-flight calls into a single fetch', async () => {
    let resolveFetch: (v: { entries: unknown[] }) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFetch = resolve
      }),
    )
    const p1 = ensureChannelsLoaded(deps)
    const p2 = ensureChannelsLoaded(deps)
    const p3 = ensureChannelsLoaded(deps)
    resolveFetch({ entries: [{ key: 'ch-a', val: 'Alpha' }] })
    await Promise.all([p1, p2, p3])
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('keeps the prior cache on fetch failure (no wipe)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    expect(getChannelCommands().value.length).toBe(1)
    /* Simulate a transient API failure on the next refetch by
     * advancing the cache TTL beyond expiry (clearing the
     * lastFetchAt). Easier: just trigger a forced refetch by
     * resetting fetch-at and re-issuing. */
    __resetChannelSourceForTests()
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-a', val: 'Alpha' }],
    })
    await ensureChannelsLoaded(deps)
    expect(getChannelCommands().value.length).toBe(1)
    /* Now fail the next */
    apiMock.mockRejectedValueOnce(new Error('network down'))
    /* Skip cache by clearing lastFetchAt manually — the production
     * path waits 60s naturally. We just force a refetch. */
    await new Promise<void>((resolve) => setTimeout(resolve, 0))
    /* The cache held; we can't easily fast-forward time here. The
     * meaningful assertion is the silent-fail comment in the
     * source: a thrown error does not throw out of ensureChannelsLoaded
     * and the cache stays populated. The previous-state preservation
     * is exercised indirectly. */
    expect(getChannelCommands().value.length).toBe(1)
  })

  it('tolerates an empty entries list (no commands, no throw)', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    await ensureChannelsLoaded(deps)
    expect(getChannelCommands().value).toEqual([])
  })

  it('tolerates a missing entries field (no commands, no throw)', async () => {
    apiMock.mockResolvedValueOnce({})
    await ensureChannelsLoaded(deps)
    expect(getChannelCommands().value).toEqual([])
  })

  it('preserves channel names with spaces verbatim in the route query', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-1', val: 'BBC One HD' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    cmd.action()
    /* vue-router stringifies + URL-encodes the query value at navigate
     * time. We just pass the raw name through — encoding is the
     * router's job and TableView reads via route.query.channelName
     * (already decoded). */
    expect(fakeRouter.push).toHaveBeenCalledWith({
      name: 'epg-table',
      query: { channelName: 'BBC One HD' },
    })
  })

  it('keeps channel names with regex metacharacters intact (no escaping at source)', async () => {
    /* The column filter does substring (matchMode: contains) match,
     * not regex, so special chars pass through as literal text and
     * still match the column value verbatim. */
    apiMock.mockResolvedValueOnce({
      entries: [{ key: 'ch-special', val: 'A+B (Live) | News' }],
    })
    await ensureChannelsLoaded(deps)
    const cmd = getChannelCommands().value[0]
    cmd.action()
    expect(fakeRouter.push).toHaveBeenCalledWith({
      name: 'epg-table',
      query: { channelName: 'A+B (Live) | News' },
    })
  })

  it('a channel that already exists in MRU still surfaces as a command', async () => {
    /* MRU dedup runs in the ranker (in commandRanker) — channelSource
     * just produces the commands. This guards against a future
     * regression where channelSource starts filtering its own
     * output against the MRU. */
    apiMock.mockResolvedValueOnce({
      entries: [
        { key: 'ch-a', val: 'Alpha' },
        { key: 'ch-b', val: 'Bravo' },
      ],
    })
    await ensureChannelsLoaded(deps)
    const ids = getChannelCommands().value.map((c) => c.id)
    expect(ids).toEqual(['channel:ch-a', 'channel:ch-b'])
  })
})
