// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useHelp — module-level singleton for the wizard's in-app
 * help dock. The dock supports in-dock navigation between
 * cross-linked help pages (a `firstconfig` page that links to
 * `class/mpegts_service`, etc.), so state is a navigation history
 * stack rather than a single current page.
 *
 * Each entry caches its rendered HTML so back / breadcrumb
 * navigation is instant — no refetch round-trip when popping
 * back to a previously-viewed page in the same session.
 *
 * Lifecycle:
 *   - `open(page)`        — entry point from the Help button.
 *                           Resets the stack and loads `page`.
 *   - `navigateTo(page)`  — push another page onto the stack
 *                           (called by the dock's body click
 *                           handler when the user clicks an
 *                           internal markdown link).
 *   - `back()`            — pop the top entry; no-op when at the
 *                           root of the stack.
 *   - `goTo(index)`       — truncate the stack at `index + 1`,
 *                           used by breadcrumb item clicks.
 *   - `close()`           — flip `isOpen` to false AND clear the
 *                           stack so the next Help-button click
 *                           starts fresh.
 *   - `toggle(page)`      — Help-button entry point. Closes when
 *                           open (regardless of which page is
 *                           visible), else opens at `page`.
 *
 * Trust + fetch model: same as before — plain `fetch()` against
 * the C-side `/markdown/<page>` (ACCESS_ANONYMOUS, ok pre-login),
 * run through `renderMarkdown` → `rewriteStaticUrls` →
 * `addExternalLinkAttrs`. Compiled-in docs, no sanitisation
 * required.
 */
import { computed, ref } from 'vue'
import {
  addExternalLinkAttrs,
  escapeHtml,
  extractFirstHeading,
  renderMarkdown,
  rewriteStaticUrls,
} from '@/utils/markdown'

export interface HelpHistoryEntry {
  /* Page id, e.g. `firstconfig` or `class/mpegts_service`. The
   * same string used in the `/markdown/<page>` fetch URL and in
   * markdown source links. */
  page: string
  /* Human-readable label — first `<h1>` of the rendered content,
   * or a fallback path-segment of `page` when no heading. Drives
   * the breadcrumb crumb text. */
  label: string
  /* Cached rendered HTML so back / breadcrumb navigation is
   * instant. */
  html: string
}

const isOpen = ref(false)
const history = ref<HelpHistoryEntry[]>([])
const loading = ref(false)
const error = ref<string | null>(null)

/* Table-of-contents drawer state. `tocHtml` is the rendered
 * `/markdown/toc` index — fetched once and cached for the session,
 * mirroring Classic's `tvheadend.docs_toc`. `tocOpen` is the
 * drawer's visibility; it starts closed and only HelpDialog ever
 * surfaces the toggle (never the wizard dock). */
const tocOpen = ref(false)
const tocHtml = ref<string | null>(null)
const tocLoading = ref(false)
const tocError = ref<string | null>(null)

/* Top-of-stack convenience accessors so consumers (the dock
 * template, footer button, tests) don't have to index the array. */
const current = computed<HelpHistoryEntry | null>(
  () => history.value[history.value.length - 1] ?? null,
)
const currentPage = computed<string | null>(() => current.value?.page ?? null)
const html = computed<string | null>(() => current.value?.html ?? null)

/* Encode each path segment, keeping the literal `/` between
 * segments. `encodeURIComponent` on the whole page name escapes
 * the slash to `%2F`, which the C-side `page_markdown` handler's
 * path-based tokenizer (`doc_md.c:304-337`) does NOT decode
 * back into a path separator — so `class/mpegts_service` would
 * 404 instead of resolving to the `class/<name>` form. */
function encodePagePath(page: string): string {
  return page.split('/').map(encodeURIComponent).join('/')
}

