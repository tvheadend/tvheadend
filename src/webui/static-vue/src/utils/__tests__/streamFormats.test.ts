// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * streamFormats unit tests — container/encoder → MIME resolution
 * and the browser decode probe.
 */
import { describe, expect, it } from 'vitest'
import { probeBrowserFormat, resolveBrowserFormat } from '../streamFormats'

describe('resolveBrowserFormat', () => {
  it('resolves a WebM / VP8 / Vorbis transcode profile', () => {
    expect(resolveBrowserFormat(6, 'libvpx', 'libvorbis')).toEqual({
      videoContentType: 'video/webm; codecs="vp8"',
      audioContentType: 'audio/webm; codecs="vorbis"',
    })
  })

  it('maps a hardware VP8 encoder the same as the software one', () => {
    const sw = resolveBrowserFormat(6, 'libvpx', 'libvorbis')
    const hw = resolveBrowserFormat(6, 'vp8_vaapi', 'libvorbis')
    expect(hw).toEqual(sw)
    expect(hw?.videoContentType).toContain('vp8')
  })

  it('returns null for containers a native <video> cannot play', () => {
    /* decodingInfo over-reports x-matroska, so the container gate
     * must exclude Matroska (and MP4 / MPEG-TS) up front — only
     * WebM is a reliably progressive <video> container. */
    expect(resolveBrowserFormat(1, 'libvpx', 'libvorbis')).toBeNull() // MC_MATROSKA
    expect(resolveBrowserFormat(7, 'libvpx', 'libvorbis')).toBeNull() // MC_AVMATROSKA
    expect(resolveBrowserFormat(9, 'libvpx', 'libvorbis')).toBeNull() // MC_AVMP4
    expect(resolveBrowserFormat(2, 'libvpx', 'libvorbis')).toBeNull() // MC_MPEGTS
  })

  it('returns null when a codec is copy / unset / unknown', () => {
    expect(resolveBrowserFormat(6, undefined, 'libvorbis')).toBeNull()
    expect(resolveBrowserFormat(6, 'libvpx', undefined)).toBeNull()
    expect(resolveBrowserFormat(6, 'some-exotic-encoder', 'libvorbis')).toBeNull()
  })
})

/* Stub `navigator.mediaCapabilities.decodingInfo` to report a fixed
 * support result. Module-scoped so it isn't redefined per describe. */
function stubDecodingInfo(supported: boolean) {
  Object.defineProperty(navigator, 'mediaCapabilities', {
    configurable: true,
    value: {
      decodingInfo: () =>
        Promise.resolve({ supported, smooth: true, powerEfficient: true }),
    },
  })
}

describe('probeBrowserFormat', () => {
  const fmt = {
    videoContentType: 'video/webm; codecs="vp8"',
    audioContentType: 'audio/webm; codecs="vorbis"',
  }

  it('is true when decodingInfo reports supported', async () => {
    stubDecodingInfo(true)
    expect(await probeBrowserFormat(fmt)).toBe(true)
  })

  it('is false when decodingInfo reports unsupported', async () => {
    stubDecodingInfo(false)
    expect(await probeBrowserFormat(fmt)).toBe(false)
  })

  it('is false (not a throw) when MediaCapabilities is unavailable', async () => {
    Object.defineProperty(navigator, 'mediaCapabilities', {
      configurable: true,
      value: undefined,
    })
    expect(await probeBrowserFormat(fmt)).toBe(false)
  })
})
