// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgScrollDaySync — intent-scroll + cascade-suppression tests.
 *
 * Covers three behaviours:
 *   - Today / Now click resolves to the half-hour-snapped Now-time,
 *     NOT 00:00 of today (otherwise the server filter drops every
 *     event and the leading edge shows empty cells).
 *   - Future-day click leaves a 30 min preroll of the previous day
 *     for visual continuity.
 *   - Long-distance Now click (e.g. from day +4) does not cascade
 *     through intermediate days: the scroll-listener writeback is
 *     suppressed for the duration of the smooth-scroll, so the
 *     browser actually lands at today's Now-snap on the first
 *     click instead of stopping at an intermediate day's 00:00.
 *
 * The time zone is pinned to Europe/Berlin so the DST-transition
 * case is deterministic regardless of the host TZ.
 */
process.env.TZ = 'Europe/Berlin'

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises } from '@vue/test-utils'
import { computed, ref } from 'vue'
import { useEpgScrollDaySync } from '../useEpgScrollDaySync'

const ONE_DAY = 86400
const NOW_SNAP = 30 * 60
const PREROLL = 30 * 60

interface ScrollState {
  scrollLeft: number
  scrollTop: number
  clientWidth: number
  clientHeight: number
}

function makeScrollEl(state: ScrollState): {
  el: HTMLElement
  scrollTo: ReturnType<typeof vi.fn>
  fireScrollend: () => void
} {
  const scrollendListeners = new Set<EventListener>()
  const scrollTo = vi.fn((opts: ScrollToOptions) => {
    if (typeof opts.left === 'number') state.scrollLeft = Math.max(0, opts.left)
    if (typeof opts.top === 'number') state.scrollTop = Math.max(0, opts.top)
  })
  const el = {
    get scrollLeft() {
      return state.scrollLeft
    },
    get scrollTop() {
      return state.scrollTop
    },
    get clientWidth() {
      return state.clientWidth
    },
    get clientHeight() {
      return state.clientHeight
    },
    scrollTo,
    addEventListener(evt: string, fn: EventListener) {
      if (evt === 'scrollend') scrollendListeners.add(fn)
    },
    removeEventListener(evt: string, fn: EventListener) {
      if (evt === 'scrollend') scrollendListeners.delete(fn)
    },
  } as unknown as HTMLElement
  return {
    el,
    scrollTo,
    fireScrollend: () => {
      const fns = [...scrollendListeners]
      scrollendListeners.clear()
      fns.forEach((fn) => fn(new Event('scrollend')))
    },
  }
}

/* Minimal useEpgViewState surface — only the slice
 * useEpgScrollDaySync touches. dayStart is a real ref so the
 * composable's watch fires on writes. */
function makeState(initialDayStart: number, trackStart: number) {
  const dayStart = ref(initialDayStart)
  /* Mirror the real useEpgViewState: setDayStart honours a `silent`
   * opt that marks a highlight-only change, and the scroll-sync
   * watch consumes it to decide whether to scroll. One-shot. */
  let dayStartScrollSuppressed = false
  return {
    dayStart,
    trackStart: ref(trackStart),
    trackEnd: ref(trackStart + 14 * ONE_DAY),
    setDayStart: (epoch: number, opts?: { silent?: boolean }) => {
      if (dayStart.value === epoch) return
      dayStartScrollSuppressed = opts?.silent === true
      dayStart.value = epoch
    },
    consumeDayStartScrollSuppressed: () => {
      const s = dayStartScrollSuppressed
      dayStartScrollSuppressed = false
      return s
    },
    ensureDaysLoaded: vi.fn(),
  } as unknown as Parameters<typeof useEpgScrollDaySync>[0]['state']
}

