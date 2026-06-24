// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useI18n unit tests. Covers the gettext-text-as-key lookup
 * surface that the wizard slice consumes — fallback to English
 * literal, current-language read, locale (re)load + reactive
 * bump.
 *
 * happy-dom note: appending a `<script src=...>` to the DOM
 * synchronously fires `onerror` with "JavaScript file loading is
 * disabled" — happy-dom doesn't actually fetch network scripts.
 * Tests for `loadLocale()` therefore stub `document.head.appendChild`
 * so the script is captured for inspection without triggering
 * happy-dom's onerror, then manually drive `onload` / `onerror`
 * to exercise the resolve / reject paths.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { computed, isRef } from 'vue'

interface TvhLocaleGlobals {
  tvh_locale?: Record<string, string>
  tvh_locale_lang?: string
}

const g = globalThis as unknown as TvhLocaleGlobals

let capturedScripts: HTMLScriptElement[] = []
let appendChildSpy: ReturnType<typeof vi.spyOn> | null = null

beforeEach(() => {
  delete g.tvh_locale
  delete g.tvh_locale_lang
  capturedScripts = []
  /* Capture script elements without triggering happy-dom's
   * synchronous onerror. The captured array exposes them to
   * the tests so they can drive onload / onerror manually. */
  appendChildSpy = vi.spyOn(document.head, 'appendChild').mockImplementation(
    (node: Node) => {
      if (node instanceof HTMLScriptElement) {
        capturedScripts.push(node)
      }
      return node
    },
  )
  vi.resetModules()
})

afterEach(() => {
  delete g.tvh_locale
  delete g.tvh_locale_lang
  appendChildSpy?.mockRestore()
  appendChildSpy = null
})

describe('t() — static lookup', () => {
  it('returns the English literal when no locale dict is loaded', async () => {
    const { t } = await import('../useI18n')
    expect(t('Setup Wizard')).toBe('Setup Wizard')
  })

  it('returns the translated value when present in the dict', async () => {
    g.tvh_locale = { 'Setup Wizard': 'Einrichtungsassistent' }
    const { t } = await import('../useI18n')
    expect(t('Setup Wizard')).toBe('Einrichtungsassistent')
  })

  it('falls back to the English literal when key not in dict', async () => {
    g.tvh_locale = { 'Other String': 'Andere Zeichenkette' }
    const { t } = await import('../useI18n')
    expect(t('Save & Next')).toBe('Save & Next')
  })

  it('returns empty-string translation when value is intentionally empty', async () => {
    /* Defensive: gettext does sometimes emit "" as a translation
     * for fields that the translator chose to leave blank. The
     * lookup must NOT treat "" as undefined. */
    g.tvh_locale = { Cancel: '' }
    const { t } = await import('../useI18n')
    expect(t('Cancel')).toBe('')
  })
})

describe('t() — positional {N} substitution', () => {
  /* The variadic surface lets callers extract a whole sentence
   * as one msgid even when part of it is dynamic. Translators
   * own the placement of {0}, {1}, … in their language —
   * `'Move {0} up'` can re-order to `'{0} nach oben schieben'`. */

  it('substitutes a single {0} placeholder against the catalog value', async () => {
    g.tvh_locale = { 'Move {0} up': 'Schiebe {0} nach oben' }
    const { t } = await import('../useI18n')
    expect(t('Move {0} up', 'Channel')).toBe('Schiebe Channel nach oben')
  })

  it('substitutes against the English literal when not in catalog', async () => {
    const { t } = await import('../useI18n')
    expect(t('Move {0} up', 'Channel')).toBe('Move Channel up')
  })

  it('supports multiple placeholders in any order', async () => {
    g.tvh_locale = { '{0} of {1} selected': '{1} ausgewählt von {0}' }
    const { t } = await import('../useI18n')
    /* Translators can reverse argument order via placeholders
     * — that's the whole reason this exists. */
    expect(t('{0} of {1} selected', 3, 12)).toBe('12 ausgewählt von 3')
  })

  it('stringifies non-string arguments (numbers, booleans)', async () => {
    const { t } = await import('../useI18n')
    expect(t('{0} items', 5)).toBe('5 items')
    expect(t('Visible: {0}', true)).toBe('Visible: true')
  })

  it('passes through tokens whose index has no corresponding arg', async () => {
    /* Author bug catcher — keeping the literal {N} visible
     * makes the mismatch obvious in the rendered UI rather
     * than silently dropping the placeholder. */
    const { t } = await import('../useI18n')
    expect(t('Move {0} {1} up', 'Channel')).toBe('Move Channel {1} up')
  })

  it('leaves {N} tokens alone when no args are passed', async () => {
    /* Zero-arg call to a msgid that happens to contain {N}
     * — the catalog value comes through verbatim, which is
     * acceptable since the substitution-less form is what
     * pre-existing zero-arg call sites have always done. */
    g.tvh_locale = { 'Move {0} up': 'Schiebe {0} nach oben' }
    const { t } = await import('../useI18n')
    expect(t('Move {0} up')).toBe('Schiebe {0} nach oben')
  })
})

