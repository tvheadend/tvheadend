// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useHelp unit tests.
 *
 * The composable is a module-level singleton; `vi.resetModules()`
 * gives us a fresh singleton per test (via the `importHelp()`
 * helper). The markdown pipeline is mocked so each pass leaves
 * recognisable tags, letting us verify both the rendering order
 * and the label-extraction behaviour without booting the real
 * marked.js bundle.
 */
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest'

vi.mock('@/utils/markdown', () => ({
  renderMarkdown: (s: string) => `<h1>${s}</h1><RM>${s}</RM>`,
  rewriteStaticUrls: (s: string) => `<RW>${s}</RW>`,
  addExternalLinkAttrs: (s: string) => `<EXT>${s}</EXT>`,
  escapeHtml: (s: string) => s.replaceAll('<', '&lt;').replaceAll('>', '&gt;'),
  /* Pull the page-key out of the wrapped output by reading the
   * `<RM>` tag's inner text — gives the test a deterministic
   * label without coupling to DOMParser specifics. */
  extractFirstHeading: (html: string, fallback: string) => {
    const m = /<RM>([^<]+)<\/RM>/.exec(html)
    return m ? m[1] : fallback
  },
}))

async function importHelp() {
  vi.resetModules()
  const mod = await import('../useHelp')
  return mod.useHelp()
}

const fetchMock = vi.fn()

beforeEach(() => {
  fetchMock.mockReset()
  vi.stubGlobal('fetch', fetchMock)
})

afterEach(() => {
  vi.unstubAllGlobals()
})

function mockFetchOk(body: string): void {
  fetchMock.mockResolvedValueOnce({
    ok: true,
    status: 200,
    text: () => Promise.resolve(body),
  })
}

function mockFetchFail(status: number): void {
  fetchMock.mockResolvedValueOnce({
    ok: false,
    status,
    text: () => Promise.resolve(''),
  })
}

describe('useHelp — initial + open / toggle', () => {
  it('starts closed with an empty history', async () => {
    const h = await importHelp()
    expect(h.isOpen.value).toBe(false)
    expect(h.history.value).toEqual([])
    expect(h.currentPage.value).toBeNull()
    expect(h.html.value).toBeNull()
    expect(h.loading.value).toBe(false)
    expect(h.error.value).toBeNull()
  })

  it('toggle from closed → opens, fetches, and seeds the history with one entry', async () => {
    const h = await importHelp()
    mockFetchOk('firstconfig-body')
    await h.toggle('firstconfig')
    expect(fetchMock).toHaveBeenCalledTimes(1)
    expect(fetchMock).toHaveBeenCalledWith('/markdown/firstconfig')
    expect(h.isOpen.value).toBe(true)
    expect(h.history.value).toHaveLength(1)
    expect(h.history.value[0].page).toBe('firstconfig')
    /* Label round-trips via the mocked extractFirstHeading,
     * which reads the inner `<RM>...</RM>` text. */
    expect(h.history.value[0].label).toBe('firstconfig-body')
    expect(h.currentPage.value).toBe('firstconfig')
  })

  it('toggle when open closes regardless of which page is at the top of history', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.toggle('firstconfig')
    /* Push a second entry via navigateTo so the top of stack is
     * a different page than the toggle target. */
    mockFetchOk('b')
    await h.navigateTo('class/mpegts_service')
    expect(h.history.value).toHaveLength(2)
    /* Now toggle with the ORIGINAL helpPage — closes the dock,
     * doesn't try to rebase to that page. */
    await h.toggle('firstconfig')
    expect(h.isOpen.value).toBe(false)
    expect(h.history.value).toEqual([])
  })

  it('close clears the history so the next toggle starts fresh', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.toggle('firstconfig')
    h.close()
    expect(h.isOpen.value).toBe(false)
    expect(h.history.value).toEqual([])
    /* Toggle again → refetch + fresh history. */
    mockFetchOk('a-fresh')
    await h.toggle('firstconfig')
    expect(fetchMock).toHaveBeenCalledTimes(2)
    expect(h.history.value).toHaveLength(1)
    expect(h.history.value[0].label).toBe('a-fresh')
  })

  it('encodes path SEGMENTS (not the slash) in the fetch URL', async () => {
    /* The C-side `page_markdown` handler routes on the URL path
     * — `%2F` would NOT be decoded back into a path separator,
     * so the literal `/` between segments must be preserved.
     * Per-segment encodeURIComponent escapes everything else
     * (spaces, etc.) without touching the slash. */
    const h = await importHelp()
    mockFetchOk('x')
    await h.open('class/setup wizard')
    expect(fetchMock).toHaveBeenCalledWith('/markdown/class/setup%20wizard')
  })
})

