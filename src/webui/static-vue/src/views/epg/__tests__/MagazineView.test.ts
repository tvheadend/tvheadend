// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * MagazineView — sticky-position save coordinates.
 *
 * The Magazine track is continuous and its sticky header floats
 * OVER the track, so `scrollTop` IS the leading-edge offset in
 * track coordinates — the same coordinate system the restore path
 * (`restoreToPosition` in useEpgScrollDaySync) and the viewport
 * emitter use. The save handler must therefore not subtract the
 * header height: doing so makes every save/restore cycle land
 * ~headerHeight px earlier and the drift compounds across
 * navigations.
 *
 * Child components are stubbed; the EpgMagazine stub exposes a
 * real scroll element so the view's scroll listener and debounce
 * run for real.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { computed, defineComponent, h, ref } from 'vue'
import { flushPromises, mount } from '@vue/test-utils'
import MagazineView from '../MagazineView.vue'
import type { useEpgViewState } from '@/composables/useEpgViewState'
import type { EpgViewOptions } from '@/views/epg/epgViewOptions'

const ONE_DAY = 86400
const TRACK_START = (() => {
  const d = new Date(1_700_000_000 * 1000)
  d.setHours(0, 0, 0, 0)
  return Math.floor(d.getTime() / 1000)
})()
const PX_PER_MINUTE = 4
const HEADER_HEIGHT = 88

const saveStickyPosition = vi.fn()

/* Minimal useEpgViewState surface — the slices MagazineView, its
 * template bindings and useEpgScrollDaySync touch. */
function makeFakeState(): ReturnType<typeof useEpgViewState> {
  const dayStart = ref(TRACK_START)
  const viewOptions = ref<EpgViewOptions>({
    tagFilter: { tag: null },
    channelDisplay: { logo: true, name: true, number: false },
    channelSort: 'number',
    tooltipMode: 'off',
    density: { timeline: 'default', magazine: 'default' },
    dvrOverlay: 'event',
    dvrOverlayShowDisabled: false,
    progressDisplay: 'bar',
    progressColoured: false,
    stickyTitles: true,
    darkChannelBackground: false,
    columnVisibility: {},
    groupField: null,
    groupOrder: 'ASC',
    titleSearchMode: 'title',
    timeWindow: 'all',
    genre: [],
    newOnly: false,
    durationMinMinutes: null,
    durationMaxMinutes: null,
  })
  return {
    dayStart,
    dayEnd: computed(() => dayStart.value + ONE_DAY),
    isToday: computed(() => true),
    goToToday: vi.fn(),
    goToTomorrow: vi.fn(),
    setDayStart: (epoch: number) => {
      dayStart.value = epoch
    },
    consumeDayStartScrollSuppressed: () => false,
    dayStartForOffset: (o: number) => TRACK_START + o * ONE_DAY,
    trackStart: computed(() => TRACK_START),
    trackEnd: computed(() => TRACK_START + 14 * ONE_DAY),
    loadedDays: ref(new Set<number>()),
    loadingDays: ref(new Set<number>()),
    ensureDaysLoaded: vi.fn(),
    toolbarEl: ref(null),
    setToolbarEl: vi.fn(),
    inlineDayButtons: computed(() => []),
    picklistOptions: computed(() => []),
    picklistActive: computed(() => false),
    restoredPosition: ref(null),
    saveStickyPosition,
    saveLastView: vi.fn(),
    channels: ref([]),
    events: ref([]),
    filteredChannels: computed(() => [{ uuid: 'ch-1' }]),
    dvrEntries: computed(() => []),
    tags: computed(() => []),
    tagsLoading: ref(false),
    tagsError: ref(null),
    loadTags: vi.fn(),
    channelsLoading: ref(false),
    eventsLoading: ref(false),
    channelsError: ref(null),
    eventsError: ref(null),
    loading: computed(() => false),
    error: computed(() => null),
    loadChannels: vi.fn(),
    eventsTotalCount: ref(0),
    loadingMorePage: ref(false),
    hasMorePages: computed(() => false),
    refreshMatchedCount: vi.fn(),
    loadPage: vi.fn(),
    isPhone: ref(false),
    noHover: ref(false),
    selectedEvent: computed(() => null),
    openDrawer: vi.fn(),
    toggleDrawer: vi.fn(),
    closeDrawer: vi.fn(),
    viewOptions,
    setViewOptions: vi.fn(),
    currentDefaults: computed(() => viewOptions.value),
  }
}

vi.mock('@/composables/useEpgViewState', () => ({
  useEpgViewState: () => makeFakeState(),
}))

vi.mock('@/composables/useEpgViewWrapper', () => ({
  useEpgViewWrapper: () => ({
    now: { value: 1_700_000_000 },
    pxPerMinute: { value: PX_PER_MINUTE },
    titleOnly: { value: false },
    onDvrClick: () => undefined,
    loadingDaysArray: { value: [] },
    channels: { value: [] },
    events: { value: [] },
  }),
}))

vi.mock('@/composables/useMagazineScroll', () => ({
  useMagazineScroll: () => ({
    scrollToNow: () => undefined,
    scrollToTime: () => undefined,
  }),
}))

vi.mock('@/composables/useEpgInitialScrollToNow', () => ({
  useEpgInitialScrollToNow: () => undefined,
}))

vi.mock('@/composables/useTextScale', async () => {
  const { ref: vueRef } = await import('vue')
  return { useTextScale: () => vueRef(1) }
})

/* EpgMagazine stub exposing a REAL scroll element, mirroring the
 * component's defineExpose surface (scrollEl / headerHeight /
 * effectiveStart). */
let fakeScrollEl: HTMLElement

function makeMagazineStub() {
  return defineComponent({
    name: 'EpgMagazine',
    setup(_, { expose }) {
      expose({
        scrollEl: fakeScrollEl,
        headerHeight: HEADER_HEIGHT,
        effectiveStart: TRACK_START,
      })
      return () => h('div')
    },
  })
}

beforeEach(() => {
  vi.useFakeTimers()
  saveStickyPosition.mockClear()
  fakeScrollEl = document.createElement('div')
})

afterEach(() => {
  vi.useRealTimers()
})

describe('MagazineView — position save', () => {
  it('saves scrollTime in track coordinates so restore lands back at the same scrollTop', async () => {
    const wrapper = mount(MagazineView, {
      global: {
        stubs: {
          EpgMagazine: makeMagazineStub(),
          EpgToolbar: true,
          EpgEventDrawer: true,
        },
      },
    })
    await flushPromises()

    /* User parks the viewport at scrollTop = 400 px. */
    fakeScrollEl.scrollTop = 400
    fakeScrollEl.dispatchEvent(new Event('scroll'))
    vi.advanceTimersByTime(500) /* save debounce */

    expect(saveStickyPosition).toHaveBeenCalledTimes(1)
    const { scrollTime } = saveStickyPosition.mock.calls[0][0] as {
      scrollTime: number
    }
    /* scrollTop is already the leading-edge track offset (the
     * sticky header floats over the track), so the saved time
     * derives from the raw value — no header subtraction. */
    expect(scrollTime).toBe(TRACK_START + (400 / PX_PER_MINUTE) * 60)

    /* Restore math (restoreToPosition in useEpgScrollDaySync):
     * targetPx = (scrollTime − trackStart)/60 · pxm — must give
     * back the exact scrollTop the save observed, i.e. the two
     * sides share one coordinate system. */
    const restoredPx = ((scrollTime - TRACK_START) / 60) * PX_PER_MINUTE
    expect(restoredPx).toBe(400)

    wrapper.unmount()
  })
})
