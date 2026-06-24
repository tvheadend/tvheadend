// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const apiMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiMock }))

import {
  __resetEpgEventSourceForTests,
  getEpgEventCommands,
  updateEpgQuery,
  type EpgEventSourceDeps,
} from '../epgEventSource'

const fakeRouter = { push: vi.fn().mockResolvedValue(undefined) }

function makeAccess(canRecord = true) {
  return { has: (k: string) => (k === 'dvr' ? canRecord : false) }
}

const fakeToast = {
  success: vi.fn(),
  error: vi.fn(),
  info: vi.fn(),
  warn: vi.fn(),
}

const deps: EpgEventSourceDeps = {
  router: fakeRouter as unknown as EpgEventSourceDeps['router'],
  access: makeAccess() as unknown as EpgEventSourceDeps['access'],
  toast: fakeToast,
}

describe('epgEventSource', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    apiMock.mockReset()
    fakeRouter.push.mockReset()
    /* Re-establish the resolved value after mockReset — the
     * source's `.catch()`-handled router.push expects a Promise back. */
    fakeRouter.push.mockResolvedValue(undefined)
    fakeToast.success.mockReset()
    fakeToast.error.mockReset()
    __resetEpgEventSourceForTests()
  })

  afterEach(() => {
    vi.useRealTimers()
    __resetEpgEventSourceForTests()
  })

  describe('debounce + min-length gate', () => {
    it('does not fetch when the query is below the 3-character minimum', () => {
      updateEpgQuery('ab', deps)
      vi.advanceTimersByTime(1000)
      expect(apiMock).not.toHaveBeenCalled()
      expect(getEpgEventCommands().value).toEqual([])
    })

    it('fires the fetch only after the 300ms debounce', async () => {
      apiMock.mockResolvedValue({ entries: [] })
      updateEpgQuery('star', deps)
      /* Just before the debounce — no fetch yet. */
      vi.advanceTimersByTime(250)
      expect(apiMock).not.toHaveBeenCalled()
      /* Cross the debounce — fetch fires. */
      vi.advanceTimersByTime(60)
      expect(apiMock).toHaveBeenCalledTimes(1)
    })

    it('coalesces fast typing into one fetch', () => {
      apiMock.mockResolvedValue({ entries: [] })
      updateEpgQuery('s', deps)
      updateEpgQuery('st', deps)
      updateEpgQuery('sta', deps)
      updateEpgQuery('star', deps)
      vi.advanceTimersByTime(400)
      expect(apiMock).toHaveBeenCalledTimes(1)
      expect(apiMock).toHaveBeenCalledWith('epg/events/grid', expect.objectContaining({
        title: 'star',
      }))
    })

    it('clears results synchronously when query drops below min length', async () => {
      apiMock.mockResolvedValue({
        entries: [
          { eventId: 1, title: 'Star Trek', channelName: 'BBC One', start: 1700000000 },
        ],
      })
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value.length).toBe(1)
      updateEpgQuery('s', deps)
      /* Synchronous clear — no need to await. */
      expect(getEpgEventCommands().value).toEqual([])
    })

    it('clears results when query is the empty string', async () => {
      apiMock.mockResolvedValue({
        entries: [
          { eventId: 1, title: 'Star Trek', channelName: 'BBC One', start: 1700000000 },
        ],
      })
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value.length).toBe(1)
      updateEpgQuery('', deps)
      expect(getEpgEventCommands().value).toEqual([])
    })
  })

  describe('result shape', () => {
    it('passes title, limit, sort, and dir to the server', async () => {
      apiMock.mockResolvedValue({ entries: [] })
      updateEpgQuery('star trek', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(apiMock).toHaveBeenCalledWith('epg/events/grid', {
        title: 'star trek',
        /* Tight cap so the EPG section doesn't dominate the
         * palette. Users see the rest via Enter → Table. */
        limit: 5,
        sort: 'start',
        dir: 'ASC',
      })
    })

    it('builds one Command per entry under the EPG section', async () => {
      apiMock.mockResolvedValue({
        entries: [
          { eventId: 1, title: 'Star Trek', channelName: 'BBC One', start: 1700000000 },
          { eventId: 2, title: 'Star Wars', channelName: 'ITV', start: 1700100000 },
        ],
      })
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmds = getEpgEventCommands().value
      expect(cmds.length).toBe(2)
      expect(cmds[0].id).toBe('epg-event:1')
      expect(cmds[0].label).toBe('Star Trek')
      expect(cmds[0].section).toBe('EPG')
      expect(cmds[0].actionLabel).toBe('Open in EPG')
    })

    it('description carries channel name + formatted start time', async () => {
      apiMock.mockResolvedValue({
        entries: [
          { eventId: 1, title: 'Star Trek', channelName: 'BBC One', start: 1700000000 },
        ],
      })
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      expect(cmd.description).toContain('BBC One')
      /* Time format is locale-dependent; just verify the separator
       * + that SOMETHING after the channel name was rendered. */
      expect(cmd.description).toContain(' · ')
    })

    it('tolerates missing channel name (description is time-only)', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 1, title: 'Mystery', start: 1700000000 }],
      })
      updateEpgQuery('mystery', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      const desc = cmd.description ?? ''
      expect(desc).not.toContain(' · ')
      expect(desc.length).toBeGreaterThan(0)
    })

    it('uses "(Untitled)" when title is missing', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 1, channelName: 'BBC One', start: 1700000000 }],
      })
      updateEpgQuery('xxx', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value[0].label).toBe('(Untitled)')
    })

    it('action navigates to /epg/table with the title in the query', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 1, title: 'Star Trek', channelName: 'BBC One', start: 1700000000 }],
      })
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      cmd.action()
      expect(fakeRouter.push).toHaveBeenCalledWith({
        name: 'epg-table',
        query: { title: 'Star Trek' },
      })
    })

    it('tolerates empty entries (no commands, no throw)', async () => {
      apiMock.mockResolvedValue({ entries: [] })
      updateEpgQuery('zzznoresults', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value).toEqual([])
    })

    it('tolerates a missing entries field', async () => {
      apiMock.mockResolvedValue({})
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value).toEqual([])
    })

    it('swallows fetch errors and shows no results (no throw)', async () => {
      apiMock.mockRejectedValue(new Error('network down'))
      updateEpgQuery('star', deps)
      await vi.advanceTimersByTimeAsync(400)
      expect(getEpgEventCommands().value).toEqual([])
    })
  })

  describe('cancellation', () => {
    it('drops out-of-order responses (older query overwrites newer result)', async () => {
      /* First fetch resolves SLOWLY; second fetch fires while
       * first is still pending and resolves FAST with newer
       * results. The older one's resolution must not overwrite. */
      let resolveFirst: (v: { entries: unknown[] }) => void = () => {}
      const firstPromise = new Promise((r) => {
        resolveFirst = r
      })
      apiMock.mockReturnValueOnce(firstPromise)
      apiMock.mockResolvedValueOnce({
        entries: [{ eventId: 99, title: 'Newer Result', channelName: 'X', start: 1 }],
      })
      updateEpgQuery('starwars', deps)
      await vi.advanceTimersByTimeAsync(400)
      /* Now type a new query; the second fetch fires and
       * resolves before we resolve the first. */
      updateEpgQuery('startrek', deps)
      await vi.advanceTimersByTimeAsync(400)
      /* Newer result is now in. Resolve the older one with
       * stale data — should be ignored. */
      resolveFirst({
        entries: [{ eventId: 1, title: 'Stale Result', channelName: 'Y', start: 1 }],
      })
      await Promise.resolve()
      const labels = getEpgEventCommands().value.map((c) => c.label)
      expect(labels).toContain('Newer Result')
      expect(labels).not.toContain('Stale Result')
    })
  })

  describe('Record secondary action', () => {
    it('exposes a Record secondary action for users with dvr permission', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      expect(cmd.secondaryAction?.label).toBe('Record')
    })

    it('omits the Record secondary action when user lacks dvr permission', async () => {
      const noDvr: EpgEventSourceDeps = {
        ...deps,
        access: makeAccess(false) as unknown as EpgEventSourceDeps['access'],
      }
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', noDvr)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      expect(cmd.secondaryAction).toBeUndefined()
      /* Primary still works — non-DVR users can still navigate. */
      expect(cmd.action).toBeDefined()
    })

    it('Record handler posts dvr/entry/create_by_event with event_id and toasts success', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      /* Reset the apiMock so we can assert on JUST the Record
       * call (not the prior search call). */
      apiMock.mockReset()
      apiMock.mockResolvedValueOnce({ uuid: 'new-recording-uuid' })
      await cmd.secondaryAction!.handler()
      expect(apiMock).toHaveBeenCalledWith('dvr/entry/create_by_event', {
        event_id: 42,
        /* `config_uuid: ''` makes the server fall back to the
         * user's default DVR config; without this param the
         * server returns 400. */
        config_uuid: '',
      })
      expect(fakeToast.success).toHaveBeenCalledWith(
        expect.stringContaining('Doc Martin'),
      )
    })

    it('Record handler surfaces error toast on api failure', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      apiMock.mockReset()
      apiMock.mockRejectedValueOnce(new Error('disk full'))
      await cmd.secondaryAction!.handler()
      expect(fakeToast.error).toHaveBeenCalledWith(
        'disk full',
        expect.objectContaining({ summary: 'Could not schedule recording' }),
      )
    })
  })

  describe('Record-series tertiary action', () => {
    it('exposes a Record-series tertiary action for users with dvr permission', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      expect(cmd.tertiaryAction?.label).toBe('Record series')
    })

    it('omits the Record-series tertiary action when user lacks dvr permission', async () => {
      const noDvr: EpgEventSourceDeps = {
        ...deps,
        access: makeAccess(false) as unknown as EpgEventSourceDeps['access'],
      }
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', noDvr)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      expect(cmd.tertiaryAction).toBeUndefined()
      /* Secondary (Record) is also dvr-gated and must drop too. */
      expect(cmd.secondaryAction).toBeUndefined()
      /* Primary still works — non-DVR users can still navigate. */
      expect(cmd.action).toBeDefined()
    })

    it('tertiary handler posts dvr/autorec/create_by_series with event_id + config_uuid="" and toasts', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      apiMock.mockReset()
      apiMock.mockResolvedValueOnce({ uuid: 'new-autorec-uuid' })
      await cmd.tertiaryAction!.handler()
      expect(apiMock).toHaveBeenCalledWith('dvr/autorec/create_by_series', {
        event_id: 42,
        /* Same shape as create_by_event — server requires both
         * fields per api_dvr_entry_create_from_single. */
        config_uuid: '',
      })
      expect(fakeToast.success).toHaveBeenCalledWith(
        expect.stringContaining('Doc Martin'),
      )
    })

    it('tertiary handler surfaces error toast on api failure', async () => {
      apiMock.mockResolvedValue({
        entries: [{ eventId: 42, title: 'Doc Martin', channelName: 'ITV', start: 1 }],
      })
      updateEpgQuery('doc', deps)
      await vi.advanceTimersByTimeAsync(400)
      const cmd = getEpgEventCommands().value[0]
      apiMock.mockReset()
      apiMock.mockRejectedValueOnce(new Error('storage gone'))
      await cmd.tertiaryAction!.handler()
      expect(fakeToast.error).toHaveBeenCalledWith(
        'storage gone',
        expect.objectContaining({ summary: 'Could not set series recording' }),
      )
    })
  })
})
