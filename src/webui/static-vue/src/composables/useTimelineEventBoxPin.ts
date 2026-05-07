/*
 * useTimelineEventBoxPin — imperative DOM-mutation layer for
 * Timeline event-block geometry.
 *
 * Replaces the earlier `useTimelineEventTitleSticky` which kept
 * the box scrolling naturally and used `transform: translateX`
 * on the inner title to fake a sticky-left effect. That scheme
 * caused 1-2 px sub-pixel jitter on the BOX itself during
 * continuous scroll (non-composite element with fractional
 * `left:`, browser-side pixel-snap toggling). No amount of
 * inner-element rounding can fix the compositor-vs-main-thread
 * sync between the row's smooth scroll and the JS-driven
 * transform write.
 *
 * The new scheme pins the BOX itself: when the user has
 * scrolled past the event's natural left edge, write
 * `box.style.left = scrollLeft` and shrink `box.style.width`
 * accordingly. The box's viewport-x in body coords is then
 * constantly 0 — no rounding for the browser to disagree
 * about. The title sits at natural padding inside the box; the
 * `::after` gradient at `left: 0` paints over the box's
 * natural left edge, with `--title-shift-on` gating opacity.
 *
 * Same plumbing as before: Map<eventId, HTMLElement>,
 * IntersectionObserver scoping, per-event memo, `:ref`
 * callback-driven lifecycle.
 */

import { onBeforeUnmount, watch, type Ref } from 'vue'
import { computeBoxPin } from '@/views/epg/epgBoxPin'

interface TimelineEventLike {
  eventId: number
  start?: number
  stop?: number
}

export interface UseTimelineEventBoxPinOpts<E extends TimelineEventLike> {
  /* Reactive scroll element. Composable reads `scrollLeft`
   * directly during scroll-tick callbacks. */
  scrollEl: Ref<HTMLElement | null>
  /* Event list — same reactive ref the v-for iterates.
   * Reassignment triggers a watcher that re-applies for every
   * registered element (Comet-update fallback). */
  events: Ref<E[]>
  /* Reactive accessors for inputs. Functions (not refs) so the
   * composable re-reads on each call without binding tightly
   * to prop shape. */
  stickyTitles: () => boolean
  pxPerMinute: () => number
  effectiveStart: Ref<number>
  effectiveEnd: Ref<number>
}

export interface TimelineEventBoxPin {
  /* `:ref` callback target. Vue calls with el on mount and
   * patch, with null on unmount. */
  bind(eventId: number, ev: TimelineEventLike, el: HTMLElement | null): void
  /* Scroll-tick callback. Wire into useEpgViewportEmitter's
   * onTick option. */
  applyVisible(): void
  /* Test-only inspection. */
  _internals: {
    elements: Map<number, HTMLElement>
    eventData: Map<number, TimelineEventLike>
    visible: Set<number>
    setPropertyCount: () => number
    resetSetPropertyCount: () => void
  }
}

interface CachedPin {
  /* Last-applied integer-pixel left + width. We compare on
   * these directly (not buckets) so every 1-pixel scroll change
   * triggers a write — bucketing would cause the box to lag
   * the scroll position by up to bucket-size pixels, which is
   * the very jitter this composable was rewritten to fix. */
  left: number
  width: number
  /* Gradient-affordance gate. Distinct from `pinned` (box
   * geometry) because midnight-spanning events whose true
   * `ev.start` precedes `effectiveStart` should always show
   * the gradient — the yesterday-portion is "off-screen left"
   * even at scrollLeft=0, regardless of whether the box is
   * geometrically pinned. */
  shiftOn: 0 | 1
}

