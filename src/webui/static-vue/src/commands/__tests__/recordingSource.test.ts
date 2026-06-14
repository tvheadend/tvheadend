// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const apiMock = vi.hoisted(() => vi.fn())
vi.mock('@/api/client', () => ({ apiCall: apiMock }))

import {
  __resetRecordingSourceForTests,
  ensureRecordingsLoaded,
  getRecordingCommands,
  type RecordingSourceDeps,
} from '../recordingSource'

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

/* Confirm stub. Default: accept (returns true). Individual tests
 * override to reject (false) to verify the "user said no" branch. */
const fakeConfirm = {
  ask: vi.fn().mockResolvedValue(true),
}

const deps: RecordingSourceDeps = {
  entityEditor: fakeEntityEditor as unknown as RecordingSourceDeps['entityEditor'],
  router: fakeRouter as unknown as RecordingSourceDeps['router'],
  access: makeAccess() as unknown as RecordingSourceDeps['access'],
  toast: fakeToast,
  confirm: fakeConfirm,
}

describe('recordingSource', () => {
  beforeEach(() => {
    apiMock.mockReset()
    fakeEntityEditor.open.mockReset()
    fakeRouter.push.mockReset()
    fakeToast.success.mockReset()
    fakeToast.error.mockReset()
    fakeConfirm.ask.mockReset()
    /* Default: user accepts the confirm. Tests that need the
     * reject branch override this. */
    fakeConfirm.ask.mockResolvedValue(true)
    __resetRecordingSourceForTests()
  })

  afterEach(() => {
    __resetRecordingSourceForTests()
  })

  it('fetches dvr/entry/grid_finished on first ensureRecordingsLoaded call', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    await ensureRecordingsLoaded(deps)
    expect(apiMock).toHaveBeenCalledWith(
      'dvr/entry/grid_finished',
      expect.objectContaining({ sort: 'start_real', dir: 'DESC' }),
    )
  })

  it('builds one Command per entry under the Recordings section', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { uuid: 'r-a', disp_title: 'Show A', channelname: 'BBC One', start_real: 1700000000 },
        { uuid: 'r-b', disp_title: 'Show B', channelname: 'ITV', start_real: 1700100000 },
      ],
    })
    await ensureRecordingsLoaded(deps)
    const cmds = getRecordingCommands().value
    expect(cmds.length).toBe(2)
    expect(cmds[0].section).toBe('Recordings')
    expect(cmds[0].id).toBe('recording:r-a')
    expect(cmds[0].label).toBe('Show A')
    expect(cmds[0].actionLabel).toBe('Show details')
  })

  it('appends disp_extratext to the label when present', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'r-a',
          disp_title: 'Show A',
          disp_extratext: 'Episode 3',
          channelname: 'BBC One',
          start_real: 1,
        },
      ],
    })
    await ensureRecordingsLoaded(deps)
    expect(getRecordingCommands().value[0].label).toBe('Show A — Episode 3')
  })

  it('primary action opens the entity editor with the entry uuid', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    getRecordingCommands().value[0].action()
    expect(fakeEntityEditor.open).toHaveBeenCalledWith('r-a')
  })

  it('secondary action opens the external player URL in a new tab', async () => {
    const openSpy = vi.spyOn(globalThis.window, 'open').mockImplementation(() => null)
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Doc Martin' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    cmd.secondaryAction!.handler()
    expect(openSpy).toHaveBeenCalledWith(
      '/play/ticket/dvrfile/r-a?title=Doc%20Martin',
      '_blank',
      'noopener',
    )
    openSpy.mockRestore()
  })

  it('tertiary "Delete recording" action present for users with dvr permission', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    expect(cmd.tertiaryAction?.label).toBe('Delete recording')
  })

  it('omits the tertiary Delete action for users without dvr permission', async () => {
    const noDvr: RecordingSourceDeps = {
      ...deps,
      access: makeAccess(false) as unknown as RecordingSourceDeps['access'],
    }
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(noDvr)
    const cmd = getRecordingCommands().value[0]
    expect(cmd.tertiaryAction).toBeUndefined()
    /* Primary + secondary stay. */
    expect(cmd.action).toBeDefined()
    expect(cmd.secondaryAction).toBeDefined()
  })

  it('Delete handler asks for confirmation first (danger severity)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    apiMock.mockReset()
    apiMock.mockResolvedValueOnce({})
    await cmd.tertiaryAction!.handler()
    expect(fakeConfirm.ask).toHaveBeenCalledWith(
      expect.stringContaining('Show A'),
      expect.objectContaining({ severity: 'danger' }),
    )
  })

  it('Delete handler posts remove + toasts success when user confirms', async () => {
    fakeConfirm.ask.mockResolvedValueOnce(true)
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    apiMock.mockReset()
    apiMock.mockResolvedValueOnce({})
    await cmd.tertiaryAction!.handler()
    expect(apiMock).toHaveBeenCalledWith('dvr/entry/remove', {
      uuid: JSON.stringify(['r-a']),
    })
    expect(fakeToast.success).toHaveBeenCalledWith(expect.stringContaining('Show A'))
  })

  it('Delete handler aborts cleanly when user cancels the confirm', async () => {
    fakeConfirm.ask.mockResolvedValueOnce(false)
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    apiMock.mockReset()
    await cmd.tertiaryAction!.handler()
    /* No API call, no toast — silent abort. */
    expect(apiMock).not.toHaveBeenCalled()
    expect(fakeToast.success).not.toHaveBeenCalled()
    expect(fakeToast.error).not.toHaveBeenCalled()
  })

  it('Delete handler surfaces error toast on api failure (post-confirm)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    const cmd = getRecordingCommands().value[0]
    apiMock.mockReset()
    apiMock.mockRejectedValueOnce(new Error('storage busy'))
    await cmd.tertiaryAction!.handler()
    expect(fakeToast.error).toHaveBeenCalledWith(
      'storage busy',
      expect.objectContaining({ summary: 'Could not delete recording' }),
    )
  })

  it('does not refetch within the 60s cache window', async () => {
    apiMock.mockResolvedValue({
      entries: [{ uuid: 'r-a', disp_title: 'Show A' }],
    })
    await ensureRecordingsLoaded(deps)
    await ensureRecordingsLoaded(deps)
    await ensureRecordingsLoaded(deps)
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('coalesces concurrent in-flight calls into a single fetch', async () => {
    let resolveFetch: (v: { entries: unknown[] }) => void = () => {}
    apiMock.mockReturnValueOnce(
      new Promise((resolve) => {
        resolveFetch = resolve
      }),
    )
    const p1 = ensureRecordingsLoaded(deps)
    const p2 = ensureRecordingsLoaded(deps)
    resolveFetch({ entries: [{ uuid: 'r-a', disp_title: 'Show A' }] })
    await Promise.all([p1, p2])
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('tolerates an empty entries list', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    await ensureRecordingsLoaded(deps)
    expect(getRecordingCommands().value).toEqual([])
  })

  it('tolerates a missing entries field', async () => {
    apiMock.mockResolvedValueOnce({})
    await ensureRecordingsLoaded(deps)
    expect(getRecordingCommands().value).toEqual([])
  })
})
