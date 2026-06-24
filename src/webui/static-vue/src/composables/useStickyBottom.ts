// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useStickyBottom — tracks whether a scrollable element is at the
 * top or bottom (within a small slop) and exposes an imperative
 * `scrollToBottom()` for log-tail patterns.
 *
 * Two consumers in practice:
 *  - Log viewer auto-scroll: the page listens for new lines and
 *    conditionally re-scrolls only when the user is still pinned
 *    to the bottom — scrolling up to inspect a recent line pauses
 *    auto-tail until the user manually returns.
 *  - Scroll-shadow gradients: the page tints top / bottom edges
 *    with a fade gradient when there's content above / below the
 *    visible viewport, signalling "you can scroll this way".
 *    `isAtTop` / `isAtBottom` drive the visibility of those
 *    overlay gradients.
 */

import { onBeforeUnmount, ref, watch, type Ref } from 'vue'

/* Default slop between an edge and the content before "at top /
 * at bottom" flips. Generous (30 px) for the log-tail auto-scroll
 * case — smooth-scroll inertia + sub-pixel fractional positions can
 * leave a sliver of headroom even at the true end. A scroll-shadow
 * consumer wants a much tighter value so the gradient shows for ANY
 * real overflow (a list can overflow by only a few px on a short
 * viewport); pass `slopPx` to override. */
const DEFAULT_SLOP_PX = 30

export interface UseStickyBottomOptions {
  /* Edge slop in pixels — see DEFAULT_SLOP_PX. */
  slopPx?: number
}

export interface UseStickyBottomReturn {
  isAtBottom: Ref<boolean>
  isAtTop: Ref<boolean>
  scrollToBottom: () => void
}

export function useStickyBottom(
  scrollEl: Ref<HTMLElement | null>,
  options: UseStickyBottomOptions = {},
): UseStickyBottomReturn {
  const slopPx = options.slopPx ?? DEFAULT_SLOP_PX
  const isAtBottom = ref(true)
  const isAtTop = ref(true)

  function check(): void {
    const el = scrollEl.value
    if (!el) return
    const distBottom = el.scrollHeight - el.scrollTop - el.clientHeight
    isAtBottom.value = distBottom <= slopPx
    isAtTop.value = el.scrollTop <= slopPx
  }

  function scrollToBottom(): void {
    const el = scrollEl.value
    if (!el) return
    el.scrollTop = el.scrollHeight
    /* Re-check synchronously so the caller can read the updated
     * flag without waiting for the next tick. */
    check()
  }

  /* A layout / viewport change can make a previously-fitting list
   * overflow (or stop overflowing) with no scroll event ever firing —
   * observe the element's size as well so the at-top / at-bottom flags
   * (and any scroll-shadow gradients bound to them) stay accurate.
   * Guarded for non-DOM test environments without ResizeObserver. */
  let resizeObserver: ResizeObserver | null = null

  /* Re-binding: the scroll element may not be mounted at composable
   * setup time (`templateRef`s are null on first run). Watch the ref
   * + attach/detach listeners as it becomes available. */
  watch(
    scrollEl,
    (el, oldEl) => {
      if (oldEl) oldEl.removeEventListener('scroll', check)
      resizeObserver?.disconnect()
      resizeObserver = null
      if (el) {
        el.addEventListener('scroll', check, { passive: true })
        if (typeof ResizeObserver !== 'undefined') {
          resizeObserver = new ResizeObserver(check)
          resizeObserver.observe(el)
        }
        check()
      }
    },
    { immediate: true },
  )

  onBeforeUnmount(() => {
    scrollEl.value?.removeEventListener('scroll', check)
    resizeObserver?.disconnect()
  })

  return { isAtBottom, isAtTop, scrollToBottom }
}
