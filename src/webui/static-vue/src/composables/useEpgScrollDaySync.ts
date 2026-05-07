/*
 * useEpgScrollDaySync — shared continuous-scroll ↔ day-cursor
 * synchronisation for the EPG view wrappers (TimelineView /
 * MagazineView). Both views maintain a continuous N-day track
 * scrolled along ONE axis (Timeline: horizontal; Magazine:
 * vertical) with the scroll-listener fanning out two signals:
 *
 *   - `update:activeDay` (epoch of viewport-centre day) → write
 *     `state.dayStart` so the toolbar day-button highlight
 *     follows.
 *   - `update:viewportRange` ({start, stop} in epoch seconds) →
 *     ensure ±1 day around the visible window is loaded.
 *
 * On the reverse path, when `state.dayStart` changes from the
 * toolbar (a button click), smooth-scroll the track to the new
 * day's offset. During that smooth-scroll we suppress the
 * scroll-listener writeback so the picklist label doesn't
 * flicker back to the starting day on the rAF tick that fires
 * before the browser starts moving the scroll position.
 *
 * Axis-agnostic: callers pass `axis: 'horizontal' | 'vertical'`
 * plus the corresponding `axisOffset` (Timeline: channel column
 * width; Magazine: header height) — those are the two pixel-axis
 * parameters that differ between layouts.
 */
import { watch, type ComputedRef } from 'vue'
import { ref } from 'vue'
import type { useEpgViewState } from './useEpgViewState'

const ONE_DAY_SEC = 24 * 60 * 60

function startOfLocalDay(epoch: number): number {
  const d = new Date(epoch * 1000)
  d.setHours(0, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
}

export function useEpgScrollDaySync(opts: {
  axis: 'horizontal' | 'vertical'
  scrollEl: ComputedRef<HTMLElement | null>
  /* Horizontal axis: channel column width subtracted from
   * clientWidth. Vertical axis: header height subtracted from
   * clientHeight. The composable subtracts it from the visible
   * viewport extent so the centre-day calculation respects the
   * visible content area, not the full scroll container. */
  axisOffset: ComputedRef<number>
  pxPerMinute: ComputedRef<number>
  state: ReturnType<typeof useEpgViewState>
}) {
  const { axis, scrollEl, axisOffset, pxPerMinute, state } = opts
  const isHorizontal = axis === 'horizontal'

  const expectingButtonScroll = ref(false)
  let scrollSettleTimer: ReturnType<typeof setTimeout> | null = null

  function onActiveDayChanged(epoch: number) {
    if (expectingButtonScroll.value) return
    if (state.dayStart.value !== epoch) state.setDayStart(epoch)
  }

  function onViewportRangeChanged(range: { start: number; end: number }) {
    const startDay = startOfLocalDay(range.start) - ONE_DAY_SEC
    const endDay = startOfLocalDay(range.end) + ONE_DAY_SEC
    const days: number[] = []
    for (let d = startDay; d <= endDay; d += ONE_DAY_SEC) {
      if (d >= state.trackStart.value && d < state.trackEnd.value) days.push(d)
    }
    state.ensureDaysLoaded(days)
  }

  /* Reverse path: dayStart changes from toolbar → smooth-scroll
   * to that day's pixel offset. */
  watch(
    () => state.dayStart.value,
    (newDay) => {
      const el = scrollEl.value
      if (!el) return

      const visibleExtent = isHorizontal
        ? Math.max(0, el.clientWidth - axisOffset.value)
        : Math.max(0, el.clientHeight - axisOffset.value)
      const currentScroll = isHorizontal ? el.scrollLeft : el.scrollTop
      const centrePx = currentScroll + visibleExtent / 2
      const centreTime = state.trackStart.value + (centrePx / pxPerMinute.value) * 60
      const currentScrollDay = startOfLocalDay(centreTime)
      if (currentScrollDay === newDay) return

      /* Suppress the listener writeback for the duration of the
       * smooth-scroll. The renderer's rAF-throttled scroll listener
       * can fire one more time before the browser starts moving the
       * scroll position, reading the still-old position and
       * emitting the OLD day — which would flip the picklist label
       * back to the starting day for ~1 s. Clear any pending settle
       * from a previous chained pick first. */
      if (scrollSettleTimer !== null) {
        clearTimeout(scrollSettleTimer)
        scrollSettleTimer = null
      }
      expectingButtonScroll.value = true

      const dayMin = (newDay - state.trackStart.value) / 60
      const targetPx = dayMin * pxPerMinute.value
      const scrollOpts: ScrollToOptions = isHorizontal
        ? { left: targetPx, behavior: 'smooth' }
        : { top: targetPx, behavior: 'smooth' }
      el.scrollTo(scrollOpts)

      /* Resume listener writebacks once the scroll settles. Use
       * the native `scrollend` event when supported (Chrome 114+,
       * Firefox 109+), with a 1 s timeout fallback for older
       * browsers — well over typical smooth-scroll duration even
       * at the wide end of the 14-day track. */
      const onSettle = () => {
        expectingButtonScroll.value = false
        el.removeEventListener('scrollend', onSettle)
        if (scrollSettleTimer !== null) {
          clearTimeout(scrollSettleTimer)
          scrollSettleTimer = null
        }
      }
      el.addEventListener('scrollend', onSettle, { once: true })
      scrollSettleTimer = setTimeout(onSettle, 1000)
    },
  )

  return { onActiveDayChanged, onViewportRangeChanged }
}
