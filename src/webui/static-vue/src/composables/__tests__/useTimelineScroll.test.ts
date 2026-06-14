// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useTimelineScroll — density-change anchor preservation tests.
 *
 * `scrollLeft` is the absolute-coord offset past the time-grid
 * origin (the sticky channel column floats over it; it's not a
 * viewport-coord subtraction). Subtracting `channelColumnWidth`
 * from it mixes the two coord systems and produces an anchor
 * `channelColumnWidth / oldPxm` minutes earlier than what the
 * user is actually looking at; each density toggle compounds the
 * error and the program drifts further right.
 *
 * These tests pin the inversion math: anchorTime computed from
 * scrollLeft must round-trip through scrollToTime such that the
 * post-density scrollLeft places the user at the SAME wall-clock
 * time they were looking at pre-density. Two toggles in a row
 * must not accumulate drift.
 *
 * scrollToTime's forward math:
 *   xPx = channelColumnWidth + (target − effectiveStart)/60 * pxm
 *   scrollLeft (align='left') = xPx − channelColumnWidth
 *                              = (target − effectiveStart)/60 * pxm
 *
 * Anchor inversion (what we're testing):
 *   anchorTime = effectiveStart + scrollLeft / oldPxm * 60
 *
 * No `- channelColumnWidth`. The previous implementation had the
 * subtraction and is what we're guarding against here.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises } from '@vue/test-utils'
import { ref, type Ref } from 'vue'
import { useTimelineScroll } from '../useTimelineScroll'

interface ScrollState {
  scrollLeft: number
  clientWidth: number
}

function makeScrollEl(state: ScrollState): {
  el: HTMLElement
  scrollTo: ReturnType<typeof vi.fn>
} {
  const scrollTo = vi.fn((opts: { left?: number; behavior?: ScrollBehavior }) => {
    /* Mirror the browser's saturation behaviour: scrollLeft
     * clamps to [0, scrollWidth - clientWidth]. Tests don't
     * exercise the upper bound here, so just floor at 0. */
    if (typeof opts.left === 'number') state.scrollLeft = Math.max(0, opts.left)
  })
  const el = {
    get scrollLeft() {
      return state.scrollLeft
    },
    set scrollLeft(v: number) {
      state.scrollLeft = v
    },
    get clientWidth() {
      return state.clientWidth
    },
    scrollTo,
  } as unknown as HTMLElement
  return { el, scrollTo }
}

describe('useTimelineScroll — density-change anchor preservation', () => {
  let state: ScrollState
  let scrollTo: ReturnType<typeof vi.fn>
  let scrollEl: Ref<HTMLElement | null>
  const channelColumnWidth = ref(200)
  const pxPerMinute = ref(4)
  /* Arbitrary epoch — tests use offsets relative to this. */
  const EFFECTIVE_START = 1_700_000_000
  const effectiveStart = ref(EFFECTIVE_START)

  beforeEach(() => {
    state = { scrollLeft: 0, clientWidth: 1024 }
    const made = makeScrollEl(state)
    scrollTo = made.scrollTo
    scrollEl = ref(made.el)
    channelColumnWidth.value = 200
    pxPerMinute.value = 4
    effectiveStart.value = EFFECTIVE_START
  })

  it('preserves the visible-leading wall-clock time across a density change', async () => {
    /* User is at scrollLeft=800 with pxm=4. At pxm=4, the visible-
     * leading time is effectiveStart + 800/4*60 = +200 minutes. */
    useTimelineScroll({ scrollEl, channelColumnWidth, pxPerMinute, effectiveStart })
    state.scrollLeft = 800
    pxPerMinute.value = 8
    await flushPromises()
    /* After re-scroll at pxm=8 the SAME wall-clock time (200 min)
     * must sit at the visible-leading edge. That requires
     * scrollLeft = 200 min * 8 px/min = 1600. */
    expect(scrollTo).toHaveBeenCalledTimes(1)
    expect(scrollTo).toHaveBeenLastCalledWith({ left: 1600, behavior: 'instant' })
  })

  it('does not drift across multiple back-and-forth density toggles', async () => {
    /* Two-toggle round trip (pxm=4 → 8 → 4) must land the user
     * back at the exact same scrollLeft. The bug's symptom was
     * accumulating drift across many toggles; the fix's
     * correctness is the round-trip invariant. */
    useTimelineScroll({ scrollEl, channelColumnWidth, pxPerMinute, effectiveStart })
    state.scrollLeft = 800
    const startingScrollLeft = state.scrollLeft

    pxPerMinute.value = 8
    await flushPromises()
    pxPerMinute.value = 4
    await flushPromises()

    expect(scrollTo).toHaveBeenCalledTimes(2)
    expect(state.scrollLeft).toBe(startingScrollLeft)
  })

  it('skips re-scroll when scrollLeft is negative (defensive)', async () => {
    useTimelineScroll({ scrollEl, channelColumnWidth, pxPerMinute, effectiveStart })
    /* Negative scrollLeft is unreachable in normal browser behaviour
     * but the guard exists; verify the watch silently skips. */
    Object.defineProperty(scrollEl.value, 'scrollLeft', {
      get: () => -1,
      configurable: true,
    })
    pxPerMinute.value = 8
    await flushPromises()
    expect(scrollTo).not.toHaveBeenCalled()
  })

  it('skips re-scroll when the densities match (no-op guard)', async () => {
    useTimelineScroll({ scrollEl, channelColumnWidth, pxPerMinute, effectiveStart })
    state.scrollLeft = 500
    /* Trigger the watch with the same value — Vue still fires it
     * once for the assignment but the handler early-returns. */
    pxPerMinute.value = 4
    await flushPromises()
    expect(scrollTo).not.toHaveBeenCalled()
  })

  it('correctly anchors when scrollLeft is below the channel column width', async () => {
    /* Regression guard for the original bug shape: with the buggy
     * `- channelColumnWidth` subtraction, any scrollLeft < column-
     * width triggered the early return and skipped re-scroll
     * entirely, dropping the anchor preservation in the first few
     * hundred pixels of scroll. With the fix, scrollLeft=100 (less
     * than the 200-px channel column) anchors to a real
     * (effectiveStart + 100/oldPxm*60) time and re-scrolls
     * correctly. */
    useTimelineScroll({ scrollEl, channelColumnWidth, pxPerMinute, effectiveStart })
    state.scrollLeft = 100
    pxPerMinute.value = 8
    await flushPromises()
    /* anchorTime = effectiveStart + 100/4*60 = +25 min
     * new scrollLeft = 25 * 8 = 200 */
    expect(scrollTo).toHaveBeenCalledWith({ left: 200, behavior: 'instant' })
  })
})
