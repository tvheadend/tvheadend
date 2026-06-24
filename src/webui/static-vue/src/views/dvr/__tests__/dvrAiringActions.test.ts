// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrAiringActions unit tests — the Record / Switch DVR write helpers
 * behind the related / alternative dialog's per-row actions. apiCall
 * is mocked so the create-then-cancel sequencing is verified without a
 * server.
 */
import { afterEach, describe, expect, it, vi } from 'vitest'
import { recordAiring, switchToAiring } from '../dvrAiringActions'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

afterEach(() => {
  apiMock.mockReset()
})

describe('recordAiring', () => {
  it('posts create_by_event with the event id and the default profile', async () => {
    apiMock.mockResolvedValueOnce({})
    await recordAiring(4242)
    expect(apiMock).toHaveBeenCalledWith('dvr/entry/create_by_event', {
      event_id: 4242,
      config_uuid: '',
    })
  })

  it('propagates a create failure', async () => {
    apiMock.mockRejectedValueOnce(new Error('nope'))
    await expect(recordAiring(1)).rejects.toThrow('nope')
  })
})

describe('switchToAiring', () => {
  it('records the airing, then cancels the original entry', async () => {
    apiMock.mockResolvedValue({})
    const outcome = await switchToAiring(4242, 'orig-uuid')
    expect(outcome).toBe('ok')
    expect(apiMock).toHaveBeenNthCalledWith(1, 'dvr/entry/create_by_event', {
      event_id: 4242,
      config_uuid: '',
    })
    expect(apiMock).toHaveBeenNthCalledWith(2, 'dvr/entry/cancel', {
      uuid: JSON.stringify(['orig-uuid']),
    })
  })

  it('rejects without cancelling when the create fails', async () => {
    apiMock.mockRejectedValueOnce(new Error('create failed'))
    await expect(switchToAiring(1, 'orig-uuid')).rejects.toThrow('create failed')
    /* Create only — the original entry is left untouched. */
    expect(apiMock).toHaveBeenCalledTimes(1)
  })

  it('returns cancel-failed when the create succeeds but the cancel fails', async () => {
    apiMock.mockResolvedValueOnce({}) // create OK
    apiMock.mockRejectedValueOnce(new Error('cancel failed')) // cancel fails
    const outcome = await switchToAiring(1, 'orig-uuid')
    expect(outcome).toBe('cancel-failed')
    expect(apiMock).toHaveBeenCalledTimes(2)
  })
})
