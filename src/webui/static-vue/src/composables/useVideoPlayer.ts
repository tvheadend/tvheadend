// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useVideoPlayer — module-level singleton for the in-browser video
 * player modal. Mirrors the `useHelp` pattern: module-scoped refs,
 * one `<VideoPlayerDialog>` mounted in AppShell binds to them.
 *
 * In-browser playback is live-channel only. Recordings can't be
 * transcoded on playback (`/dvrfile/<uuid>` serves the file raw),
 * so the EPG drawer offers "Play in browser" for live events only;
 * `current` therefore carries just a channel UUID + a display title.
 *
 * `profile` is the active stream profile and is mutable while the
 * dialog is open — changing it live-switches the stream. The dialog
 * owns the selection logic (it picks the initial value from the
 * `streamProfiles` store and remembers the last choice); the
 * composable just holds the ref so the `<select>` and the `<video>`
 * src share one source of truth.
 */
import { ref } from 'vue'

export interface PlayTarget {
  channelUuid: string
  /* Display title for the dialog header — the event title, or the
   * channel name when there's no better label. */
  title: string
}

const isOpen = ref(false)
const current = ref<PlayTarget | null>(null)
/* Active stream-profile name. Set by the dialog on open and mutated
 * by its profile dropdown; drives the `?profile=` query parameter on
 * the `<video>` element's stream URL. */
const profile = ref<string>('')

function open(target: PlayTarget): void {
  current.value = target
  isOpen.value = true
}

function close(): void {
  isOpen.value = false
  current.value = null
}

export function useVideoPlayer() {
  return { isOpen, current, profile, open, close }
}
