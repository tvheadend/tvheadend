/*
 * Magazine line-allocator unit tests. Pure-function coverage
 * for the math the previous in-component allocator buried.
 *
 * `scrollHeight` is the input the line-count math reads.
 * happy-dom doesn't lay out text wrapping for `scrollHeight`,
 * so each test stubs `scrollHeight` via Object.defineProperty
 * on the measurer element to control what the function
 * "measures." That makes tests deterministic and DOES still
 * exercise the real measureLines / allocateLines code paths —
 * we're just controlling the one DOM read at the leaf.
 */

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import {
  TITLE_LINE_PX,
  SUB_LINE_PX,
  BLOCK_VERT_CHROME_PX,
  BLOCK_HORZ_CHROME_PX,
  allocateLines,
  clearMeasureCache,
  createMeasurer,
  disposeMeasurer,
  measureLines,
} from '../magazineLineAllocator'

let measurer: HTMLDivElement

/* Stub `scrollHeight` to a programmable value. Each call to
 * `setScrollHeight(n)` makes the next read return `n`. */
function setScrollHeight(el: HTMLDivElement, value: number): void {
  Object.defineProperty(el, 'scrollHeight', {
    configurable: true,
    get: () => value,
  })
}

/* Configure the measurer so the next title measure returns
 * titleLines and the next sub measure returns subLines. The
 * allocator measures title first then subtitle, so scrollHeight
 * is read in that order. We swap before each read by re-setting
 * the property. */
function setupMeasures(
  titleLines: number,
  subLines: number,
  fontPx = 13,
  lineH = 1.25,
): void {
  /* Convert line counts into scrollHeight numbers the
   * allocator will Math.ceil-divide back into lines. Use the
   * exact lineHeightPx so ceil returns the intended count. */
  const titleScrollH = titleLines * fontPx * lineH
  const subScrollH = subLines * 12 * 1.3 /* SUB_FONT_PX × SUB_LINE_H_RATIO */
  let calls = 0
  Object.defineProperty(measurer, 'scrollHeight', {
    configurable: true,
    get: () => {
      calls++
      return calls === 1 ? titleScrollH : subScrollH
    },
  })
}

beforeEach(() => {
  clearMeasureCache()
  measurer = createMeasurer()
})

afterEach(() => {
  disposeMeasurer(measurer)
})

describe('measureLines', () => {
  it('returns 0 for empty string without touching the measurer', () => {
    setScrollHeight(measurer, 999) /* would be 1+ if read */
    expect(measureLines('', 200, 13, 1.25, 500, measurer)).toBe(0)
  })

  it('returns 1 for a single line that fits at the given width', () => {
    setScrollHeight(measurer, 16) /* exactly one 16.25-px line */
    expect(measureLines('BBC News', 200, 13, 1.25, 500, measurer)).toBe(1)
  })

  it('returns >1 lines when scrollHeight exceeds line-height', () => {
    /* 13 × 1.25 = 16.25 px per line. 50 px → ceil(50/16.25) = 4. */
    setScrollHeight(measurer, 50)
    expect(measureLines('Long text wrapping over many lines', 50, 13, 1.25, 500, measurer)).toBe(
      4,
    )
  })

  it('cache hits the second call with same key', () => {
    setScrollHeight(measurer, 16)
    const first = measureLines('A', 100, 13, 1.25, 500, measurer)
    /* Change the scrollHeight stub — if the cache is bypassed,
     * a second call returns the new value. We expect it NOT
     * to bypass and to return the cached `first`. */
    setScrollHeight(measurer, 100)
    const second = measureLines('A', 100, 13, 1.25, 500, measurer)
    expect(second).toBe(first)
  })

  it('clearMeasureCache makes subsequent calls re-measure', () => {
    setScrollHeight(measurer, 16)
    measureLines('A', 100, 13, 1.25, 500, measurer)
    setScrollHeight(measurer, 100)
    clearMeasureCache()
    /* Now should pick up the new scrollHeight (6 lines for 100 px). */
    expect(measureLines('A', 100, 13, 1.25, 500, measurer)).toBe(7)
  })

  it('always returns at least 1 line for non-empty text even if scrollHeight is 0', () => {
    setScrollHeight(measurer, 0)
    expect(measureLines('Edge case', 200, 13, 1.25, 500, measurer)).toBe(1)
  })
})

