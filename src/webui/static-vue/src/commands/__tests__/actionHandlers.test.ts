// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * openChannelsMapper: the query the Channels page reads to open
 * the Service Mapper from the command palette.
 */
import { describe, expect, it, vi } from 'vitest'
import type { Router } from 'vue-router'
import { openChannelsMapper } from '../actionHandlers'

vi.mock('@/api/client', () => ({ apiCall: vi.fn() }))

function fakeRouter() {
  const push = vi.fn(() => Promise.resolve())
  return { router: { push } as unknown as Router, push }
}

describe('openChannelsMapper', () => {
  it('opens the empty picker by default', () => {
    const { router, push } = fakeRouter()
    openChannelsMapper(router)
    expect(push).toHaveBeenCalledWith({
      name: 'config-channel-channels',
      query: { openMapper: 'true' },
    })
  })

  it('asks for every service with all=true', () => {
    const { router, push } = fakeRouter()
    openChannelsMapper(router, true)
    expect(push).toHaveBeenCalledWith({
      name: 'config-channel-channels',
      query: { openMapper: 'all' },
    })
  })
})
