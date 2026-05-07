/*
 * useMagazineEventAllocator — imperative DOM-mutation layer
 * for Magazine event blocks. Owns BOTH the box's vertical
 * geometry (`top` / `height`) AND the line-count CSS variables
 * (`--title-lines`, `--sub-lines`, `--sub-display`,
 * `--title-shift-on`).
 *
 * Box-pinning replaces the previous CSS-sticky title scheme:
 * once the user has scrolled past an event's natural top, this
 * composable writes `box.style.top = scrollTop` and shrinks
 * `box.style.height` so the box's viewport-y is constantly at
 * the column-header bottom. Sub-pixel jitter goes away by
 * construction (no main-thread / compositor sync to lose). The
 * title sits at natural padding inside the now-pinned box; the
 * `::after` gradient paints at the box's natural top edge with
 * `--title-shift-on` gating opacity. See `epgBoxPin.ts` for
 * the geometric helper used by both Magazine and Timeline.
 *
 * Why imperative for the line allocator too: the allocator
 * runs at scroll frequency (60 Hz) for every visible event
 * when sticky-titles is on. Routing that through Vue's
 * reactive `:style` cycle means a full template-diff + DOM-
 * mutation per visible event per scroll tick — 80+ events ×
 * 60 Hz pushes the frame budget to its ceiling. Same imperative
 * pattern serves both jobs.
 *
 * Architecture:
 *
 *   - Vue's `v-for` over events mounts / unmounts event
 *     elements. Each element registers itself with this
 *     composable via a `:ref="(el) => bind(eventId, ev, el)"`
 *     callback. Vue calls the ref function on mount, on patch
 *     (when ev changes), and on unmount (with el===null).
 *
 *   - The composable writes inline `top` / `height` styles
 *     plus four CSS variables on each element. Vue's template
 *     does NOT set `:style="..."` on the box; the composable
 *     owns it.
 *
 *   - An IntersectionObserver tracks which events are currently
 *     in the viewport. The scroll-tick callback iterates only
 *     the visible Set, scaling work with visible-event count
 *     rather than total-loaded count.
 *
 *   - Per-event memoization: tracks pinTop / pinHeight exactly
 *     for box geometry (every-pixel writes — bucketing here
 *     would cause the same jitter that motivated this rewrite).
 *     Tracks allocHeightBucket separately for the EXPENSIVE
 *     allocator step — text measurement is heavy, and 4 px
 *     bucketing there is fine because line counts don't change
 *     within a 4 px height delta.
 *
 *   - Comet flow stays untouched: `events.value` updates
 *     trigger Vue's v-for re-render, which calls `:ref` for
 *     each patched element with the fresh `ev` data, which
 *     re-runs `applyAllocator` for that event.
 *
 * Cleanup: `onBeforeUnmount` disconnects the IO and clears
 * all maps. The shared `magazineLineAllocator.measureCache` is
 * module-scoped and cleared by the consumer.
 */

import { onBeforeUnmount, watch, type Ref } from 'vue'
import {
  allocateLines,
  createMeasurer,
  disposeMeasurer,
  type LineCounts,
} from '@/views/epg/magazineLineAllocator'
import { computeBoxPin } from '@/views/epg/epgBoxPin'

interface MagazineEventLike {
  eventId: number
  start?: number
  stop?: number
  title?: string
}

export interface UseMagazineEventAllocatorOpts<E extends MagazineEventLike> {
  /* Reactive scroll element. Composable reads `scrollTop` /
   * `clientHeight` directly during scroll-tick callbacks. */
  scrollEl: Ref<HTMLElement | null>
  /* Event list — the same reactive ref the v-for iterates.
   * Reassignment triggers a watcher that re-applies allocator
   * for every registered element (Comet-update fallback). */
  events: Ref<E[]>
  /* Reactive accessors for prop / state inputs. Functions
   * (not refs) so the composable can re-read on each call
   * without binding tightly to the prop shape. */
  stickyTitles: () => boolean
  pxPerMinute: () => number
  channelColumnWidth: () => number
  titleOnly: () => boolean
  effectiveStart: Ref<number>
  effectiveEnd: Ref<number>
  /* Sticky-pane height that the visible-region calculation
   * subtracts from clientHeight (the column-header row). */
  headerHeight: number
  /* Helpers the composable invokes for text content. */
  dispText: (s: string | undefined) => string
  extraText: (ev: E) => string | undefined
}

export interface MagazineEventAllocator {
  /* `:ref` callback target. Vue calls with el on mount and
   * patch, with null on unmount. */
  bind(eventId: number, ev: MagazineEventLike, el: HTMLElement | null): void
  /* Scroll-tick callback. Wire into useEpgViewportEmitter's
   * onTick option. */
  applyVisible(): void
  /* Test-only inspection — not part of the public contract.
   * Exposes internal state so tests can assert on the visible
   * Set, the element Map, etc. without poking at implementation
   * details ad-hoc. */
  _internals: {
    elements: Map<number, HTMLElement>
    eventData: Map<number, MagazineEventLike>
    visible: Set<number>
    setPropertyCount: () => number
    resetSetPropertyCount: () => void
  }
}

