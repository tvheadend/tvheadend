// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useI18n unit tests. Covers the gettext-text-as-key lookup
 * surface that the wizard slice consumes — fallback to English
 * literal, plural forms, current-language read, locale (re)load +
 * reactive bump.
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
  tvh_locale?: Record<string, string | string[]>
  tvh_locale_plural?: (n: number) => number
  tvh_locale_lang?: string
}

const g = globalThis as unknown as TvhLocaleGlobals

/* A Czech plural entry in the shape support/pojs.py writes: one
 * form per msgstr[N], and the rule for cs ("nplurals=4; plural=
 * (n == 1 && n % 1 == 0) ? 0 : (n >= 2 && n <= 4 && n % 1 == 0) ?
 * 1: (n % 1 != 0 ) ? 2 : 3;") as a function. */
const CZECH_ROWS = ['{0} řádek', '{0} řádky', '{0} řádku', '{0} řádků']

function czechPlural(n: number): number {
  if (n === 1) return 0
  if (n >= 2 && n <= 4 && n % 1 === 0) return 1
  if (n % 1 !== 0) return 2
  return 3
}

let capturedScripts: HTMLScriptElement[] = []
let appendChildSpy: ReturnType<typeof vi.spyOn> | null = null

beforeEach(() => {
  delete g.tvh_locale
  delete g.tvh_locale_plural
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
  delete g.tvh_locale_plural
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

describe('tn() — plural forms', () => {
  it('takes the English singular for one and the plural otherwise', async () => {
    const { tn } = await import('../useI18n')
    expect(tn('{0} row', '{0} rows', 0)).toBe('0 rows')
    expect(tn('{0} row', '{0} rows', 1)).toBe('1 row')
    expect(tn('{0} row', '{0} rows', 2)).toBe('2 rows')
  })

  it('substitutes explicit args instead of the count', async () => {
    const { tn } = await import('../useI18n')
    expect(tn('{0} ({1} entry)', '{0} ({1} entries)', 1, 'Edit', 1)).toBe('Edit (1 entry)')
    expect(tn('{0} ({1} entry)', '{0} ({1} entries)', 3, 'Edit', 3)).toBe('Edit (3 entries)')
  })

  it('picks the form for the count from the catalog', async () => {
    g.tvh_locale = { '{0} row': CZECH_ROWS }
    g.tvh_locale_plural = czechPlural
    const { tn } = await import('../useI18n')
    expect([1, 2, 4, 5, 21, 22].map((n) => tn('{0} row', '{0} rows', n))).toEqual([
      '1 řádek',
      '2 řádky',
      '4 řádky',
      '5 řádků',
      '21 řádků',
      '22 řádků',
    ])
  })

  it('falls back to English when the entry, the rule or the form is unusable', async () => {
    const { tn } = await import('../useI18n')
    g.tvh_locale_plural = czechPlural
    g.tvh_locale = { '{0} row': '{0} řádek' }
    expect(tn('{0} row', '{0} rows', 2)).toBe('2 rows')

    g.tvh_locale = { '{0} row': CZECH_ROWS }
    delete g.tvh_locale_plural
    expect(tn('{0} row', '{0} rows', 2)).toBe('2 rows')

    g.tvh_locale_plural = () => 7
    expect(tn('{0} row', '{0} rows', 1)).toBe('1 row')

    g.tvh_locale_plural = czechPlural
    expect(tn('{0} row', '{0} rows', Number.NaN)).toBe('NaN rows')
  })

  it('ignores a rule left over from the previous language', async () => {
    /* The server's empty catalog for English resets only
     * tvh_locale and tvh_locale_lang, so the Czech rule stays. */
    g.tvh_locale = {}
    g.tvh_locale_plural = czechPlural
    const { tn } = await import('../useI18n')
    expect(tn('{0} row', '{0} rows', 2)).toBe('2 rows')
  })

  it('leaves plural entries to tn(), t() gives the English text', async () => {
    g.tvh_locale = { '{0} row': CZECH_ROWS }
    g.tvh_locale_plural = czechPlural
    const { t } = await import('../useI18n')
    expect(t('{0} row')).toBe('{0} row')
    expect(t('{0} row', 3)).toBe('3 row')
  })
})

describe('N_()', () => {
  it('returns the text it marks', async () => {
    const { N_ } = await import('../useI18n')
    expect(N_('Welcome')).toBe('Welcome')
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
  it('exposes t, tn and currentLang functions', async () => {
    const { useI18n } = await import('../useI18n')
    const { t, tn, currentLang } = useI18n()
    expect(typeof t).toBe('function')
    expect(typeof tn).toBe('function')
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

  it('reactive plural consumers re-evaluate after a locale swap', async () => {
    const { useI18n, loadLocale } = await import('../useI18n')
    const { tn } = useI18n()
    const computedLabel = computed(() => tn('{0} row', '{0} rows', 3))
    expect(computedLabel.value).toBe('3 rows')

    g.tvh_locale = { '{0} row': CZECH_ROWS }
    g.tvh_locale_plural = czechPlural
    const promise = loadLocale()
    capturedScripts[0].onload?.(new Event('load'))
    await promise

    expect(computedLabel.value).toBe('3 řádky')
  })
})

describe('loadLocale()', () => {
  it('appends a script tag with the locale-vue.js source', async () => {
    const { loadLocale } = await import('../useI18n')
    const promise = loadLocale()
    expect(capturedScripts).toHaveLength(1)
    const script = capturedScripts[0]
    expect(script.src).toContain('/redir/locale-vue.js?_=')
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
     * /locale-vue.js. The cache-bust query string differs. */
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
    await expect(promise).rejects.toThrow(/locale-vue\.js/)
  })
})
