/*
 * useTimelineScroll — scroll-control helpers for the EPG timeline.
 *
 * Caller passes a template ref to the scroll container element
 * (`<div class="epg-timeline" ref="scrollEl">`). The composable
 * returns:
 *   - scrollToTime(targetSeconds, opts?) — pan the container so the
 *     given epoch-seconds is centred (or aligned per opts.align).
 *   - scrollToNow(opts?) — convenience for "scroll to current time"
 *
 * Centring vs left-aligning: most EPG viewers align "now" to a third
 * of the visible width so the user sees a bit of past plus the
 * upcoming events. Configurable via the `align` option:
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

export function useTimelineScroll(args: UseTimelineScrollArgs) {
  function scrollToTime(targetSeconds: number, opts: TimelineScrollOptions = {}) {
    const el = args.scrollEl.value
    if (!el) return
    const align = opts.align ?? 'leftThird'
    const fraction = ALIGN_FRACTIONS[align]
    const offsetMin = (targetSeconds - args.effectiveStart.value) / 60
    /* Channel column eats the leftmost N pixels of the scroll container,
     * so the absolute x for a given minute is offset by that width. */
    const xPx = args.channelColumnWidth.value + offsetMin * args.pxPerMinute.value
    /* Subtract the visible-area fraction so the target lands at the
     * desired alignment within the visible viewport. clientWidth here
     * is the SCROLL container's visible width — not its full content
     * width. */
    const targetScrollLeft = Math.max(0, xPx - el.clientWidth * fraction)
    el.scrollTo({
      left: targetScrollLeft,
      behavior: opts.behavior ?? 'smooth',
    })
  }

  function scrollToNow(opts: TimelineScrollOptions = {}) {
    const nowSec = Math.floor(Date.now() / 1000)
    scrollToTime(nowSec, opts)
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
   * `align: 'left'` because the user was reading from the visible-
   * left edge; centring would shift their reference frame. Negative
   * anchor offset (scroll-anchor before dayStart) is unreachable on
   * this view but defensively skipped anyway.
   */
  watch(args.pxPerMinute, async (newPxm, oldPxm) => {
    if (newPxm === oldPxm) return
    const el = args.scrollEl.value
    if (!el) return
    const anchorOffset = el.scrollLeft - args.channelColumnWidth.value
    if (anchorOffset < 0) return
    const anchorTime = args.effectiveStart.value + (anchorOffset / oldPxm) * 60
    await nextTick()
    scrollToTime(anchorTime, { align: 'left', behavior: 'instant' })
  })

  return { scrollToTime, scrollToNow }
}
