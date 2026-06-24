// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useResizableDrawerWidth — per-drawer persisted-width state +
 * mouse-drag plumbing for a resize handle.
 *
 * Drives the optional "drag the inside edge to resize the drawer"
 * affordance on overlay drawers (PrimeVue `<Drawer>`). The drawer
 * stays an overlay — the page behind doesn't reflow; the drawer
 * just covers more or less of it as the user drags.
 *
 * Persistence: localStorage, keyed by the caller-supplied
 * `storageKey`. Null persisted value means "use default" — the
 * `isAtDefault` predicate the SettingsPopover reads gates the
 * Reset-to-defaults button's disabled state.
 *
 * Used today: ChannelManageDrawer.vue. The composable is shape-
 * reusable for any future overlay drawer that wants a resize
 * handle.
 */
import { computed, ref, type ComputedRef } from 'vue'
import { useIsPhone } from './useIsPhone'
import { readStoredJson, removeStoredKey, writeStoredJson } from '@/utils/storage'

export interface UseResizableDrawerWidthOptions {
  /** localStorage key — should be drawer-specific, e.g.
   *  `'channel-manage-drawer:width'`. */
  storageKey: string
  /** Width in px when no user value is persisted. The drawer's
   *  inline style additionally caps at 95vw via CSS `min()` so
   *  the drawer never overflows a narrow viewport. */
  defaultPx: number
  /** Drag clamps: drawer can't be made narrower than `minPx`
   *  nor wider than `maxPx`. */
  minPx: number
  maxPx: number
}

export interface ResizableDrawerWidth {
  widthPx: ComputedRef<number>
  widthStyle: ComputedRef<{ width: string }>
  isAtDefault: ComputedRef<boolean>
  setWidth: (px: number) => void
  reset: () => void
  /** Attach a mousedown listener that initiates the drag tracking.
   *  Returns a cleanup fn that removes the listener — call from
   *  `onBeforeUnmount`. */
  attachHandle: (handleEl: HTMLElement) => () => void
}

/* The persisted value is a bare pixel number ("420"), which is
 * valid JSON — the shared helper round-trips the same on-disk
 * shape the previous raw-string code wrote. Anything non-finite
 * falls through to default-width behaviour. */
function isFiniteNumber(v: unknown): v is number {
  return typeof v === 'number' && Number.isFinite(v)
}

function loadPx(storageKey: string): number | null {
  return readStoredJson(storageKey, isFiniteNumber)
}

function savePx(storageKey: string, px: number): void {
  writeStoredJson(storageKey, px)
}

function clearPx(storageKey: string): void {
  removeStoredKey(storageKey)
}

export function useResizableDrawerWidth(
  opts: UseResizableDrawerWidthOptions,
): ResizableDrawerWidth {
  const persistedPx = ref<number | null>(loadPx(opts.storageKey))

  const widthPx = computed(() => persistedPx.value ?? opts.defaultPx)
  const isAtDefault = computed(() => persistedPx.value === null)

  /* Phone-mode detection (shared breakpoint singleton). On phone
   * widths the drawer goes full-screen (`width: 100vw`) — matches
   * the conventional mobile-drawer pattern used elsewhere in the
   * app and gives the user-resized width no role to play (the
   * resize handle is already hidden on phone via CSS). Reactive
   * so a desktop → phone transition (e.g. dev-tools responsive
   * preview, browser pane resize) flips behaviour live. */
  const isPhone = useIsPhone()

  /* Width style for the drawer's inline `:style` binding.
   *   - phone: `width: 100vw` (full-screen — matches the
   *     conventional mobile drawer pattern; resize handle is
   *     hidden on phone anyway).
   *   - desktop: `min(<picked>px, 95vw)` — the `min()` caps
   *     against viewport so the drawer never overflows a narrow
   *     window even after the user picks a large width. */
  const widthStyle = computed(() => {
    if (isPhone.value) return { width: '100vw' }
    return { width: `min(${widthPx.value}px, 95vw)` }
  })

  function setWidth(px: number): void {
    const clamped = Math.max(opts.minPx, Math.min(opts.maxPx, Math.round(px)))
    persistedPx.value = clamped
    savePx(opts.storageKey, clamped)
  }

  function reset(): void {
    persistedPx.value = null
    clearPx(opts.storageKey)
  }

  /* Direction note: the drawer slides in from the right, so the
   * resize handle sits on the drawer's LEFT edge. Dragging the
   * handle leftward (cursor / finger moves left, `clientX`
   * decreases) widens the drawer. So the delta is
   * `startX - clientX`.
   *
   * Both mouse and touch are wired. PrimeVue Splitter takes the
   * same pattern (Splitter.vue:254-267) — same gesture works on
   * desktop, tablet, and touchscreen-equipped laptops. */
  function attachHandle(handleEl: HTMLElement): () => void {
    let startX = 0
    let startW = 0

    function applyDelta(clientX: number): void {
      setWidth(startW + (startX - clientX))
    }

    function onMouseMove(e: MouseEvent): void {
      applyDelta(e.clientX)
    }

    function onMouseUp(): void {
      document.removeEventListener('mousemove', onMouseMove)
      document.removeEventListener('mouseup', onMouseUp)
    }

    function onMouseDown(e: MouseEvent): void {
      e.preventDefault()
      startX = e.clientX
      startW = widthPx.value
      document.addEventListener('mousemove', onMouseMove)
      document.addEventListener('mouseup', onMouseUp)
    }

    function onTouchMove(e: TouchEvent): void {
      if (e.touches.length === 0) return
      /* preventDefault on the move (not just start) blocks the
       * browser's default touch-scrolling so the page doesn't
       * pan while the user drags the handle. */
      e.preventDefault()
      applyDelta(e.touches[0].clientX)
    }

    function onTouchEnd(): void {
      document.removeEventListener('touchmove', onTouchMove)
      document.removeEventListener('touchend', onTouchEnd)
      document.removeEventListener('touchcancel', onTouchEnd)
    }

    function onTouchStart(e: TouchEvent): void {
      if (e.touches.length === 0) return
      e.preventDefault()
      startX = e.touches[0].clientX
      startW = widthPx.value
      /* `{ passive: false }` lets the touchmove handler call
       * preventDefault — browsers default newer touch listeners
       * to passive for scroll-perf reasons. */
      document.addEventListener('touchmove', onTouchMove, { passive: false })
      document.addEventListener('touchend', onTouchEnd)
      document.addEventListener('touchcancel', onTouchEnd)
    }

    handleEl.addEventListener('mousedown', onMouseDown)
    handleEl.addEventListener('touchstart', onTouchStart, { passive: false })

    return (): void => {
      handleEl.removeEventListener('mousedown', onMouseDown)
      handleEl.removeEventListener('touchstart', onTouchStart)
      /* Defensive: if the component unmounts mid-drag, also
       * tear down the document-level listeners so we don't leak
       * them past the component's lifetime. */
      document.removeEventListener('mousemove', onMouseMove)
      document.removeEventListener('mouseup', onMouseUp)
      document.removeEventListener('touchmove', onTouchMove)
      document.removeEventListener('touchend', onTouchEnd)
      document.removeEventListener('touchcancel', onTouchEnd)
    }
  }

  return { widthPx, widthStyle, isAtDefault, setWidth, reset, attachHandle }
}
