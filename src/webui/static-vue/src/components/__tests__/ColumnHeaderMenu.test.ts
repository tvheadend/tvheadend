// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ColumnHeaderMenu unit tests. Covers the visible chrome
 * (kebab + active-state indicators), menu open/close, and the
 * setSort / hide / autoFit emits. Filter… is covered by an
 * integration-style test that mocks the underlying PrimeVue
 * filter button via DOM lookup.
 *
 * The menu panel is teleported to document.body to escape the
 * th's `overflow: hidden` clipping. Tests therefore query the
 * panel via `document.querySelector` rather than `wrapper.find`,
 * which only searches the component's own element tree.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import ColumnHeaderMenu from '../ColumnHeaderMenu.vue'
import { _resetColumnHeaderMenuStateForTests } from '../columnHeaderMenuState'

/* Track every mounted wrapper so afterEach can unmount them all
 * in one shot. Vue's unmount lifecycle removes any `<Teleport>`
 * content too — manual `document.body.removeChild` would leave
 * Vue's internal pointers stale and cause "insertBefore null"
 * errors on the next test's render. */
const wrappers: VueWrapper[] = []

beforeEach(() => {
  /* Module-level shared open state survives between tests by
   * default — reset so a previously-open instance from another
   * test doesn't leak into the next case. */
  _resetColumnHeaderMenuStateForTests()
})

afterEach(() => {
  wrappers.forEach((w) => w.unmount())
  wrappers.length = 0
})

function mountMenu(props: Partial<InstanceType<typeof ColumnHeaderMenu>['$props']>) {
  const w = mount(ColumnHeaderMenu, {
    attachTo: document.body,
    props: {
      field: 'username',
      label: 'Username',
      sortable: true,
      filterable: true,
      /* Default to all-supported in tests so existing assertions
       * about Sort items / Filter… / Hide / Reset stay valid.
       * Cases that exercise the gating override these per-test. */
      supportsSort: true,
      supportsFilter: true,
      supportsHide: true,
      supportsResetWidth: true,
      sortDir: null,
      filterActive: false,
      ...props,
    },
  })
  wrappers.push(w)
  return w
}

async function openMenu(w: VueWrapper) {
  await w.find('.column-header-menu__trigger').trigger('click')
  await flushPromises()
}

function findPanel(): HTMLElement | null {
  return document.querySelector('.column-header-menu__panel')
}

function findItems(): HTMLElement[] {
  const p = findPanel()
  return p ? Array.from(p.querySelectorAll<HTMLElement>('.settings-popover__option')) : []
}

async function clickItem(idx: number): Promise<void> {
  const items = findItems()
  items[idx].click()
  await flushPromises()
}