async function fetchAndRender(page: string): Promise<HelpHistoryEntry> {
  const res = await fetch(`/markdown/${encodePagePath(page)}`)
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}`)
  }
  const raw = await res.text()
  const rendered = addExternalLinkAttrs(rewriteStaticUrls(renderMarkdown(raw)))
  const label = extractFirstHeading(rendered, page)
  return { page, label, html: rendered }
}

/* Build a synthetic history entry for a failed navigation —
 * pushed onto the stack when navigateTo can't fetch the target
 * so the user gets visible feedback (instead of a silent no-op)
 * and a working Back button to return to the previous page. */
function makeErrorEntry(page: string, message: string, labelHint?: string): HelpHistoryEntry {
  const slash = page.lastIndexOf('/')
  const fallbackLabel = slash >= 0 ? page.slice(slash + 1) : page
  const label = labelHint?.trim() || fallbackLabel
  const html =
    `<div class="help-panel__nav-error">` +
    `<p><strong>Could not load this help page.</strong></p>` +
    `<p><code>${escapeHtml(page)}</code></p>` +
    `<p>${escapeHtml(message)}</p>` +
    `</div>`
  return { page, label, html }
}

async function open(page: string): Promise<void> {
  /* Entry point from the Help button — start a fresh stack. If
   * the same page is already at the root of the stack and we're
   * just re-opening (after close), reuse the cached entry rather
   * than refetching. */
  if (
    isOpen.value === false &&
    history.value.length === 1 &&
    history.value[0].page === page
  ) {
    isOpen.value = true
    return
  }
  loading.value = true
  error.value = null
  try {
    const entry = await fetchAndRender(page)
    history.value = [entry]
    isOpen.value = true
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
    history.value = []
    isOpen.value = true
  } finally {
    loading.value = false
  }
}

async function navigateTo(page: string, labelHint?: string): Promise<void> {
  /* In-dock link click — push the target onto the history
   * stack. No-op when the user clicks a link to the current
   * page (defensive).
   *
   * `labelHint` is the visible link text the user clicked
   * (e.g. "Services"). Used only when the fetch fails so the
   * synthetic error entry's breadcrumb crumb reads like what
   * the user actually clicked, rather than the raw page id.
   * On success the label is extracted from the loaded page's
   * `<h1>` instead. */
  if (currentPage.value === page) return
  loading.value = true
  error.value = null
  try {
    const entry = await fetchAndRender(page)
    history.value.push(entry)
  } catch (e) {
    const message = e instanceof Error ? e.message : String(e)
    error.value = message
    /* Push a synthetic entry so the user sees that navigation
     * happened and gets a working Back button. Without this
     * the prior page's HTML keeps rendering and the click
     * appears to do nothing. */
    history.value.push(makeErrorEntry(page, message, labelHint))
  } finally {
    loading.value = false
  }
}

function back(): void {
  /* Root-of-stack guard — back doesn't close the dock; the user
   * clicks Help (or ×) for that. */
  if (history.value.length > 1) {
    history.value.pop()
  }
}

function goTo(index: number): void {
  /* Truncate the stack to length `index + 1`. Valid range is
   * 0..history.length-1; out-of-range silently no-ops. The
   * top-of-stack case (index = history.length - 1) is a no-op
   * since we'd be slicing to the same length. */
  if (index < 0 || index >= history.value.length) return
  if (index === history.value.length - 1) return
  history.value = history.value.slice(0, index + 1)
}

function close(): void {
  isOpen.value = false
  history.value = []
  error.value = null
  /* The TOC drawer doesn't survive a help-close — the next open
   * starts with it shut. `tocHtml` stays cached, so reopening it
   * costs no refetch. */
  tocOpen.value = false
  tocError.value = null
}

async function toggle(page: string): Promise<void> {
  /* When the dock is open, the Help button closes it regardless
   * of which page is currently visible. The user's mental model
   * is "Help is on / Help is off"; navigating deeper inside the
   * dock doesn't change that toggle's semantics. */
  if (isOpen.value) {
    close()
    return
  }
  await open(page)
}

async function loadToc(): Promise<void> {
  /* Fetch the `/markdown/toc` index once and cache it — it's a
   * static compiled-in document, so a session-lifetime cache is
   * safe. Already-loaded and in-flight are both guarded, so the
   * lazy first-open trigger and an explicit call can't double up. */
  if (tocHtml.value !== null || tocLoading.value) return
  tocLoading.value = true
  tocError.value = null
  try {
    const entry = await fetchAndRender('toc')
    tocHtml.value = entry.html
  } catch (e) {
    tocError.value = e instanceof Error ? e.message : String(e)
  } finally {
    tocLoading.value = false
  }
}

function toggleToc(): void {
  /* Drawer toggle from the panel header's Contents button. The
   * first open kicks off the lazy `/markdown/toc` fetch. */
  tocOpen.value = !tocOpen.value
  if (tocOpen.value) void loadToc()
}

function closeToc(): void {
  tocOpen.value = false
}

export function useHelp() {
  return {
    isOpen,
    history,
    current,
    currentPage,
    html,
    loading,
    error,
    open,
    navigateTo,
    back,
    goTo,
    close,
    toggle,
    tocOpen,
    tocHtml,
    tocLoading,
    tocError,
    loadToc,
    toggleToc,
    closeToc,
  }
}
