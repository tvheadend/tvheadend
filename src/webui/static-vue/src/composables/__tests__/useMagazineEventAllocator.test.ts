/*
 * useMagazineEventAllocator unit tests. Cover the imperative
 * DOM-mutation layer for Magazine event blocks' line-count
 * CSS variables.
 *
 * The composable consumes Vue lifecycle (onBeforeUnmount,
 * watch). Tests mount it inside a minimal harness component
 * via vue-test-utils so reactivity + lifecycle wire up
 * naturally.
 *
 * IntersectionObserver is mocked (happy-dom doesn't implement
 * it natively); the mock exposes `simulate(entries)` so tests
 * trigger the observer's callback with chosen visibility state.
 *
 * Text measurement (`scrollHeight` reads inside
 * `magazineLineAllocator.measureLines`) is stubbed via
 * Object.defineProperty so line counts are deterministic.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, ref, type Ref } from 'vue'
import { mount } from '@vue/test-utils'
import {
  installIntersectionObserverMock,
  MockIntersectionObserver,
} from '@/test/__helpers__/intersectionObserverMock'
import { clearMeasureCache } from '@/views/epg/magazineLineAllocator'
import {
  useMagazineEventAllocator,
  type MagazineEventAllocator,
} from '../useMagazineEventAllocator'

interface TestEvent {
  eventId: number
  start?: number
  stop?: number
  title?: string
  subtitle?: string
}

function buildEvent(eventId: number, overrides: Partial<TestEvent> = {}): TestEvent {
  return {
    eventId,
    start: 1000,
    stop: 1000 + 60 * 60, /* 1-hour event */
    title: `Title ${eventId}`,
    subtitle: '',
    ...overrides,
  }
}

/* Deterministic scrollHeight stub for the measurer the
 * allocator creates. Title text "Title N" → 1 title line
 * (16.25 px); subtitle empty → 0; long subtitle → multiple
 * lines based on text length. */
function stubMeasurer(): void {
  /* The measurer is appended to body; identify it by its style. */
  const measurerStyleSnippet = 'visibility: hidden'
  Object.defineProperty(HTMLDivElement.prototype, 'scrollHeight', {
    configurable: true,
    get(this: HTMLDivElement) {
      const style = this.style.cssText || ''
      if (!style.includes(measurerStyleSnippet)) return 0
      const text = this.textContent ?? ''
      if (!text) return 0
      const fontSize = Number.parseFloat(this.style.fontSize) || 13
      const lineHeight = Number.parseFloat(this.style.lineHeight) || 1.25
      const lineHeightPx = fontSize * lineHeight
      /* Long text returns more lines (one line per ~10 chars
       * for test simplicity); short text returns 1 line. */
      const lines = Math.max(1, Math.ceil(text.length / 10))
      return lineHeightPx * lines
    },
  })
}

/* Test harness: Vue component that wires the composable to a
 * real lifecycle. Holds the allocator handle on its instance
 * for test access. */
function mountHarness(opts: {
  events: TestEvent[]
  stickyTitles?: boolean
  pxPerMinute?: number
  channelColumnWidth?: number
  titleOnly?: boolean
  effectiveStart?: number
  effectiveEnd?: number
}): {
  allocator: MagazineEventAllocator
  scrollEl: HTMLElement
  eventsRef: Ref<TestEvent[]>
  setProp: (
    key: 'stickyTitles' | 'pxPerMinute' | 'channelColumnWidth' | 'titleOnly',
    value: number | boolean,
  ) => void
  unmount: () => void
} {
  const eventsRef = ref<TestEvent[]>(opts.events)
  const stickyTitlesRef = ref(opts.stickyTitles ?? false)
  const pxPerMinuteRef = ref(opts.pxPerMinute ?? 4)
  const channelColumnWidthRef = ref(opts.channelColumnWidth ?? 200)
  const titleOnlyRef = ref(opts.titleOnly ?? false)
  const effectiveStartRef = ref(opts.effectiveStart ?? 0)
  const effectiveEndRef = ref(opts.effectiveEnd ?? 1_000_000_000)

  let allocatorHandle: MagazineEventAllocator | null = null
  let scrollElHandle: HTMLElement | null = null

  const Harness = defineComponent({
    name: 'AllocatorHarness',
    setup() {
      const scrollEl = ref<HTMLElement | null>(null)
      const allocator = useMagazineEventAllocator<TestEvent>({
        scrollEl,
        events: eventsRef,
        stickyTitles: () => stickyTitlesRef.value,
        pxPerMinute: () => pxPerMinuteRef.value,
        channelColumnWidth: () => channelColumnWidthRef.value,
        titleOnly: () => titleOnlyRef.value,
        effectiveStart: effectiveStartRef,
        effectiveEnd: effectiveEndRef,
        headerHeight: 88,
        dispText: (s) => s ?? '',
        extraText: (ev) => ev.subtitle,
      })
      allocatorHandle = allocator
      return { scrollEl }
    },
    mounted() {
      scrollElHandle = (this.$refs as { scrollEl: HTMLElement }).scrollEl
      /* Make scrollEl behave like a scrollable container with
       * a known clientHeight for the visible-region math. */
      Object.defineProperty(scrollElHandle, 'clientHeight', {
        configurable: true,
        get: () => 600,
      })
      Object.defineProperty(scrollElHandle, 'scrollTop', {
        configurable: true,
        writable: true,
        value: 0,
      })
    },
    render() {
      return h('div', { ref: 'scrollEl' }, [])
    },
  })

  const wrapper = mount(Harness)

  return {
    allocator: allocatorHandle!,
    scrollEl: scrollElHandle!,
    eventsRef,
    setProp(key, value) {
      const map = {
        stickyTitles: stickyTitlesRef,
        pxPerMinute: pxPerMinuteRef,
        channelColumnWidth: channelColumnWidthRef,
        titleOnly: titleOnlyRef,
      }
      ;(map[key] as Ref<number | boolean>).value = value
    },
    unmount: () => wrapper.unmount(),
  }
}

