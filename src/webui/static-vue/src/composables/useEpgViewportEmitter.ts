/*
 * useEpgViewportEmitter — rAF-throttled scroll listener that
 * derives `update:activeDay` and `update:viewportRange` signals
 * from a continuous-scroll EPG component (EpgTimeline /
 * EpgMagazine). Both components share the same compute shape;
 * the only per-axis difference is which scroll-position +
 * client-extent pair to read and which sticky-pane offset to
 * subtract from the visible area.
 *
 *   - `activeDay`: the day-start epoch occupying the largest
 *     portion of the viewport (centre-day; falls naturally out
 *     of `floor_to_day(viewportCentreTime)`). Clamped to the
 *     track bounds so the day-button highlight can't go outside
 *     the legal range.
 *   - `viewportRange`: the `[start, end]` epoch range in seconds.
 *     The parent uses it to lazy-fetch any unloaded day touching
 *     the visible window.
 *
 * Setup attaches the listener via a `watch(scrollEl)` so the
 * emitter latches on as soon as the rendering component exposes
 * its scroll element. Cleanup cancels any pending rAF and
 * detaches the listener on unmount.
 */
import { onBeforeUnmount, watch, type Ref } from 'vue'
import { startOfLocalDayEpoch } from '@/views/epg/epgGridShared'

const ONE_DAY_SEC = 24 * 60 * 60

export function useEpgViewportEmitter(opts: {
  axis: 'horizontal' | 'vertical'
  /* Scroll element ref. Listener attaches as soon as the ref
   * resolves (rendering component sets it after first paint). */
  scrollEl: Ref<HTMLElement | null>
  /* Sticky pane offset to subtract from the visible viewport
   * (Timeline: channel column width; Magazine: header row
   * height). Accessor so reactive sources update without an
   * extra computed ref level in the caller. */
  axisOffset: () => number
  trackStart: () => number
  trackEnd: () => number
  pxPerMinute: () => number
  onActiveDay: (epoch: number) => void
  onViewportRange: (range: { start: number; end: number }) => void
  /* Fires on every rAF-throttled scroll tick alongside the
   * activeDay / viewportRange emits. Optional — used by EPG
   * Magazine's allocator-aware-of-reduced-space path to bump a
   * reactive tick counter so per-event line-allocation re-runs
   * as the user scrolls past events. */
  onTick?: () => void
}) {
  const isHoriz = opts.axis === 'horizontal'
  let rafToken: number | null = null

  function emitScrollState() {
    rafToken = null
    const el = opts.scrollEl.value
    const ppm = opts.pxPerMinute()
    if (!el || ppm <= 0) return
    const visiblePx = isHoriz ? el.scrollLeft : el.scrollTop
    const visibleExtent = isHoriz
      ? Math.max(0, el.clientWidth - opts.axisOffset())
      : Math.max(0, el.clientHeight - opts.axisOffset())
    const startTime = opts.trackStart() + (visiblePx / ppm) * 60
    const endTime = opts.trackStart() + ((visiblePx + visibleExtent) / ppm) * 60
    const centreTime = (startTime + endTime) / 2
    const activeDay = startOfLocalDayEpoch(centreTime)
    const clampedActive = Math.max(
      opts.trackStart(),
      Math.min(opts.trackEnd() - ONE_DAY_SEC, activeDay),
    )
    opts.onActiveDay(clampedActive)
    opts.onViewportRange({ start: startTime, end: endTime })
    opts.onTick?.()
  }

  function onScroll() {
    if (rafToken !== null) return
    rafToken = requestAnimationFrame(emitScrollState)
  }

  /* Fire once on mount with the initial scroll position so the
   * parent loads today + tomorrow even before the user touches
   * the scrollbar. */
  watch(opts.scrollEl, (el) => {
    if (el) {
      el.addEventListener('scroll', onScroll, { passive: true })
      /* Defer one frame so the parent's data refs settle before
       * the first emission triggers a render. */
      requestAnimationFrame(emitScrollState)
    }
  })

  onBeforeUnmount(() => {
    if (rafToken !== null) cancelAnimationFrame(rafToken)
    opts.scrollEl.value?.removeEventListener('scroll', onScroll)
  })
}
