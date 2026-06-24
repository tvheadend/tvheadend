// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useResizableDrawerWidth — covers persistence, clamping, reset,
 * the style computed, and the mouse/touch drag plumbing.
 *
 * The drag tests synthesise events and assert against the
 * composable's reactive state — no DOM mount needed because
 * `attachHandle` operates on a bare HTMLElement.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

/* Mock the shared phone-breakpoint singleton with a test-driven
 * ref — happy-dom's matchMedia wiring can't be flipped reliably
 * from inside a test, and the composable's behaviour at each
 * breakpoint is what's under test, not the listener plumbing. */
const phoneFlag = vi.hoisted(() => ({
  set: (() => {}) as (v: boolean) => void,
}))
vi.mock('@/composables/useIsPhone', async () => {
  const { ref } = await import('vue')
  const isPhone = ref(false)
  phoneFlag.set = (v: boolean) => {
    isPhone.value = v
  }
  return {
    PHONE_MAX_WIDTH: 768,
    useIsPhone: () => isPhone,
    isPhoneNow: () => isPhone.value,
  }
})

import { useResizableDrawerWidth } from '../useResizableDrawerWidth'

const KEY = 'test-drawer-width'

beforeEach(() => {
  localStorage.clear()
})

afterEach(() => {
  localStorage.clear()
})

function mk() {
  return useResizableDrawerWidth({
    storageKey: KEY,
    defaultPx: 1100,
    minPx: 480,
    maxPx: 1600,
  })
}

describe('useResizableDrawerWidth — state', () => {
  it('returns defaultPx when no value persisted', () => {
    const r = mk()
    expect(r.widthPx.value).toBe(1100)
    expect(r.isAtDefault.value).toBe(true)
  })

  it('hydrates from localStorage on init', () => {
    localStorage.setItem(KEY, '880')
    const r = mk()
    expect(r.widthPx.value).toBe(880)
    expect(r.isAtDefault.value).toBe(false)
  })

  it('corrupt persisted value falls back to default + isAtDefault true', () => {
    localStorage.setItem(KEY, 'not-a-number')
    const r = mk()
    expect(r.widthPx.value).toBe(1100)
    expect(r.isAtDefault.value).toBe(true)
  })

  it('two composable instances with different storageKeys do not share state', () => {
    const a = useResizableDrawerWidth({
      storageKey: 'k-a',
      defaultPx: 800,
      minPx: 100,
      maxPx: 2000,
    })
    a.setWidth(900)
    const b = useResizableDrawerWidth({
      storageKey: 'k-b',
      defaultPx: 800,
      minPx: 100,
      maxPx: 2000,
    })
    expect(b.isAtDefault.value).toBe(true)
    expect(a.widthPx.value).toBe(900)
    localStorage.removeItem('k-a')
    localStorage.removeItem('k-b')
  })
})

describe('useResizableDrawerWidth — setWidth + reset', () => {
  it('setWidth clamps to maxPx', () => {
    const r = mk()
    r.setWidth(5000)
    expect(r.widthPx.value).toBe(1600)
    expect(localStorage.getItem(KEY)).toBe('1600')
  })

  it('setWidth clamps to minPx', () => {
    const r = mk()
    r.setWidth(100)
    expect(r.widthPx.value).toBe(480)
    expect(localStorage.getItem(KEY)).toBe('480')
  })

  it('setWidth rounds fractional pixels', () => {
    const r = mk()
    r.setWidth(900.7)
    expect(r.widthPx.value).toBe(901)
  })

  it('reset clears persisted state, returns to default', () => {
    const r = mk()
    r.setWidth(900)
    expect(r.isAtDefault.value).toBe(false)
    r.reset()
    expect(r.widthPx.value).toBe(1100)
    expect(r.isAtDefault.value).toBe(true)
    expect(localStorage.getItem(KEY)).toBeNull()
  })

  it('widthStyle produces `min(<n>px, 95vw)` shape', () => {
    const r = mk()
    expect(r.widthStyle.value).toEqual({ width: 'min(1100px, 95vw)' })
    r.setWidth(900)
    expect(r.widthStyle.value).toEqual({ width: 'min(900px, 95vw)' })
  })
})

function setViewportWidth(px: number): void {
  phoneFlag.set(px < 768)
}

