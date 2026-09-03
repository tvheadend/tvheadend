// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Palette contract for the Dark theme.
 *
 * Dark is the low-glare night palette (as opposed to Access, the
 * high-contrast accessibility variant). Three things about it are
 * easy to break by accident and expensive to notice by eye, so they
 * are pinned here:
 *
 *   1. Completeness. Every literal colour declared in :root has to be
 *      re-declared for the theme. A token added to :root later with a
 *      light value would otherwise be inherited verbatim and render a
 *      light patch on a dark surface. The test derives the required
 *      set from tokens.css itself rather than restating it, so a new
 *      token is covered the moment it lands.
 *   2. Native chrome. `color-scheme: dark` makes the browser draw
 *      checkboxes, radios, scrollbars and date pickers dark. Without
 *      it :root's `color-scheme: light` is inherited and native
 *      widgets render as light rectangles on the dark surface.
 *   3. Legibility. The foreground/background pairs the palette is
 *      built around meet WCAG AA (4.5:1).
 *
 * Parsed with postcss for the same reason as themeScale.test.ts:
 * happy-dom's getComputedStyle doesn't resolve custom properties from
 * injected stylesheets, and parsing the shipped CSS pins the real
 * source of truth.
 */
import { describe, it, expect } from 'vitest'
import postcss, { type Root, type Rule, type Declaration } from 'postcss'
import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'

const tokensCss = readFileSync(
  resolve(process.cwd(), 'src/styles/tokens.css'),
  'utf8',
)
const ast: Root = postcss.parse(tokensCss)

function findTopLevelRule(selector: string): Rule {
  let found: Rule | undefined
  ast.walkRules((rule) => {
    if (rule.parent?.type !== 'root') return
    if (rule.selectors.map((s) => s.trim()).includes(selector)) found = rule
  })
  if (!found) throw new Error(`Top-level rule not found: ${selector}`)
  return found
}

/* Map of custom-property name -> value for one rule. */
function declsOf(rule: Rule): Map<string, string> {
  const out = new Map<string, string>()
  rule.walkDecls((d: Declaration) => {
    if (d.prop.startsWith('--')) out.set(d.prop, d.value.trim())
  })
  return out
}

const LITERAL_COLOR = /#[0-9a-f]{3,8}\b|\brgba?\(/i

/*
 * A token needs a per-theme value when it holds a literal colour.
 * Tokens built from other tokens (--tvh-dvr-overlay-bg mixes
 * --tvh-primary, for instance) are derived on purpose and must keep
 * inheriting, so anything referencing var() is excluded.
 */
function literalColorTokens(rule: Rule): string[] {
  return [...declsOf(rule)]
    .filter(([, v]) => LITERAL_COLOR.test(v) && !v.includes('var('))
    .map(([prop]) => prop)
}

const root = findTopLevelRule(':root')
const dark = findTopLevelRule("[data-theme='dark']")
const darkDecls = declsOf(dark)

/* --- WCAG relative luminance / contrast, per WCAG 2.x definitions. --- */

function parseHex(hex: string): [number, number, number] {
  let h = hex.replace('#', '').trim()
  if (h.length === 3) h = [...h].map((c) => c + c).join('')
  return [0, 2, 4].map((i) => Number.parseInt(h.slice(i, i + 2), 16) / 255) as [
    number,
    number,
    number,
  ]
}

function luminance(hex: string): number {
  const [r, g, b] = parseHex(hex).map((c) =>
    c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4,
  )
  return 0.2126 * r + 0.7152 * g + 0.0722 * b
}

function contrast(a: string, b: string): number {
  const [la, lb] = [luminance(a), luminance(b)]
  return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05)
}

function token(name: string): string {
  const v = darkDecls.get(name)
  if (v === undefined) throw new Error(`Dark theme does not declare ${name}`)
  return v
}

describe('Dark theme completeness', () => {
  it('re-declares every literal colour token from :root', () => {
    const required = literalColorTokens(root)
    /* Sanity check on the derivation itself — if the filter ever
     * matches nothing, the completeness assertion below would pass
     * vacuously. */
    expect(required.length).toBeGreaterThan(5)
    expect(required.filter((t) => !darkDecls.has(t))).toEqual([])
  })

  it('opts native widgets into dark rendering', () => {
    /* :root pins `color-scheme: light` so light themes keep light
     * native controls; a dark palette has to opt back out or the
     * browser draws checkboxes and scrollbars light. */
    let colorScheme: string | undefined
    dark.walkDecls('color-scheme', (d: Declaration) => {
      colorScheme = d.value.trim()
    })
    expect(colorScheme).toBe('dark')
  })

  it('defines --tvh-on-primary, which the light themes leave unset', () => {
    /* The primary has to be light enough to read as text on the dark
     * page, which makes #fff over it illegible — so text on primary
     * fills reads --tvh-on-primary instead, falling back to #fff for
     * the light themes that do not declare it. */
    expect(darkDecls.has('--tvh-on-primary')).toBe(true)
    expect(declsOf(root).has('--tvh-on-primary')).toBe(false)
  })
})

describe('Dark theme surfaces', () => {
  it('keeps the surface lighter than the page, as the light themes do', () => {
    /* Elevation has to read the same way in every theme: a raised
     * surface is lighter than the page it sits on. */
    expect(luminance(token('--tvh-bg-surface'))).toBeGreaterThan(
      luminance(token('--tvh-bg-page')),
    )
  })

  it('stops short of pure black and pure white', () => {
    /* #fff on #000 haloes on OLED panels and is tiring at night. */
    expect(luminance(token('--tvh-bg-page'))).toBeGreaterThan(0)
    expect(luminance(token('--tvh-text'))).toBeLessThan(1)
  })
})

describe('Dark theme contrast (WCAG AA, 4.5:1)', () => {
  it.each([
    ['body text on surface', '--tvh-text', '--tvh-bg-surface'],
    ['body text on page', '--tvh-text', '--tvh-bg-page'],
    ['muted text on surface', '--tvh-text-muted', '--tvh-bg-surface'],
    ['muted text on page', '--tvh-text-muted', '--tvh-bg-page'],
    ['primary as text on page', '--tvh-primary', '--tvh-bg-page'],
    ['primary as text on surface', '--tvh-primary', '--tvh-bg-surface'],
    ['on-primary over primary', '--tvh-on-primary', '--tvh-primary'],
    ['success on surface', '--tvh-success', '--tvh-bg-surface'],
    ['warning on surface', '--tvh-warning', '--tvh-bg-surface'],
    ['error on surface', '--tvh-error', '--tvh-bg-surface'],
  ])('%s meets 4.5:1', (_label, fg, bg) => {
    expect(contrast(token(fg), token(bg))).toBeGreaterThanOrEqual(4.5)
  })
})