export function useTimelineEventBoxPin<E extends TimelineEventLike>(
  opts: UseTimelineEventBoxPinOpts<E>,
): TimelineEventBoxPin {
  const elements = new Map<number, HTMLElement>()
  const eventData = new Map<number, TimelineEventLike>()
  const visible = new Set<number>()
  const memo = new Map<number, CachedPin>()

  let setPropertyCalls = 0

  let io: IntersectionObserver | null = null
  function ensureObserver(): IntersectionObserver {
    if (io) return io
    io = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          let foundId: number | undefined
          for (const [eventId, el] of elements) {
            if (el === entry.target) {
              foundId = eventId
              break
            }
          }
          if (foundId === undefined) continue
          if (entry.isIntersecting) {
            const wasVisible = visible.has(foundId)
            visible.add(foundId)
            if (!wasVisible) applyPin(foundId)
          } else {
            visible.delete(foundId)
          }
        }
      },
      { root: opts.scrollEl.value, threshold: 0 },
    )
    return io
  }

  function bind(eventId: number, ev: TimelineEventLike, el: HTMLElement | null): void {
    if (el === null) {
      const prev = elements.get(eventId)
      if (prev && io) io.unobserve(prev)
      elements.delete(eventId)
      eventData.delete(eventId)
      visible.delete(eventId)
      memo.delete(eventId)
      return
    }
    const prev = elements.get(eventId)
    if (prev && prev !== el && io) io.unobserve(prev)
    elements.set(eventId, el)
    eventData.set(eventId, ev)
    /* Invalidate memo on every bind — event data may have
     * changed (Comet update) even when the element reference
     * is the same. */
    memo.delete(eventId)
    const observer = ensureObserver()
    if (prev !== el) observer.observe(el)
    /* Synchronous apply so first paint has correct geometry. */
    applyPin(eventId)
  }

  function applyPin(eventId: number): void {
    const el = elements.get(eventId)
    const ev = eventData.get(eventId)
    if (!el || !ev) return
    if (typeof ev.start !== 'number' || typeof ev.stop !== 'number') return

    const visibleStart = Math.max(ev.start, opts.effectiveStart.value)
    const visibleStop = Math.min(ev.stop, opts.effectiveEnd.value)
    if (visibleStop <= visibleStart) return

    const ppm = opts.pxPerMinute()
    if (ppm <= 0) return

    /* Round to integer pixels. The block's CSS `left:` is
     * always integer; the browser never has to disagree with
     * itself about pixel-snap targets between paints. */
    const naturalLeft = Math.round(((visibleStart - opts.effectiveStart.value) / 60) * ppm)
    const naturalRight = Math.round(((visibleStop - opts.effectiveStart.value) / 60) * ppm)

    /* When sticky-titles is off, position is always natural. */
    let pinnedStart = naturalLeft
    let pinnedSize = Math.max(0, naturalRight - naturalLeft)
    let shiftOn: 0 | 1 = 0

    if (opts.stickyTitles()) {
      const scrollEl = opts.scrollEl.value
      const scrollLeft = Math.round(scrollEl?.scrollLeft ?? 0)
      const r = computeBoxPin(naturalLeft, naturalRight, scrollLeft)
      pinnedStart = r.pinnedStart
      pinnedSize = r.pinnedSize
      /* Gradient gate: pinned (box at non-natural geometry) OR
       * the event's real start is before the track's effective
       * start. The latter handles midnight-spanning events
       * where the yesterday-portion is clamped off — gradient
       * should still flag "started before what's visible" even
       * at scrollLeft=0 with the box at track-x=0. */
      const clippedStart = ev.start < opts.effectiveStart.value
      shiftOn = r.pinned || clippedStart ? 1 : 0
    }

    const cached = memo.get(eventId)
    if (
      cached?.left === pinnedStart &&
      cached?.width === pinnedSize &&
      cached?.shiftOn === shiftOn
    ) {
      return
    }

    el.style.left = `${pinnedStart}px`
    el.style.width = `${pinnedSize}px`
    el.style.setProperty('--title-shift-on', String(shiftOn))
    setPropertyCalls += 3
    memo.set(eventId, { left: pinnedStart, width: pinnedSize, shiftOn })
  }

  function applyVisible(): void {
    for (const eventId of visible) {
      applyPin(eventId)
    }
  }

  function applyAll(): void {
    /* Invalidate full memo — input change (events / props)
     * may have shifted output. */
    memo.clear()
    for (const eventId of elements.keys()) {
      applyPin(eventId)
    }
  }

  /* Reactivity wiring — input changes trigger applyAll. */
  watch(
    opts.events,
    () => {
      applyAll()
    },
    { flush: 'post' },
  )
  watch(
    () => opts.stickyTitles(),
    () => {
      applyAll()
    },
  )
  watch(
    () => opts.pxPerMinute(),
    () => {
      applyAll()
    },
  )

  onBeforeUnmount(() => {
    if (io) {
      io.disconnect()
      io = null
    }
    elements.clear()
    eventData.clear()
    visible.clear()
    memo.clear()
  })

  return {
    bind,
    applyVisible,
    _internals: {
      elements,
      eventData,
      visible,
      setPropertyCount: () => setPropertyCalls,
      resetSetPropertyCount: () => {
        setPropertyCalls = 0
      },
    },
  }
}
