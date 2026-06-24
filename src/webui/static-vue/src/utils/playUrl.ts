// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Helpers for playing a tvheadend stream — in an external media
 * player (via an m3u/xspf playlist) or in-browser (a direct
 * `/stream/...` URL for a `<video>` element).
 *
 * Browser-playability detection lives in `utils/streamFormats.ts`;
 * the negotiation that applies it is in the `streamProfiles` store.
 */

/*
 * External-player handoff.
 *
 * The browser fetches the m3u/xspf playlist from tvheadend with the
 * user's session cookie / basic-auth, then hands the file off to the
 * OS MIME handler (VLC, Kodi, mpv, …). That media player is a
 * SEPARATE PROCESS — it has no browser session cookies, so the
 * stream URLs INSIDE the playlist need their own auth payload.
 *
 * Three /play endpoint variants exist on the server
 * (`src/webui/webui.c`), each routing to `page_play_()` with a
 * different `urlauth` mode:
 *
 *   /play         — URLAUTH_NONE   (bare stream URLs in the m3u)
 *   /play/ticket  — URLAUTH_TICKET (stream URLs gain ?ticket=<id>)
 *   /play/auth    — URLAUTH_CODE   (stream URLs gain ?auth=<code>)
 *
 * `URLAUTH_TICKET` mints a short-lived random ticket per stream URL
 * via `access_ticket_create()`, storing a snapshot of the user's
 * access rights server-side. External players can then fetch the
 * stream URL without cookies. Classic's `playLink` helper uses the
 * ticket variant for every Play link; we do the same — it is the
 * only mode that works reliably across the "browser opens m3u → OS
 * hands to external player" handoff.
 */

/* Opens the given resource path in the user's external media player
 * via the ticket-auth `/play` endpoint. `path` is the portion after
 * `/play/ticket/` — e.g. `stream/channel/<uuid>`,
 * `stream/mux/<uuid>`, or `dvrfile/<uuid>` — and may carry a query
 * string (`?profile=…`, `?title=…`). */
export function openPlay(path: string): void {
  globalThis.open(`/play/ticket/${path}`, '_blank', 'noopener,noreferrer')
}

/*
 * Direct stream URL for in-browser playback of a live channel.
 * Unlike `openPlay()`, this is consumed by a `<video>` element
 * INSIDE the authenticated browser session — the session cookie
 * carries auth, so it hits `/stream/...` directly with no
 * `/play/ticket/` wrapper. `profile` is the stream-profile name; the
 * server's `http_stream_channel` resolves it via `profile_find_by_list`.
 *
 * Recordings are intentionally not covered: `/dvrfile/<uuid>` serves
 * the file raw with no transcode-on-playback, so in-browser playback
 * is live-channel only; recordings keep the external-player path.
 */
export function channelStreamUrl(channelUuid: string, profile: string): string {
  return `/stream/channel/${channelUuid}?profile=${encodeURIComponent(profile)}`
}