describe('useHelp — navigation (navigateTo / back / goTo)', () => {
  it('navigateTo pushes a new entry onto the history', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('firstconfig')
    mockFetchOk('b')
    await h.navigateTo('class/mpegts_service')
    expect(h.history.value).toHaveLength(2)
    expect(h.history.value[1].page).toBe('class/mpegts_service')
    expect(h.currentPage.value).toBe('class/mpegts_service')
  })

  it('navigateTo to the current page is a no-op (no second fetch, no duplicate entry)', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('firstconfig')
    /* No mockFetchOk for this one — verifies fetch never runs. */
    await h.navigateTo('firstconfig')
    expect(fetchMock).toHaveBeenCalledTimes(1)
    expect(h.history.value).toHaveLength(1)
  })

  it('back pops the top entry', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('firstconfig')
    mockFetchOk('b')
    await h.navigateTo('class/x')
    h.back()
    expect(h.history.value).toHaveLength(1)
    expect(h.currentPage.value).toBe('firstconfig')
  })

  it('back is a no-op at the root of the stack', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('firstconfig')
    h.back()
    expect(h.history.value).toHaveLength(1)
    expect(h.isOpen.value).toBe(true)
  })

  it('back walks the stack one entry at a time across multiple pops', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('firstconfig')
    mockFetchOk('b')
    await h.navigateTo('class/x')
    mockFetchOk('c')
    await h.navigateTo('class/y')
    expect(h.history.value).toHaveLength(3)
    h.back()
    expect(h.currentPage.value).toBe('class/x')
    h.back()
    expect(h.currentPage.value).toBe('firstconfig')
    h.back() // no-op at root
    expect(h.currentPage.value).toBe('firstconfig')
  })

  it('goTo(index) truncates the stack to length index+1', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    mockFetchOk('b')
    await h.navigateTo('p2')
    mockFetchOk('c')
    await h.navigateTo('p3')
    h.goTo(0)
    expect(h.history.value).toHaveLength(1)
    expect(h.currentPage.value).toBe('p1')
  })

  it('goTo(top-of-stack) is a no-op (already there)', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    mockFetchOk('b')
    await h.navigateTo('p2')
    h.goTo(1)
    expect(h.history.value).toHaveLength(2)
  })

  it('goTo with out-of-range index is silently ignored', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    h.goTo(-1)
    h.goTo(99)
    expect(h.history.value).toHaveLength(1)
  })

  it('history entries cache the rendered HTML — back uses the cache, no refetch', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    mockFetchOk('b')
    await h.navigateTo('p2')
    h.back()
    /* `html` now reads the cached entry from p1 — no new fetch. */
    expect(fetchMock).toHaveBeenCalledTimes(2)
    expect(h.html.value).toContain('<RM>a</RM>')
  })
})

describe('useHelp — render pipeline', () => {
  it('runs marked → rewriteStaticUrls → addExternalLinkAttrs in order', async () => {
    const h = await importHelp()
    mockFetchOk('raw')
    await h.toggle('p')
    /* The mocked helpers wrap their input with distinct tags so
     * the nesting order proves the pipeline composition. */
    expect(h.html.value).toBe('<EXT><RW><h1>raw</h1><RM>raw</RM></RW></EXT>')
  })

  it('label is extracted from the rendered HTML, falling back to the page id', async () => {
    const h = await importHelp()
    /* The mock's extractFirstHeading reads the inner <RM> text;
     * here we drive it through `open()` and verify the label
     * ends up on the history entry. */
    mockFetchOk('Welcome')
    await h.open('firstconfig')
    expect(h.history.value[0].label).toBe('Welcome')
  })
})

