// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Grid header and row rhythm.
 *
 *   1. Column titles may take two lines, so a label of several
 *      words is not cut to "Automatic checki…".
 *   2. Every grid body row has the height of one token. A cell with
 *      a 28 px Play or Info button used to make a 45 px row, while
 *      the virtual scroller of those grids assumes `itemSize: 36`.
 *      The token and every grid `itemSize` in the source must agree.
 *
 * Parsed with postcss, as in themeScale.test.ts: happy-dom does not
 * lay out tables or resolve custom properties from stylesheets.
 */
import { describe, expect, it } from 'vitest'
import postcss, { type Root, type Rule } from 'postcss'
import { readdirSync, readFileSync, statSync } from 'node:fs'
import { join, resolve } from 'node:path'

const SRC = resolve(process.cwd(), 'src')

function read(rel: string): string {
  return readFileSync(join(SRC, rel), 'utf8')
}

/* The component's scoped style block: a `<style scoped>` tag at the
 * start of a line (comments elsewhere mention `<style>` too). */
function scopedStyle(vueRel: string): Root {
  const m = /^<style scoped>$([\s\S]*?)^<\/style>$/m.exec(read(vueRel))
  if (!m) throw new Error(`no <style scoped> block in ${vueRel}`)
  return postcss.parse(m[1])
}

/* Declarations of the rules whose selector list contains `selector`. */
function declsFor(ast: Root, selector: string): Map<string, string> {
  const out = new Map<string, string>()
  ast.walkRules((rule: Rule) => {
    if (!rule.selectors.map((s) => s.trim()).includes(selector)) return
    rule.walkDecls((d) => {
      out.set(d.prop, d.value.trim())
    })
  })
  return out
}

function rootToken(name: string, selector = ':root'): string | undefined {
  return declsFor(postcss.parse(read('styles/tokens.css')), selector).get(name)
}

function sourceFiles(dir: string): string[] {
  const out: string[] = []
  for (const name of readdirSync(dir)) {
    const p = join(dir, name)
    if (statSync(p).isDirectory()) {
      if (name !== '__tests__') out.push(...sourceFiles(p))
    } else if (/\.(vue|ts)$/.test(name)) {
      out.push(p)
    }
  }
  return out
}

describe('grid column titles', () => {
  const title = declsFor(postcss.parse(read('styles/primevue.css')), '.p-datatable-column-title')

  it('wrap onto at most two lines', () => {
    /* The resizable table sets nowrap on the th. */
    expect(title.get('white-space')).toBe('normal')
    expect(title.get('display')).toBe('-webkit-box')
    expect(title.get('-webkit-box-orient')).toBe('vertical')
    expect(title.get('-webkit-line-clamp')).toBe('2')
  })

  it('keep a one-line header as tall as a body row', () => {
    const th = declsFor(postcss.parse(read('styles/primevue.css')), '.p-datatable-thead > tr > th')
    expect(th.get('height')).toBe('var(--tvh-row-height)')
  })

  it('still end in an ellipsis when they do not fit', () => {
    expect(title.get('overflow')).toBe('hidden')
    expect(title.get('text-overflow')).toBe('ellipsis')
    expect(title.get('min-width')).toBe('0')
  })
})

describe('grid row height', () => {
  const td = declsFor(
    scopedStyle('components/DataGrid.vue'),
    '.data-grid__table :deep(.p-datatable-tbody > tr > td)',
  )

  it('comes from --tvh-row-height, not from the cell padding', () => {
    expect(td.get('height')).toBe('var(--tvh-row-height)')
    expect(td.get('padding-block')).toBe('0')
  })

  it('keeps wrapped tag chips off the row borders', () => {
    /* The Reorganize drawer's chip cell is the one cell that can
     * grow past the token, so it brings its own vertical padding. */
    const chips = declsFor(scopedStyle('components/EditableTagChipCell.vue'), '.tag-chip-cell')
    expect(chips.get('flex-wrap')).toBe('wrap')
    expect(chips.get('padding-block')).toBe('var(--tvh-space-1)')
  })

  it('fits the 28 px Play and Info buttons', () => {
    const px = Number.parseFloat(rootToken('--tvh-row-height') ?? '')
    expect(px).toBeGreaterThanOrEqual(28 + 1 /* bottom border */)
  })

  it('keeps the Access theme rows as tall as its 1.5× text needs', () => {
    /* 13 px × 1.5 × 1.45 line height, plus 8 px above and below. */
    const px = Number.parseFloat(rootToken('--tvh-row-height', "[data-theme='access']") ?? '')
    expect(px).toBeGreaterThanOrEqual(Math.floor(13 * 1.5 * 1.45 + 16))
  })

  it('matches the itemSize every grid virtual scroller assumes', () => {
    const token = Number.parseFloat(rootToken('--tvh-row-height') ?? '')
    const sizes = new Set<number>()
    for (const file of sourceFiles(SRC)) {
      /* Form fields scroll Listbox options, not grid rows. */
      if (file.includes('/idnode-fields/')) continue
      for (const m of readFileSync(file, 'utf8').matchAll(/itemSize:\s*(\d+)/g)) {
        sizes.add(Number(m[1]))
      }
    }
    /* Guard against a vacuous pass. */
    expect(sizes.size).toBeGreaterThan(0)
    expect([...sizes]).toEqual([token])
  })
})