describe('ColumnHeaderMenu', () => {
  it('renders only the kebab when neither sorted nor filtered', () => {
    const w = mountMenu({})
    expect(w.find('.column-header-menu__trigger').exists()).toBe(true)
    expect(w.find('.column-header-menu__indicator--sort').exists()).toBe(false)
    expect(w.find('.column-header-menu__indicator--filter').exists()).toBe(false)
  })

  it('renders the ↑ sort indicator when sorted asc', () => {
    const w = mountMenu({ sortDir: 'asc' })
    const ind = w.find('.column-header-menu__indicator--sort')
    expect(ind.exists()).toBe(true)
    expect(ind.text()).toBe('↑')
  })

  it('renders the ↓ sort indicator when sorted desc', () => {
    const w = mountMenu({ sortDir: 'desc' })
    expect(w.find('.column-header-menu__indicator--sort').text()).toBe('↓')
  })

  it('renders the filter indicator when filterActive is true', () => {
    const w = mountMenu({ filterActive: true })
    expect(w.find('.column-header-menu__indicator--filter').exists()).toBe(true)
  })

  it('binds filterTitle as the funnel indicator native `title` for hover preview', () => {
    /* Mirrors the column header's server-description tooltip:
     * hovering the funnel surfaces what the active filter is
     * doing without a click. DataGrid composes the string (it
     * knows the matchMode); the menu only renders it. */
    const w = mountMenu({
      filterActive: true,
      filterTitle: 'Filter: contains "abc"',
    })
    const btn = w.find('.column-header-menu__indicator--filter')
    expect(btn.attributes('title')).toBe('Filter: contains "abc"')
  })

  it('omits the title attribute when filterTitle is empty', () => {
    /* Defensive: if the parent forgot to compose a string for
     * an active filter, we don't want the browser to show the
     * literal empty tooltip — `:title="filterTitle || undefined"`
     * suppresses the attribute entirely. */
    const w = mountMenu({ filterActive: true, filterTitle: '' })
    const btn = w.find('.column-header-menu__indicator--filter')
    expect(btn.attributes('title')).toBeUndefined()
  })

  it('renders BOTH sort and filter indicators when both active', () => {
    const w = mountMenu({ sortDir: 'asc', filterActive: true })
    expect(w.find('.column-header-menu__indicator--sort').exists()).toBe(true)
    expect(w.find('.column-header-menu__indicator--filter').exists()).toBe(true)
  })

  it('panel is closed by default', () => {
    mountMenu({})
    expect(findPanel()).toBeNull()
  })

  it('opens the panel on kebab click', async () => {
    const w = mountMenu({})
    await openMenu(w)
    expect(findPanel()).not.toBeNull()
  })

  it('closes the panel on second kebab click', async () => {
    const w = mountMenu({})
    await openMenu(w)
    await openMenu(w)
    expect(findPanel()).toBeNull()
  })

  it("emits setSort('asc') when clicking Sort A → Z", async () => {
    const w = mountMenu({})
    await openMenu(w)
    await clickItem(0)
    const events = w.emitted('setSort') ?? []
    expect(events).toHaveLength(1)
    expect(events[0]).toEqual(['username', 'asc'])
  })

  it("emits setSort('desc') when clicking Sort Z → A", async () => {
    const w = mountMenu({})
    await openMenu(w)
    await clickItem(1)
    const events = w.emitted('setSort') ?? []
    expect(events).toHaveLength(1)
    expect(events[0]).toEqual(['username', 'desc'])
  })

  it('does NOT show a "Clear sort" entry — always-defined-sort policy', async () => {
    /* The menu used to surface a "Clear sort" button when
     * sortDir was set; that's been dropped because every
     * grid now maintains an always-defined sort (Classic
     * parity). Confirm it's gone in both sorted and
     * unsorted states. */
    const noSort = mountMenu({})
    await openMenu(noSort)
    expect(findPanel()?.textContent ?? '').not.toContain('Clear sort')
    noSort.unmount()
    wrappers.splice(wrappers.indexOf(noSort), 1)

    const sorted = mountMenu({ sortDir: 'asc' })
    await openMenu(sorted)
    expect(findPanel()?.textContent ?? '').not.toContain('Clear sort')
  })

  it('emits hide when clicking Hide column', async () => {
    const w = mountMenu({})
    await openMenu(w)
    /* Sort A→Z, Sort Z→A, Filter…, Hide column, Auto-fit — index 3. */
    await clickItem(3)
    expect(w.emitted('hide')).toEqual([['username']])
  })

  it('emits resetWidth when clicking Reset to default width', async () => {
    const w = mountMenu({})
    await openMenu(w)
    await clickItem(4)
    expect(w.emitted('resetWidth')).toEqual([['username']])
  })

  it('inline ↑ click cycles asc → desc', async () => {
    const w = mountMenu({ sortDir: 'asc' })
    await w.find('.column-header-menu__indicator--sort').trigger('click')
    expect(w.emitted('setSort')).toEqual([['username', 'desc']])
  })

  it('inline ↓ click cycles desc → asc (no unsorted leg)', async () => {
    /* Classic parity: the inline indicator click cycles
     * asc → desc → asc, never landing on an unsorted state. */
    const w = mountMenu({ sortDir: 'desc' })
    await w.find('.column-header-menu__indicator--sort').trigger('click')
    expect(w.emitted('setSort')).toEqual([['username', 'asc']])
  })

  it('omits Sort entries when column is not sortable', async () => {
    const w = mountMenu({ sortable: false })
    await openMenu(w)
    const text = findPanel()?.textContent ?? ''
    expect(text).not.toContain('Sort A')
    expect(text).not.toContain('Sort Z')
    /* Hide / Reset width still present. */
    expect(text).toContain('Hide column')
    expect(text).toContain('Reset to default width')
  })

  /*
   * Sort-locked-by-group is the per-grid signal that the
   * backend can't accept a within-cluster sort (EPG Table is
   * the canonical case — `epg/events/grid` takes one sort key,
   * which the cluster order consumes). Multi-sort-capable
   * grids (every IdnodeGrid client-side grid) leave the prop
   * default false and keep Sort entries visible while grouped
   * so PrimeVue's auto-prepend of the group field as primary
   * + user's chosen sort as secondary still works.
   *
   * Regression coverage: easy to over-broaden the gate and
   * hide sort everywhere grouping is on, which silently breaks
   * within-cluster sort on every DVR / Channels / etc. grid.
   */
  it('hides Sort entries when grouped AND sortLockedByGroup is true', async () => {
    const w = mountMenu({
      sortable: true,
      groupActive: true,
      sortLockedByGroup: true,
    })
    await openMenu(w)
    const text = findPanel()?.textContent ?? ''
    expect(text).not.toContain('Sort A')
    expect(text).not.toContain('Sort Z')
  })

  it('keeps Sort entries when grouped but sortLockedByGroup is false (multi-sort capable backend)', async () => {
    const w = mountMenu({
      sortable: true,
      groupActive: true,
      sortLockedByGroup: false,
    })
    await openMenu(w)
    const text = findPanel()?.textContent ?? ''
    expect(text).toContain('Sort A')
    expect(text).toContain('Sort Z')
  })

  it('keeps Sort entries when sortLockedByGroup is true but no group is active', async () => {
    /* The lock only kicks in when both conditions hold —
     * setting the prop true on an ungrouped grid is fine
     * (covers the user toggling grouping off while the lock
     * config stays static). */
    const w = mountMenu({
      sortable: true,
      groupActive: false,
      sortLockedByGroup: true,
    })
    await openMenu(w)
    const text = findPanel()?.textContent ?? ''
    expect(text).toContain('Sort A')
    expect(text).toContain('Sort Z')
  })

  it('omits Filter entry when column is not filterable', async () => {
    const w = mountMenu({ filterable: false })
    await openMenu(w)
    expect(findPanel()?.textContent ?? '').not.toContain('Filter…')
  })

  it('Filter… click invokes the underlying PrimeVue filter button via DOM', async () => {
    /* Set up a fake th with a sibling .p-datatable-column-filter-button
     * so the menu's openPrimeFilter() can find it via closest('th') +
     * querySelector. */
    const th = document.createElement('th')
    document.body.appendChild(th)
    const fakeBtn = document.createElement('button')
    fakeBtn.className = 'p-datatable-column-filter-button'
    const clickSpy = vi.fn()
    fakeBtn.addEventListener('click', clickSpy)
    th.appendChild(fakeBtn)

    const w = mount(ColumnHeaderMenu, {
      attachTo: th,
      props: {
        field: 'username',
        label: 'Username',
        sortable: true,
        filterable: true,
        supportsSort: true,
        supportsFilter: true,
        supportsHide: true,
        supportsResetWidth: true,
        sortDir: null,
        filterActive: false,
      },
    })

    await openMenu(w)
    /* Sort A→Z, Sort Z→A, Filter…, Hide, Auto-fit — Filter… is index 2. */
    await clickItem(2)

    expect(clickSpy).toHaveBeenCalledTimes(1)

    w.unmount()
    th.remove()
  })

  it('closes the panel when the document is clicked outside', async () => {
    const w = mountMenu({})
    await openMenu(w)
    expect(findPanel()).not.toBeNull()
    /* Click on body itself (outside the trigger and outside the
     * panel). The doc-click handler should close the panel. */
    document.body.dispatchEvent(new MouseEvent('click', { bubbles: true }))
    await flushPromises()
    expect(findPanel()).toBeNull()
  })

  it('opening a second menu closes the first (only one open at a time)', async () => {
    const a = mountMenu({ field: 'username', label: 'Username' })
    const b = mountMenu({ field: 'email', label: 'Email' })

    await openMenu(a)
    expect(document.querySelectorAll('.column-header-menu__panel').length).toBe(1)

    /* Open the second menu — the first should auto-close so only
     * one panel exists in the DOM. */
    await openMenu(b)
    await flushPromises()
    expect(document.querySelectorAll('.column-header-menu__panel').length).toBe(1)
  })

  it('does NOT close on click inside the teleported panel', async () => {
    const w = mountMenu({})
    await openMenu(w)
    const panel = findPanel()
    expect(panel).not.toBeNull()
    /* Synthetic click bubbling from a non-button child of the
     * panel — should be treated as inside-the-menu so the
     * doc-click handler doesn't close it. */
    panel!.dispatchEvent(new MouseEvent('click', { bubbles: true }))
    await flushPromises()
    expect(findPanel()).not.toBeNull()
  })

  it('does not render the kebab when no actions are supported', () => {
    const w = mountMenu({
      sortable: true,
      filterable: true,
      supportsSort: false,
      supportsFilter: false,
      supportsHide: false,
      supportsResetWidth: false,
    })
    expect(w.find('.column-header-menu__trigger').exists()).toBe(false)
    expect(w.find('.column-header-menu').exists()).toBe(false)
  })

  it('hides Sort items when supportsSort is false', async () => {
    const w = mountMenu({ supportsSort: false })
    await openMenu(w)
    const text = findPanel()?.textContent ?? ''
    expect(text).not.toContain('Sort A')
    expect(text).not.toContain('Sort Z')
    /* Other items still render. */
    expect(text).toContain('Filter…')
    expect(text).toContain('Hide column')
  })

  it('hides Filter… when supportsFilter is false', async () => {
    const w = mountMenu({ supportsFilter: false })
    await openMenu(w)
    expect(findPanel()?.textContent ?? '').not.toContain('Filter…')
  })

  it('hides Hide column when supportsHide is false', async () => {
    const w = mountMenu({ supportsHide: false })
    await openMenu(w)
    expect(findPanel()?.textContent ?? '').not.toContain('Hide column')
  })

  it('hides Reset to default width when supportsResetWidth is false', async () => {
    const w = mountMenu({ supportsResetWidth: false })
    await openMenu(w)
    expect(findPanel()?.textContent ?? '').not.toContain('Reset to default width')
  })

  it('closes the panel on Escape', async () => {
    const w = mountMenu({})
    await openMenu(w)
    expect(findPanel()).not.toBeNull()
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }))
    await flushPromises()
    expect(findPanel()).toBeNull()
  })
})