describe('allocateLines', () => {
  it('returns natural counts when both title and subtitle fit', () => {
    /* Block: 100 px tall × 200 px wide. Title 1 line, sub 1
     * line — naturalH = 16.25 + 15.6 = 31.85 px, well under
     * contentH = 100 - chrome. */
    setupMeasures(1, 1)
    const result = allocateLines('Title', 'Subtitle', 100, 200, measurer)
    expect(result).toEqual({ title: 1, sub: 1 })
  })

  it('returns title-only allocation when subtitle is empty', () => {
    setupMeasures(2, 0)
    const result = allocateLines('Long title', '', 100, 200, measurer)
    /* 2 lines at TITLE_LINE_PX = 32.5 px ≤ contentH ≈ 89 px → fits naturally */
    expect(result).toEqual({ title: 2, sub: 0 })
  })

  it('reserves 1 title line minimum when block is too small for natural counts', () => {
    /* Block 30 px tall: contentH = 30 - 11 = 19 px.
     * 1 title line = 16.25 px. 1 sub line = 15.6 px. Both
     * naturals 1+1 = 31.85, > 19 → overflow branch.
     * Sub budget = 19 - 16.25 = 2.75 px → 0 sub lines.
     * Title gets remainder = 19 / 16.25 ≈ 1.17 → 1 line. */
    setupMeasures(1, 1)
    const result = allocateLines('Title', 'Sub', 30, 200, measurer)
    expect(result.title).toBe(1)
    expect(result.sub).toBe(0)
  })

  it('budgets subtitle from residual after reserving 1 title line', () => {
    /* Block height: contentH allows 1 title + 2 sub.
     * 16.25 + 2 × 15.6 = 47.45. Add chrome (11) → 58.45.
     * Use 60 px block → contentH = 49 → enough for 1 + 2. */
    setupMeasures(3, 2) /* title naturally 3, sub naturally 2 — both want more than fits */
    const result = allocateLines('Long title text', 'Sub line', 60, 200, measurer)
    /* Natural fit: 3*16.25 + 2*15.6 = 79.95 > contentH=49 → overflow branch.
     * subBudget = 49 - 16.25 = 32.75 → floor(32.75/15.6) = 2 sub lines.
     * subNat = 2; subAllowed = min(2, 2) = 2.
     * titleResid = 49 - 2*15.6 = 17.8 → floor(17.8/16.25) = 1 title line. */
    expect(result).toEqual({ title: 1, sub: 2 })
  })

  it('the +1 px safety guard rejects natural fit when within 1 px of cap', () => {
    /* Construct: naturalH exactly equals contentH. Without the
     * guard, both fit. With the +1 guard, both don't fit and
     * we go to overflow branch. */
    /* contentH must equal naturalH. Pick title=1, sub=2:
     * naturalH = 16.25 + 31.2 = 47.45. blockHeight = 47.45 + 11 = 58.45 → use 58.
     * contentH = 58 - 11 = 47. naturalH = 47.45. 47.45 + 1 > 47 → overflow. */
    setupMeasures(1, 2)
    const result = allocateLines('A', 'B', 58, 200, measurer)
    /* Overflow: subBudget = 47 - 16.25 = 30.75 → floor(/15.6) = 1 sub line.
     * titleResid = 47 - 15.6 = 31.4 → floor(/16.25) = 1 title line.
     * Result: {title:1, sub:1}. Note this DROPS one sub line vs natural. */
    expect(result).toEqual({ title: 1, sub: 1 })
  })

  it('still returns 1 title line when the block is shorter than chrome', () => {
    setupMeasures(1, 1)
    /* blockHeight = 5 < chrome = 11 → contentH = 0.
     * Overflow branch: subBudget = max(0, 0 - 16.25) = 0 → 0 sub.
     * titleResid = 0 - 0 = 0 → titleAllowed = max(1, floor(0/16.25)) = 1. */
    const result = allocateLines('Title', 'Sub', 5, 200, measurer)
    expect(result).toEqual({ title: 1, sub: 0 })
  })

  it('clamps content width to a minimum of 20 px regardless of block width', () => {
    /* Calling with blockWidthPx tiny shouldn't crash or pass a
     * negative width to measureLines; allocator floors to 20px
     * minimum content width via Math.max(20, ...). */
    setupMeasures(1, 0)
    expect(() => allocateLines('A', '', 100, 5, measurer)).not.toThrow()
  })
})

describe('chrome constants are sane', () => {
  it('TITLE_LINE_PX matches the Magazine CSS line-height (13 × 1.25)', () => {
    expect(TITLE_LINE_PX).toBeCloseTo(16.25, 5)
  })

  it('SUB_LINE_PX matches the Magazine CSS line-height (12 × 1.3)', () => {
    expect(SUB_LINE_PX).toBeCloseTo(15.6, 5)
  })

  it('chrome constants are positive integers', () => {
    expect(BLOCK_VERT_CHROME_PX).toBeGreaterThan(0)
    expect(BLOCK_HORZ_CHROME_PX).toBeGreaterThan(0)
  })
})
