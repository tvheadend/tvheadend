// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgViewportEmitter — rAF-throttled scroll listener that
 * derives `update:activeDay` and `update:viewportRange` signals
 * from a continuous-scroll EPG component (EpgTimeline /
 * EpgMagazine). Both components share the same compute shape;
 * the only per-axis difference is which scroll-position +
 * client-extent pair to read and which sticky-pane offset to
 * subtract from the visible area.
 *
 *   - `activeDay`: the day-start epoch of the LEADING-EDGE time
 *     (start of viewport, i.e. `floor_to_day(startTime)`). This
 *     matches what the user is reading — the leftmost / topmost
 *     time-grid cell, immediately after the sticky channel
 *     column / header row. Clamped to the track bounds so the
 *     day-button highlight can't go outside the legal range.
 *
 *     Why not the viewport CENTRE: at late-evening anchors
 *     (e.g. 23:00) the leading edge sits at today 22:30 but the
 *     viewport stretches ~4 h forward, so the centre falls in
 *     tomorrow's early hours. A centre-time rule would emit
 *     Tomorrow even right after a Now click — the picker would
 *     disagree with what the user just asked for. The cascading
 *     `state.dayStart = tomorrow` write would also poison the
 *     sticky-position save (next mount would fetch tomorrow's
 *     events, the stored "today 22:30" scrollTime would compute
 *     a negative offset against the shifted effectiveStart, and
 *     scrollLeft would clamp to 0).
 *
 *     Leading-edge is the day the user is *anchored to*, which
 *     is what the same-day clamp check on restore needs to fire
 *     correctly.
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

/* Scroll snapshot piped to `onTick` consumers. The emitter
 * already reads these values once per rAF to derive
 * activeDay / viewportRange, so passing them through saves
 * downstream pin / allocator code from re-reading layout
 * properties inside the per-event loop — that re-read is the
 * iPhone reflow hot spot. */
export interface ViewportScrollState {
  /* `scrollLeft` (timeline) or `scrollTop` (magazine). */
  scrollPos: number
  /* `clientWidth - axisOffset` (timeline) or
   * `clientHeight - axisOffset` (magazine). The
   * axis-offset-subtracted extent is what
   * activeDay / viewportRange use; consumers needing the
   * raw extent (e.g. magazine allocator subtracting a
   * header height that isn't the same as axisOffset) read
   * `rawClientExtent`. */
  clientExtent: number
  rawClientExtent: number
  /* Perpendicular-axis scroll position. Timeline
   * (`axis: 'horizontal'`) reads `scrollTop`; Magazine
   * (`axis: 'vertical'`) reads `scrollLeft`. Drives DOM
   * virtualisation of the channel-list axis (rows for
   * Timeline, columns for Magazine) so the views can
   * render only the channels currently in view + an
   * overscan buffer. The axis-driven scroll is enough for
   * the time-axis virtualisation because that's the same
   * direction `useEpgViewportEmitter` was already
   * tracking; the perpendicular pair is the new piece. */
  crossScrollPos: number
  crossClientExtent: number
}

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
   * activeDay / viewportRange emits, with the scroll snapshot
   * the emitter just read. Optional — Timeline's pin and
   * Magazine's allocator both consume this so per-event work
   * doesn't have to re-read scrollPos / clientHeight on every
   * iteration. */
  onTick?: (state: ViewportScrollState) => void
}) {
  const isHoriz = opts.axis === 'horizontal'
  let rafToken: number | null = null

  function emitScrollState() {
    rafToken = null
    const el = opts.scrollEl.value
    const ppm = opts.pxPerMinute()
    if (!el || ppm <= 0) return
    const visiblePx = isHoriz ? el.scrollLeft : el.scrollTop
    const rawClientExtent = isHoriz ? el.clientWidth : el.clientHeight
    const crossScrollPos = isHoriz ? el.scrollTop : el.scrollLeft
    const crossClientExtent = isHoriz ? el.clientHeight : el.clientWidth
    const visibleExtent = Math.max(0, rawClientExtent - opts.axisOffset())
    const startTime = opts.trackStart() + (visiblePx / ppm) * 60
    const endTime = opts.trackStart() + ((visiblePx + visibleExtent) / ppm) * 60
    /* Leading-edge day — see the file header for the rationale.
     * Centre-time worked for mid-day scrolls but flipped to the
     * next day at late-evening Now anchors and corrupted the
     * sticky restore path through `state.dayStart`. */
    const activeDay = startOfLocalDayEpoch(startTime)
    const clampedActive = Math.max(
      opts.trackStart(),
      Math.min(opts.trackEnd() - ONE_DAY_SEC, activeDay),
    )
    opts.onActiveDay(clampedActive)
    opts.onViewportRange({ start: startTime, end: endTime })
    opts.onTick?.({
      scrollPos: visiblePx,
      clientExtent: visibleExtent,
      rawClientExtent,
      crossScrollPos,
      crossClientExtent,
    })
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
