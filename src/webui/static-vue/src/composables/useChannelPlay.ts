// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useChannelPlay — start playback of a live channel, choosing the
 * in-browser player when the server offers a stream profile and
 * falling back to the external player otherwise.
 *
 * Shared by every channel-level play entry point (the EPG channel-cell
 * click in Timeline / Magazine and the Watch TV launcher) so the
 * in-browser-vs-external decision lives in one place — mirroring the
 * EPG event drawer's "Play in browser" behaviour but without needing
 * an EPG event. This is what lets a channel with no EPG data still be
 * played (issue #2183): playback no longer depends on a live event.
 */
import { useStreamProfilesStore } from '@/stores/streamProfiles'
import { useVideoPlayer } from '@/composables/useVideoPlayer'
import { openPlay } from '@/utils/playUrl'

export function useChannelPlay() {
  const streamProfiles = useStreamProfilesStore()
  const player = useVideoPlayer()

  /* Play `channelUuid`; `title` is the player header label (the
   * channel name). Opens the in-browser modal when a stream profile
   * is available, otherwise hands off to the external player — the
   * same fallback the event drawer's play menu uses. */
  async function play(channelUuid: string, title: string): Promise<void> {
    if (!channelUuid) return
    await streamProfiles.ensure()
    if (streamProfiles.canPlayInBrowser) {
      player.open({ channelUuid, title })
    } else {
      openPlay(`stream/channel/${channelUuid}`)
    }
  }

  return { play }
}