describe('useHelp — errors', () => {
  it('fetch HTTP error keeps the dock open with an error message', async () => {
    const h = await importHelp()
    mockFetchFail(404)
    await h.toggle('missing')
    expect(h.error.value).toBe('HTTP 404')
    expect(h.isOpen.value).toBe(true)
    expect(h.history.value).toEqual([])
    expect(h.loading.value).toBe(false)
  })

  it('network rejection surfaces as an Error message', async () => {
    const h = await importHelp()
    fetchMock.mockRejectedValueOnce(new Error('network down'))
    await h.toggle('p')
    expect(h.error.value).toBe('network down')
  })

  it('navigateTo error pushes a synthetic error entry (visible feedback + working Back)', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    fetchMock.mockRejectedValueOnce(new Error('nope'))
    await h.navigateTo('p2')
    expect(h.error.value).toBe('nope')
    /* History grew — the synthetic entry takes the top slot so
     * the dock body switches to "couldn't load that page" rather
     * than silently leaving the prior content on screen. */
    expect(h.history.value).toHaveLength(2)
    expect(h.currentPage.value).toBe('p2')
    expect(h.html.value).toContain('Could not load this help page')
    expect(h.html.value).toContain('nope')
  })

  it('navigateTo error uses labelHint for the crumb when provided', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    fetchMock.mockRejectedValueOnce(new Error('boom'))
    await h.navigateTo('class/mpegts_service', 'Services')
    expect(h.history.value[1].label).toBe('Services')
  })

  it('navigateTo error falls back to the page id when no labelHint', async () => {
    const h = await importHelp()
    mockFetchOk('a')
    await h.open('p1')
    fetchMock.mockRejectedValueOnce(new Error('boom'))
    await h.navigateTo('class/mpegts_service')
    expect(h.history.value[1].label).toBe('mpegts_service')
  })

  it('Back after a nav error returns to the previous successful page', async () => {
    const h = await importHelp()
    mockFetchOk('p1-body')
    await h.open('p1')
    fetchMock.mockRejectedValueOnce(new Error('404'))
    await h.navigateTo('p2')
    expect(h.currentPage.value).toBe('p2')
    h.back()
    expect(h.currentPage.value).toBe('p1')
    expect(h.html.value).toContain('p1-body')
  })

  it('clearing via close() drops a sticky error from a prior session', async () => {
    const h = await importHelp()
    mockFetchFail(500)
    await h.toggle('p')
    expect(h.error.value).toBe('HTTP 500')
    h.close()
    expect(h.error.value).toBeNull()
  })
})

describe('useHelp — table of contents', () => {
  it('loadToc fetches /markdown/toc and caches the rendered html', async () => {
    const h = await importHelp()
    mockFetchOk('toc-index')
    await h.loadToc()
    expect(fetchMock).toHaveBeenCalledWith('/markdown/toc')
    expect(h.tocHtml.value).toBe('<EXT><RW><h1>toc-index</h1><RM>toc-index</RM></RW></EXT>')
    expect(h.tocLoading.value).toBe(false)
    expect(h.tocError.value).toBeNull()
  })

  it('loadToc is idempotent — a cached TOC is not refetched', async () => {
    const h = await importHelp()
    mockFetchOk('idx')
    await h.loadToc()
    await h.loadToc()
    expect(fetchMock).toHaveBeenCalledTimes(1)
  })

  it('loadToc surfaces a fetch error in tocError', async () => {
    const h = await importHelp()
    mockFetchFail(404)
    await h.loadToc()
    expect(h.tocError.value).toBe('HTTP 404')
    expect(h.tocHtml.value).toBeNull()
  })

  it('toggleToc flips the drawer and lazy-loads the TOC on first open', async () => {
    const h = await importHelp()
    mockFetchOk('idx')
    expect(h.tocOpen.value).toBe(false)
    h.toggleToc()
    expect(h.tocOpen.value).toBe(true)
    await vi.waitFor(() => expect(fetchMock).toHaveBeenCalledWith('/markdown/toc'))
    h.toggleToc()
    expect(h.tocOpen.value).toBe(false)
  })

  it('closeToc shuts the drawer', async () => {
    const h = await importHelp()
    mockFetchOk('idx')
    h.toggleToc()
    expect(h.tocOpen.value).toBe(true)
    h.closeToc()
    expect(h.tocOpen.value).toBe(false)
  })

  it('close() shuts the drawer but keeps the cached TOC', async () => {
    const h = await importHelp()
    mockFetchOk('idx')
    await h.loadToc()
    h.toggleToc()
    expect(h.tocOpen.value).toBe(true)
    h.close()
    expect(h.tocOpen.value).toBe(false)
    /* Reopening costs no refetch — tocHtml stays cached. */
    await h.loadToc()
    expect(fetchMock).toHaveBeenCalledTimes(1)
  })
})

describe('useHelp — singleton identity', () => {
  it('multiple useHelp() calls in the same module instance share refs', async () => {
    const { useHelp } = await import('../useHelp')
    const a = useHelp()
    const b = useHelp()
    expect(a.isOpen).toBe(b.isOpen)
    expect(a.history).toBe(b.history)
  })

  it('a fresh import (via vi.resetModules) yields a NEW singleton', async () => {
    const h1 = await importHelp()
    const h2 = await importHelp()
    expect(h1.isOpen).not.toBe(h2.isOpen)
  })
})
