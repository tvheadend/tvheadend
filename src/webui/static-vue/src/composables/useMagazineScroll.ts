// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useMagazineScroll — scroll-control helpers for the EPG Magazine.
 *
 * Vertical-axis counterpart of `useTimelineScroll`. Caller passes a
 * template ref to the scroll container element; the composable
 * returns:
 *   - scrollToTime(targetSeconds, opts?) — pan the container so the
 *     given epoch-seconds is aligned per opts.align on the Y axis.
 *   - scrollToNow(opts?) — opinionated "show what's airing now":
 *     snaps to the last half-hour boundary and aligns it to the
 *     viewport's TOP edge. opts.align is ignored — Now's
 *     semantics are fixed.
 *
 * Math:
 *   yPx = headerHeight + (targetSeconds - dayStart) / 60 * pxPerMinute
 *   scrollTop = yPx − viewport*alignFraction
 *
 * The header row is the magazine's sticky-top channel labels; just
 * like Timeline's channel column eats the leftmost N pixels of the
 * scroll container, the magazine's header row eats the topmost N
 * pixels. Subtract that from the target so the scroll position
 * accounts for it.
 */
import { nextTick, watch, type Ref } from 'vue'

export interface MagazineScrollOptions {
  align?: 'center' | 'topThird' | 'top'
  behavior?: ScrollBehavior
}

export interface UseMagazineScrollArgs {
  scrollEl: Ref<HTMLElement | null>
  headerHeight: Ref<number>
  pxPerMinute: Ref<number>
  /*
   * Origin time of the rendered track — the time at the topmost
   * scroll position. Was `dayStart` (full calendar day); the
   * `EpgMagazine` component now computes `effectiveStart` (snapped
   * to an hour boundary, derived from the earliest event) so the
   * track is trimmed to actual data, and scroll math uses the same
   * origin. Pass that here.
   */
  effectiveStart: Ref<number>
}

const ALIGN_FRACTIONS = {
  center: 0.5,
  topThird: 1 / 3,
  top: 0,
} as const

/* "Now" snap interval (seconds) — same rationale as the timeline
 * variant. The leftmost / TOPMOST visible cell in Magazine is the
 * last half-hour boundary so currently-airing events render
 * without the server-filtered "stop < now" blank wedge. */
const NOW_SNAP_SECONDS = 30 * 60

export function useMagazineScroll(args: UseMagazineScrollArgs) {
  function scrollToTime(targetSeconds: number, opts: MagazineScrollOptions = {}) {
    const el = args.scrollEl.value
    if (!el) return
    const align = opts.align ?? 'topThird'
    const offsetMin = (targetSeconds - args.effectiveStart.value) / 60
    const yPx = args.headerHeight.value + offsetMin * args.pxPerMinute.value
    /* For 'top', anchor below the sticky channel-header row — the
     * topmost `headerHeight` viewport pixels are obscured by the
     * sticky header, so anchoring to viewport-pixel 0 would put
     * the target behind it. (Mirrors the channel-column fix in
     * useTimelineScroll — `align: 'top'` for scrollToNow's
     * snap-to-half-hour was shifting visible rows ~headerHeight/
     * pxPerMinute minutes downward.) 'topThird' / 'center' sit
     * well below the header row already so the original math
     * stays. */
    const targetViewportPixel =
      align === 'top'
        ? args.headerHeight.value
        : el.clientHeight * ALIGN_FRACTIONS[align]
    const targetScrollTop = Math.max(0, yPx - targetViewportPixel)
    el.scrollTo({
      top: targetScrollTop,
      behavior: opts.behavior ?? 'smooth',
    })
  }

  function scrollToNow(opts: MagazineScrollOptions = {}) {
    const nowSec = Math.floor(Date.now() / 1000)
    /* Snap to the previous half-hour boundary and pin to the TOP
     * edge — same reasoning as the timeline variant: server
     * filters past events (`stop < now`), so any topThird-aligned
     * scroll would leave the upper cells blank. `align` from
     * opts is intentionally overridden; callers wanting an
     * arbitrary scroll use scrollToTime. */
    const snapped = Math.floor(nowSec / NOW_SNAP_SECONDS) * NOW_SNAP_SECONDS
    scrollToTime(snapped, { ...opts, align: 'top' })
  }

  /*
   * Density-change anchor preservation — vertical-axis counterpart
   * of the same trick in `useTimelineScroll`. When pxPerMinute
   * changes, capture the time at the viewport's top edge BEFORE the
   * new layout renders, await the DOM update, then re-scroll to
   * that time so the user keeps reading from the same hour.
   *
   * Anchor-time inversion of `scrollToTime`'s forward math:
   *
   *   scrollToTime sets scrollTop = yPx − headerHeight
   *     where yPx = headerHeight + (target − effectiveStart)/60 * pxm
   *
   * so the visible-top time at any scrollTop is
   *
   *   anchorTime = effectiveStart + scrollTop / oldPxm * 60
   *
   * `scrollTop` IS the offset past the time-grid origin in
   * absolute coords — the sticky header floats over it, it isn't
   * a viewport-coord subtraction. Subtracting `headerHeight`
   * here mixes the two coord systems and captures an anchor
   * `headerHeight / oldPxm` minutes EARLIER than what the user
   * is actually looking at; on each density toggle the re-scroll
   * lands at that earlier time and the programme drifts down,
   * compounding toggle-after-toggle.
   *
   * `align: 'top'` matches the reading direction (top-to-bottom);
   * negative scrollTop is unreachable on this view but defensively
   * skipped anyway.
   */
  watch(args.pxPerMinute, async (newPxm, oldPxm) => {
    if (newPxm === oldPxm) return
    const el = args.scrollEl.value
    if (!el) return
    if (el.scrollTop < 0) return
    const anchorTime = args.effectiveStart.value + (el.scrollTop / oldPxm) * 60
    await nextTick()
    scrollToTime(anchorTime, { align: 'top', behavior: 'instant' })
  })

  return { scrollToTime, scrollToNow }
}
