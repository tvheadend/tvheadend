// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, describe, expect, it, vi } from 'vitest'
import { openPlay, channelStreamUrl } from '../playUrl'

describe('openPlay', () => {
  afterEach(() => {
    vi.restoreAllMocks()
  })

  it('opens the ticket-auth /play URL in a new tab', () => {
    const spy = vi.spyOn(globalThis, 'open').mockReturnValue(null)
    openPlay('stream/channel/abc-123')
    expect(spy).toHaveBeenCalledWith(
      '/play/ticket/stream/channel/abc-123',
      '_blank',
      'noopener,noreferrer',
    )
  })

  it('preserves the dvrfile path shape verbatim', () => {
    const spy = vi.spyOn(globalThis, 'open').mockReturnValue(null)
    openPlay('dvrfile/def-456')
    expect(spy.mock.calls[0][0]).toBe('/play/ticket/dvrfile/def-456')
  })

  it('preserves a path with a query string (?profile=)', () => {
    const spy = vi.spyOn(globalThis, 'open').mockReturnValue(null)
    openPlay('stream/channel/ghi-789?profile=webtv')
    expect(spy.mock.calls[0][0]).toBe('/play/ticket/stream/channel/ghi-789?profile=webtv')
  })
})

describe('channelStreamUrl', () => {
  it('builds a direct /stream URL with the chosen profile', () => {
    expect(channelStreamUrl('abc-123', 'webtv-vp8-vorbis-webm')).toBe(
      '/stream/channel/abc-123?profile=webtv-vp8-vorbis-webm',
    )
  })

  it('uses no /play/ticket wrapper (cookie-authed, in-session)', () => {
    expect(channelStreamUrl('abc-123', 'webtv-h264-aac-mp4').startsWith('/stream/')).toBe(
      true,
    )
  })

  it('URL-encodes the profile name', () => {
    expect(channelStreamUrl('abc', 'web tv/x')).toBe(
      '/stream/channel/abc?profile=web%20tv%2Fx',
    )
  })
})
