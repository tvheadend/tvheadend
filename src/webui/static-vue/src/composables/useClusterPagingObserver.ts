// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useClusterPagingObserver — IntersectionObserver wrapper that
 * fires `onIntersect(clusterKey)` when a sentinel row (placed at
 * the bottom of a cluster body in the EPG Table's grouped mode)
 * scrolls into the viewport.
 *
 * Mirrors the lazy-`ensureObserver` + `bind(id, el)` pattern
 * already used by `useTimelineEventBoxPin` and
 * `useMagazineEventAllocator`. Single observer instance per
 * composable; each sentinel row registers via `bind(key, el)`
 * on mount and deregisters via `bind(key, null)` on unmount.
 *
 * Cleanup: caller invokes `destroy()` from `onBeforeUnmount` to
 * disconnect the observer + clear the maps. Re-running the
 * composable creates a fresh instance.
 */

export interface ClusterPagingObserver {
  /* Lazy-create the IntersectionObserver with the given scroll
   * container as `root`. No-op if already created. Calling this
   * is optional — `bind()` auto-creates the observer with
   * `root: null` (viewport) on first call if `ensureObserver`
   * was never invoked. Pass `ensureObserver` explicitly only
   * when you need a non-viewport root (a scroll container
   * inside the page). */
  ensureObserver(rootEl: Element | null): void

  /* Register a sentinel element for a cluster key. Passing
   * `null` deregisters (unobserves the previous element if any).
   * Replacing the element for a key (rebind on Vue re-render)
   * unobserves the old element first. Auto-creates the
   * observer (root=null) if not already created — so the
   * caller is never required to wire `ensureObserver` for the
   * common viewport-root case. */
  bind(key: string, el: Element | null): void

  /* Disconnect + clear all maps. Idempotent. After destroy(),
   * a subsequent bind() will NOT re-create the observer —
   * destroy is a terminal state for the composable. */
  destroy(): void
}

export function useClusterPagingObserver(
  onIntersect: (clusterKey: string) => void,
): ClusterPagingObserver {
  let observer: IntersectionObserver | null = null
  /* Sticky flag so a destroy() can't be undone by a stray
   * post-unmount bind() resurrecting the observer. */
  let destroyed = false
  /* Two parallel maps so we can resolve element → key for the
   * IO callback AND find the current element for a key when the
   * caller rebinds (sentinel row's DOM ref changes across Vue
   * re-renders). */
  const keyByElement = new Map<Element, string>()
  const elementByKey = new Map<string, Element>()

  function callback(entries: IntersectionObserverEntry[]): void {
    for (const entry of entries) {
      if (!entry.isIntersecting) continue
      const key = keyByElement.get(entry.target)
      if (key !== undefined) onIntersect(key)
    }
  }

  function ensureObserver(rootEl: Element | null): void {
    if (destroyed) return
    if (observer !== null) return
    /* `root: null` means viewport — acceptable but the caller
     * should normally pass the DataGrid's scroll container so
     * intersection events fire as the user scrolls within the
     * grid, not on the page itself.
     *
     * `rootMargin: '1000px 0px'` extends the intersection root
     * 1000px below the visible area so the sentinel "intersects"
     * before it's actually on-screen — the next page fetch
     * fires preemptively and the new rows are typically in
     * place by the time the user scrolls to where the sentinel
     * would have been. Mirrors the row-distance lookahead the
     * ungrouped lazy-paging path uses (LAZY_PAGE_PRELOAD_BUFFER
     * = 50 rows ≈ 1800px at 36px/row). 1000px ≈ 28 rows of
     * lookahead — large enough to hide the fetch on normal
     * latency without preloading excessively for short
     * clusters. */
    observer = new IntersectionObserver(callback, {
      root: rootEl,
      rootMargin: '1000px 0px',
      threshold: 0,
    })
  }

  function bind(key: string, el: Element | null): void {
    if (destroyed) return
    /* Auto-create the observer on first bind. Mounting a
     * sentinel + calling bind() is the canonical trigger; not
     * requiring the caller to wire ensureObserver as a separate
     * step eliminates a footgun where a forgotten setup call
     * silently disables the entire paging machinery. */
    if (observer === null) ensureObserver(null)
    /* Deregister path: el === null OR key already has a
     * different element registered. Unobserve the previous
     * element + clear both maps for it. */
    const prev = elementByKey.get(key)
    if (prev && prev !== el) {
      observer?.unobserve(prev)
      keyByElement.delete(prev)
      elementByKey.delete(key)
    }
    if (el === null) return
    /* Register the new element. Defensive: skip if the same
     * element is already observed (no-op). */
    if (keyByElement.has(el)) return
    keyByElement.set(el, key)
    elementByKey.set(key, el)
    observer?.observe(el)
  }

  function destroy(): void {
    destroyed = true
    if (observer === null) {
      keyByElement.clear()
      elementByKey.clear()
      return
    }
    observer.disconnect()
    observer = null
    keyByElement.clear()
    elementByKey.clear()
  }

  return { ensureObserver, bind, destroy }
}
