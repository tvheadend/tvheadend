/*
 * epgBoxPin — pure math for pinning an EPG event box at the
 * leading edge of its scroll axis.
 *
 * Replaces the earlier `timelineTitleOffset` shift-based scheme
 * (which positioned only the inner title element via
 * `transform: translateX(...)`). The box-pin scheme positions
 * the BOX itself imperatively, eliminating the irreducible
 * sub-pixel sync problem between the compositor's smooth scroll
 * and the main-thread's per-tick transform write.
 *
 * Axis-agnostic by design: Timeline calls it with
 * (eventLeftPx, eventRightPx, scrollLeft); Magazine with
 * (blockTrackTop, blockTrackBottom, scrollTop). Both consume
 * the same `pinnedStart` / `pinnedSize` / `pinned` triple.
 *
 * Behaviour:
 *   - scrollPos ≤ naturalStart  → box at natural geometry,
 *     pinned = 0 (gradient affordance off).
 *   - scrollPos > naturalStart  → box pinned: leading edge at
 *     scrollPos, trailing edge unchanged. Width/height shrinks
 *     from the leading edge as the user scrolls further past.
 *     pinned = 1 (gradient on).
 *
 * When pinned, the box's viewport-relative position is constant
 * (`pinnedStart − scrollPos = 0` in body coords) frame-for-
 * frame, so there is no rounding for the browser to disagree
 * about between the compositor and main thread.
 */

export interface BoxPin {
  pinnedStart: number
  pinnedSize: number
  pinned: 0 | 1
}

export function computeBoxPin(
  naturalStart: number,
  naturalEnd: number,
  scrollPos: number,
): BoxPin {
  const naturalSize = Math.max(0, naturalEnd - naturalStart)
  if (scrollPos > naturalStart) {
    return {
      pinnedStart: scrollPos,
      pinnedSize: Math.max(0, naturalEnd - scrollPos),
      pinned: 1,
    }
  }
  return {
    pinnedStart: naturalStart,
    pinnedSize: naturalSize,
    pinned: 0,
  }
}