interface CachedAllocation {
  /* Exact integer pinTop / pinHeight — written every 1-px
   * scroll change so the box stays in lock-step with scroll
   * (no bucketing-induced lag). */
  pinTop: number
  pinHeight: number
  /* Gradient-affordance gate. Distinct from box geometry —
   * for midnight-spanning events whose `ev.start` precedes
   * `effectiveStart`, the gradient should be on at scrollTop=0
   * even though the box is geometrically at the track's edge
   * (no pinning yet). */
  shiftOn: 0 | 1
  /* 4-px-bucketed allocHeight, used to skip the EXPENSIVE
   * allocator step when only sub-bucket height changes happen.
   * Line counts can't change inside a 4-px window so this is
   * lossless for the allocator's output. */
  allocHeightBucket: number
  titleText: string
  subText: string
  columnWidth: number
  lines: LineCounts
}

/* Round allocHeight to 4 px buckets for the allocator-skip
 * decision. Box geometry does NOT use this bucket — see the
 * comment on CachedAllocation.pinTop above. */
const HEIGHT_BUCKET_PX = 4

/* Shape used to compare a candidate state against the cached
 * one — same fields as `CachedAllocation` minus the heavy
 * `lines` payload (which we only fetch when the allocator
 * actually needs to run). */
type MemoCheckShape = Omit<CachedAllocation, 'lines'>

/* Full-memo hit: nothing changed, skip the entire write. */
function isFullMemoHit(
  cached: CachedAllocation | undefined,
  next: MemoCheckShape,
): boolean {
  return (
    cached?.pinTop === next.pinTop &&
    cached?.pinHeight === next.pinHeight &&
    cached?.shiftOn === next.shiftOn &&
    cached?.allocHeightBucket === next.allocHeightBucket &&
    cached?.titleText === next.titleText &&
    cached?.subText === next.subText &&
    cached?.columnWidth === next.columnWidth
  )
}

/* Allocator needs to re-run: text-measurement inputs changed,
 * or there's no cached value to reuse. Pure box-geometry
 * changes (pinTop / pinHeight / shiftOn) reuse cached lines. */
function needsAllocatorRerun(
  cached: CachedAllocation | undefined,
  next: MemoCheckShape,
): boolean {
  return (
    cached?.allocHeightBucket !== next.allocHeightBucket ||
    cached?.titleText !== next.titleText ||
    cached?.subText !== next.subText ||
    cached?.columnWidth !== next.columnWidth
  )
}

