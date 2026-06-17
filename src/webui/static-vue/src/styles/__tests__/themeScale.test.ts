// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Theme-scale resolution tests for the --tvh-text-* token system.
 *
 * Verifies the contract that --tvh-text-scale resolves to the right
 * value per [data-theme=...] selector at desktop, and per
 * (data-theme × phone media query) combination. Components consume
 * named tokens (--tvh-text-md, --tvh-text-lg, …) which are calc()
 * products of the scale, so as long as the scale is correct, every
 * consuming site scales correctly.
 *
 * happy-dom's getComputedStyle doesn't resolve CSS custom properties
 * from injected stylesheets, so we parse tokens.css directly with
 * postcss and walk the resulting AST. The tests pin the source of
 * truth (the CSS file shipped to the browser) rather than re-stating
 * the contract in a fixture.
 */
import { describe, it, expect } from 'vitest'
import postcss, { type Root, type Rule, type AtRule, type Declaration } from 'postcss'
import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'

/* Load tokens.css directly via Node fs — Vite's `?raw` import query
 * isn't honoured by Vitest in this codebase. Vitest runs from the
 * project root (where vitest.config.ts lives), so the source-tree
 * path resolves there. The tests still pin the canonical CSS file
 * shipped to the browser. */
const tokensCss = readFileSync(
  resolve(process.cwd(), 'src/styles/tokens.css'),
  'utf8',
)
const ast: Root = postcss.parse(tokensCss)

function findTopLevelRule(selector: string): Rule {
  let found: Rule | undefined
  ast.walkRules((rule) => {
    if (rule.parent?.type !== 'root') return
    /* Selectors can include multiple comma-separated forms; match if
     * any one equals the target. */
    const parts = rule.selectors.map((s) => s.trim())
    if (parts.includes(selector)) found = rule
  })
  if (!found) throw new Error(`Top-level rule not found: ${selector}`)
  return found
}

function findMediaRule(mediaParams: string, selector: string): Rule {
  let found: Rule | undefined
  ast.walkAtRules('media', (atrule: AtRule) => {
    if (!atrule.params.includes(mediaParams)) return
    atrule.walkRules((rule) => {
      const parts = rule.selectors.map((s) => s.trim())
      if (parts.includes(selector)) found = rule
    })
  })
  if (!found) {
    throw new Error(
      `Media rule not found: @media ${mediaParams} -> ${selector}`,
    )
  }
  return found
}

function getDecl(rule: Rule, prop: string): string {
  let value: string | undefined
  rule.walkDecls(prop, (d: Declaration) => {
    value = d.value
  })
  if (value === undefined) {
    throw new Error(`Declaration ${prop} not found in rule ${rule.selector}`)
  }
  return value
}

describe('--tvh-text-scale per theme (desktop)', () => {
  it(':root declares the default scale of 1', () => {
    const root = findTopLevelRule(':root')
    expect(getDecl(root, '--tvh-text-scale')).toBe('1')
  })

  it("[data-theme='access'] declares a desktop scale of 1.5", () => {
    /* Access is the high-contrast white-on-dark theme; the larger
     * text-scale is part of its readability contract. */
    const rule = findTopLevelRule("[data-theme='access']")
    expect(getDecl(rule, '--tvh-text-scale')).toBe('1.5')
  })

  it("[data-theme='gray'] inherits the :root scale of 1", () => {
    /* Gray intentionally does NOT override --tvh-text-scale on
     * desktop — the cascade from :root delivers scale=1. Asserting
     * absence is part of the contract: if a future edit accidentally
     * declares the variable on the Gray block, the test catches it. */
    const gray = findTopLevelRule("[data-theme='gray']")
    expect(() => getDecl(gray, '--tvh-text-scale')).toThrow()
  })
})

describe('--tvh-text-scale per theme (phone @media max-width: 767px)', () => {
  it('Blue and Gray have no phone override — inherit desktop scale 1', () => {
    /* Asserting absence: if a future edit accidentally adds a phone
     * override for either default-scale theme, the test fails. The
     * architecture supports the override (just add the selector to
     * the @media block); we deliberately don't apply one today. */
    expect(() => findMediaRule('max-width: 767px', ':root')).toThrow()
    expect(() => findMediaRule('max-width: 767px', "[data-theme='gray']")).toThrow()
  })

  it("[data-theme='access'] scales 1.15 on phone", () => {
    /* Access ratchets back from desktop's 1.5× so grid headers and
     * toolbar action labels stay inside the phone viewport. */
    const rule = findMediaRule('max-width: 767px', "[data-theme='access']")
    expect(getDecl(rule, '--tvh-text-scale')).toBe('1.15')
  })
})

describe('--tvh-text-* named tokens compose with the scale', () => {
  /*
   * The 8-step semantic scale lives at :root and is the entry point
   * for every consuming style. If any of these tokens are renamed or
   * detached from --tvh-text-scale, downstream code breaks silently;
   * these tests catch that early.
   */
  const root = findTopLevelRule(':root')

  it.each([
    ['--tvh-text-xs', '11px'],
    ['--tvh-text-sm', '12px'],
    ['--tvh-text-md', '13px'],
    ['--tvh-text-lg', '14px'],
    ['--tvh-text-xl', '16px'],
    ['--tvh-text-2xl', '18px'],
    ['--tvh-text-3xl', '22px'],
    ['--tvh-text-display', '28px'],
  ])('%s = calc(%s * var(--tvh-text-scale))', (token, base) => {
    const value = getDecl(root, token)
    expect(value.replace(/\s+/g, ' ')).toBe(
      `calc(${base} * var(--tvh-text-scale))`,
    )
  })

  it('--tvh-font-size legacy alias points at --tvh-text-md', () => {
    expect(getDecl(root, '--tvh-font-size')).toBe('var(--tvh-text-md)')
  })
})
