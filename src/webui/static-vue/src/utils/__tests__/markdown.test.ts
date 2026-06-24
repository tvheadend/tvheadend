// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for the post-render HTML helpers in src/utils/markdown.ts.
 * `loadMarkedScript` + `renderMarkdown` aren't tested directly
 * — they're thin script-tag loader + globalThis.marked() pass-
 * through whose behaviour is verified end-to-end by the wizard
 * step component tests that render real Markdown.
 *
 * This file pins the post-processing helpers that ARE testable
 * in isolation.
 */
import { describe, expect, it } from 'vitest'
import {
  addExternalLinkAttrs,
  extractFirstHeading,
  rewriteStaticUrls,
} from '../markdown'

describe('addExternalLinkAttrs', () => {
  it('injects target=_blank + rel into a plain external link', () => {
    const input = '<p>See <a href="https://tvheadend.org">Tvheadend.org</a></p>'
    const output = addExternalLinkAttrs(input)
    /* DOM-based rewrite preserves existing attrs + appends new
     * ones; attribute order is implementation-defined, so
     * assert on individual attrs rather than a specific
     * ordering. */
    expect(output).toContain('href="https://tvheadend.org"')
    expect(output).toContain('target="_blank"')
    expect(output).toContain('rel="noopener noreferrer"')
  })

  it('handles multiple links in one fragment', () => {
    const input =
      '<a href="https://a.com">A</a> and <a href="https://b.com">B</a> and <a href="https://c.com">C</a>'
    const output = addExternalLinkAttrs(input)
    /* All three rewritten. */
    expect(output.match(/target="_blank"/g)?.length).toBe(3)
  })

  it('preserves existing href + title attributes', () => {
    const input = '<a href="https://example.com" title="Example">Click</a>'
    const output = addExternalLinkAttrs(input)
    expect(output).toContain('href="https://example.com"')
    expect(output).toContain('title="Example"')
    expect(output).toContain('target="_blank"')
    expect(output).toContain('rel="noopener noreferrer"')
  })

  it('does not double-add target when one already exists', () => {
    const input = '<a href="https://example.com" target="_self">Stay</a>'
    const output = addExternalLinkAttrs(input)
    /* Author intent preserved — no second target. */
    expect(output).toBe(input)
  })

  it('leaves in-page anchor links alone (href starts with #)', () => {
    const input = '<a href="#section">Jump</a>'
    const output = addExternalLinkAttrs(input)
    expect(output).toBe(input)
    expect(output).not.toContain('target=')
  })

  it('handles linked images (the OpenCollective donate banner shape)', () => {
    const input =
      '<a href="https://opencollective.com/tvheadend/donate"><img src="/static/img/opencollective.png" alt="Donate" /></a>'
    const output = addExternalLinkAttrs(input)
    expect(output).toContain('target="_blank"')
    expect(output).toContain('rel="noopener noreferrer"')
    expect(output).toContain('<img src="/static/img/opencollective.png"')
  })

  it('returns input unchanged when no anchors present', () => {
    const input = '<p>Plain paragraph with no links.</p>'
    const output = addExternalLinkAttrs(input)
    expect(output).toBe(input)
  })

  it('handles empty string', () => {
    expect(addExternalLinkAttrs('')).toBe('')
  })

  it('is case-insensitive for the tag match', () => {
    /* Defensive — most renderers emit lowercase, but the regex
     * should still match if a future renderer / sanitizer
     * normalises to upper-case. */
    const input = '<A HREF="https://example.com">Click</A>'
    const output = addExternalLinkAttrs(input)
    expect(output).toContain('target="_blank"')
  })

  it('leaves RELATIVE href values alone (no target=_blank)', () => {
    /* Markdown-internal links like `class/mpegts_service` must
     * NOT be rewritten — the help dock's body click handler
     * intercepts them for in-dock navigation. Stamping
     * target=_blank would cause the browser to open them in a
     * new tab against the wrong base URL. */
    const input = '<a href="class/mpegts_service">Services</a>'
    expect(addExternalLinkAttrs(input)).toBe(input)
  })

  it('leaves ROOT-ABSOLUTE href values alone', () => {
    /* `/static/img/...` after rewriteStaticUrls, or any other
     * root-relative path, should NOT pick up target=_blank. */
    const input = '<a href="/static/img/x.png">Asset</a>'
    expect(addExternalLinkAttrs(input)).toBe(input)
  })
})

describe('extractFirstHeading', () => {
  it('returns the text of the first <h1>', () => {
    expect(extractFirstHeading('<h1>Services</h1><p>body</p>', 'class/x')).toBe(
      'Services',
    )
  })

  it('strips nested tags inside the heading', () => {
    expect(
      extractFirstHeading('<h1>Service <em>Mapping</em></h1>', 'class/x'),
    ).toBe('Service Mapping')
  })

  it('trims surrounding whitespace', () => {
    expect(extractFirstHeading('<h1>   Welcome   </h1>', 'firstconfig')).toBe(
      'Welcome',
    )
  })

  it('falls back to the last path segment when no <h1>', () => {
    expect(extractFirstHeading('<p>no heading</p>', 'class/mpegts_service')).toBe(
      'mpegts_service',
    )
  })

  it('falls back to the whole page id when no slash + no <h1>', () => {
    expect(extractFirstHeading('<p>no heading</p>', 'firstconfig')).toBe(
      'firstconfig',
    )
  })

  it('uses fallback on empty html', () => {
    expect(extractFirstHeading('', 'firstconfig')).toBe('firstconfig')
  })

  it('ignores <h2> and deeper — only <h1> counts', () => {
    expect(extractFirstHeading('<h2>Sub</h2><h1>Main</h1>', 'fallback')).toBe(
      'Main',
    )
  })
})

describe('rewriteStaticUrls', () => {
  it('prepends a leading slash to `src="static/..."`', () => {
    const input = '<img src="static/img/doc/wizard.png" alt="x">'
    expect(rewriteStaticUrls(input)).toBe(
      '<img src="/static/img/doc/wizard.png" alt="x">',
    )
  })

  it('prepends a leading slash to `href="static/..."`', () => {
    const input = '<a href="static/help.html">help</a>'
    expect(rewriteStaticUrls(input)).toBe('<a href="/static/help.html">help</a>')
  })

  it('leaves already-absolute paths alone', () => {
    const input = '<img src="/static/img/x.png"><a href="/static/y.html"></a>'
    expect(rewriteStaticUrls(input)).toBe(input)
  })

  it('leaves full URLs alone', () => {
    const input =
      '<img src="https://example.com/static/x.png"><a href="http://other/static/y"></a>'
    expect(rewriteStaticUrls(input)).toBe(input)
  })

  it('leaves in-doc anchors alone', () => {
    const input = '<a href="#section">x</a>'
    expect(rewriteStaticUrls(input)).toBe(input)
  })

  it('rewrites multiple occurrences in one pass', () => {
    const input =
      '<img src="static/a.png"><img src="static/b.png"><a href="static/c">x</a>'
    expect(rewriteStaticUrls(input)).toBe(
      '<img src="/static/a.png"><img src="/static/b.png"><a href="/static/c">x</a>',
    )
  })

  it('only rewrites the EXACT `static/` prefix, not arbitrary substrings', () => {
    /* A path like `assets/static/x.png` is NOT a bare static
     * reference — leave it alone. */
    const input = '<img src="assets/static/x.png">'
    expect(rewriteStaticUrls(input)).toBe(input)
  })
})