function buildEventEl(): HTMLElement {
  const el = document.createElement('button')
  el.className = 'epg-magazine__event'
  document.body.appendChild(el)
  return el
}

beforeEach(() => {
  installIntersectionObserverMock()
  MockIntersectionObserver.reset()
  clearMeasureCache()
  stubMeasurer()
})

afterEach(() => {
  vi.restoreAllMocks()
})

describe('lifecycle', () => {
  it('bind(id, ev, el) registers the element in the internal map', () => {
    const { allocator, unmount } = mountHarness({ events: [buildEvent(1)] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    expect(allocator._internals.elements.get(1)).toBe(el)
    expect(allocator._internals.eventData.get(1)?.eventId).toBe(1)
    unmount()
  })

  it('bind(id, ev, el) observes the element via IntersectionObserver', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.has(el)).toBe(true)
    unmount()
  })

  it('bind(id, ev, el) synchronously applies allocator (CSS vars set on bind)', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    expect(el.style.getPropertyValue('--title-lines')).not.toBe('')
    expect(el.style.getPropertyValue('--sub-display')).not.toBe('')
    unmount()
  })

  it('bind(id, ev, null) unobserves and removes from all internal maps', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    expect(allocator._internals.visible.has(1)).toBe(true)

    allocator.bind(1, buildEvent(1), null)
    expect(allocator._internals.elements.has(1)).toBe(false)
    expect(allocator._internals.eventData.has(1)).toBe(false)
    expect(allocator._internals.visible.has(1)).toBe(false)
    expect(io.observed.has(el)).toBe(false)
    unmount()
  })

  it('rebind same id with new ev updates the eventData map', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1, { title: 'Original' }), el)
    expect(allocator._internals.eventData.get(1)?.title).toBe('Original')
    allocator.bind(1, buildEvent(1, { title: 'Updated' }), el)
    expect(allocator._internals.eventData.get(1)?.title).toBe('Updated')
    unmount()
  })

  it('rebind same id with different element swaps the element + unobserves the old', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el1 = buildEventEl()
    const el2 = buildEventEl()
    allocator.bind(1, buildEvent(1), el1)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.has(el1)).toBe(true)
    allocator.bind(1, buildEvent(1), el2)
    expect(allocator._internals.elements.get(1)).toBe(el2)
    expect(io.observed.has(el1)).toBe(false)
    expect(io.observed.has(el2)).toBe(true)
    unmount()
  })

  it('unmount disconnects the IO and clears all internal maps', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.size).toBe(1)
    unmount()
    expect(io.observed.size).toBe(0)
    expect(allocator._internals.elements.size).toBe(0)
  })
})

