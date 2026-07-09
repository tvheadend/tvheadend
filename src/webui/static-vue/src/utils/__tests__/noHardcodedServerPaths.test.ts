// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Webroot regression fence. Every URL the app sends to the server
 * must go through `serverUrl()` / `appBase` (utils/base.ts) so it
 * carries the tvheadend_webroot prefix. A hardcoded root-absolute
 * literal like fetch('/api/...') works on a root-mounted server but
 * silently breaks behind --http_root (the server's catch-all only
 * rescues it via a 302 that downgrades POSTs to GETs).
 *
 * This test scans the source tree for root-absolute server-route
 * literals in string quotes and in call/assignment-position template
 * literals. Doc comments use backtick code spans and don't match the
 * patterns. If it flags your new code: route the URL through
 * `serverUrl()` instead of widening the allowlist.
 */
import { describe, expect, it } from 'vitest'
import { readFileSync, readdirSync, statSync } from 'node:fs'
import { join, relative, resolve } from 'node:path'

const SRC_ROOT = resolve(process.cwd(), 'src')

/* Server routes registered under the webroot (http_path_add). Router
 * paths like '/dvr/upcoming' are internal and deliberately absent. */
const ROUTES = String.raw`(api|comet|play|dvrfile|stream|imagecache|markdown|redir|static|extjs\.html|login|logout|gui)`

/* Root-absolute server route inside ' or " quotes. Comments quote
 * paths with backticks, so plain prose doesn't trip this. */
const QUOTED = new RegExp(`['"]/${ROUTES}(/|['"?])`)
/* Template literal starting with a server route, in call or
 * assignment position — catches fetch(\`/api/...\`) etc. without
 * matching backtick code spans inside doc comments. */
const TEMPLATE = new RegExp(String.raw`[(=]\s*` + '`' + `/${ROUTES}/`)

/* Files allowed to hold root-absolute literals. */
const ALLOWED = new Set([
  'utils/base.ts', // the fallback constants live here by design
])

function collect(dir: string, out: string[]): string[] {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name)
    if (statSync(p).isDirectory()) {
      if (name === '__tests__' || name === 'node_modules') continue
      collect(p, out)
    } else if (/\.(ts|vue)$/.test(name)) {
      out.push(p)
    }
  }
  return out
}

describe('no hardcoded root-absolute server URLs', () => {
  it('every server URL goes through serverUrl()/appBase', () => {
    const violations: string[] = []
    for (const file of collect(SRC_ROOT, [])) {
      const rel = relative(SRC_ROOT, file)
      if (ALLOWED.has(rel)) continue
      const lines = readFileSync(file, 'utf8').split('\n')
      lines.forEach((line, i) => {
        /* Comment lines quote paths as backtick code spans (often
         * parenthesised, which mimics call position) — skip them. */
        const trimmed = line.trim()
        if (trimmed.startsWith('*') || trimmed.startsWith('//') || trimmed.startsWith('/*'))
          return
        if (QUOTED.test(line) || TEMPLATE.test(line)) {
          violations.push(`${rel}:${i + 1}: ${trimmed}`)
        }
      })
    }
    expect(violations, violations.join('\n')).toEqual([])
  })
})
