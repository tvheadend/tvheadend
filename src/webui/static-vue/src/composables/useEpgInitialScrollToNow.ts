// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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
import { nextTick, ref, watch, type ComputedRef } from 'vue'
import type { useEpgViewState } from './useEpgViewState'
import type { StickyPosition } from './epgPositionStorage'

/* `Align` is unioned per-axis at the call site (Timeline:
 * 'left' | 'center' | 'leftThird'; Magazine: 'top' | 'center' |
 * 'topThird'). Generic over the parameter shape so the wrapper
 * doesn't widen each axis's type. */
export function useEpgInitialScrollToNow<Align extends string>(opts: {
  state: ReturnType<typeof useEpgViewState>
  scrollEl: ComputedRef<HTMLElement | null>
  scrollToNow: (o?: { behavior?: ScrollBehavior; align?: Align }) => void
  /* When provided, the scroll-to-now paths (default + wasClamped) route
   * through this instead of `scrollToNow`, so the viewport starts
   * "following" now (see useEpgScrollDaySync.followNow). The normal
   * restore path deliberately does NOT call it — a user with a saved
   * scroll position isn't following the live edge. */
  onFollowNow?: (behavior: ScrollBehavior) => void
  /* B2 — sticky position. Optional so callers without restore
   * support (e.g. a future flat-list view) still work. When both
   * `restoreToPosition` AND `scrollToTime` are absent the restore
   * path no-ops; with either present the restore is taken (prefer
   * `restoreToPosition` since it goes through the day-sync latch
   * and avoids the race with the dayStart watch's scrollToDay). */
  scrollToTime?: (time: number, o?: { behavior?: ScrollBehavior; align?: Align }) => void
  /* Preferred B2 entry point — when provided, takes priority over
   * scrollToTime for non-clamped restores. The view should source
   * this from `useEpgScrollDaySync` so the day-sync latch is set
   * before the scroll, preventing the dayStart watch from racing
   * with a competing scrollToDay. */
  restoreToPosition?: (pos: StickyPosition) => void
  /* B2 — top-channel restoration. Optional for the same reason.
   * Called AFTER the time-axis restore so the layout is already at
   * the restored day/time before we touch the channel-axis scroll. */
  restoreTopChannel?: (uuid: string) => void
  align: Align
}) {
  const initialScrollDone = ref(false)
  watch(
    [opts.scrollEl, opts.state.filteredChannels, opts.state.events, opts.state.isToday],
    async () => {
      if (initialScrollDone.value) return
      if (!opts.scrollEl.value) return
      if (
        opts.state.filteredChannels.value.length === 0 ||
        opts.state.events.value.length === 0
      )
        return

      const restored = opts.state.restoredPosition.value
      const doRestore = !!restored && !!(opts.restoreToPosition || opts.scrollToTime)
      /* The scroll-to-now path needs today; the restore path does not
       * (restoring a future day is fine). */
      if (!doRestore && !opts.state.isToday.value) return

      /* Latch before the waits below. Deciding to scroll is the commit
       * point — a re-entrant watch fire while we wait must not kick
       * off a second scroll. */
      initialScrollDone.value = true

      /* Wait for the track to reach its final layout before the scroll
       * math runs. The triggering change (events arriving) has only
       * just rendered; `nextTick` drains any pending Vue update and the
       * animation frame lets the browser finish layout — so scrollTo
       * computes against the real track size, not a mid-render one,
       * and isn't clamped to ~0. (Same reason the density-change
       * watchers in useTimelineScroll / useMagazineScroll await a
       * tick before re-scrolling.) */
      await nextTick()
      await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()))
      if (!opts.scrollEl.value) return

      if (restored) {
        /* Restored path. Two cases:
         *
         * - `wasClamped`: persisted scroll-time was stale and got
         *   pushed forward to nowEpoch by
         *   `clampSameDayScrollTimeForward`. User's effective
         *   intent is "show me now" — use scrollToNow (snap to last
         *   :30, align='left') instead of scrolling to the literal
         *   clamped time, which would have no snap + leftThird
         *   alignment and would put the leading 1/3 of the viewport
         *   on server-filtered past events (src/epg.c:2335),
         *   producing a blank wedge before the now-cursor.
         *
         * - Normal restore: prefer `restoreToPosition` (goes through
         *   useEpgScrollDaySync's intent-latch so the dayStart watch
         *   can't fire a competing scrollToDay for the latched day's
         *   preroll target). Fall back to `scrollToTime` when the
         *   view didn't provide restoreToPosition — older flow,
         *   still works for views that haven't migrated.
         *
         *   The leftThird/topThird alignment of the scrollToTime
         *   fallback shifts the leading edge ~viewport/3 px before
         *   the persisted scrollTime, so the user sees a slightly-
         *   earlier slice than what was saved. restoreToPosition
         *   instead treats the persisted scrollTime as the
         *   LEADING-edge target, putting the user exactly where the
         *   save snapshot was taken. */
        if (restored.wasClamped) {
          /* "Show me now" intent → start following (see onFollowNow). */
          if (opts.onFollowNow) opts.onFollowNow('instant')
          else opts.scrollToNow({ behavior: 'instant' as ScrollBehavior, align: opts.align })
        } else if (opts.restoreToPosition) {
          opts.restoreToPosition(restored)
        } else if (opts.scrollToTime) {
          opts.scrollToTime(restored.scrollTime, {
            behavior: 'instant' as ScrollBehavior,
            align: opts.align,
          })
        }
        opts.restoreTopChannel?.(restored.topChannelUuid)
        return
      }

      /* Default path: scroll to now AND start following the live edge.
       * `instant` keeps the initial position from feeling like an
       * animation; later user-triggered "Now" clicks animate. */
      if (opts.onFollowNow) opts.onFollowNow('instant')
      else opts.scrollToNow({ behavior: 'instant' as ScrollBehavior, align: opts.align })
    },
    { flush: 'post' },
  )
}
