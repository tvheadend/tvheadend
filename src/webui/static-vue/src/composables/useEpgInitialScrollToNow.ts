/*
 * useEpgInitialScrollToNow — first-mount scroll latch shared by
 * TimelineView and MagazineView. Watches the scroll element +
 * filtered channels + filtered events; fires exactly once with
 * one of two paths depending on `state.restoredPosition`:
 *
 *   - **Restored** (B2 — sticky position): when
 *     `state.restoredPosition` is non-null (set during onMounted
 *     after a successful sessionStorage read + freshness check),
 *     scroll to the persisted scroll-time and ask the caller to
 *     position the persisted top channel. The day cursor was
 *     already moved to `restoredPosition.dayStart` before the
 *     fetches started, so the loaded events cover that day.
 *
 *   - **Scroll-to-now** (default): when no restored position
 *     exists AND the active day is today, scroll the cursor to
 *     wall-clock now. This is the original behaviour.
 *
 * Both paths require the scroll element + channels + events to
 * be ready (otherwise the scroll math has no target). The
 * isToday gate applies ONLY to the scroll-to-now path —
 * restoring a future day doesn't need it.
 *
 * Subsequent day changes don't re-fire — once the user is
 * scrolling, their scroll position IS their day choice and we
 * don't re-anchor underneath them.
 *
 * The `align` argument is per-axis ('leftThird' for Timeline,
 * 'topThird' for Magazine); the caller passes the variant their
 * scrollToNow / scrollToTime accept.
 */
import { ref, watch, type ComputedRef } from 'vue'
import type { useEpgViewState } from './useEpgViewState'

/* `Align` is unioned per-axis at the call site (Timeline:
 * 'left' | 'center' | 'leftThird'; Magazine: 'top' | 'center' |
 * 'topThird'). Generic over the parameter shape so the wrapper
 * doesn't widen each axis's type. */
export function useEpgInitialScrollToNow<Align extends string>(opts: {
  state: ReturnType<typeof useEpgViewState>
  scrollEl: ComputedRef<HTMLElement | null>
  scrollToNow: (o?: { behavior?: ScrollBehavior; align?: Align }) => void
  /* B2 — sticky position. Optional so callers without restore
   * support (e.g. a future flat-list view) still work. */
  scrollToTime?: (time: number, o?: { behavior?: ScrollBehavior; align?: Align }) => void
  /* B2 — top-channel restoration. Optional for the same reason.
   * Called AFTER scrollToTime so the layout is already at the
   * restored day/time before we touch the channel-axis scroll. */
  restoreTopChannel?: (uuid: string) => void
  align: Align
}) {
  const initialScrollDone = ref(false)
  watch(
    [opts.scrollEl, opts.state.filteredChannels, opts.state.filteredEvents, opts.state.isToday],
    () => {
      if (initialScrollDone.value) return
      if (!opts.scrollEl.value) return
      if (
        opts.state.filteredChannels.value.length === 0 ||
        opts.state.filteredEvents.value.length === 0
      )
        return

      const restored = opts.state.restoredPosition.value
      if (restored && opts.scrollToTime) {
        /* Restored path: jump to the persisted time + top
         * channel. `instant` so the user doesn't see an animated
         * scroll on every nav-back. */
        opts.scrollToTime(restored.scrollTime, {
          behavior: 'instant' as ScrollBehavior,
          align: opts.align,
        })
        if (opts.restoreTopChannel) {
          opts.restoreTopChannel(restored.topChannelUuid)
        }
        initialScrollDone.value = true
        return
      }

      /* Default path: scroll to now (today only). */
      if (!opts.state.isToday.value) return
      /* `instant` keeps the initial position from feeling like an
       * animation; subsequent user-triggered "Now" clicks animate. */
      opts.scrollToNow({ behavior: 'instant' as ScrollBehavior, align: opts.align })
      initialScrollDone.value = true
    },
    { flush: 'post' },
  )
}
