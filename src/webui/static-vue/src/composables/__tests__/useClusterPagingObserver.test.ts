// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Tests for the cluster-paging IntersectionObserver wrapper.
 * Uses the shared mock (`test/__helpers__/intersectionObserverMock.ts`)
 * to simulate intersection events without depending on happy-dom
 * implementing IntersectionObserver.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import {
  installIntersectionObserverMock,
  MockIntersectionObserver,
} from '@/test/__helpers__/intersectionObserverMock'
import { useClusterPagingObserver } from '../useClusterPagingObserver'

beforeEach(() => {
  installIntersectionObserverMock()
  MockIntersectionObserver.reset()
})

/* Make a synthetic Element that the observer can store + compare
 * by reference. happy-dom provides DOM types; a plain document.
 * createElement('div') works as the mock just stores references. */
function el(): Element {
  return document.createElement('div')
}

describe('ensureObserver', () => {
  it('is lazy — no observer until first call', () => {
    useClusterPagingObserver(() => {})
    expect(MockIntersectionObserver.instances).toHaveLength(0)
  })

  it('creates the observer on first call', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    expect(MockIntersectionObserver.instances).toHaveLength(1)
  })

  it('is idempotent — second call does not create a second observer', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    o.ensureObserver(document.body)
    expect(MockIntersectionObserver.instances).toHaveLength(1)
  })

  it('passes the supplied root to the IntersectionObserver constructor', () => {
    const root = el()
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(root)
    expect(MockIntersectionObserver.lastInstance()!.options.root).toBe(root)
  })

  it('uses threshold = 0 (sentinel fires as soon as any pixel is visible)', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    expect(MockIntersectionObserver.lastInstance()!.options.threshold).toBe(0)
  })

  it('uses rootMargin to preempt — fires before sentinel is on-screen', () => {
    /* Lookahead margin (1000px below the viewport) means the
     * next page fetch fires before the user actually scrolls
     * to the sentinel — mirrors the row-distance lookahead
     * the ungrouped lazy path uses (LAZY_PAGE_PRELOAD_BUFFER
     * = 50 rows ≈ 1800px at 36px/row). On typical latency the
     * sentinel becomes invisible to the user; only fast
     * scrolls past multiple cluster ends or slow networks
     * should briefly expose it. */
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    expect(MockIntersectionObserver.lastInstance()!.options.rootMargin).toBe(
      '1000px 0px',
    )
  })
})

describe('bind', () => {
  it('observes the element after first bind', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    const sentinelA = el()
    o.bind('A', sentinelA)
    expect(MockIntersectionObserver.lastInstance()!.observed.has(sentinelA)).toBe(true)
  })

  it('auto-creates the observer (root=null) when ensureObserver was never called', () => {
    /* Production wiring footgun guard: TableView used to forget
     * to call ensureObserver, which silently disabled the
     * entire load-more flow (bind no-op'd because observer was
     * null). The composable now auto-creates with viewport
     * root on first bind so the common case Just Works. */
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    const sentinelA = el()
    o.bind('A', sentinelA)
    expect(MockIntersectionObserver.instances).toHaveLength(1)
    expect(MockIntersectionObserver.lastInstance()!.options.root).toBeNull()
    expect(MockIntersectionObserver.lastInstance()!.observed.has(sentinelA)).toBe(true)
  })

  it('unobserves on bind(key, null)', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    const sentinelA = el()
    o.bind('A', sentinelA)
    o.bind('A', null)
    expect(MockIntersectionObserver.lastInstance()!.observed.has(sentinelA)).toBe(false)
  })

  it('rebinding a key swaps the observed element (unobserves old, observes new)', () => {
    /* Vue re-renders can replace a row's DOM element while the
     * cluster key stays the same. The composable must drop the
     * old observation and pick up the new one. */
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    const sentinelOld = el()
    const sentinelNew = el()
    o.bind('A', sentinelOld)
    o.bind('A', sentinelNew)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.has(sentinelOld)).toBe(false)
    expect(io.observed.has(sentinelNew)).toBe(true)
  })

  it('double-bind of the SAME element is a no-op (defensive)', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    const sentinel = el()
    o.bind('A', sentinel)
    o.bind('A', sentinel)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.size).toBe(1)
  })

  it('different keys can register different sentinel elements', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    const sA = el()
    const sB = el()
    o.bind('A', sA)
    o.bind('B', sB)
    const io = MockIntersectionObserver.lastInstance()!
    expect(io.observed.size).toBe(2)
    expect(io.observed.has(sA)).toBe(true)
    expect(io.observed.has(sB)).toBe(true)
  })
})

