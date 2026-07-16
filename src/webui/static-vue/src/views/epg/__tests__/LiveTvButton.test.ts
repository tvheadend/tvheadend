// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * LiveTvButton — the toolbar launcher that lets any EPG view start
 * playback without an EPG event (issue #2183). Channel selection lives
 * in VideoPlayerDialog, so the button's whole job is to open the
 * player with no channel preselected.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import LiveTvButton from '../LiveTvButton.vue'

const openMock = vi.fn()
vi.mock('@/composables/useVideoPlayer', () => ({
  useVideoPlayer: () => ({ open: openMock }),
}))

enableAutoUnmount(afterEach)
beforeEach(() => {
  openMock.mockReset()
})

describe('LiveTvButton', () => {
  it('renders a text-only "Live TV" button (no icon)', () => {
    const w = mount(LiveTvButton)
    const btn = w.find('.live-tv-btn')
    expect(btn.text()).toBe('Live TV')
    expect(btn.find('svg').exists()).toBe(false)
  })

  it('opens the player with no preselected channel when clicked', async () => {
    const w = mount(LiveTvButton)
    await w.find('.live-tv-btn').trigger('click')
    expect(openMock).toHaveBeenCalledTimes(1)
    expect(openMock).toHaveBeenCalledWith()
  })
})