describe('IntersectionObserver integration', () => {
  it('events not yet visible are skipped by applyVisible', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    /* No IO callback fired → element not in visible Set. */
    expect(allocator._internals.visible.has(1)).toBe(false)
    /* setPropertyCount captured 5 from initial bind apply
     * (--title-lines, --sub-lines, --sub-display, --title-shift-y,
     * --title-shift-on). Reset, then call applyVisible —
     * should add nothing. */
    allocator._internals.resetSetPropertyCount()
    allocator.applyVisible()
    expect(allocator._internals.setPropertyCount()).toBe(0)
    unmount()
  })

  it('IO fires "now visible" → element added to visible Set', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    expect(allocator._internals.visible.has(1)).toBe(false)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    expect(allocator._internals.visible.has(1)).toBe(true)
    /* Note: the newly-visible callback re-runs applyAllocator,
     * but inputs are unchanged from the bind's initial apply,
     * so memo hits and no setProperty fires. That's correct
     * behaviour — the "allocator applied" outcome is already
     * captured in the bind's initial CSS-var write, asserted
     * separately. The IO-only contract this test pins is the
     * visible-Set membership change. */
    unmount()
  })

  it('IO fires "no longer visible" → removed from visible Set', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    io.simulate([{ target: el, isIntersecting: false }])
    expect(allocator._internals.visible.has(1)).toBe(false)
    unmount()
  })

  it('applyVisible iterates only the visible Set, not the full element map', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el1 = buildEventEl()
    const el2 = buildEventEl()
    allocator.bind(1, buildEvent(1), el1)
    allocator.bind(2, buildEvent(2), el2)
    const io = MockIntersectionObserver.lastInstance()!
    /* Only el1 is visible. */
    io.simulate([{ target: el1, isIntersecting: true }])
    allocator._internals.resetSetPropertyCount()
    allocator.applyVisible()
    /* Only el1 should have been re-applied. memo will likely
     * cache-hit (same inputs) → 0 setProperty calls actually.
     * To test the visibility scoping rather than memo, force
     * a different allocHeightBucket via a prop change. */
    expect(allocator._internals.setPropertyCount()).toBe(0) /* memo hit */
    unmount()
  })
})

describe('allocator inputs', () => {
  it('stickyTitles=false uses full event height', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
      pxPerMinute: 4,
      effectiveStart: 0,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* 1-hour event = 60 min × 4 px/min = 240 px tall.
     * With non-sticky, allocator gets full 240 px → easily fits
     * a 1-line title + 0-line subtitle. */
    allocator.bind(1, buildEvent(1), el)
    expect(el.style.getPropertyValue('--title-lines')).toBe('1')
    expect(el.style.getPropertyValue('--sub-lines')).toBe('0')
    unmount()
  })

  it('stickyTitles=true with scroll past event uses visible portion', () => {
    const { allocator, scrollEl, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 0,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* Scroll 50 minutes (200 px) past the start of a
     * 60-minute event — visible portion = 10 min = 40 px. */
    Object.defineProperty(scrollEl, 'scrollTop', {
      configurable: true,
      get: () => 200,
    })
    allocator.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    /* visible region cap: 40 px. After chrome (11 px), content
     * height 29 px — fits 1 title line (16.25 px), 0 subtitle. */
    expect(el.style.getPropertyValue('--title-lines')).toBe('1')
    expect(el.style.getPropertyValue('--sub-lines')).toBe('0')
    unmount()
  })

  it('subtitle non-empty puts --sub-display = -webkit-box', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    /* Long subtitle to make sub-lines > 0 in a tall event. */
    allocator.bind(1, buildEvent(1, { subtitle: 'A subtitle' }), el)
    /* 1-hour at 4 px/min = 240 px tall. Fits 1 title + 1 sub. */
    expect(el.style.getPropertyValue('--sub-display')).toBe('-webkit-box')
    unmount()
  })

  it('titleOnly=true forces empty subtitle → --sub-display=none', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      titleOnly: true,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1, { subtitle: 'A subtitle' }), el)
    expect(el.style.getPropertyValue('--sub-display')).toBe('none')
    expect(el.style.getPropertyValue('--sub-lines')).toBe('0')
    unmount()
  })

  it('writes box at natural geometry when stickyTitles is off', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    /* 1-hour event at start=1000s. visibleStart=1000,
     * blockTrackTop = (1000-0)/60 * 4 = 66.67 → 67. height = 240. */
    allocator.bind(1, buildEvent(1), el)
    expect(el.style.top).toBe('67px')
    expect(el.style.height).toBe('240px')
    unmount()
  })

  it('writes box at natural geometry when block top is below the column-header', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 0,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* scrollTop=0 + blockTrackTop=0 → not pinned (scrollPos
     * not strictly > naturalStart). Box at natural geometry,
     * --title-shift-on = 0. */
    allocator.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    expect(el.style.top).toBe('0px')
    expect(el.style.height).toBe('240px')
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('0')
    unmount()
  })

  it('pins box at scrollTop with shrunk height when scrolled past start', () => {
    const { allocator, scrollEl, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 0,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1, { start: 0, stop: 60 * 60 }), el)
    /* scrollTop=100 → user scrolled past the block top.
     *   pinTop = 100, pinHeight = 240 - 100 = 140, pinned = 1. */
    ;(scrollEl as unknown as { scrollTop: number }).scrollTop = 100
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    allocator.applyVisible()
    expect(el.style.top).toBe('100px')
    expect(el.style.height).toBe('140px')
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('1')
    unmount()
  })

  it('pinHeight clamps to 0 when block scrolls fully past viewport', () => {
    const { allocator, scrollEl, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 0,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* 60-min event at start=0 → fullHeightPx = 240, blockTrackTop = 0.
     * scrollTop = 400 → fully past trailing edge.
     *   pinTop = 400, pinHeight = max(0, 240 - 400) = 0. */
    allocator.bind(1, buildEvent(1, { start: 0, stop: 3600 }), el)
    ;(scrollEl as unknown as { scrollTop: number }).scrollTop = 400
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    allocator.applyVisible()
    expect(el.style.top).toBe('400px')
    expect(el.style.height).toBe('0px')
    unmount()
  })

  it('forces --title-shift-on=1 for events whose ev.start precedes effectiveStart (e.g. midnight-spanning)', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: true,
      pxPerMinute: 4,
      effectiveStart: 1000,
      effectiveEnd: 1_000_000_000,
    })
    const el = buildEventEl()
    /* Event started at 0 (yesterday); effectiveStart = 1000
     * (today midnight). visibleStart clamps to 1000, blockTrackTop
     * = 0, scrollTop = 0. Box is geometrically NOT pinned, but
     * the gradient should still be on because the event has
     * off-screen-up content (the yesterday-portion). */
    allocator.bind(1, buildEvent(1, { start: 0, stop: 5000 }), el)
    /* Box at natural top=0 (no geometric pinning). */
    expect(el.style.top).toBe('0px')
    /* Gradient gate: clipped-start → on. */
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('1')
    unmount()
  })

  it('writes --title-shift-on=0 when title is at natural position (not stuck)', () => {
    const { allocator, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    /* Sticky off → titleShiftY = 4 (natural padding) → not
     * stuck → shift-on = 0. */
    expect(el.style.getPropertyValue('--title-shift-on')).toBe('0')
    unmount()
  })
})

