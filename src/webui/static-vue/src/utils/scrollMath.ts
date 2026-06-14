// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Compute the scrollTop position that centres a row of height
 * `itemSize` (at logical index `index`) within a viewport of
 * `clientHeight` pixels. Floors at 0 — if the centring math
 * would scroll above the start of the list, the function
 * returns 0 so the top row stays visible. The caller is
 * responsible for clamping at the maximum scroll position
 * (i.e. for very small datasets where the row's centred
 * position exceeds `scrollHeight - clientHeight`); the browser
 * native `scrollTo` clamps automatically when the value is too
 * large, so this helper only floors at the lower bound. */
export function centredScrollTop(
  index: number,
  itemSize: number,
  clientHeight: number
): number {
  return Math.max(0, index * itemSize - clientHeight / 2 + itemSize / 2)
}
