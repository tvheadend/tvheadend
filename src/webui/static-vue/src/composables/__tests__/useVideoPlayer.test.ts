// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useVideoPlayer composable unit tests. A small module-level
 * singleton: open() sets the target + flips isOpen; close() clears
 * both. The active stream profile is a separate mutable ref. The
 * singleton state persists across useVideoPlayer() calls, so each
 * test resets via close().
 */
import { afterEach, describe, expect, it } from 'vitest'
import { useVideoPlayer } from '../useVideoPlayer'

afterEach(() => {
  const player = useVideoPlayer()
  player.close()
  player.profile.value = ''
})

describe('useVideoPlayer', () => {
  it('starts closed with no target', () => {
    const player = useVideoPlayer()
    expect(player.isOpen.value).toBe(false)
    expect(player.current.value).toBeNull()
  })

  it('open() sets the target and flips isOpen', () => {
    const player = useVideoPlayer()
    player.open({ channelUuid: 'ch-1', title: 'BBC One' })
    expect(player.isOpen.value).toBe(true)
    expect(player.current.value).toEqual({ channelUuid: 'ch-1', title: 'BBC One' })
  })

  it('close() clears the target and flips isOpen', () => {
    const player = useVideoPlayer()
    player.open({ channelUuid: 'ch-1', title: 'BBC One' })
    player.close()
    expect(player.isOpen.value).toBe(false)
    expect(player.current.value).toBeNull()
  })

  it('shares one singleton across calls', () => {
    useVideoPlayer().open({ channelUuid: 'ch-2', title: 'Channel Two' })
    /* A fresh useVideoPlayer() call sees the same state. */
    expect(useVideoPlayer().current.value).toEqual({
      channelUuid: 'ch-2',
      title: 'Channel Two',
    })
  })

  it('exposes a mutable active-profile ref shared across calls', () => {
    useVideoPlayer().profile.value = 'webtv-vp8-vorbis-webm'
    expect(useVideoPlayer().profile.value).toBe('webtv-vp8-vorbis-webm')
  })
})
