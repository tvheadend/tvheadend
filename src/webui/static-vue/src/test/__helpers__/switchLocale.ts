// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Runtime language change for component tests.
 *
 * Installs `dict` as the `tvh_locale` catalog and runs
 * `loadLocale()` the way the wizard's language pick does, so every
 * reactive `useI18n().t` consumer re-renders. happy-dom doesn't
 * fetch the locale script (see useI18n.test.ts), so the script
 * element is captured and its `onload` driven by hand.
 *
 * Pair it with `clearLocale()` in `afterEach` so the catalog
 * doesn't leak into later tests.
 */
import { vi } from 'vitest'
import { nextTick } from 'vue'
import { loadLocale } from '@/composables/useI18n'

interface TvhLocaleGlobals {
  tvh_locale?: Record<string, string>
}

export async function switchLocale(dict: Record<string, string>): Promise<void> {
  const scripts: HTMLScriptElement[] = []
  const spy = vi.spyOn(document.head, 'appendChild').mockImplementation((node: Node) => {
    if (node instanceof HTMLScriptElement) scripts.push(node)
    return node
  })
  try {
    ;(globalThis as unknown as TvhLocaleGlobals).tvh_locale = dict
    const loaded = loadLocale()
    scripts[0]?.onload?.(new Event('load'))
    await loaded
  } finally {
    spy.mockRestore()
  }
  await nextTick()
}

export function clearLocale(): void {
  delete (globalThis as unknown as TvhLocaleGlobals).tvh_locale
}
