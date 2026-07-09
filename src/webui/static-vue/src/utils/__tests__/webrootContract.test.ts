// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Build-config contract for webroot support. Three pieces must stay
 * in agreement or the app breaks behind --http_root:
 *
 *   - index.html carries `<base href="/gui/static/">` — the token
 *     vue.c rewrites to `{webroot}/gui/static/` at serve time and
 *     the anchor utils/base.ts derives every URL from.
 *   - the production Vite base is relative ('./') so hashed assets
 *     resolve through that tag and lazy chunks via import.meta.url.
 *   - the dev base stays '/gui/' (no <base>-rewrite in dev).
 *
 * vite.config.ts is asserted as text (importing it would execute its
 * file-URL alias resolution, which the happy-dom transform breaks).
 */
import { describe, expect, it } from 'vitest'
import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'

const viteConfig = readFileSync(resolve(process.cwd(), 'vite.config.ts'), 'utf8')
const indexHtml = readFileSync(resolve(process.cwd(), 'index.html'), 'utf8')

describe('webroot build contract', () => {
  it('production build uses a relative Vite base, dev keeps /gui/', () => {
    expect(viteConfig).toContain("base: command === 'build' ? './' : '/gui/'")
  })

  it('index.html carries the injectable <base> tag', () => {
    expect(indexHtml).toContain('<base href="/gui/static/">')
  })
})
