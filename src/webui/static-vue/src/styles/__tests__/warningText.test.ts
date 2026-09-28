// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Warning text in the Light theme.
 *
 * --tvh-warning (#d97706) is an icon and fill colour. As text it
 * reads 3.2:1 on white and 2.7:1 on the tinted Home "failed
 * recordings" chip, below WCAG AA (4.5:1). --tvh-warning-text is
 * the colour for words, and has to pass on the chip's own tint as
 * well as on plain surfaces. The dark palettes keep their light
 * amber, which already passes, through an alias.
 *
 * Parsed with postcss, as in darkTheme.test.ts.
 */
import { describe, expect, it } from 'vitest'
import postcss, { type Root, type Rule } from 'postcss'
import { readFileSync } from 'node:fs'
import { join, resolve } from 'node:path'

const SRC = resolve(process.cwd(), 'src')
const tokens = postcss.parse(readFileSync(join(SRC, 'styles/tokens.css'), 'utf8'))

function declsFor(ast: Root, selector: string): Map<string, string> {
  const out = new Map<string, string>()
  ast.walkRules((rule: Rule) => {
    if (rule.parent?.type !== 'root') return
    if (!rule.selectors.map((s) => s.trim()).includes(selector)) return
    rule.walkDecls((d) => {
      out.set(d.prop, d.value.trim())
    })
  })
  return out
}

function scopedStyle(rel: string): Root {
  const m = /^<style scoped>$([\s\S]*?)^<\/style>$/m.exec(readFileSync(join(SRC, rel), 'utf8'))
  if (!m) throw new Error(`no <style scoped> block in ${rel}`)
  return postcss.parse(m[1])
}

/* --- WCAG 2.x relative luminance and contrast. --- */

type Rgb = [number, number, number]

function rgb(hex: string): Rgb {
  const h = hex.replace('#', '')
  return [0, 2, 4].map((i) => Number.parseInt(h.slice(i, i + 2), 16) / 255) as Rgb
}

function luminance(c: Rgb): number {
  const [r, g, b] = c.map((v) => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4))
  return 0.2126 * r + 0.7152 * g + 0.0722 * b
}

function contrast(a: Rgb, b: Rgb): number {
  const [la, lb] = [luminance(a), luminance(b)]
  return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05)
}

/* `color-mix(in srgb, <tint> <pct>, transparent)` over `under`. */
function tint(color: Rgb, pct: number, under: Rgb): Rgb {
  return color.map((c, i) => c * pct + under[i] * (1 - pct)) as Rgb
}

const light = declsFor(tokens, ':root')

function lightColor(name: string): Rgb {
  const v = light.get(name)
  if (!v || !v.startsWith('#')) throw new Error(`${name} is not a literal colour: ${v}`)
  return rgb(v)
}

describe('Light theme warning text', () => {
  const text = () => lightColor('--tvh-warning-text')
  const warning = () => lightColor('--tvh-warning')

  it.each(['--tvh-bg-surface', '--tvh-bg-page'])('meets 4.5:1 on %s', (bg) => {
    expect(contrast(text(), lightColor(bg))).toBeGreaterThanOrEqual(4.5)
  })

  it.each([
    ['the alert chip', 0.15],
    ['the hovered alert chip', 0.25],
  ])('meets 4.5:1 on %s', (_label, pct) => {
    /* RecentlyRecorded.vue tints its chip with the warning colour on
     * the card surface. */
    const bg = tint(warning(), pct, lightColor('--tvh-bg-surface'))
    expect(contrast(text(), bg)).toBeGreaterThanOrEqual(4.5)
  })

  it('is needed: the plain warning colour fails as text', () => {
    expect(contrast(warning(), lightColor('--tvh-bg-surface'))).toBeLessThan(4.5)
  })
})

describe('dark palettes', () => {
  it.each(["[data-theme='dark']", "[data-theme='access']"])(
    '%s keeps its warning colour for text',
    (selector) => {
      expect(declsFor(tokens, selector).get('--tvh-warning-text')).toBe('var(--tvh-warning)')
    },
  )
})

describe('warning text users', () => {
  it.each([
    ['views/home/RecentlyRecorded.vue', '.recently-recorded__alert'],
    ['components/IdnodeEditor.vue', '.ifld-row__counter--warn'],
  ])('%s colours %s with --tvh-warning-text', (file, selector) => {
    expect(declsFor(scopedStyle(file), selector).get('color')).toBe('var(--tvh-warning-text)')
  })
})
