// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Pure helper for the PageTabs overflow indicator. Takes the
 * three numeric scroll-state inputs and returns booleans for
 * whether the left and right edge fades should be visible.
 * Extracted from `PageTabs.vue` so the predicate is unit-
 * testable without standing up a real DOM layout (jsdom doesn't
 * compute scrollWidth / clientWidth).
 *
 * The 1 px tolerance on the right edge accounts for sub-pixel
 * rounding when the row is exactly at the end — without it,
 * fractional layouts sometimes leave the fade visible by 0.4px
 * worth of "overflow". */

export interface OverflowInput {
  scrollLeft: number
  scrollWidth: number
  clientWidth: number
}

export interface OverflowState {
  hasLeft: boolean
  hasRight: boolean
}

export function computeOverflow(input: OverflowInput): OverflowState {
  return {
    hasLeft: input.scrollLeft > 0,
    hasRight: input.scrollLeft + input.clientWidth < input.scrollWidth - 1,
  }
}
