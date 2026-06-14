// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import { computeOverflow } from '../pageTabsOverflow'

/* Pure-helper unit tests for the PageTabs overflow predicates.
 * Same testing posture as `idnodeMove.ts` / `cloneIdnode.ts` /
 * `multiEditIdnode.ts` — no Vue, no DOM. */

describe('computeOverflow', () => {
  it('no overflow when content fits within viewport', () => {
    expect(
      computeOverflow({ scrollLeft: 0, scrollWidth: 600, clientWidth: 800 }),
    ).toEqual({ hasLeft: false, hasRight: false })
  })

  it('right overflow when scrolled to start with wider content', () => {
    expect(
      computeOverflow({ scrollLeft: 0, scrollWidth: 1200, clientWidth: 800 }),
    ).toEqual({ hasLeft: false, hasRight: true })
  })

  it('both overflows when scrolled to the middle', () => {
    expect(
      computeOverflow({ scrollLeft: 200, scrollWidth: 1200, clientWidth: 800 }),
    ).toEqual({ hasLeft: true, hasRight: true })
  })

  it('left overflow only when scrolled to the very end', () => {
    expect(
      computeOverflow({ scrollLeft: 400, scrollWidth: 1200, clientWidth: 800 }),
    ).toEqual({ hasLeft: true, hasRight: false })
  })

  it('right edge stays clean within the 1 px sub-pixel tolerance', () => {
    /* When the row is "at the end" but rounding leaves
     * scrollLeft + clientWidth = scrollWidth - 0.5 due to
     * fractional layout, the user has effectively reached the
     * bottom of the strip — the fade shouldn't be visible. */
    expect(
      computeOverflow({ scrollLeft: 399.6, scrollWidth: 1200, clientWidth: 800 }),
    ).toEqual({ hasLeft: true, hasRight: false })
  })

  it('content exactly equal to viewport — no overflow on either edge', () => {
    expect(
      computeOverflow({ scrollLeft: 0, scrollWidth: 800, clientWidth: 800 }),
    ).toEqual({ hasLeft: false, hasRight: false })
  })

  it('zero-width edge cases (empty container) — neither overflow', () => {
    expect(
      computeOverflow({ scrollLeft: 0, scrollWidth: 0, clientWidth: 0 }),
    ).toEqual({ hasLeft: false, hasRight: false })
  })
})
