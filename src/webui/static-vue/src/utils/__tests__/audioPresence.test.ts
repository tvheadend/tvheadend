// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { audioPresence, type AudioPresence } from '../audioPresence'

describe('audioPresence', () => {
  it.each<[string, object, AudioPresence]>([
    [
      'Chromium decoding video only',
      { webkitAudioDecodedByteCount: 0, webkitVideoDecodedByteCount: 1000 },
      'none',
    ],
    [
      'Chromium decoding audio',
      { webkitAudioDecodedByteCount: 10, webkitVideoDecodedByteCount: 1000 },
      'present',
    ],
    [
      'Chromium before anything decoded',
      { webkitAudioDecodedByteCount: 0, webkitVideoDecodedByteCount: 0 },
      'unknown',
    ],
    ['no counters (Safari, other browsers)', {}, 'unknown'],
    /* A track that exists is not a track that decodes, so Firefox's
     * mozHasAudio gets no say. */
    ['only mozHasAudio (Firefox)', { mozHasAudio: false }, 'unknown'],
    /* audioTracks does not say whether the track decodes either. */
    ['only audioTracks (Safari)', { audioTracks: { length: 0 } }, 'unknown'],
  ])('%s', (_name, el, expected) => {
    expect(audioPresence(el as HTMLMediaElement)).toBe(expected)
  })
})