function startOfLocalDay(epoch: number): number {
  const d = new Date(epoch * 1000)
  d.setHours(0, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
}

describe('useEpgScrollDaySync', () => {
  const TODAY = startOfLocalDay(1_700_000_000)
  const TOMORROW = TODAY + ONE_DAY
  const DAY_PLUS_4 = TODAY + 4 * ONE_DAY
  const NOW_SEC = TODAY + 22 * 3600 + 45 * 60 /* 22:45 today */
  const NOW_SNAP_PX = (() => {
    const snapped = Math.floor(NOW_SEC / NOW_SNAP) * NOW_SNAP /* 22:30 today */
    return ((snapped - TODAY) / 60) * 4
  })()

  /* Queue rAF callbacks instead of firing them sync — tests need
   * to interleave scrollend, then mid-flight emits, then drain the
   * deferred rAF to verify the order-of-operations the latch
   * defends against. `drainRAF()` runs one queued frame. */
  let rafQueue: FrameRequestCallback[] = []
  function drainRAF(): void {
    const callbacks = rafQueue
    rafQueue = []
    callbacks.forEach((cb) => cb(0))
  }

  beforeEach(() => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date(NOW_SEC * 1000))
    rafQueue = []
    vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => {
      rafQueue.push(cb)
      return rafQueue.length
    })
  })

  afterEach(() => {
    vi.unstubAllGlobals()
    vi.useRealTimers()
    rafQueue = []
  })

  it('Today click resolves to Now-snap (NOT 00:00 of today)', async () => {
    /* Without the now-snap special case, the dayStart watch
     * scrolls to (today - trackStart)/60 * pxm = 0 (because
     * dayStart === trackStart for today). The user lands at 00:00
     * today, which is mostly server-filtered → empty cells. */
    const state = makeState(DAY_PLUS_4, TODAY)
    const made = makeScrollEl({
      scrollLeft: 4 * ONE_DAY * (1 / 60) * 4,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })
    /* Simulate the Today button: state.setDayStart(today) →
     * dayStart watch fires. */
    state.setDayStart(TODAY)
    await flushPromises()

    expect(made.scrollTo).toHaveBeenCalledTimes(1)
    expect(made.scrollTo).toHaveBeenCalledWith({
      left: NOW_SNAP_PX,
      behavior: 'smooth',
    })
  })

  it('future-day click lands at day-start minus 30-min preroll', async () => {
    /* Tomorrow click from today → leading edge sits 30 min before
     * midnight tomorrow (= 23:30 today) so the user sees the last
     * :30 of today as visual context for the day transition. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })
    state.setDayStart(TOMORROW)
    await flushPromises()

    /* (TOMORROW − TODAY − PREROLL) / 60 * 4 = 23.5h * 60 * 4 */
    const expectedPx = ((TOMORROW - TODAY - PREROLL) / 60) * 4
    expect(made.scrollTo).toHaveBeenCalledWith({
      left: expectedPx,
      behavior: 'smooth',
    })
  })

  it('backward-by-one click scrolls even when its 30-min preroll is on screen', async () => {
    /* Regression: after a forward jump to day +4 the viewport leading
     * edge sits at (day +4 − 30 min) = day +3 23:30, so the raw
     * leading-edge day is day +3. Picking day +3 from the dropdown
     * must still scroll — the old same-day no-op guard derived
     * "current day" from that preroll edge and wrongly treated the
     * jump as already-there, leaving the grid stuck on day +4. */
    const DAY_PLUS_3 = TODAY + 3 * ONE_DAY
    const prerollEdgePx = ((DAY_PLUS_4 - PREROLL - TODAY) / 60) * 4 /* day +3 23:30 */
    const state = makeState(DAY_PLUS_4, TODAY)
    const made = makeScrollEl({
      scrollLeft: prerollEdgePx,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })

    /* Dropdown pick of the day whose preroll is currently visible. */
    state.setDayStart(DAY_PLUS_3)
    await flushPromises()

    /* Scrolls to day +3's own preroll target — not a no-op. */
    const expectedPx = ((DAY_PLUS_3 - PREROLL - TODAY) / 60) * 4
    expect(made.scrollTo).toHaveBeenCalledTimes(1)
    expect(made.scrollTo).toHaveBeenCalledWith({ left: expectedPx, behavior: 'smooth' })
    expect(state.dayStart.value).toBe(DAY_PLUS_3)
  })

  it('Now click force-scrolls even when state.dayStart is already today', async () => {
    /* Same-day no-op short-circuit must NOT apply to Now —
     * user explicitly asked for "scroll to now-cursor" and a
     * silent no-op feels broken. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 100 /* user panned somewhere earlier in today */,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { jumpToNow } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })

    jumpToNow()
    await flushPromises()

    expect(made.scrollTo).toHaveBeenCalledWith({
      left: NOW_SNAP_PX,
      behavior: 'smooth',
    })
  })

  it('suppresses activeDay writeback during the smooth-scroll (cascade prevention)', async () => {
    /* Long-distance Now: a smooth-scroll passes through
     * intermediate days. The rAF-throttled scroll listener fires
     * mid-flight, the leading-edge day derivation writes
     * state.dayStart = mid-flight day → would trigger a competing
     * scrollTo. The suppression flag must drop those mid-flight
     * emits until scrollend. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 4 * ONE_DAY * (1 / 60) * 4,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { onActiveDayChanged, jumpToNow } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })

    jumpToNow()
    await flushPromises()
    /* Simulate the scroll-listener firing mid-flight with an
     * intermediate day. Without suppression, this would write
     * state.dayStart and trigger a fresh scrollTo. */
    onActiveDayChanged(TODAY + 2 * ONE_DAY)
    await flushPromises()

    /* Exactly one scrollTo — the Now-target. The mid-flight emit
     * was dropped. */
    expect(made.scrollTo).toHaveBeenCalledTimes(1)
    expect(state.dayStart.value).toBe(TODAY)

    /* After scrollend the suppression lifts and free-scroll
     * writebacks resume — but only after the deferred-frame
     * latch lift fires. */
    made.fireScrollend()
    drainRAF() /* run the deferred latch-lift rAF */
    onActiveDayChanged(TODAY) /* user scrolls within today — no-op */
    onActiveDayChanged(TOMORROW) /* user pans forward into tomorrow */
    await flushPromises()
    expect(state.dayStart.value).toBe(TOMORROW)
  })

  it('drops the queued emit fired by the smooth-scroll\'s final tick (day-click preroll race)', async () => {
    /* The smooth-scroll's final scroll event queues an
     * emitScrollState rAF; scrollend fires synchronously after.
     * Without the deferred latch-lift, the queued rAF fires AFTER
     * scrollend lifts the latch, reads the preroll-zone leading
     * edge (day+4 − 30 min = day+3 23:30), emits activeDay=day+3,
     * and the picker flips back from day+4 to day+3. Deferring
     * the lift by one rAF keeps the latch on for that queued
     * tick. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { onActiveDayChanged } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })

    state.setDayStart(DAY_PLUS_4)
    await flushPromises()
    /* scrollend fires synchronously after the smooth-scroll's
     * final scroll event. onSettle queues a deferred rAF to lift
     * the latch — it sits in the queue but hasn't run yet. */
    made.fireScrollend()
    /* Simulate the queued emitScrollState rAF (queued by the
     * smooth-scroll's final scroll event) firing BEFORE the
     * deferred lift rAF. Without the deferral, the latch would
     * already be off here and this write would land — flipping
     * the picker to day +3 23:30's day = day +3. */
    onActiveDayChanged(DAY_PLUS_4 - ONE_DAY)
    await flushPromises()

    /* state.dayStart still DAY_PLUS_4 because the latch was held
     * across the queued emit. */
    expect(state.dayStart.value).toBe(DAY_PLUS_4)

    /* Drain the deferred rAF — latch lifts. */
    drainRAF()
    /* Subsequent (genuine free-scroll) emits land normally. */
    onActiveDayChanged(DAY_PLUS_4 - ONE_DAY)
    await flushPromises()
    expect(state.dayStart.value).toBe(DAY_PLUS_4 - ONE_DAY)
  })

  it('a second day click during an intent scroll wins (re-arms onto the new target)', async () => {
    /* Click day A, then day B before A's smooth-scroll settles.
     * The latch is held for A, but B is a NEW intent — it must
     * re-arm the latch and issue its own scroll. Dropping it
     * leaves the grid on A while the toolbar highlights B, and
     * re-clicking B no-ops (setDayStart dedupes unchanged
     * epochs). */
    const DAY_PLUS_2 = TODAY + 2 * ONE_DAY
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { onActiveDayChanged } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })

    state.setDayStart(DAY_PLUS_2)
    await flushPromises()
    expect(made.scrollTo).toHaveBeenCalledTimes(1)

    /* Second click lands while A's scroll is still in flight. */
    state.setDayStart(DAY_PLUS_4)
    await flushPromises()

    expect(made.scrollTo).toHaveBeenCalledTimes(2)
    const expectedPx = ((DAY_PLUS_4 - PREROLL - TODAY) / 60) * 4
    expect(made.scrollTo).toHaveBeenLastCalledWith({
      left: expectedPx,
      behavior: 'smooth',
    })
    expect(state.dayStart.value).toBe(DAY_PLUS_4)

    /* The re-armed scroll is NOT immediately treated as settled:
     * mid-flight writebacks stay suppressed until B's own settle. */
    onActiveDayChanged(DAY_PLUS_2)
    await flushPromises()
    expect(state.dayStart.value).toBe(DAY_PLUS_4)

    /* B settles; after the deferred lift, free scroll resumes. */
    made.fireScrollend()
    drainRAF()
    onActiveDayChanged(TOMORROW)
    await flushPromises()
    expect(state.dayStart.value).toBe(TOMORROW)
  })

  it('works on the vertical axis (Magazine layout)', async () => {
    /* Same composable handles Magazine via axis:'vertical'.
     * Verify scrollTo receives `top` not `left`. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 800,
      clientHeight: 1200,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    useEpgScrollDaySync({
      axis: 'vertical',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })
    state.setDayStart(TOMORROW)
    await flushPromises()

    const expectedPx = ((TOMORROW - TODAY - PREROLL) / 60) * 4
    expect(made.scrollTo).toHaveBeenCalledWith({
      top: expectedPx,
      behavior: 'smooth',
    })
  })

  it('range-changed loads ±1 day around the visible window', async () => {
    /* Sanity guard — refactor preserved the existing viewport-range
     * fetch behavior. */
    const state = makeState(TODAY, TODAY)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { onViewportRangeChanged } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })
    onViewportRangeChanged({ start: TODAY + 22 * 3600, end: TOMORROW + 2 * 3600 })
    const stateAny = state as unknown as { ensureDaysLoaded: ReturnType<typeof vi.fn> }
    expect(stateAny.ensureDaysLoaded).toHaveBeenCalled()
    const days = stateAny.ensureDaysLoaded.mock.calls[0][0] as number[]
    /* Range covers today + tomorrow + ±1 day padding → at least 3
     * distinct days in the list (yesterday, today, tomorrow, day-
     * after). */
    expect(days.length).toBeGreaterThanOrEqual(3)
  })

  it('range-changed emits true local-midnight day keys across fall-back', async () => {
    /* Track origin three days before the 25h fall-back day
     * (2026-10-25); the visible window straddles the transition.
     * Stepping the day tiling by a flat 86 400 s would emit a
     * 01:00 key past the boundary, which never matches the
     * loadedDays bookkeeping or the toolbar's day epochs. */
    const trackStart = Math.floor(new Date(2026, 9, 22).getTime() / 1000)
    const state = makeState(trackStart, trackStart)
    const made = makeScrollEl({
      scrollLeft: 0,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    })
    const scrollEl = computed(() => made.el as HTMLElement | null)

    const { onViewportRangeChanged } = useEpgScrollDaySync({
      axis: 'horizontal',
      scrollEl,
      pxPerMinute: computed(() => 4),
      state,
    })
    onViewportRangeChanged({
      start: Math.floor(new Date(2026, 9, 24, 12).getTime() / 1000),
      end: Math.floor(new Date(2026, 9, 26, 12).getTime() / 1000),
    })
    const stateAny = state as unknown as { ensureDaysLoaded: ReturnType<typeof vi.fn> }
    const days = stateAny.ensureDaysLoaded.mock.calls[0][0] as number[]
    for (const d of days) {
      expect(new Date(d * 1000).getHours()).toBe(0)
    }
    expect(days).toContain(Math.floor(new Date(2026, 9, 26).getTime() / 1000))
  })
})
