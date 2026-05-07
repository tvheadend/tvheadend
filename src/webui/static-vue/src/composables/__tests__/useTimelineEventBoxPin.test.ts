/*
 * useTimelineEventBoxPin unit tests. Mirror of
 * useMagazineEventAllocator.test.ts plumbing: same harness
 * pattern, same IntersectionObserver mock, same lifecycle /
 * IO / memo / reactivity coverage. Assertions target the
 * imperative box geometry (`box.style.left`, `box.style.width`)
 * rather than CSS variables — the inner-element transform path
 * is gone (see composable docstring for why).
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, ref, type Ref } from 'vue'
import { mount } from '@vue/test-utils'
import {
  installIntersectionObserverMock,
  MockIntersectionObserver,
} from '@/test/__helpers__/intersectionObserverMock'
import {
  useTimelineEventBoxPin,
  type TimelineEventBoxPin,
} from '../useTimelineEventBoxPin'

interface TestEvent {
  eventId: number
  start?: number
  stop?: number
}

function buildEvent(eventId: number, overrides: Partial<TestEvent> = {}): TestEvent {
  return {
    eventId,
    start: 1000,
    stop: 1000 + 60 * 60 /* 1-hour event */,
    ...overrides,
  }
}

function mountHarness(opts: {
  events: TestEvent[]
  stickyTitles?: boolean
  pxPerMinute?: number
  effectiveStart?: number
  effectiveEnd?: number
}): {
  pin: TimelineEventBoxPin
  scrollEl: HTMLElement
  eventsRef: Ref<TestEvent[]>
  setProp: (key: 'stickyTitles' | 'pxPerMinute', value: number | boolean) => void
  setScrollLeft: (value: number) => void
  unmount: () => void
} {
  const eventsRef = ref<TestEvent[]>(opts.events)
  const stickyTitlesRef = ref(opts.stickyTitles ?? false)
  const pxPerMinuteRef = ref(opts.pxPerMinute ?? 4)
  const effectiveStartRef = ref(opts.effectiveStart ?? 0)
  const effectiveEndRef = ref(opts.effectiveEnd ?? 1_000_000_000)

  let pinHandle: TimelineEventBoxPin | null = null
  let scrollElHandle: HTMLElement | null = null
  let scrollLeftValue = 0

  const Harness = defineComponent({
    name: 'TimelineBoxPinHarness',
    setup() {
      const scrollEl = ref<HTMLElement | null>(null)
      const pin = useTimelineEventBoxPin<TestEvent>({
        scrollEl,
        events: eventsRef,
        stickyTitles: () => stickyTitlesRef.value,
        pxPerMinute: () => pxPerMinuteRef.value,
        effectiveStart: effectiveStartRef,
        effectiveEnd: effectiveEndRef,
      })
      pinHandle = pin
      return { scrollEl }
    },
    mounted() {
      scrollElHandle = (this.$refs as { scrollEl: HTMLElement }).scrollEl
      Object.defineProperty(scrollElHandle, 'clientWidth', {
        configurable: true,
        get: () => 800,
      })
      Object.defineProperty(scrollElHandle, 'scrollLeft', {
        configurable: true,
        get: () => scrollLeftValue,
        set: (v: number) => {
          scrollLeftValue = v
        },
      })
    },
    render() {
      return h('div', { ref: 'scrollEl' }, [])
    },
  })

  const wrapper = mount(Harness)

  return {
    pin: pinHandle!,
    scrollEl: scrollElHandle!,
    eventsRef,
    setProp(key, value) {
      const map = {
        stickyTitles: stickyTitlesRef,
        pxPerMinute: pxPerMinuteRef,
      }
      ;(map[key] as Ref<number | boolean>).value = value
    },
    setScrollLeft(value) {
      scrollLeftValue = value
    },
    unmount: () => wrapper.unmount(),
  }
}

function buildEventEl(): HTMLElement {
  const el = document.createElement('button')
  el.className = 'epg-timeline__event'
  document.body.appendChild(el)
  return el
}

