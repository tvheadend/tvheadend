// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEpgInitialScrollToNow — restore-path routing tests.
 *
 * When `clampSameDayScrollTimeForward` pushes a stale same-day
 * scrollTime forward to nowEpoch, the restore path must NOT use
 * the literal clamped time with leftThird/topThird alignment —
 * that would put the leading 1/3 of the viewport on events with
 * `stop < now` which the server filter drops, producing a blank
 * wedge before the now-cursor. Clamped restores route through
 * `scrollToNow` instead, which forces align='left' + a half-hour
 * snap so the leftmost cell is always a currently-airing event.
 */

import { describe, expect, it, vi } from 'vitest'
import { computed, ref, type Ref } from 'vue'
import { flushPromises } from '@vue/test-utils'
import { useEpgInitialScrollToNow } from '../useEpgInitialScrollToNow'
import type { StickyPosition } from '../epgPositionStorage'

interface StateStub {
  filteredChannels: Ref<unknown[]>
  events: Ref<unknown[]>
  isToday: Ref<boolean>
  restoredPosition: Ref<StickyPosition | null>
}

function makeState(initial?: Partial<StateStub>): StateStub {
  return {
    filteredChannels: initial?.filteredChannels ?? ref([{ uuid: 'a' }]),
    events: initial?.events ?? ref([{ eventId: 1 }]),
    isToday: initial?.isToday ?? ref(true),
    restoredPosition: initial?.restoredPosition ?? ref(null),
  }
}

/* The composable awaits a `requestAnimationFrame` for layout to
 * settle before issuing the scroll. happy-dom provides rAF, but
 * stubbing to fire synchronously keeps the test tight without a
 * sleep loop. */
function stubRAF(): void {
  vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => {
    cb(0)
    return 0
  })
}

describe('useEpgInitialScrollToNow — restore vs scroll-to-now routing', () => {
  it('routes a clamped restored position through scrollToNow (not scrollToTime)', async () => {
    /* Same-day stale restore that got clamped forward by
     * clampSameDayScrollTimeForward MUST take the snap+left-align
     * scrollToNow branch — otherwise the caller's leftThird/
     * topThird alignment leaves a blank wedge before the now-
     * cursor. */
    stubRAF()
    const state = makeState({
      restoredPosition: ref({
        dayStart: 1_700_000_000,
        scrollTime: 1_700_050_000,
        topChannelUuid: 'a',
        wasClamped: true,
      }),
    })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToNow = vi.fn()
    const scrollToTime = vi.fn()
    const restoreTopChannel = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow,
      scrollToTime,
      restoreTopChannel,
      align: 'leftThird',
    })
    /* Trigger the watch — events array swap acts as the wake-up
     * (mirrors how the real composable fires when events first
     * arrive). */
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(scrollToNow).toHaveBeenCalledTimes(1)
    expect(scrollToTime).not.toHaveBeenCalled()
    /* Top channel still restored — the clamp only affects the
     * time axis, the channel axis is orthogonal. */
    expect(restoreTopChannel).toHaveBeenCalledWith('a')
  })

  it('routes a normal (non-clamped) restored position through scrollToTime', async () => {
    /* Forward-scrolled position (e.g. user planning into tomorrow)
     * — the clamp helper leaves wasClamped undefined and we want
     * to land EXACTLY at the persisted time, not snap to now. */
    stubRAF()
    const state = makeState({
      restoredPosition: ref({
        dayStart: 1_700_000_000,
        scrollTime: 1_700_080_000,
        topChannelUuid: 'a',
      }),
    })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToNow = vi.fn()
    const scrollToTime = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow,
      scrollToTime,
      align: 'leftThird',
    })
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(scrollToTime).toHaveBeenCalledTimes(1)
    expect(scrollToTime).toHaveBeenCalledWith(1_700_080_000, expect.objectContaining({ align: 'leftThird' }))
    expect(scrollToNow).not.toHaveBeenCalled()
  })

  it('falls through to scrollToNow when no restored position exists and today is active', async () => {
    /* Default first-paint path: fresh session, no sticky position. */
    stubRAF()
    const state = makeState({ restoredPosition: ref(null) })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToNow = vi.fn()
    const scrollToTime = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow,
      scrollToTime,
      align: 'leftThird',
    })
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(scrollToNow).toHaveBeenCalledTimes(1)
    expect(scrollToTime).not.toHaveBeenCalled()
  })

  it('starts following on the default scroll-to-now path via onFollowNow', async () => {
    /* When the view provides onFollowNow (the follow-now wiring), the
     * scroll-to-now path routes through it instead of scrollToNow, so
     * the viewport starts pinned to the live edge. */
    stubRAF()
    const state = makeState({ restoredPosition: ref(null) })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToNow = vi.fn()
    const onFollowNow = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow,
      align: 'leftThird',
      onFollowNow,
    })
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(onFollowNow).toHaveBeenCalledTimes(1)
    expect(onFollowNow).toHaveBeenCalledWith('instant')
    expect(scrollToNow).not.toHaveBeenCalled()
  })

  it('starts following on the wasClamped restore path via onFollowNow', async () => {
    /* A clamped restore is a "show me now" intent, so it should follow
     * too. */
    stubRAF()
    const state = makeState({
      restoredPosition: ref({
        dayStart: 1_700_000_000,
        scrollTime: 1_700_050_000,
        topChannelUuid: 'a',
        wasClamped: true,
      }),
    })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToNow = vi.fn()
    const onFollowNow = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow,
      restoreTopChannel: vi.fn(),
      align: 'leftThird',
      onFollowNow,
    })
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(onFollowNow).toHaveBeenCalledTimes(1)
    expect(scrollToNow).not.toHaveBeenCalled()
  })

  it('does NOT start following on a normal restore (user had a saved position)', async () => {
    /* A non-clamped restore lands the user where they saved (possibly a
     * future day) — the live-edge follow must stay off. */
    stubRAF()
    const state = makeState({
      restoredPosition: ref({
        dayStart: 1_700_000_000,
        scrollTime: 1_700_080_000,
        topChannelUuid: 'a',
      }),
    })
    const el = document.createElement('div')
    const scrollEl = computed(() => el as HTMLElement | null)
    const scrollToTime = vi.fn()
    const onFollowNow = vi.fn()

    useEpgInitialScrollToNow({
      state: state as unknown as Parameters<typeof useEpgInitialScrollToNow>[0]['state'],
      scrollEl,
      scrollToNow: vi.fn(),
      scrollToTime,
      align: 'leftThird',
      onFollowNow,
    })
    state.events.value = [{ eventId: 1 }, { eventId: 2 }]
    await flushPromises()
    await flushPromises()

    expect(onFollowNow).not.toHaveBeenCalled()
    expect(scrollToTime).toHaveBeenCalledTimes(1)
  })
})
