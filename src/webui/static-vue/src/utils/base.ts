// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * URL bases for serving the app behind a webroot (`--http_root`).
 *
 * The server registers every route under `tvheadend_webroot`
 * (http_path_add prepends it, src/http.c), so the app must prefix
 * every server URL it builds. The prefix is discovered at runtime
 * from the document's <base> tag: index.html carries
 * `<base href="/gui/static/">` and vue.c rewrites it to
 * `<base href="{webroot}/gui/static/">` at serve time. Vite builds
 * with a relative base (`base: './'`), so the hashed assets resolve
 * through the same tag and lazy chunks resolve via import.meta.url —
 * nothing else in the bundle needs the prefix baked in.
 *
 * Derivation: assetBase is the <base> pathname; appBase (the Vue
 * Router base) strips the trailing `static/`; serverBase (the prefix
 * for /api, /comet, /static, … — always slash-terminated) strips the
 * trailing `gui/`. When no <base> tag matches the expected shape
 * (vitest's happy-dom, unexpected hosting) everything falls back to
 * the root-mounted defaults, which are byte-identical to the
 * pre-webroot behaviour.
 */

export interface Bases {
  /** Where the dist assets are served from, e.g. `/tvheadend/gui/static/`. */
  assetBase: string
  /** Vue Router base, e.g. `/tvheadend/gui/`. */
  appBase: string
  /** Server root for API/static routes, e.g. `/tvheadend/`. */
  serverBase: string
}

const ASSET_SUFFIX = 'gui/static/'

export function computeBases(baseUri: string): Bases {
  let pathname: string | null = null
  try {
    pathname = new URL(baseUri).pathname
  } catch {
    pathname = null
  }
  if (pathname?.endsWith(`/${ASSET_SUFFIX}`)) {
    const appBase = pathname.slice(0, -'static/'.length)
    return {
      assetBase: pathname,
      appBase,
      serverBase: appBase.slice(0, -'gui/'.length),
    }
  }
  return { assetBase: '/gui/static/', appBase: '/gui/', serverBase: '/' }
}

const bases = computeBases(typeof document !== 'undefined' ? document.baseURI : '')

export const assetBase = bases.assetBase
export const appBase = bases.appBase
export const serverBase = bases.serverBase

/**
 * Absolute URL for a server route, honouring the webroot. Input is
 * the server-root-relative path with or without a leading slash:
 * `serverUrl('api/epg/events/grid')` → `/tvheadend/api/epg/events/grid`.
 */
export function serverUrl(path: string): string {
  return serverBase + path.replace(/^\/+/, '')
}