beforeEach(() => {
  installIntersectionObserverMock()
  MockIntersectionObserver.reset()
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('lifecycle', () => {
  it('bind(id, ev, el) registers the element in the internal map', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    expect(pin._internals.elements.get(1)).toBe(el)
    expect(pin._internals.eventData.get(1)?.eventId).toBe(1)
    unmount()
  })

  it('bind(id, ev, el) observes the element via IntersectionObserver', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.has(el)).toBe(true)
    unmount()
  })

  it('bind synchronously applies (box.style.left set on bind)', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    /* eventLeft = (1000 - 0)/60 * 4 = 66.666… → rounded to 67 */
    pin.bind(1, buildEvent(1), el)
    expect(el.style.left).toBe('67px')
    /* width = (60min * 4 px/min) → 240, but rounded boundaries
     * give right=307 - left=67 → 240. */
    expect(el.style.width).toBe('240px')
    unmount()
  })

  it('bind(id, ev, null) unobserves and removes from all internal maps', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    expect(pin._internals.visible.has(1)).toBe(true)

    pin.bind(1, buildEvent(1), null)
    expect(pin._internals.elements.has(1)).toBe(false)
    expect(pin._internals.eventData.has(1)).toBe(false)
    expect(pin._internals.visible.has(1)).toBe(false)
    expect(io.observed.has(el)).toBe(false)
    unmount()
  })

  it('rebind same id with new ev updates the eventData map', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 100 }), el)
    expect(pin._internals.eventData.get(1)?.start).toBe(100)
    pin.bind(1, buildEvent(1, { start: 200 }), el)
    expect(pin._internals.eventData.get(1)?.start).toBe(200)
    unmount()
  })

  it('rebind same id with different element swaps the element + unobserves the old', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el1 = buildEventEl()
    const el2 = buildEventEl()
    pin.bind(1, buildEvent(1), el1)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.has(el1)).toBe(true)
    pin.bind(1, buildEvent(1), el2)
    expect(pin._internals.elements.get(1)).toBe(el2)
    expect(io.observed.has(el1)).toBe(false)
    expect(io.observed.has(el2)).toBe(true)
    unmount()
  })

  it('unmount disconnects the IO and clears all internal maps', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.size).toBe(1)
    unmount()
    expect(io.observed.size).toBe(0)
    expect(pin._internals.elements.size).toBe(0)
  })
})

describe('IntersectionObserver integration', () => {
  it('events not yet visible are skipped by applyVisible', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    expect(pin._internals.visible.has(1)).toBe(false)
    pin._internals.resetSetPropertyCount()
    pin.applyVisible()
    expect(pin._internals.setPropertyCount()).toBe(0)
    unmount()
  })

  it('IO fires "now visible" → element added to visible Set', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    expect(pin._internals.visible.has(1)).toBe(true)
    unmount()
  })

  it('IO fires "no longer visible" → removed from visible Set', () => {
    const { pin, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    io.simulate([{ target: el, isIntersecting: false }])
    expect(pin._internals.visible.has(1)).toBe(false)
    unmount()
  })
})

