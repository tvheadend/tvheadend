// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Markdown rendering — bootstraps the same `marked` library the
 * ExtJS UI ships, then provides a sync renderer.
 *
 * Why share ExtJS's `marked.js` instead of installing the npm
 * package: identical rendering to the ExtJS wizard, zero Vue
 * bundle impact, and one fewer place to update if the library
 * ever needs a security bump. Mirrors the `/locale.js` pattern
 * established by `useI18n.ts` — same "share with ExtJS where
 * possible" principle.
 *
 * The script defines `globalThis.marked` as a function
 * `(src: string, opts?: object) => string`. We don't reach into
 * its types beyond that signature.
 *
 * Bootstrap is awaited from `main.ts` before app mount. Failure
 * is non-fatal — `renderMarkdown()` then falls back to escaping
 * the raw text so untranslated Markdown source still renders
 * legibly (without `**` artifacts becoming HTML).
 */

interface MarkedGlobal {
  marked?: (src: string, opts?: Record<string, unknown>) => string
}

export function loadMarkedScript(): Promise<void> {
  return new Promise((resolve, reject) => {
    const existing = document.getElementById('tvh-marked-script')
    if (existing) {
      existing.remove()
    }
    const s = document.createElement('script')
    s.id = 'tvh-marked-script'
    s.src = '/static/app/marked.js'
    s.onload = () => resolve()
    s.onerror = () => reject(new Error('Failed to load /static/app/marked.js'))
    document.head.appendChild(s)
  })
}

/*
 * Render Markdown to HTML. Falls back to a plain escaped-text
 * paragraph when the script failed to load — guarantees the
 * caller never gets the raw `**bold**` syntax shown as-is.
 *
 * Trust model: every consumer in this codebase feeds Markdown
 * that originates from compiled-in C strings (wizard step
 * descriptions in `docs/wizard/*.md`), so HTML sanitization
 * isn't required. Matches ExtJS's
 * `static/app/wizard.js:105 — text = marked(text)` which also
 * skips sanitization. If a future consumer feeds user-supplied
 * Markdown, route it through DOMPurify before passing here.
 */
export function renderMarkdown(text: string): string {
  if (!text) return ''
  const g = globalThis as unknown as MarkedGlobal
  if (typeof g.marked === 'function') {
    return g.marked(text)
  }
  /* marked.js didn't load — return a safely-escaped fallback so
   * the user sees the raw paragraphs without HTML injection. */
  return `<p>${escapeHtml(text)}</p>`
}

export function escapeHtml(s: string): string {
  return s
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;')
}

/*
 * Add `target="_blank" rel="noopener noreferrer"` to every
 * `<a>` tag in rendered HTML so external links open in a new
 * tab. Used on the wizard's description Markdown: the channels
 * step has Tvheadend.org / IRC / donate / banner-image links
 * that would otherwise drop the user out of the wizard flow.
 *
 * Skipped for:
 *   - tags that already declare a `target` attribute (don't
 *     clobber author intent);
 *   - in-page anchors (`href="#..."`) — those should stay
 *     in-document.
 *
 * The `rel="noopener noreferrer"` pair is the safe default for
 * any new-tab link: prevents the opened tab from driving us
 * via `window.opener`, and strips the Referer header so we
 * don't leak the wizard route path to the external site.
 */
export function addExternalLinkAttrs(html: string): string {
  if (!html) return ''
  /* DOMParser instead of a regex pass: avoids Sonar's S5852
   * heuristic (which flags any unbounded quantifier in a regex,
   * including provably-linear shapes like `[^>]*`), AND
   * sidesteps real edge cases the regex can't handle — e.g.
   * attribute values containing `>` characters, malformed
   * tag whitespace, mixed case, entity references. */
  const doc = new DOMParser().parseFromString(html, 'text/html')
  for (const a of Array.from(doc.body.querySelectorAll('a'))) {
    /* Skip when the author already declared a target — don't
     * clobber their intent. */
    if (a.hasAttribute('target')) continue
    const href = a.getAttribute('href') ?? ''
    /* Only stamp http/https URLs. Internal markdown-page links
     * (relative paths like `class/mpegts_service`) and root-
     * absolute paths (`/static/img/...`) should NOT open in a
     * new tab — relative ones get intercepted by the help dock's
     * click handler for in-dock navigation, and root-absolute
     * ones either resolve to in-app routes or to static assets
     * the existing tab handles fine. In-doc anchors (`#section`)
     * fall through here naturally — they aren't http/https. */
    if (!/^https?:\/\//i.test(href)) continue
    a.setAttribute('target', '_blank')
    a.setAttribute('rel', 'noopener noreferrer')
  }
  return doc.body.innerHTML
}

/*
 * Pull the first `<h1>` text from rendered markdown to use as a
 * page label (breadcrumb crumb, dock title etc.). Falls back to
 * the last `/`-separated segment of the page id when the markdown
 * has no H1 (rare — every help page in the tree opens with one).
 *
 * Trust model matches the rest of this module: the input HTML is
 * always renderMarkdown's output of compiled-in server docs, so
 * no sanitization is required. The DOMParser pass avoids regex
 * fragility around nested tags inside the heading (`<h1>Service
 * <em>Mapping</em></h1>` → "Service Mapping").
 */
export function extractFirstHeading(html: string, fallback: string): string {
  if (html) {
    try {
      const doc = new DOMParser().parseFromString(html, 'text/html')
      const h1 = doc.body.querySelector('h1')
      const text = h1?.textContent?.trim()
      if (text) return text
    } catch {
      /* DOMParser shouldn't throw on `text/html` mode, but if it
       * ever does (some sandboxed envs), fall through to the
       * path-based fallback. */
    }
  }
  const slash = fallback.lastIndexOf('/')
  return slash >= 0 ? fallback.slice(slash + 1) : fallback
}

/*
 * Rewrite root-relative `static/…` attribute values to absolute
 * `/static/…`. The compiled-in markdown (wizard step
 * descriptions + `/markdown/firstconfig` help page) uses paths
 * like `static/img/doc/firstconfig/wizard.png` that work
 * verbatim under ExtJS's `/` root but resolve against the
 * current route under Vue (`/gui/wizard/login` →
 * `/gui/wizard/static/...` → 404). Prepending `/` anchors them
 * at the server root so the existing /static handler serves
 * the asset.
 *
 * Only touches `src=` and `href=` whose value starts with
 * exactly `static/` (no leading slash, no scheme) — external
 * links, in-doc anchors, and already-absolute paths pass
 * through unchanged.
 */
export function rewriteStaticUrls(html: string): string {
  return html.replace(
    /(\s(?:src|href))="(static\/[^"]*)"/g,
    (_match, attr: string, path: string) => `${attr}="/${path}"`,
  )
}