describe('currentLang()', () => {
  it('returns empty string when no locale loaded', async () => {
    const { currentLang } = await import('../useI18n')
    expect(currentLang()).toBe('')
  })

  it('returns the loaded language code', async () => {
    g.tvh_locale_lang = 'de'
    const { currentLang } = await import('../useI18n')
    expect(currentLang()).toBe('de')
  })
})

describe('useI18n() reactive composable', () => {
  it('exposes t and currentLang functions', async () => {
    const { useI18n } = await import('../useI18n')
    const { t, currentLang } = useI18n()
    expect(typeof t).toBe('function')
    expect(typeof currentLang).toBe('function')
  })

  it('returns translation when dict is loaded at call time', async () => {
    g.tvh_locale = { 'Save & Next': 'Speichern und weiter' }
    g.tvh_locale_lang = 'de'
    const { useI18n } = await import('../useI18n')
    const { t, currentLang } = useI18n()
    expect(t('Save & Next')).toBe('Speichern und weiter')
    expect(currentLang()).toBe('de')
  })

  it('reactive consumers re-evaluate after a locale swap', async () => {
    const { useI18n, loadLocale } = await import('../useI18n')
    const { t } = useI18n()
    const computedLabel = computed(() => t('Setup Wizard'))
    expect(isRef(computedLabel)).toBe(true)
    expect(computedLabel.value).toBe('Setup Wizard')

    /* Swap globals + drive the loadLocale onload to bump the
     * reactive ref. */
    g.tvh_locale = { 'Setup Wizard': 'Einrichtungsassistent' }
    const promise = loadLocale()
    const script = capturedScripts[0]
    expect(script).toBeDefined()
    script.onload?.(new Event('load'))
    await promise

    expect(computedLabel.value).toBe('Einrichtungsassistent')
  })
})

describe('loadLocale()', () => {
  it('appends a script tag with the locale.js source', async () => {
    const { loadLocale } = await import('../useI18n')
    const promise = loadLocale()
    expect(capturedScripts).toHaveLength(1)
    const script = capturedScripts[0]
    expect(script.src).toContain('/locale.js?_=')
    expect(script.id).toBe('tvh-locale-script')
    /* Resolve the promise so the test doesn't leak. */
    script.onload?.(new Event('load'))
    await promise
  })

  it('appends a fresh script element on each call with a different cache-bust', async () => {
    const { loadLocale } = await import('../useI18n')

    const first = loadLocale()
    expect(capturedScripts).toHaveLength(1)
    const firstScript = capturedScripts[0]
    firstScript.onload?.(new Event('load'))
    await first
    const firstSrc = firstScript.src

    /* Wait a tick so the cache-bust timestamp can advance. */
    await new Promise((r) => globalThis.setTimeout(r, 5))

    const second = loadLocale()
    expect(capturedScripts).toHaveLength(2)
    const secondScript = capturedScripts[1]
    /* Each call creates a fresh element so the browser
     * refetches against the user's current per-language
     * /locale.js. The cache-bust query string differs. */
    expect(secondScript).not.toBe(firstScript)
    expect(secondScript.src).not.toBe(firstSrc)
    secondScript.onload?.(new Event('load'))
    await second
  })

  it('rejects on script error', async () => {
    const { loadLocale } = await import('../useI18n')
    const promise = loadLocale()
    const script = capturedScripts[0]
    expect(script).toBeDefined()
    script.onerror?.(new Event('error'))
    await expect(promise).rejects.toThrow(/locale\.js/)
  })
})
