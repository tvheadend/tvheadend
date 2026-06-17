// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgViewportEmitter — `activeDay` derivation tests.
 *
 * Pins the leading-edge rule. A viewport-centre rule would break
 * late-evening Now anchors: at 23:00 with the default density,
 * the leading edge sits at today 22:30 but the centre lands ~2 h
 * into tomorrow — the picker would say tomorrow and the cascading
 * `state.dayStart` write would poison the sticky-position restore
 * (next-day fetch shifts effectiveStart, stored scrollTime
 * computes a negative offset, scrollLeft clamps to 0).
 *
 * Leading-edge matches reading direction and keeps the saved
 * dayStart consistent with the day the user is anchored to.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises } from '@vue/test-utils'
import { ref } from 'vue'
import { useEpgViewportEmitter } from '../useEpgViewportEmitter'
import { startOfLocalDayEpoch } from '../../views/epg/epgGridShared'

interface ScrollState {
  scrollLeft: number
  scrollTop: number
  clientWidth: number
  clientHeight: number
}

function makeScrollEl(state: ScrollState): HTMLElement {
  const listeners = new Set<EventListener>()
  return {
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
    addEventListener(_evt: string, fn: EventListener) {
      listeners.add(fn)
    },
    removeEventListener(_evt: string, fn: EventListener) {
      listeners.delete(fn)
    },
    /* Test hook — fire a scroll event so the rAF emitter ticks. */
    __dispatchScroll() {
      listeners.forEach((fn) => fn(new Event('scroll')))
    },
  } as unknown as HTMLElement & { __dispatchScroll(): void }
}

const ONE_DAY = 86400
/* Local-midnight reference. Compute via `startOfLocalDayEpoch` so
 * the test stays correct regardless of the test runner's timezone
 * (happy-dom inherits the host TZ). Then DAY+1 is just +86400 —
 * fine because no DST boundary sits inside this synthetic range. */
const TODAY_MIDNIGHT = startOfLocalDayEpoch(1_700_000_000)
const TOMORROW_MIDNIGHT = TODAY_MIDNIGHT + ONE_DAY

/* Stub rAF to fire synchronously so the test doesn't have to wait
 * a real animation frame; happy-dom provides one but we want the
 * tick to land before the assertion. */
beforeEach(() => {
  vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => {
    cb(0)
    return 0
  })
})

afterEach(() => {
  vi.unstubAllGlobals()
})

describe('useEpgViewportEmitter — activeDay derivation', () => {
  it('emits the LEADING-edge day when the viewport straddles a day boundary (Now after 22:00)', async () => {
    /* Late-evening Now anchor: leading edge today 22:30,
     * viewport ~4 h wide → trailing edge ~02:30 tomorrow. The
     * centre-time rule would have emitted TOMORROW; leading-edge
     * emits TODAY because the leading edge is what the user
     * reads. */
    const state: ScrollState = {
      scrollLeft: (22 * 60 + 30) * 4,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    }
    const el = makeScrollEl(state)
    /* Start with `null` so the composable's `watch(scrollEl)` actually
     * fires on assignment (the watch isn't `immediate: true` — it
     * latches when the template ref resolves). */
    const scrollEl = ref<HTMLElement | null>(null)
    const onActiveDay = vi.fn()
    const onViewportRange = vi.fn()

    useEpgViewportEmitter({
      axis: 'horizontal',
      scrollEl,
      axisOffset: () => 200,
      trackStart: () => TODAY_MIDNIGHT,
      trackEnd: () => TODAY_MIDNIGHT + ONE_DAY * 7,
      pxPerMinute: () => 4,
      onActiveDay,
      onViewportRange,
    })
    scrollEl.value = el
    await flushPromises()

    expect(onActiveDay).toHaveBeenCalledWith(TODAY_MIDNIGHT)
  })

  it('emits today when leading edge is mid-today (regression guard)', async () => {
    /* Sanity: scrolls into the middle of today must still emit
     * today, not a neighbouring day. */
    const state: ScrollState = {
      scrollLeft: 14 * 60 * 4 /* 14:00 today */,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    }
    const el = makeScrollEl(state)
    const scrollEl = ref<HTMLElement | null>(null)
    const onActiveDay = vi.fn()

    useEpgViewportEmitter({
      axis: 'horizontal',
      scrollEl,
      axisOffset: () => 200,
      trackStart: () => TODAY_MIDNIGHT,
      trackEnd: () => TODAY_MIDNIGHT + ONE_DAY * 7,
      pxPerMinute: () => 4,
      onActiveDay,
      onViewportRange: vi.fn(),
    })
    scrollEl.value = el
    await flushPromises()

    expect(onActiveDay).toHaveBeenCalledWith(TODAY_MIDNIGHT)
  })

  it('emits tomorrow when leading edge is on tomorrow (forward-scroll case)', async () => {
    /* When the user genuinely scrolled into tomorrow, the leading
     * edge is on tomorrow — picker should follow. */
    const state: ScrollState = {
      scrollLeft: (24 * 60 + 8 * 60) * 4 /* 08:00 tomorrow */,
      scrollTop: 0,
      clientWidth: 1200,
      clientHeight: 800,
    }
    const el = makeScrollEl(state)
    const scrollEl = ref<HTMLElement | null>(null)
    const onActiveDay = vi.fn()

    useEpgViewportEmitter({
      axis: 'horizontal',
      scrollEl,
      axisOffset: () => 200,
      trackStart: () => TODAY_MIDNIGHT,
      trackEnd: () => TODAY_MIDNIGHT + ONE_DAY * 7,
      pxPerMinute: () => 4,
      onActiveDay,
      onViewportRange: vi.fn(),
    })
    scrollEl.value = el
    await flushPromises()

    expect(onActiveDay).toHaveBeenCalledWith(TOMORROW_MIDNIGHT)
  })
})
