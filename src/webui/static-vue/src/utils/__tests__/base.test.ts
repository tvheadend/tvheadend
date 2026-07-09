// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * URL-base derivation for webroot support. The served <base> tag is
 * `{webroot}/gui/static/` (rewritten by vue.c when --http_root is
 * set); computeBases must derive the router base and server root
 * from any such value and fall back to root-mounted defaults for
 * everything else — the fallback is what keeps the whole vitest
 * suite (happy-dom, no <base> tag) running against unprefixed URLs.
 */
import { describe, expect, it } from 'vitest'
import { computeBases, serverUrl } from '../base'

const ROOT_BASES = { assetBase: '/gui/static/', appBase: '/gui/', serverBase: '/' }

describe('computeBases', () => {
  it.each([
    [
      'root-mounted default <base> href',
      'http://localhost:9981/gui/static/',
      ROOT_BASES,
    ],
    [
      'webroot prefix (--http_root /tvheadend)',
      'https://example.org/tvheadend/gui/static/',
      { assetBase: '/tvheadend/gui/static/', appBase: '/tvheadend/gui/', serverBase: '/tvheadend/' },
    ],
    [
      'nested multi-segment webroot',
      'https://h/a/b/gui/static/',
      { assetBase: '/a/b/gui/static/', appBase: '/a/b/gui/', serverBase: '/a/b/' },
    ],
    ['fallback: vitest page URL with pathname /', 'http://localhost/', ROOT_BASES],
    ['fallback: unrelated path', 'http://localhost/some/other/path/', ROOT_BASES],
    ['fallback: empty base URI', '', ROOT_BASES],
    ['fallback: unparseable base URI', 'not a url', ROOT_BASES],
  ])('%s', (_name, baseUri, expected) => {
    expect(computeBases(baseUri)).toEqual(expected)
  })
})

describe('serverUrl', () => {
  /* Module-level serverBase is '/' under vitest (fallback), so these
   * assert the join semantics; the prefixed form is covered by the
   * computeBases cases above (serverUrl is serverBase + stripped path). */
  it('joins a bare path onto the server base', () => {
    expect(serverUrl('api/epg/events/grid')).toBe('/api/epg/events/grid')
  })

  it('strips leading slashes so no double slash is produced', () => {
    expect(serverUrl('/imagecache/42')).toBe('/imagecache/42')
    expect(serverUrl('//imagecache/42')).toBe('/imagecache/42')
  })

  it('leaves query strings untouched', () => {
    expect(serverUrl('redir/locale.js?_=123')).toBe('/redir/locale.js?_=123')
  })
})
