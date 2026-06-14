// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * streamFormats — derive in-browser playability from a stream
 * profile's SETTINGS rather than its name.
 *
 * ----------------------------------------------------------------
 * CLIENT-SIDE WORKAROUND.
 *
 * To know whether a `<video>` element can play a given tvheadend
 * stream profile we need its output container + video/audio codec.
 * The server already resolves that (stream profile -> codec profile
 * -> FFmpeg encoder -> muxer), but exposes none of it: `profile/list`
 * returns only name + uuid. So this module re-derives it client-side
 * by joining the transcode profile's `container` / `pro_vcodec` /
 * `pro_acodec` against the codec profiles' read-only `codec_name`,
 * then mapping (container, encoder) to MIME content-type strings for
 * `navigator.mediaCapabilities.decodingInfo()`.
 *
 * This duplicates resolution the server owns, and the two maps below
 * have to track tvheadend's container + encoder set. The correct fix
 * is a server endpoint that reports each profile's resolved output
 * format; when that lands, this module collapses to a thin consumer
 * of it. Until then, deriving from settings (not the profile name)
 * is the robust option — names are user-editable, encoder names and
 * the container enum are not.
 *
 * CURRENTLY DORMANT. This module has no importer. The settings join
 * it fed on (`idnode/load?class=profile` / `?class=codec_profile`)
 * is admin-only, so it resolved to nothing for the non-admin
 * streaming users the in-browser player serves; `streamProfiles.ts`
 * now offers every profile and lets the player attempt playback.
 * The probing logic below is kept intact, ready to be rewired once
 * the server exposes each profile's resolved output format directly.
 * ----------------------------------------------------------------
 */

/* tvheadend muxer container enum (`src/muxer.h`). **WebM only** —
 * it is the one container a native HTML5 `<video>` element reliably
 * plays as a continuous live stream.
 *
 * Matroska (MC_MATROSKA / MC_AVMATROSKA) is deliberately excluded
 * even though WebM is a Matroska subset: `<video>` does not support
 * the `video/x-matroska` container, and `decodingInfo()` is
 * unreliable here — it validates only the codec and returns
 * `supported: true` for an `x-matroska` config the element then
 * cannot actually play (observed with H.264 in Firefox). The
 * container allowlist therefore has to gate this up front; the
 * `decodingInfo` probe only decides codec support *within* WebM.
 *
 * MP4 and MPEG-TS need an MSE-based player — continuous fMP4 / TS
 * is not progressively playable via a plain `<video src>`. When
 * that lands, add those containers here together with their codecs
 * (H.264/H.265 + AAC) in the maps below. */
const CONTAINER_MIME: Record<number, { video: string; audio: string }> = {
  6: { video: 'video/webm', audio: 'audio/webm' }, // MC_WEBM
  8: { video: 'video/webm', audio: 'audio/webm' }, // MC_AVWEBM
}

/* FFmpeg encoder name (a codec profile's read-only `codec_name`) ->
 * a `codecs="..."` string for the decode probe. Scoped to the
 * codecs WebM can carry — VP8 / VP9 video, Vorbis / Opus audio —
 * since WebM is the only container allow-listed above. Software and
 * hardware (VAAPI) encoders for the same codec map to the same
 * string: the browser decodes a codec, not an encoder. An encoder
 * absent from these maps yields no match, so its profile is simply
 * not offered (fail-safe). */
const VIDEO_ENCODER_CODEC: Record<string, string> = {
  libvpx: 'vp8',
  vp8_vaapi: 'vp8',
  'libvpx-vp9': 'vp09.00.10.08',
  vp9_vaapi: 'vp09.00.10.08',
}
const AUDIO_ENCODER_CODEC: Record<string, string> = {
  vorbis: 'vorbis',
  libvorbis: 'vorbis',
  opus: 'opus',
  libopus: 'opus',
}

/* Nominal probe parameters. `decodingInfo` requires these members;
 * the exact values don't change the `supported` verdict (that turns
 * on codec support) — they'd only affect the `smooth` /
 * `powerEfficient` hints, which we don't use. */
const PROBE_VIDEO = { width: 1280, height: 720, bitrate: 2_000_000, framerate: 25 }
const PROBE_AUDIO = { channels: '2', bitrate: 128_000, samplerate: 48_000 }

/* The per-track MIME content-type strings for a profile, ready to
 * feed to `decodingInfo`. */
export interface BrowserFormat {
  videoContentType: string
  audioContentType: string
}

/*
 * Resolve a transcode profile's output format to decode-probe
 * content-types. `container` is the profile's `container` enum
 * value; `videoEncoder` / `audioEncoder` are the FFmpeg encoder
 * names obtained by following `pro_vcodec` / `pro_acodec` to the
 * referenced codec profile's `codec_name`.
 *
 * Returns null when the format can't be determined or isn't a
 * browser target: an unknown/non-`<video>` container, or a codec
 * that is `copy` / unset / not in the maps above (the codec
 * arguments arrive undefined in those cases).
 */
export function resolveBrowserFormat(
  container: number,
  videoEncoder: string | undefined,
  audioEncoder: string | undefined,
): BrowserFormat | null {
  const mime = CONTAINER_MIME[container]
  if (!mime) return null
  const vCodec = videoEncoder ? VIDEO_ENCODER_CODEC[videoEncoder] : undefined
  const aCodec = audioEncoder ? AUDIO_ENCODER_CODEC[audioEncoder] : undefined
  if (!vCodec || !aCodec) return null
  return {
    videoContentType: `${mime.video}; codecs="${vCodec}"`,
    audioContentType: `${mime.audio}; codecs="${aCodec}"`,
  }
}

/*
 * Whether this browser can decode the given format. Uses
 * `navigator.mediaCapabilities.decodingInfo()` — the accurate
 * capability API (`HTMLMediaElement.canPlayType()` is unreliable:
 * Safari returns `'probably'` for VP8 yet cannot decode it). Any
 * failure — no MediaCapabilities, a malformed config, a rejection —
 * resolves to false: the conservative default keeps an undecodable
 * profile out of the offered set.
 */
export async function probeBrowserFormat(fmt: BrowserFormat): Promise<boolean> {
  try {
    const info = await navigator.mediaCapabilities.decodingInfo({
      type: 'file',
      video: { contentType: fmt.videoContentType, ...PROBE_VIDEO },
      audio: { contentType: fmt.audioContentType, ...PROBE_AUDIO },
    })
    return info.supported
  } catch {
    return false
  }
}
