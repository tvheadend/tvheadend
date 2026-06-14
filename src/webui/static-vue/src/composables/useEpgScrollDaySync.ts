// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgScrollDaySync — shared continuous-scroll ↔ day-cursor
 * synchronisation for the EPG view wrappers (TimelineView /
 * MagazineView). Both views maintain a continuous N-day track
 * scrolled along ONE axis (Timeline: horizontal; Magazine:
 * vertical) with the scroll-listener fanning out two signals:
 *
 *   - `update:activeDay` (epoch of leading-edge day) → write
 *     `state.dayStart` so the toolbar day-button highlight
 *     follows.
 *   - `update:viewportRange` ({start, stop} in epoch seconds) →
 *     ensure ±1 day around the visible window is loaded.
 *
 * On the reverse path, any "intent" scroll (toolbar day click,
 * Now-button click, route restore) flows through `scrollToDay`,
 * which:
 *
 *   1. Computes the target pixel — `today` clicks resolve to the
 *      half-hour-snapped Now-time so they don't dump the user at
 *      a server-filtered empty 00:00 of today; future-day clicks
 *      land 30 min BEFORE midnight as a preroll so the last :30
 *      of the previous day is visually present as context.
 *   2. Sets `state.dayStart = newDay` so the picker highlight
 *      matches the user's intent immediately, not after the
 *      smooth-scroll settles.
 *   3. Suppresses the activeDay writeback path for the duration
 *      of the smooth-scroll. This is critical for long-distance
 *      jumps (day +4 → Now): without it, the scroll passes
 *      through intermediate days, the rAF-throttled listener
 *      emits each intermediate day, the dayStart watch would re-
 *      enter scrollToDay and start a fresh competing scrollTo,
 *      and the browser cancels the original Now-scroll to
 *      service the new mid-flight one — landing the user at an
 *      intermediate day's 00:00 and requiring a second Now click
 *      to reach the cursor.
 *
 * Axis-agnostic: callers pass `axis: 'horizontal' | 'vertical'`;
 * the composable reads `scrollLeft` or `scrollTop` accordingly and
 * derives the leading-edge time directly from the track origin —
 * no sticky-pane width subtraction needed (the scroll-container's
 * sticky element floats over the same coords the track uses, so
 * `scrollLeft`/`scrollTop` IS the leading-edge offset in track
 * coords).
 */
import { watch, type ComputedRef } from 'vue'
import { ref } from 'vue'
import { addLocalDaysEpoch, startOfLocalDayEpoch as startOfLocalDay } from '@/utils/localDay'
import type { useEpgViewState } from './useEpgViewState'
import type { StickyPosition } from './epgPositionStorage'

/* Now-button snap interval — must match `NOW_SNAP_SECONDS` in
 * useTimelineScroll / useMagazineScroll so clicking Today produces
 * the same scroll target as clicking Now. */
const NOW_SNAP_SECONDS = 30 * 60
/* Preroll for future-day clicks — viewport leading edge sits 30
 * min before midnight of the clicked day, so the last :30 of the
 * previous day is visually present as context. The picker still
 * highlights the clicked day because the intent latch overrides
 * the leading-edge writeback for the duration of the scroll. */
const PREROLL_SECONDS = 30 * 60