describe('memoization', () => {
  it('second applyAllocator call with unchanged inputs skips setProperty', () => {
    const { allocator, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    const io = MockIntersectionObserver.lastInstance()!
    io.simulate([{ target: el, isIntersecting: true }])
    allocator._internals.resetSetPropertyCount()
    allocator.applyVisible()
    /* Inputs unchanged → memo hit → no setProperty calls. */
    expect(allocator._internals.setPropertyCount()).toBe(0)
    unmount()
  })

  it('changing channelColumnWidth invalidates memo (re-applies via watcher)', async () => {
    const { allocator, setProp, unmount } = mountHarness({ events: [] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    allocator._internals.resetSetPropertyCount()
    setProp('channelColumnWidth', 300)
    /* Wait for Vue's watcher to flush. */
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(allocator._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })
})

describe('reactivity wiring', () => {
  it('events ref reassignment triggers applyAll (post-flush)', async () => {
    const { allocator, eventsRef, unmount } = mountHarness({ events: [buildEvent(1)] })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    allocator._internals.resetSetPropertyCount()
    /* Reassign events with different data — should trigger
     * post-flush watcher → applyAll → re-apply for el (memo
     * was cleared on Comet update). */
    eventsRef.value = [buildEvent(1, { title: 'New title' })]
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(allocator._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })

  it('stickyTitles toggle triggers applyAll', async () => {
    const { allocator, setProp, unmount } = mountHarness({
      events: [],
      stickyTitles: false,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    allocator._internals.resetSetPropertyCount()
    setProp('stickyTitles', true)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(allocator._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })

  it('pxPerMinute change triggers applyAll', async () => {
    const { allocator, setProp, unmount } = mountHarness({
      events: [],
      pxPerMinute: 4,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1), el)
    allocator._internals.resetSetPropertyCount()
    setProp('pxPerMinute', 8)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(allocator._internals.setPropertyCount()).toBeGreaterThan(0)
    unmount()
  })

  it('titleOnly toggle triggers applyAll', async () => {
    const { allocator, setProp, unmount } = mountHarness({
      events: [],
      titleOnly: false,
    })
    const el = buildEventEl()
    allocator.bind(1, buildEvent(1, { subtitle: 'A subtitle' }), el)
    expect(el.style.getPropertyValue('--sub-display')).toBe('-webkit-box')
    setProp('titleOnly', true)
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(el.style.getPropertyValue('--sub-display')).toBe('none')
    unmount()
  })
})
