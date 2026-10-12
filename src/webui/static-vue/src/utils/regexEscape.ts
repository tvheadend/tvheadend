// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Escape text so the server matches it literally inside a regex.
 *
 * The EPG title search and the channel-name filter are compiled
 * server-side as caseless, unanchored regexes (`src/epg.c`
 * `epg_query`, PCRE or POSIX ERE depending on the build). A title
 * taken from a known event, such as "News (Late)", must be escaped
 * before it goes out, or the parentheses turn into a group and the
 * event no longer matches itself.
 */
export function regexEscape(s: string): string {
  return s.replace(/[-/\\^$*+?.()|[\]{}]/g, '\\$&')
}
