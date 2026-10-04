// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Is a playing <video> decoding any audio? Best effort and
 * Chromium-only: no standard API says so, and this reads Chromium's
 * non-standard cumulative decoded-byte counters,
 * webkitAudioDecodedByteCount and webkitVideoDecodedByteCount.
 *
 * Measured on Chrome 154: with H.264 + MP2 in Matroska the video
 * bytes grow while the audio bytes stay 0, also while muted.
 *
 * Elsewhere it returns 'unknown' and the player's "No sound" notice
 * does nothing: a macOS WKWebView (Safari's engine) reports both
 * counters as undefined, and Firefox was not measured.
 * Firefox's mozHasAudio is deliberately not used: it says whether the
 * media has an audio track, not whether the track decodes.
 *
 * 'none' requires video bytes above 0, so a stalled pipeline that
 * decoded nothing at all never reads as silence.
 */

export type AudioPresence = 'none' | 'present' | 'unknown'

/* Chromium-only members, not standard. */
interface DecodedByteCounts {
  webkitAudioDecodedByteCount?: unknown
  webkitVideoDecodedByteCount?: unknown
}

export function audioPresence(el: HTMLMediaElement): AudioPresence {
  const m = el as HTMLMediaElement & DecodedByteCounts
  const a = m.webkitAudioDecodedByteCount
  const v = m.webkitVideoDecodedByteCount
  if (typeof a !== 'number' || typeof v !== 'number') return 'unknown'
  if (a > 0) return 'present'
  return v > 0 ? 'none' : 'unknown'
}