export function useEpgScrollDaySync(opts: {
  axis: 'horizontal' | 'vertical'
  scrollEl: ComputedRef<HTMLElement | null>
  pxPerMinute: ComputedRef<number>
  state: ReturnType<typeof useEpgViewState>
}) {
  const { axis, scrollEl, pxPerMinute, state } = opts
  const isHorizontal = axis === 'horizontal'

  const expectingButtonScroll = ref(false)
  let scrollSettleTimer: ReturnType<typeof setTimeout> | null = null
  /* Day epoch the in-flight intent scroll targets; null when no
   * intent scroll holds the latch. The dayStart watch re-fires for
   * the SAME target as a side-effect of the intent's own
   * setDayStart — that's dropped — but a NEW target while the
   * latch is held is a fresh user intent and must win (re-arm +
   * re-scroll), otherwise the grid settles on the first day while
   * the toolbar highlights the second. */
  let pendingScrollTarget: number | null = null
  /* Monotonic arm counter — a settle handler from a superseded
   * intent must not lift the latch the newer intent re-armed. */
  let scrollIntentToken = 0
  let activeSettleHandler: (() => void) | null = null
  let activeSettleEl: HTMLElement | null = null

  function onActiveDayChanged(epoch: number) {
    if (expectingButtonScroll.value) return
    /* Highlight-only writeback from the scroll listener — mark it
     * silent so the dayStart watch doesn't bounce it back into a
     * counter-scroll against the user's own free scroll. */
    if (state.dayStart.value !== epoch) state.setDayStart(epoch, { silent: true })
  }

  function onViewportRangeChanged(range: { start: number; end: number }) {
    /* Day keys are TRUE local midnights (a local day is 23/25 h
     * across a DST transition) so they match the loadedDays
     * bookkeeping and the toolbar's day epochs. */
    const startDay = addLocalDaysEpoch(startOfLocalDay(range.start), -1)
    const endDay = addLocalDaysEpoch(startOfLocalDay(range.end), 1)
    const days: number[] = []
    for (let d = startDay; d <= endDay; d = addLocalDaysEpoch(d, 1)) {
      if (d >= state.trackStart.value && d < state.trackEnd.value) days.push(d)
    }
    state.ensureDaysLoaded(days)
  }

  /* Pixel target for an intent scroll to a given day:
   *   - today  → half-hour-snapped Now (Today click == Now click)
   *   - other  → day-start minus PREROLL_SECONDS (preroll for
   *              visual continuity); clamped at 0 so day +0 fall-
   *              back can't ask for a negative scroll. */
  function targetPxForDay(newDay: number): number {
    const nowSec = Math.floor(Date.now() / 1000)
    const today = startOfLocalDay(nowSec)
    if (newDay === today) {
      const snappedSec = Math.floor(nowSec / NOW_SNAP_SECONDS) * NOW_SNAP_SECONDS
      const offsetMin = Math.max(0, (snappedSec - state.trackStart.value) / 60)
      return offsetMin * pxPerMinute.value
    }
    const offsetMin = Math.max(
      0,
      (newDay - state.trackStart.value - PREROLL_SECONDS) / 60,
    )
    return offsetMin * pxPerMinute.value
  }

  /* Settle handler shared by scrollToDay + restoreToPosition:
   * detach itself, clear the fallback timer, then lift the
   * suppression latch ONE animation frame late so any
   * `emitScrollState` rAF queued by the smooth-scroll's FINAL
   * scroll event drains while the latch is still on. Without the
   * deferral the queued tick fires AFTER scrollend lifts the latch,
   * reads the preroll-zone leading edge (day+N − 30 min → previous
   * day under the raw leading-edge rule) and writes state.dayStart
   * back to the previous day, flipping the picker to day+N−1 after
   * the content loads. Same shape as the long-distance-Now cascade
   * but milder because only the queued emit fires, not a full
   * cascading scroll. */
  function makeScrollSettleHandler(el: HTMLElement): () => void {
    const token = scrollIntentToken
    const onSettle = () => {
      el.removeEventListener('scrollend', onSettle)
      /* A newer intent already re-armed — it detached us before
       * issuing its scroll, but the fallback timer can still fire
       * this handler. Its timer + latch are not ours to touch. */
      if (token !== scrollIntentToken) return
      if (activeSettleHandler === onSettle) {
        activeSettleHandler = null
        activeSettleEl = null
      }
      if (scrollSettleTimer !== null) {
        clearTimeout(scrollSettleTimer)
        scrollSettleTimer = null
      }
      requestAnimationFrame(() => {
        if (token !== scrollIntentToken) return
        expectingButtonScroll.value = false
        pendingScrollTarget = null
      })
    }
    return onSettle
  }

  /* Take (or re-take) the intent latch for a new scroll target:
   * detach the superseded intent's settle plumbing so the re-armed
   * scroll isn't treated as settled by leftovers, then latch. */
  function armIntentScroll(el: HTMLElement, targetDay: number): void {
    scrollIntentToken += 1
    if (activeSettleHandler !== null && activeSettleEl !== null) {
      activeSettleEl.removeEventListener('scrollend', activeSettleHandler)
    }
    activeSettleHandler = null
    activeSettleEl = null
    if (scrollSettleTimer !== null) {
      clearTimeout(scrollSettleTimer)
      scrollSettleTimer = null
    }
    expectingButtonScroll.value = true
    pendingScrollTarget = targetDay
    const onSettle = makeScrollSettleHandler(el)
    activeSettleHandler = onSettle
    activeSettleEl = el
    el.addEventListener('scrollend', onSettle, { once: true })
    scrollSettleTimer = setTimeout(onSettle, 1000)
  }

  /* Issue the smooth-scroll + arm the suppression latch. Shared
   * by the dayStart watch (button click) and the Now handler
   * (which uses `force: true` because Now-from-today doesn't
   * change dayStart and would otherwise short-circuit). */
  function scrollToDay(newDay: number, force = false): void {
    const el = scrollEl.value
    if (!el) return

    /* If an intent scroll to this SAME day is already holding the
     * latch (e.g. `restoreToPosition` ran first and the dayStart
     * watch is firing now as a side-effect of its
     * `state.setDayStart`), don't compete with it — the other
     * caller owns the scroll target and the lift. A DIFFERENT
     * target is a fresh user intent (second day click mid-flight)
     * and falls through: armIntentScroll detaches the superseded
     * intent and the new scroll wins. `force: true` bypasses the
     * dedupe — `jumpToNow` needs to scroll even when its own
     * setDayStart didn't actually change dayStart
     * (Now-from-today). */
    if (!force && expectingButtonScroll.value && newDay === pendingScrollTarget) {
      return
    }

    /* Deliberately no same-day short-circuit here. Deriving
     * "current day" from the raw leading edge would be wrong: during
     * a future-day view's 30-min preroll the leading edge sits in
     * the PREVIOUS day, so a backward jump to that day would be
     * mistaken for "already there" and dropped. The scroll → dayStart
     * → counter-scroll feedback loop is instead prevented at the
     * source — highlight-only writebacks mark their setDayStart
     * `silent` and the watch skips them. Reaching this point means a
     * genuine navigation intent; always scroll. Re-selecting the
     * current day never reaches here because setDayStart no-ops an
     * unchanged value, so the watch doesn't fire. */

    armIntentScroll(el, newDay)

    const targetPx = targetPxForDay(newDay)
    const scrollOpts: ScrollToOptions = isHorizontal
      ? { left: targetPx, behavior: 'smooth' }
      : { top: targetPx, behavior: 'smooth' }
    el.scrollTo(scrollOpts)
  }

  /* Now button entry point. Always sets dayStart=today so the
   * picker highlight matches what the user just asked for, then
   * force-scrolls regardless of whether dayStart changed (a
   * Now-click while already on today would otherwise short-
   * circuit and leave the user looking at the morning or
   * wherever they'd previously panned). */
  function jumpToNow(): void {
    const today = startOfLocalDay(Math.floor(Date.now() / 1000))
    if (state.dayStart.value !== today) state.setDayStart(today)
    scrollToDay(today, true)
  }

  /* Restore from a sticky position (B2 path). Called from
   * `useEpgInitialScrollToNow` once channels + events have loaded.
   * Latches BEFORE writing dayStart so the dayStart watch fires,
   * sees the latch, and skips its scrollToDay — otherwise we'd
   * race with a competing smooth-scroll to the preroll target
   * for the restored day.
   *
   * Instant scroll to the saved scrollTime treated as the
   * leading-edge time (not a leftThird-aligned target). That
   * places the user exactly where they were at save: a manual
   * mid-day scroll restores to that exact time; a click-and-
   * navigate-away pattern restores to the preroll position
   * (because the save captured the preroll leading edge as
   * scrollTime). Either way the math is one-line:
   *   targetPx = (scrollTime − trackStart)/60 * pxm
   *
   * After scroll, deferred latch lift drains the queued emit
   * fired by the scroll's last scroll event so it doesn't
   * write state.dayStart back to the leading-edge day (which
   * during preroll would be the PREVIOUS day, flipping the
   * picker — same shape as the day-button race).
   *
   * Without this unified path the restore would go through a
   * leftThird-aligned scrollToTime which races the dayStart
   * watch's scrollToDay (triggered when the restore sets
   * dayStart); one wins, the other loses its ensureDaysLoaded-
   * driven fetch, and the events the scroll lands on are never
   * requested. */
  function restoreToPosition(pos: StickyPosition): void {
    const el = scrollEl.value
    if (!el) return

    armIntentScroll(el, pos.dayStart)

    /* Write dayStart UNDER the latch — the dayStart watch will
     * fire on the next flush and skip its scrollToDay because
     * the latch is held for the same target day. Picker highlight
     * follows the restored day for free.
     *
     * Instant scroll may or may not fire scrollend (browser-
     * dependent — Chrome fires it, some older builds don't).
     * armIntentScroll's timeout fallback covers both cases; the
     * deferred-frame lift inside the settle handler is what
     * drains the queued emit. */
    if (state.dayStart.value !== pos.dayStart) {
      state.setDayStart(pos.dayStart)
    }

    const targetPx = Math.max(
      0,
      ((pos.scrollTime - state.trackStart.value) / 60) * pxPerMinute.value,
    )
    const scrollOpts: ScrollToOptions = isHorizontal
      ? { left: targetPx, behavior: 'instant' }
      : { top: targetPx, behavior: 'instant' }
    el.scrollTo(scrollOpts)
  }

  /* Reverse path: dayStart changes from toolbar → scroll.
   * Highlight-only changes (scroll-listener writeback, midnight
   * rollover) mark themselves silent via setDayStart; skip the
   * scroll for those so we don't counter-scroll the user's free
   * scroll or yank them at midnight. User navigation (day buttons,
   * picklist) is never silent, so it always scrolls. */
  watch(
    () => state.dayStart.value,
    (newDay) => {
      if (state.consumeDayStartScrollSuppressed()) return
      scrollToDay(newDay)
    },
  )

  return {
    onActiveDayChanged,
    onViewportRangeChanged,
    jumpToNow,
    restoreToPosition,
  }
}