describe('useResizableDrawerWidth — phone-mode full-screen', () => {
  /* Reset to desktop so the phone-mode tests don't leak their
   * narrow viewport into later tests. */
  afterEach(() => {
    phoneFlag.set(false)
  })

  it('phone widths return `width: 100vw` regardless of picked value', () => {
    setViewportWidth(400)
    const r = mk()
    expect(r.widthStyle.value).toEqual({ width: '100vw' })
    r.setWidth(900)
    /* Picked value is persisted but the style overrides to 100vw. */
    expect(r.widthStyle.value).toEqual({ width: '100vw' })
    expect(r.widthPx.value).toBe(900)
  })

  it('reactive on resize: desktop → phone flips widthStyle', async () => {
    setViewportWidth(1024)
    const r = mk()
    expect(r.widthStyle.value).toEqual({ width: 'min(1100px, 95vw)' })
    setViewportWidth(400)
    await Promise.resolve() /* allow ref to settle */
    expect(r.widthStyle.value).toEqual({ width: '100vw' })
  })

  it('767 is treated as phone (inclusive boundary matches the CSS media query)', () => {
    setViewportWidth(767)
    const r = mk()
    expect(r.widthStyle.value).toEqual({ width: '100vw' })
  })

  it('768 is treated as desktop', () => {
    setViewportWidth(768)
    const r = mk()
    expect(r.widthStyle.value).toEqual({ width: 'min(1100px, 95vw)' })
  })
})

describe('useResizableDrawerWidth — mouse drag', () => {
  it('mousedown + mousemove updates width by negated deltaX (drag-left = widen)', () => {
    const r = mk()
    const el = document.createElement('div')
    const cleanup = r.attachHandle(el)

    /* Start at the default width of 1100. mousedown at x=200,
     * mousemove to x=100 → cursor moved LEFT by 100 → drawer
     * widens by 100 → new width = 1200. */
    el.dispatchEvent(new MouseEvent('mousedown', { clientX: 200 }))
    document.dispatchEvent(new MouseEvent('mousemove', { clientX: 100 }))
    expect(r.widthPx.value).toBe(1200)
    document.dispatchEvent(new MouseEvent('mouseup'))

    /* After mouseup, further mousemoves should NOT update width. */
    document.dispatchEvent(new MouseEvent('mousemove', { clientX: 0 }))
    expect(r.widthPx.value).toBe(1200)

    cleanup()
  })

  it('cleanup fn removes the mousedown listener', () => {
    const r = mk()
    const el = document.createElement('div')
    const cleanup = r.attachHandle(el)
    cleanup()
    el.dispatchEvent(new MouseEvent('mousedown', { clientX: 100 }))
    document.dispatchEvent(new MouseEvent('mousemove', { clientX: 50 }))
    /* No drag started → no width change. */
    expect(r.widthPx.value).toBe(1100)
  })
})

function makeTouch(clientX: number): Touch {
  return { clientX, clientY: 0, identifier: 0 } as unknown as Touch
}

describe('useResizableDrawerWidth — touch drag', () => {
  it('touchstart + touchmove updates width same as mouse path', () => {
    const r = mk()
    const el = document.createElement('div')
    const cleanup = r.attachHandle(el)

    /* Start at default 1100. touchstart at x=200, touchmove to
     * x=120 → finger moved LEFT by 80 → drawer widens by 80 →
     * new width = 1180. */
    const startEvent = new TouchEvent('touchstart', {
      touches: [makeTouch(200)],
    })
    el.dispatchEvent(startEvent)

    const moveEvent = new TouchEvent('touchmove', {
      touches: [makeTouch(120)],
      cancelable: true,
    })
    document.dispatchEvent(moveEvent)
    expect(r.widthPx.value).toBe(1180)

    document.dispatchEvent(new TouchEvent('touchend'))

    /* After touchend, further touchmoves should NOT update width. */
    document.dispatchEvent(
      new TouchEvent('touchmove', { touches: [makeTouch(0)], cancelable: true }),
    )
    expect(r.widthPx.value).toBe(1180)

    cleanup()
  })

  it('touchcancel also tears down the drag listeners', () => {
    const r = mk()
    const el = document.createElement('div')
    const cleanup = r.attachHandle(el)

    el.dispatchEvent(new TouchEvent('touchstart', { touches: [makeTouch(300)] }))
    document.dispatchEvent(new TouchEvent('touchcancel'))

    /* Post-cancel touchmove must not update width. */
    document.dispatchEvent(
      new TouchEvent('touchmove', { touches: [makeTouch(100)], cancelable: true }),
    )
    expect(r.widthPx.value).toBe(1100)

    cleanup()
  })
})
