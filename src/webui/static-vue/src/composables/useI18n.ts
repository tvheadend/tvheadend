// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useI18n — minimal gettext-style i18n surface for the Vue UI.
 *
 * **Scope today**: wizard files only. The rest of the Vue UI keeps
 * its hardcoded English literals and gets retrofitted incrementally
 * in later slices. ADR 0007 is the architectural reference; this
 * composable is the partial implementation that lands with the
 * wizard slice.
 *
 * **How it works**: tvheadend's existing i18n pipeline already
 * generates per-language locale files (served at `/redir/locale.js`,
 * the same endpoint that powers the ExtJS UI). The catalog declares
 * two globals on `globalThis`:
 *
 *   tvh_locale       — flat dict {"English literal": "Translation"}
 *   tvh_locale_lang  — language code string (e.g. 'de', 'fr', '')
 *
 * Source of these globals at server-side: `webui.c:2622-2638`.
 * Built from `xgettext` extraction over `static/app/*.js` →
 * `intl/js/*.po` → Transifex → `static/intl/tvh.<lang>.js.gz`.
 *
 * **Why gettext-text-as-key**: the English literal IS the
 * translation key. ExtJS's `_('Setup Wizard')` looks up
 * `tvh_locale['Setup Wizard']`; we mirror that exactly. Strings
 * already extracted from `static/app/wizard.js` are translated
 * for every locale tvheadend ships — Vue wizard files reusing
 * those literals get translations for free, without touching
 * the build pipeline.
 *
 * **Novel strings** (no existing entry in `tvh_locale`) fall back
 * to the English literal until the build pipeline's `xgettext`
 * extraction is extended to scan `static-vue/` sources (pending).
 * Implementations should mark such strings with
 * `/* i18n: new string *\/` so a future audit finds them.
 */
import { ref } from 'vue'

/*
 * Reactivity hook. Bumped every time `loadLocale()` swaps the
 * locale dict. Components that consume `t()` via the
 * `useI18n()` composable read this ref, which makes their
 * template / computed dependencies refresh on language change
 * even though `globalThis.tvh_locale` itself isn't reactive.
 */
const localeBump = ref(0)

interface TvhLocaleGlobals {
  tvh_locale?: Record<string, string>
  tvh_locale_lang?: string
}

/*
 * Static lookup — for use in script-level constants where
 * reactivity isn't needed. Returns the translated string if
 * present in the loaded `tvh_locale` dict; otherwise returns
 * the English literal unchanged.
 *
 * Optional `{0}`, `{1}`, … positional substitution mirrors
 * ExtJS's `String.format` convention so a sentence with a
 * dynamic part (a column name, a count, etc.) can be
 * extracted as ONE msgid for translators — `t('Move {0}
 * up', name)` keeps the whole sentence translatable rather
 * than concatenating split fragments. Substitution runs AFTER
 * the catalog lookup so translators control word order:
 * `tvh_locale['Move {0} up'] === 'Schiebe {0} nach oben'`
 * yields German with the placeholder in the right position.
 * Missing-from-catalog falls back to substituting the English
 * literal directly.
 */
export function t(englishText: string, ...args: unknown[]): string {
  const g = globalThis as unknown as TvhLocaleGlobals
  const dict = g.tvh_locale
  const resolved = dict?.[englishText] ?? englishText
  return args.length === 0 ? resolved : substitute(resolved, args)
}

/*
 * Positional `{N}` substitution. Replaces every `{0}` /
 * `{1}` / … with `String(args[N])`. Indices not in args (or
 * negative / non-integer) pass through verbatim — keeps the
 * raw `{N}` visible in the rendered string so the bug is
 * easy to spot, rather than silently dropping the token.
 *
 * Single regex pass so a token that resolves to a string
 * containing `{0}` (rare but possible — a literal entered
 * by a user) doesn't get re-substituted recursively.
 */
function substitute(template: string, args: readonly unknown[]): string {
  return template.replace(/\{(\d+)\}/g, (match, idxStr: string) => {
    const idx = Number(idxStr)
    if (!Number.isInteger(idx) || idx < 0 || idx >= args.length) {
      return match
    }
    return String(args[idx])
  })
}

/*
 * Current language code (`'de'`, `'fr'`, …) or `''` when no
 * translation file is loaded. Useful for callers that need to
 * branch on language (e.g. `Intl.DateTimeFormat` locale param).
 */
export function currentLang(): string {
  const g = globalThis as unknown as TvhLocaleGlobals
  return g.tvh_locale_lang ?? ''
}

/*
 * Reactive version. Use inside Vue templates / computeds when
 * the rendered text must re-evaluate after a language change.
 * The composable touches `localeBump.value` so any template
 * call site is tracked as a reactive dependency on it; bumping
 * the ref triggers re-render. Non-reactive call sites can
 * import `t` directly.
 */
export function useI18n() {
  return {
    t: (englishText: string, ...args: unknown[]): string => {
      /* Read localeBump.value so Vue's reactivity tracks this
       * function call against the bump ref — when loadLocale
       * increments it, every template/computed call site
       * re-evaluates. The early-return is unreachable (bump
       * only counts up) but pins the read as a "used" value
       * for both TS noUnusedLocals and Sonar S3735. */
      const bump = localeBump.value
      if (bump < 0) return englishText
      return t(englishText, ...args)
    },
    currentLang: (): string => {
      const bump = localeBump.value
      if (bump < 0) return ''
      return currentLang()
    },
  }
}

/*
 * Bootstrap or re-bootstrap the locale. Removes any existing
 * `redir/locale.js` script tag and inserts a fresh one with a cache-
 * busting query param so the browser refetches against the
 * server's current per-user `aa_lang_ui`. Resolves on script
 * load; rejects on network error.
 *
 * Called once during app bootstrap (`main.ts`) and again from
 * the wizard's hello-step language-change orchestration to
 * pick up the new translations without a page reload.
 */
export function loadLocale(): Promise<void> {
  return new Promise((resolve, reject) => {
    /* Drop the previous script element so the new fetch isn't a
     * cache hit — different `aa_lang_ui` after a language pick
     * means the server emits a redirect to a different
     * `tvh.<lang>.js.gz` file. */
    const existing = document.getElementById('tvh-locale-script')
    existing?.remove()

    const s = document.createElement('script')
    s.id = 'tvh-locale-script'
    /* Cache bust by timestamp — the server may reasonably set
     * caching headers and we must always pick up the current
     * user's language file. */
    s.src = `/redir/locale.js?_=${Date.now()}`
    s.onload = () => {
      /* Bump the reactive ref so any reactive consumer of `t()`
       * re-evaluates against the new dict. */
      localeBump.value++
      resolve()
    }
    s.onerror = () => {
      /* Locale fetch failed — leave the previous globals in
       * place (or none, if first load). `t()` falls back to the
       * English literal silently, so the UI is still usable. */
      reject(new Error('Failed to load /redir/locale.js'))
    }
    document.head.appendChild(s)
  })
}
