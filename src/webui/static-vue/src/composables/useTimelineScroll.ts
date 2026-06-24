// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useTimelineScroll — scroll-control helpers for the EPG timeline.
 *
 * Caller passes a template ref to the scroll container element
 * (`<div class="epg-timeline" ref="scrollEl">`). The composable
 * returns:
 *   - scrollToTime(targetSeconds, opts?) — pan the container so the
 *     given epoch-seconds is centred (or aligned per opts.align).
 *   - scrollToNow(opts?) — opinionated "show what's airing now":
 *     snaps to the last half-hour boundary and aligns it to the
 *     viewport's LEFT edge so the leftmost cells render currently-
 *     airing events (not the blank wedge left behind by the
 *     server's `stop < now` event filter — see src/epg.c:2335).
 *     The `align` opt is intentionally ignored — Now's semantics
 *     are fixed; arbitrary positioning is for scrollToTime.
 *
 * scrollToTime align options:
 *   - 'center'   — target is at viewport center
 *   - 'leftThird' — target is at 1/3 from the left edge (default)
 *   - 'left'     — target is at the left edge (useful for scrollToTime
 *                   from a day-start timestamp)
 *
 * Math:
 *   xPx = channelColumnWidth + (targetSeconds - dayStart) / 60 * pxPerMinute
 *   scrollLeft = xPx − viewport*alignFraction
 */
import { nextTick, watch, type Ref } from 'vue'

export interface TimelineScrollOptions {
  align?: 'center' | 'leftThird' | 'left'
  behavior?: ScrollBehavior
}

export interface UseTimelineScrollArgs {
  scrollEl: Ref<HTMLElement | null>
  channelColumnWidth: Ref<number>
  pxPerMinute: Ref<number>
  /*
   * Origin time of the rendered track — the time at the leftmost
   * scroll position. Was `dayStart` (full calendar day); the
   * `EpgTimeline` component now computes `effectiveStart` (snapped
   * to an hour boundary, derived from the earliest event) so the
   * track is trimmed to actual data, and scroll math uses the same
   * origin. Pass that here.
   */
  effectiveStart: Ref<number>
}

const ALIGN_FRACTIONS = {
  center: 0.5,
  leftThird: 1 / 3,
  left: 0,
} as const

/* "Now" snap interval (seconds). Aligning the scroll to the most
 * recent 30-min boundary means the leftmost visible cell always
 * shows a currently-airing event (events that started at :00 or
 * :30 are visible from their start). 30 min strikes the right
 * balance: long enough that the now-cursor isn't right at the
 * edge of the viewport for most wall-clock times, short enough
 * that no large pre-now wedge appears. */
const NOW_SNAP_SECONDS = 30 * 60

export function useTimelineScroll(args: UseTimelineScrollArgs) {
  function scrollToTime(targetSeconds: number, opts: TimelineScrollOptions = {}) {
    const el = args.scrollEl.value
    if (!el) return
    const align = opts.align ?? 'leftThird'
    const offsetMin = (targetSeconds - args.effectiveStart.value) / 60
    /* Channel column eats the leftmost N pixels of the scroll container,
     * so the absolute x for a given minute is offset by that width. */
    const xPx = args.channelColumnWidth.value + offsetMin * args.pxPerMinute.value
    /* Where in the viewport do we want the target to appear?
     *
     *   'left'      — at the left edge of the TIME-GRID area, i.e.
     *                 right after the sticky channel column. The
     *                 leftmost `channelColumnWidth` viewport pixels
     *                 are obscured by the sticky channel column;
     *                 anchoring to viewport-pixel 0 would put the
     *                 target behind it. (This was the original
     *                 bug — `align: 'left'` for scrollToNow's
     *                 snap-to-half-hour was shifting the visible
     *                 cells ~channelColumnWidth/pxPerMinute minutes
     *                 to the right of the intended position.)
     *   'leftThird'/'center' — fraction of the whole viewport width.
     *                 These already sit well past the channel
     *                 column for any reasonable channelColumnWidth,
     *                 so the original math stays. */
    const targetViewportPixel =
      align === 'left'
        ? args.channelColumnWidth.value
        : el.clientWidth * ALIGN_FRACTIONS[align]
    const targetScrollLeft = Math.max(0, xPx - targetViewportPixel)
    el.scrollTo({
      left: targetScrollLeft,
      behavior: opts.behavior ?? 'smooth',
    })
  }

  function scrollToNow(opts: TimelineScrollOptions = {}) {
    const nowSec = Math.floor(Date.now() / 1000)
    /* Snap to the previous half-hour boundary; pin that to the
     * LEFT edge so currently-airing cells are visible without
     * the server-filtered "stop < now" blank wedge appearing.
     * `align` from opts is intentionally overridden — Now has
     * fixed semantics, callers wanting an arbitrary scroll use
     * scrollToTime. `behavior` is honoured. */
    const snapped = Math.floor(nowSec / NOW_SNAP_SECONDS) * NOW_SNAP_SECONDS
    scrollToTime(snapped, { ...opts, align: 'left' })
  }

  /*
   * Density-change anchor preservation.
   *
   * When `pxPerMinute` changes (user toggles the Density radio), the
   * day-track's pixel width scales but the scroll container's
   * `scrollLeft` stays the same numeric value — so the user is
   * suddenly looking at a different time. To preserve "what they
   * were looking at": capture the time at the visible-leading edge
   * BEFORE the new layout applies, await the DOM update, then re-
   * scroll to that time.
   *
   * Vue's default watch flush (`'pre'`) fires before the DOM
   * applies the prop change, so when this handler runs `el.scrollLeft`
   * still reflects the old layout — exactly what we need. The
   * `nextTick` then waits for the new track width to render before
   * we issue the corrective scroll.
   *
   * Anchor-time inversion of `scrollToTime`'s forward math:
   *
   *   scrollToTime sets scrollLeft = xPx − channelColumnWidth
   *     where xPx = channelColumnWidth + (target − effectiveStart)/60 * pxm
   *
   * so the visible-leading time at any scrollLeft is
   *
   *   anchorTime = effectiveStart + scrollLeft / oldPxm * 60
   *
   * `scrollLeft` IS the offset past the time-grid origin in
   * absolute coords — the sticky column floats over it, it isn't
   * a viewport-coord subtraction. Subtracting `channelColumnWidth`
   * here mixes the two coord systems and captures an anchor
   * `channelColumnWidth / oldPxm` minutes EARLIER than what the
   * user is actually looking at; on each density toggle the re-
   * scroll lands at that earlier time and the program drifts
   * right, compounding toggle-after-toggle.
   *
   * `align: 'left'` because the user was reading from the visible-
   * left edge; centring would shift their reference frame. Negative
   * scrollLeft is unreachable on this view but defensively skipped
   * anyway.
   */
  watch(args.pxPerMinute, async (newPxm, oldPxm) => {
    if (newPxm === oldPxm) return
    const el = args.scrollEl.value
    if (!el) return
    if (el.scrollLeft < 0) return
    const anchorTime = args.effectiveStart.value + (el.scrollLeft / oldPxm) * 60
    await nextTick()
    scrollToTime(anchorTime, { align: 'left', behavior: 'instant' })
  })

  return { scrollToTime, scrollToNow }
}
