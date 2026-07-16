// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useChannelPlay — the in-browser-vs-external decision for channel-
 * level playback (issue #2183: play a channel with no EPG). Verifies
 * it opens the in-browser player when a stream profile is available
 * and falls back to the external player otherwise.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'

const h = vi.hoisted(() => ({
  ensure: vi.fn(() => Promise.resolve()),
  open: vi.fn(),
  openPlay: vi.fn(),
  canPlayInBrowser: { value: true },
}))

vi.mock('@/stores/streamProfiles', () => ({
  useStreamProfilesStore: () => ({
    ensure: h.ensure,
    get canPlayInBrowser() {
      return h.canPlayInBrowser.value
    },
  }),
}))
vi.mock('@/composables/useVideoPlayer', () => ({
  useVideoPlayer: () => ({ open: h.open }),
}))
vi.mock('@/utils/playUrl', () => ({
  openPlay: (p: string) => h.openPlay(p),
}))

import { useChannelPlay } from '../useChannelPlay'

beforeEach(() => {
  h.ensure.mockClear()
  h.open.mockClear()
  h.openPlay.mockClear()
  h.canPlayInBrowser.value = true
})

describe('useChannelPlay', () => {
  it('opens the in-browser player when a stream profile is available', async () => {
    await useChannelPlay().play('ch-1', 'BBC One')
    expect(h.ensure).toHaveBeenCalled()
    expect(h.open).toHaveBeenCalledWith({ channelUuid: 'ch-1', title: 'BBC One' })
    expect(h.openPlay).not.toHaveBeenCalled()
  })

  it('falls back to the external player when the browser cannot play', async () => {
    h.canPlayInBrowser.value = false
    await useChannelPlay().play('ch-2', 'ITV')
    expect(h.openPlay).toHaveBeenCalledWith('stream/channel/ch-2')
    expect(h.open).not.toHaveBeenCalled()
  })

  it('is a no-op for an empty channel uuid', async () => {
    await useChannelPlay().play('', '')
    expect(h.ensure).not.toHaveBeenCalled()
    expect(h.open).not.toHaveBeenCalled()
    expect(h.openPlay).not.toHaveBeenCalled()
  })
})
