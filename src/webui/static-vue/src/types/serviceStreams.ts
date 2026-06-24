// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Wire-format types for `api/service/streams?uuid=<id>` (server
 * handler `src/api/api_service.c:109-170`, ACCESS_ADMIN).
 *
 * Response shape:
 *   {
 *     name:     <service nice-name>,
 *     streams:  ServiceStream[]    // unfiltered, includes PCR+PMT
 *     fstreams: ServiceStream[]    // post-esfilter, real ES only
 *     hbbtv?:   HbbTvData          // optional, set when the service
 *                                     advertises HbbTV applications
 *   }
 *
 * `streams[]` is the full set including synthetic PCR / PMT entries
 * prepended (those carry only `pid` + `type`, no `index`).
 * `fstreams[]` is the filtered subset; PCR / PMT are excluded;
 * each entry has the same shape as a `streams` entry. The dialog
 * merges them client-side and renders one row per `streams` entry
 * with a Used flag indicating whether the same (index, pid) appears
 * in `fstreams`.
 */

/* Per-stream type-specific extras emitted by
 * `api_service_streams_get_one` (`src/api/api_service.c:72-107`):
 *
 *   SCT_ISVIDEO   → width, height, duration, aspect_num, aspect_den
 *   SCT_ISAUDIO   → audio_type, audio_version (optional)
 *   SCT_ISSUBTITLE → composition_id, ancillary_id
 *   SCT_CA        → caids: { caid, provider }[]
 *
 * All u32 server-side. The PCR / PMT pseudo-rows emit only `pid`
 * + `type` — `index` is absent on those.
 */
export interface ServiceCaid {
  caid: number
  provider: number
}

export interface ServiceStream {
  /* Absent on the synthetic PCR / PMT entries that the server
   * prepends to `streams[]` (`api_service.c:134-145`). */
  index?: number
  pid: number
  /* Server-emitted string from `streaming_component_type2txt`
   * (`src/streaming.c:566-592`). Examples: `H264` / `HEVC` /
   * `MPEG2VIDEO` / `AC3` / `EAC3` / `AAC` / `MP4A` /
   * `MPEG2AUDIO` / `DVBSUB` / `TELETEXT` / `CA` / `RDS` / and
   * the two synthetic `PCR` / `PMT` labels. */
  type: string
  /* 3-letter ISO 639-2 language code. Empty string on streams
   * that have no language tag (most video, PCR/PMT, CA). */
  language?: string

  /* Subtitle-only (SCT_DVBSUB / SCT_TEXTSUB). */
  composition_id?: number
  ancillary_id?: number

  /* Audio-only (SCT_AC3 / SCT_EAC3 / SCT_AAC / SCT_MP4A /
   * SCT_MPEG2AUDIO / SCT_VORBIS / SCT_OPUS / SCT_FLAC / SCT_AC4). */
  audio_type?: number
  audio_version?: number

  /* Video-only (SCT_H264 / SCT_HEVC / SCT_VP8 / SCT_VP9 /
   * SCT_MPEG2VIDEO / SCT_THEORA). `aspect_num`/`aspect_den` are
   * 0 when undetermined. */
  width?: number
  height?: number
  duration?: number
  aspect_num?: number
  aspect_den?: number

  /* CA-only (SCT_CA). `fstreams[]` entries only include caids
   * with `use === 1` server-side (`api_service.c:97`). */
  caids?: ServiceCaid[]
}

/* HbbTV section. Server copies a pre-built map straight from
 * `s->s_hbbtv` (`api_service.c:158-159`) — the exact shape
 * varies by service, so we model it loosely as a record-of-
 * records and let the dialog walk it defensively. The Classic
 * UI does the same at `mpegts.js:226-250`. */
export type HbbTvData = Record<string, HbbTvSection>

export interface HbbTvSection {
  language?: string
  appName?: string
  url?: string
  /* Other fields the server may emit. Stays open-typed because
   * we don't author the C-side structure. */
  [k: string]: unknown
}

export interface ServiceStreamsResponse {
  name: string
  streams: ServiceStream[]
  fstreams: ServiceStream[]
  hbbtv?: HbbTvData
}
