// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { centredScrollTop } from '../scrollMath'

describe('centredScrollTop', () => {
  it('floors at 0 when the row is in the first half-viewport', () => {
    /* Row 0 at itemSize 36, viewport 600. Naive math:
     * 0*36 - 600/2 + 36/2 = -282 → clamped to 0. */
    expect(centredScrollTop(0, 36, 600)).toBe(0)
    expect(centredScrollTop(5, 36, 600)).toBe(0) // 180 - 300 + 18 = -102
  })

  it('centres a row when the centred position is positive', () => {
    /* Row 20 at itemSize 36, viewport 600.
     * 20*36 - 600/2 + 36/2 = 720 - 300 + 18 = 438. */
    expect(centredScrollTop(20, 36, 600)).toBe(438)
  })

  it('handles itemSize of 1 cleanly', () => {
    /* Row 100 at itemSize 1, viewport 200.
     * 100 - 100 + 0.5 = 0.5 → returned as-is (no rounding here). */
    expect(centredScrollTop(100, 1, 200)).toBe(0.5)
  })

  it('returns 0 when clientHeight exceeds 2 * (index * itemSize + itemSize/2)', () => {
    /* Tall viewport, low index — entire list fits, no scroll
     * needed even to "centre" the row. */
    expect(centredScrollTop(2, 36, 2000)).toBe(0)
  })

  it('returns positive values for large indices', () => {
    expect(centredScrollTop(1000, 36, 600)).toBe(1000 * 36 - 300 + 18)
  })

  it('itemSize 0 collapses to a non-negative result', () => {
    /* Defensive: itemSize 0 means every "row" is at scrollTop 0.
     * The formula: 0 - 300 + 0 = -300 → clamped to 0. */
    expect(centredScrollTop(50, 0, 600)).toBe(0)
  })

  it('exact viewport-centre row produces scrollTop equal to row offset', () => {
    /* When itemSize equals viewport (1 row fills the viewport),
     * row N's offset is N*itemSize and the centred scrollTop is
     * N*itemSize - itemSize/2 + itemSize/2 = N*itemSize. */
    expect(centredScrollTop(5, 600, 600)).toBe(5 * 600)
    expect(centredScrollTop(0, 600, 600)).toBe(0)
  })
})
