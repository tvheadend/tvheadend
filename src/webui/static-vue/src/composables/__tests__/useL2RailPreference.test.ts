// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

/* The composable is module-level singleton state — every test
 * group below uses `vi.resetModules()` + a fresh `import` so the
 * singleton starts clean. localStorage is mocked per test so
 * persistence behaviour can be observed without leaking between
 * cases. */

const STORAGE_KEY = 'tvh-l2sidebar:state'

const memoryStore = new Map<string, string>()

beforeEach(() => {
  memoryStore.clear()
  /* Replace the global `localStorage` with a deterministic
   * in-memory shim. window.localStorage is a getter on the global
   * `window` in jsdom; redefine the property so reads / writes hit
   * our shim. */
  Object.defineProperty(globalThis.window, 'localStorage', {
    configurable: true,
    value: {
      getItem: (k: string) => memoryStore.get(k) ?? null,
      setItem: (k: string, v: string) => {
        memoryStore.set(k, v)
      },
      removeItem: (k: string) => {
        memoryStore.delete(k)
      },
      clear: () => memoryStore.clear(),
      key: () => null,
      length: 0,
    },
  })
})

afterEach(() => {
  vi.resetModules()
})

describe('useL2RailPreference — initial state', () => {
  it('defaults to expanded when localStorage has no entry', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    expect(r.manualCollapsed.value).toBe(false)
    expect(r.autoActive.value).toBe(false)
    expect(r.autoOverride.value).toBeNull()
  })

  it('hydrates `manualCollapsed: true` from localStorage', async () => {
    memoryStore.set(STORAGE_KEY, 'collapsed')
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    expect(r.manualCollapsed.value).toBe(true)
  })

  it('treats any non-"collapsed" value as expanded', async () => {
    memoryStore.set(STORAGE_KEY, 'expanded')
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    expect(r.manualCollapsed.value).toBe(false)
  })
})

describe('useL2RailPreference — toggle (no auto active)', () => {
  it('flips manualCollapsed and persists to localStorage', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    expect(r.manualCollapsed.value).toBe(false)

    r.toggle()
    /* The watcher writes through synchronously via Vue's flush;
     * await a microtask to let it complete. */
    await new Promise<void>((resolve) => queueMicrotask(() => resolve()))
    expect(r.manualCollapsed.value).toBe(true)
    expect(memoryStore.get(STORAGE_KEY)).toBe('collapsed')

    r.toggle()
    await new Promise<void>((resolve) => queueMicrotask(() => resolve()))
    expect(r.manualCollapsed.value).toBe(false)
    expect(memoryStore.get(STORAGE_KEY)).toBe('expanded')
  })

  it('clears any stale autoOverride when toggling without auto active', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    /* Seed a stale override (would normally come from a prior
     * auto-active toggle). With autoActive false, the next toggle
     * should clear it. */
    r.autoOverride.value = true
    r.toggle()
    expect(r.autoOverride.value).toBeNull()
    expect(r.manualCollapsed.value).toBe(true)
  })
})

describe('useL2RailPreference — toggle with auto active', () => {
  it('first click sets autoOverride to false (expand on auto-collapsed view)', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    r.autoActive.value = true
    expect(r.autoOverride.value).toBeNull()

    r.toggle()
    expect(r.autoOverride.value).toBe(false)
    /* manualCollapsed must NOT change — the override is
     * per-visit; the user's standing preference is preserved
     * for non-auto routes. */
    expect(r.manualCollapsed.value).toBe(false)
  })

  it('subsequent clicks toggle autoOverride between false and true', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    r.autoActive.value = true

    r.toggle() /* null → false */
    expect(r.autoOverride.value).toBe(false)
    r.toggle() /* false → true */
    expect(r.autoOverride.value).toBe(true)
    r.toggle() /* true → false */
    expect(r.autoOverride.value).toBe(false)
  })

  it('does not write to localStorage when toggling under autoActive', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    r.autoActive.value = true

    r.toggle()
    await new Promise<void>((resolve) => queueMicrotask(() => resolve()))
    /* manualCollapsed never changes → the watcher never fires →
     * localStorage stays empty. */
    expect(memoryStore.has(STORAGE_KEY)).toBe(false)
  })
})

describe('useL2RailPreference — clearAutoOverride', () => {
  it('resets autoOverride to null', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    r.autoOverride.value = true
    r.clearAutoOverride()
    expect(r.autoOverride.value).toBeNull()
  })

  it('leaves manual + auto state untouched', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const r = useL2RailPreference()
    r.manualCollapsed.value = true
    r.autoActive.value = true
    r.autoOverride.value = false
    r.clearAutoOverride()
    expect(r.autoOverride.value).toBeNull()
    expect(r.manualCollapsed.value).toBe(true)
    expect(r.autoActive.value).toBe(true)
  })
})

describe('useL2RailPreference — singleton sharing', () => {
  it('two consumers see the same state instance', async () => {
    const { useL2RailPreference } = await import('../useL2RailPreference')
    const a = useL2RailPreference()
    const b = useL2RailPreference()
    a.manualCollapsed.value = true
    expect(b.manualCollapsed.value).toBe(true)
  })
})
