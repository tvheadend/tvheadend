// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * parseBuildTimestamp — turn the server's `build_timestamp` into
 * Unix seconds for the shared date formatter.
 *
 * The Makefile writes it with `date "+%Y-%m-%dT%H:%M:%S%z"`, so the
 * zone offset has no colon ("2026-09-24T20:08:31+0000"). Only
 * "+00:00" is the ISO form every browser's Date.parse accepts, so
 * the colon is added before parsing. Returns null for anything that
 * does not look like that shape.
 */
const BUILD_TIMESTAMP = /^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})([+-]\d{2}):?(\d{2})$/

export function parseBuildTimestamp(value: string | undefined): number | null {
  const m = BUILD_TIMESTAMP.exec(value?.trim() ?? '')
  if (!m) return null
  const ms = Date.parse(`${m[1]}${m[2]}:${m[3]}`)
  return Number.isNaN(ms) ? null : Math.floor(ms / 1000)
}