describe('box-pin geometry', () => {
  it('stickyTitles=false → box at natural geometry regardless of scroll', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    /* eventLeft = 0, width = 240. */
    expect(el.style.left).toBe('0px')
    expect(el.style.width).toBe('240px')
    /* Even though scroll moves, sticky off → no pin. The
     * internal write skips because `stickyTitles=false` short-
     * circuits to natural geometry; visibility-set is empty so
     * applyVisible is a no-op. Re-bind to force a fresh apply. */
    setScrollLeft(500)
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    expect(el.style.left).toBe('0px')
    expect(el.style.width).toBe('240px')
    unmount()
  })

  it('stickyTitles=false also writes --title-shift-on=0', () => {
    const { pin, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('0')
    unmount()
  })

  it('stickyTitles=true + scrolled past start → box pinned, width shrunk', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    /* eventLeft = 0, eventRight = 240. ScrollLeft = 100 →
     *   pinnedLeft = max(0, 100) = 100
     *   pinnedWidth = 240 - 100 = 140
     *   pinned = 1
     */
    setScrollLeft(100)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    pin.applyVisible()
    expect(el.style.left).toBe('100px')
    expect(el.style.width).toBe('140px')
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('1')
    unmount()
  })

  it('stickyTitles=true + not scrolled → box at natural geometry, --title-shift-on=0', () => {
    const { pin, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    expect(el.style.left).toBe('0px')
    expect(el.style.width).toBe('240px')
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('0')
    unmount()
  })

  it('scrolled past trailing edge → pinnedWidth clamped to 0', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    /* eventRight = 240. scrollLeft = 400 → fully past →
     * pinnedWidth = max(0, 240 - 400) = 0. */
    setScrollLeft(400)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    pin.applyVisible()
    expect(el.style.left).toBe('400px')
    expect(el.style.width).toBe('0px')
    unmount()
  })

  it('forces --title-shift-on=1 for events whose ev.start precedes effectiveStart (e.g. yesterday-spanning)', () => {
    const { pin, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 1000,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* Event started at 0 (yesterday); effectiveStart = 1000
     * (today midnight). visibleStart clamps to 1000, naturalLeft
     * = 0, scrollLeft = 0. Box is geometrically NOT pinned
     * (scrollPos === naturalStart, not strictly >), but the
     * gradient should still be on because the event has off-
     * screen-left content (the yesterday-portion). */
    pin.bind(1, buildEvent(1, { start: 0, stop: 5000 }), el)
    /* Box at natural left=0 (no geometric pinning). */
    expect(el.style.left).toBe('0px')
    /* Gradient gate: clipped-start → on. */
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('1')
    unmount()
  })

  it('effectiveStart clamps event-left to track origin', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 1000,
    })
    const el = buildEventEl()
    /* Event from 0..5000s; effectiveStart=1000 clamps
     * visibleStart=1000 → eventLeft = 0. visibleStop=5000 →
     * eventRight = (5000-1000)/60 * 4 = 266.667 → 267. */
    pin.bind(1, buildEvent(1, { start: 0, stop: 5000 }), el)
    setScrollLeft(50)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    pin.applyVisible()
    expect(el.style.left).toBe('50px')
    expect(el.style.width).toBe('217px') /* 267 - 50 */
    unmount()
  })
})

describe('memoization', () => {
  it('re-apply with same scroll position → no setProperty', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    setScrollLeft(200)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    pin.applyVisible()
    pin._internals.resetSetPropertyCount()
    /* Same scroll position → memo hit → no setProperty. */
    pin.applyVisible()
    expect(pin._internals.setPropertyCount()).toBe(0)
    unmount()
  })

  it('1 px scroll change → cache miss → fresh write (no bucketing)', () => {
    const { pin, setScrollLeft, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    setScrollLeft(200)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    pin.applyVisible()
    pin._internals.resetSetPropertyCount()
    /* No bucketing — every integer-pixel scroll change writes.
     * The previous transform-based design used a 4 px bucket
     * which caused visible jitter (block scrolled while shift
     * stayed put between bucket transitions). Box-pin writes
     * 3 properties (left, width, --title-shift-on). */
    setScrollLeft(201)
    pin.applyVisible()
    expect(pin._internals.setPropertyCount()).toBe(3)
    unmount()
  })

  it('changing pxPerMinute invalidates memo (re-applies via watcher)', async () => {
    const { pin, setProp, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    pin._internals.resetSetPropertyCount()
    setProp('pxPerMinute', 8)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(pin._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })
})

describe('reactivity wiring', () => {
  it('events ref reassignment triggers applyAll (post-flush)', async () => {
    const { pin, eventsRef, unmount } = mountHarness({ events: [buildEvent(1)] })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    pin._internals.resetSetPropertyCount()
    eventsRef.value = [buildEvent(1, { start: 5000 })]
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(pin._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })

  it('stickyTitles toggle triggers applyAll', async () => {
    const { pin, setProp, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    pin._internals.resetSetPropertyCount()
    setProp('stickyTitles', true)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(pin._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })

  it('pxPerMinute change triggers applyAll', async () => {
    const { pin, setProp, unmount } = mountHarness({
      events: [],
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    pin.bind(1, buildEvent(1), el)
    pin._internals.resetSetPropertyCount()
    setProp('pxPerMinute', 8)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(pin._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })
})
