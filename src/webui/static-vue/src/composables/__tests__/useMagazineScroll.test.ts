// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useMagazineScroll — density-change anchor preservation tests.
 *
 * Vertical-axis counterpart to useTimelineScroll's test. Same
 * shape: `scrollTop` is the absolute-coord offset past the time-
 * grid origin (the sticky header floats over it; not a viewport-
 * coord subtraction). Subtracting `headerHeight` from it would
 * mix the two coord systems and capture an anchor
 * `headerHeight / oldPxm` minutes earlier than the user's actual
 * leading time, accumulating across density toggles.
 *
 * Forward math (scrollToTime, align='top'):
 *   yPx = headerHeight + (target − effectiveStart)/60 * pxm
 *   scrollTop = yPx − headerHeight = (target − effectiveStart)/60 * pxm
 *
 * Inversion:
 *   anchorTime = effectiveStart + scrollTop / oldPxm * 60
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises } from '@vue/test-utils'
import { ref, type Ref } from 'vue'
import { useMagazineScroll } from '../useMagazineScroll'

interface ScrollState {
  scrollTop: number
  clientHeight: number
}

function makeScrollEl(state: ScrollState): {
  el: HTMLElement
  scrollTo: ReturnType<typeof vi.fn>
} {
  const scrollTo = vi.fn((opts: { top?: number; behavior?: ScrollBehavior }) => {
    if (typeof opts.top === 'number') state.scrollTop = Math.max(0, opts.top)
  })
  const el = {
    get scrollTop() {
      return state.scrollTop
    },
    set scrollTop(v: number) {
      state.scrollTop = v
    },
    get clientHeight() {
      return state.clientHeight
    },
    scrollTo,
  } as unknown as HTMLElement
  return { el, scrollTo }
}

describe('useMagazineScroll — density-change anchor preservation', () => {
  let state: ScrollState
  let scrollTo: ReturnType<typeof vi.fn>
  let scrollEl: Ref<HTMLElement | null>
  const headerHeight = ref(80)
  const pxPerMinute = ref(4)
  const EFFECTIVE_START = 1_700_000_000
  const effectiveStart = ref(EFFECTIVE_START)

  beforeEach(() => {
    state = { scrollTop: 0, clientHeight: 768 }
    const made = makeScrollEl(state)
    scrollTo = made.scrollTo
    scrollEl = ref(made.el)
    headerHeight.value = 80
    pxPerMinute.value = 4
    effectiveStart.value = EFFECTIVE_START
  })

  it('preserves the visible-top wall-clock time across a density change', async () => {
    useMagazineScroll({ scrollEl, headerHeight, pxPerMinute, effectiveStart })
    state.scrollTop = 800
    pxPerMinute.value = 8
    await flushPromises()
    /* anchorTime = effectiveStart + 800/4*60 = +200 min
     * new scrollTop = 200 * 8 = 1600 */
    expect(scrollTo).toHaveBeenCalledTimes(1)
    expect(scrollTo).toHaveBeenLastCalledWith({ top: 1600, behavior: 'instant' })
  })

  it('does not drift across multiple back-and-forth density toggles', async () => {
    useMagazineScroll({ scrollEl, headerHeight, pxPerMinute, effectiveStart })
    state.scrollTop = 800
    const startingScrollTop = state.scrollTop

    pxPerMinute.value = 8
    await flushPromises()
    pxPerMinute.value = 4
    await flushPromises()

    expect(scrollTo).toHaveBeenCalledTimes(2)
    expect(state.scrollTop).toBe(startingScrollTop)
  })

  it('correctly anchors when scrollTop is below the header height', async () => {
    /* Regression for the original bug: with the buggy
     * `- headerHeight` subtraction, scrollTop < headerHeight
     * triggered the early-return guard, dropping anchor
     * preservation entirely in the first ~headerHeight pixels
     * of vertical scroll. */
    useMagazineScroll({ scrollEl, headerHeight, pxPerMinute, effectiveStart })
    state.scrollTop = 40
    pxPerMinute.value = 8
    await flushPromises()
    /* anchorTime = effectiveStart + 40/4*60 = +10 min
     * new scrollTop = 10 * 8 = 80 */
    expect(scrollTo).toHaveBeenCalledWith({ top: 80, behavior: 'instant' })
  })
})
