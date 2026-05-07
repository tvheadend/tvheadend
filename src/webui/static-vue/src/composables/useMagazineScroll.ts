/*
 * useMagazineScroll — scroll-control helpers for the EPG Magazine.
 *
 * Vertical-axis counterpart of `useTimelineScroll`. Caller passes a
 * template ref to the scroll container element; the composable
 * returns:
 *   - scrollToTime(targetSeconds, opts?) — pan the container so the
 *     given epoch-seconds is aligned per opts.align on the Y axis.
 *   - scrollToNow(opts?) — convenience for "scroll to current time"
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

export function useMagazineScroll(args: UseMagazineScrollArgs) {
  function scrollToTime(targetSeconds: number, opts: MagazineScrollOptions = {}) {
    const el = args.scrollEl.value
    if (!el) return
    const align = opts.align ?? 'topThird'
    const fraction = ALIGN_FRACTIONS[align]
    const offsetMin = (targetSeconds - args.effectiveStart.value) / 60
    const yPx = args.headerHeight.value + offsetMin * args.pxPerMinute.value
    const targetScrollTop = Math.max(0, yPx - el.clientHeight * fraction)
    el.scrollTo({
      top: targetScrollTop,
      behavior: opts.behavior ?? 'smooth',
    })
  }

  function scrollToNow(opts: MagazineScrollOptions = {}) {
    const nowSec = Math.floor(Date.now() / 1000)
    scrollToTime(nowSec, opts)
  }

  /*
   * Density-change anchor preservation — vertical-axis counterpart
   * of the same trick in `useTimelineScroll`. When pxPerMinute
   * changes, capture the time at the viewport's top edge BEFORE the
   * new layout renders, await the DOM update, then re-scroll to
   * that time so the user keeps reading from the same hour.
   *
   * `align: 'top'` matches the reading direction (top-to-bottom);
   * negative anchorOffset (scroll-top above the header band) means
   * the user is already at the very top of the day — skip to avoid
   * a useless scroll-to-future event.
   */
  watch(args.pxPerMinute, async (newPxm, oldPxm) => {
    if (newPxm === oldPxm) return
    const el = args.scrollEl.value
    if (!el) return
    const anchorOffset = el.scrollTop - args.headerHeight.value
    if (anchorOffset < 0) return
    const anchorTime = args.effectiveStart.value + (anchorOffset / oldPxm) * 60
    await nextTick()
    scrollToTime(anchorTime, { align: 'top', behavior: 'instant' })
  })

  return { scrollToTime, scrollToNow }
}