describe('intersection callback', () => {
  it('fires onIntersect with the correct cluster key when an element intersects', () => {
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    o.bind('A', sA)
    MockIntersectionObserver.lastInstance()!.simulate([
      { target: sA, isIntersecting: true },
    ])
    expect(spy).toHaveBeenCalledWith('A')
  })

  it('ignores entries where isIntersecting is false', () => {
    /* Sentinel scrolling OUT of view should NOT trigger a fetch. */
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    o.bind('A', sA)
    MockIntersectionObserver.lastInstance()!.simulate([
      { target: sA, isIntersecting: false },
    ])
    expect(spy).not.toHaveBeenCalled()
  })

  it('handles multiple entries in one callback batch', () => {
    /* When two sentinels appear simultaneously (e.g. fast scroll
     * past both A's and B's end), the IO callback batches them
     * into one call. Both should fire onIntersect. */
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    const sB = el()
    o.bind('A', sA)
    o.bind('B', sB)
    MockIntersectionObserver.lastInstance()!.simulate([
      { target: sA, isIntersecting: true },
      { target: sB, isIntersecting: true },
    ])
    expect(spy).toHaveBeenCalledTimes(2)
    expect(spy).toHaveBeenNthCalledWith(1, 'A')
    expect(spy).toHaveBeenNthCalledWith(2, 'B')
  })

  it('does not fire onIntersect after bind(key, null)', () => {
    /* After deregistering a sentinel, a later intersection event
     * for the same DOM element should be silent. Defensive
     * against IO firing one final event after unobserve(). */
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    o.bind('A', sA)
    o.bind('A', null)
    MockIntersectionObserver.lastInstance()!.simulate([
      { target: sA, isIntersecting: true },
    ])
    expect(spy).not.toHaveBeenCalled()
  })
})

describe('destroy', () => {
  it('disconnects the observer + clears maps', () => {
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    o.bind('A', sA)
    const io = MockIntersectionObserver.lastInstance()!
    o.destroy()
    expect(io.observed.size).toBe(0)
  })

  it('is idempotent — second destroy() is a no-op', () => {
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    expect(() => {
      o.destroy()
      o.destroy()
    }).not.toThrow()
  })

  it('subsequent bind() calls after destroy do nothing', () => {
    /* After destroy, the observer is gone — bind() shouldn't try
     * to call observe() on null. Defensive against post-unmount
     * rebinds (e.g. a Vue cleanup-ordering quirk). */
    const o = useClusterPagingObserver(() => {})
    o.ensureObserver(document.body)
    o.destroy()
    const sA = el()
    expect(() => o.bind('A', sA)).not.toThrow()
  })

  it('intersection event after destroy does not fire onIntersect', () => {
    const spy = vi.fn()
    const o = useClusterPagingObserver(spy)
    o.ensureObserver(document.body)
    const sA = el()
    o.bind('A', sA)
    const io = MockIntersectionObserver.lastInstance()!
    o.destroy()
    /* Even if the (now-disconnected) observer somehow fires its
     * callback later, the cleared maps mean the key lookup
     * returns undefined → no fire. */
    io.simulate([{ target: sA, isIntersecting: true }])
    expect(spy).not.toHaveBeenCalled()
  })
})