export function useMagazineEventAllocator<E extends MagazineEventLike>(
  opts: UseMagazineEventAllocatorOpts<E>,
): MagazineEventAllocator {
  const elements = new Map<number, HTMLElement>()
  const eventData = new Map<number, MagazineEventLike>()
  const visible = new Set<number>()
  const memo = new Map<number, CachedAllocation>()

  let setPropertyCalls = 0
  let measurer: HTMLDivElement | null = null
  function ensureMeasurer(): HTMLDivElement {
    measurer ??= createMeasurer()
    return measurer
  }

  /* IntersectionObserver tracks viewport intersection per
   * event. Newly-visible events get an immediate apply; no-
   * longer-visible events drop out of the Set. The observer
   * is created lazily so test-mode constructions before the
   * mock is installed don't crash. */
  let io: IntersectionObserver | null = null
  function ensureObserver(): IntersectionObserver {
    if (io) return io
    io = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          /* `target` is the event element. We track the
           * registered-element-to-id mapping by walking back
           * through the elements map. Cheap (~80 entries). */
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
            /* Newly-visible: apply allocator so the element
             * has correct line counts before the next paint. */
            if (!wasVisible) applyAllocator(foundId)
          } else {
            visible.delete(foundId)
          }
        }
      },
      { root: opts.scrollEl.value, threshold: 0 },
    )
    return io
  }

  function bind(eventId: number, ev: MagazineEventLike, el: HTMLElement | null): void {
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
    /* Invalidate memo for this event whenever bind fires —
     * event data may have changed (Comet update) even when the
     * element reference is the same. */
    memo.delete(eventId)
    const observer = ensureObserver()
    if (prev !== el) observer.observe(el)
    /* Synchronous apply so first paint has correct values. */
    applyAllocator(eventId)
  }

  function readScrollState(): { scrollTop: number; clientHeight: number } {
    const el = opts.scrollEl.value
    if (!el) return { scrollTop: 0, clientHeight: 0 }
    return { scrollTop: el.scrollTop, clientHeight: el.clientHeight }
  }

  /* Compute the box's pinned top + height + shiftOn gate, and
   * the allocator's effective input height. Defaults to natural
   * geometry when sticky-titles is off or the event hasn't
   * scrolled past its natural top. */
  function computeGeometry(
    ev: MagazineEventLike,
    blockTrackTop: number,
    blockTrackBottom: number,
    fullHeightPx: number,
  ): { pinTop: number; pinHeight: number; shiftOn: 0 | 1; allocHeightPx: number } {
    const natural = {
      pinTop: blockTrackTop,
      pinHeight: fullHeightPx,
      shiftOn: 0 as const,
      allocHeightPx: fullHeightPx,
    }
    if (!opts.stickyTitles()) return natural
    const { scrollTop, clientHeight } = readScrollState()
    if (clientHeight <= 0) return natural

    const r = computeBoxPin(blockTrackTop, blockTrackBottom, Math.round(scrollTop))
    /* Gradient gate: pinned (box at non-natural geometry) OR
     * the event's real start precedes `effectiveStart`. The
     * latter handles events that started before today's
     * midnight — yesterday-portion is clamped off, gradient
     * still flags "started before what's visible." */
    const clippedStart = (ev.start ?? 0) < opts.effectiveStart.value
    return {
      pinTop: r.pinnedStart,
      pinHeight: r.pinnedSize,
      shiftOn: r.pinned || clippedStart ? 1 : 0,
      /* Allocator's input is the box's height clamped to the
       * visible vertical region below the column header. For
       * events taller than the viewport, the title shouldn't
       * get N lines of allocation just because the box extends
       * way below the viewport bottom — only the visible-
       * above-the-fold portion matters. */
      allocHeightPx: Math.min(r.pinnedSize, clientHeight - opts.headerHeight),
    }
  }

  /* Compute and apply box geometry + line-count allocation for
   * a single event. Skips the allocator (text measurement) when
   * its inputs are stable; skips the entire write when nothing
   * changed at all. */
  function applyAllocator(eventId: number): void {
    const el = elements.get(eventId)
    const ev = eventData.get(eventId)
    if (!el || !ev) return
    if (typeof ev.start !== 'number' || typeof ev.stop !== 'number') return

    const visibleStart = Math.max(ev.start, opts.effectiveStart.value)
    const visibleStop = Math.min(ev.stop, opts.effectiveEnd.value)
    if (visibleStop <= visibleStart) return

    const ppm = opts.pxPerMinute()
    if (ppm <= 0) return

    /* Round to integer pixels. The block's CSS `top:` is then
     * always integer; the browser never has to disagree with
     * itself about pixel-snap targets between paints. */
    const fullHeightPx = Math.round(((visibleStop - visibleStart) / 60) * ppm)
    const blockTrackTop = Math.round(((visibleStart - opts.effectiveStart.value) / 60) * ppm)
    const blockTrackBottom = blockTrackTop + fullHeightPx

    const geom = computeGeometry(ev, blockTrackTop, blockTrackBottom, fullHeightPx)

    const next: MemoCheckShape = {
      pinTop: geom.pinTop,
      pinHeight: geom.pinHeight,
      shiftOn: geom.shiftOn,
      allocHeightBucket: Math.round(geom.allocHeightPx / HEIGHT_BUCKET_PX),
      titleText: opts.dispText(ev.title),
      subText: opts.titleOnly() ? '' : opts.dispText(opts.extraText(ev as E)),
      columnWidth: opts.channelColumnWidth(),
    }

    const cached = memo.get(eventId)
    if (isFullMemoHit(cached, next)) return

    const allocatorNeedsRerun = needsAllocatorRerun(cached, next)
    const lines: LineCounts =
      !allocatorNeedsRerun && cached
        ? cached.lines
        : allocateLines(
            next.titleText,
            next.subText,
            geom.allocHeightPx,
            next.columnWidth,
            ensureMeasurer(),
          )

    /* Box geometry — written every tick when pinTop / pinHeight
     * / shiftOn changed (1-px granularity). */
    el.style.top = `${geom.pinTop}px`
    el.style.height = `${geom.pinHeight}px`
    el.style.setProperty('--title-shift-on', String(geom.shiftOn))
    setPropertyCalls += 3

    /* Line CSS variables — only written when the allocator
     * actually re-ran. When skipped, the element still carries
     * the previous (still-correct) values. */
    if (allocatorNeedsRerun) {
      el.style.setProperty('--title-lines', String(lines.title))
      el.style.setProperty('--sub-lines', String(lines.sub))
      el.style.setProperty('--sub-display', lines.sub === 0 ? 'none' : '-webkit-box')
      setPropertyCalls += 3
    }

    memo.set(eventId, { ...next, lines })
  }

  function applyVisible(): void {
    /* Read scroll state once at the top — cheap, but `clientHeight`
     * can force layout. Doing it once here means subsequent
     * `applyAllocator` calls within this batch use the same
     * snapshot via the closure-captured `readScrollState` (each
     * call re-reads, but the browser caches reads within a
     * frame so it's effectively free after the first). */
    for (const eventId of visible) {
      applyAllocator(eventId)
    }
  }

  function applyAll(): void {
    /* Invalidate full memo — input change (events / props) may
     * have shifted output even at the same allocHeightBucket. */
    memo.clear()
    for (const eventId of elements.keys()) {
      applyAllocator(eventId)
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
    () => opts.channelColumnWidth(),
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
  watch(
    () => opts.titleOnly(),
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
    disposeMeasurer(measurer)
    measurer = null
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
