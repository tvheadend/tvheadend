// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * HomeSearchCard tests — drives the component's render branches
 * (initial / loading / error / results / empty / overflow) via a
 * mocked useEpgTitleSearch composable, and checks drawer wiring
 * via an EpgEventDrawer stub.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import { ref } from 'vue'
import HomeSearchCard from '../HomeSearchCard.vue'
import type { EpgEventDetail } from '@/views/epg/EpgEventDrawer.vue'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({
    t: (s: string, ...args: Array<string | number>) =>
      s.replace(/\{(\d+)\}/g, (_m, i) => String(args[Number(i)] ?? _m)),
  }),
}))

/* Controllable mock of the search composable. Tests mutate the refs
 * before mount to drive each render branch. */
const mockSearch = {
  query: ref(''),
  events: ref<EpgEventDetail[]>([]),
  totalCount: ref(0),
  loading: ref(false),
  error: ref<Error | null>(null),
  clear: vi.fn(),
}
vi.mock('@/composables/useEpgTitleSearch', () => ({
  useEpgTitleSearch: () => mockSearch,
}))

/* EpgEventDrawer stub — exposes the bound `event` prop via a
 * data attribute, and emits `close` from a small button so the
 * close-path can be exercised. */
const drawerStub = {
  props: ['event'] as const,
  emits: ['close'] as const,
  template: `
    <div
      class="drawer-stub"
      :data-event-id="event?.eventId ?? ''"
    >
      <button class="drawer-close" @click="$emit('close')" />
    </div>
  `,
}

function mountCard(): VueWrapper {
  return mount(HomeSearchCard, {
    global: { stubs: { EpgEventDrawer: drawerStub } },
  })
}

function makeEvent(over: Partial<EpgEventDetail> = {}): EpgEventDetail {
  return {
    eventId: 100,
    title: 'House M.D.',
    channelName: 'BBC One',
    start: 1700000000,
    ...over,
  }
}

beforeEach(() => {
  mockSearch.query.value = ''
  mockSearch.events.value = []
  mockSearch.totalCount.value = 0
  mockSearch.loading.value = false
  mockSearch.error.value = null
  mockSearch.clear.mockReset()
})

afterEach(() => {
  /* Mocks are scoped per-file; nothing global to restore. */
})

describe('HomeSearchCard — initial render', () => {
  it('renders the input but no status / results / overflow', () => {
    const w = mountCard()
    expect(w.find('input[type="search"]').exists()).toBe(true)
    expect(w.find('.home-search__results').exists()).toBe(false)
    expect(w.find('.home-search__overflow').exists()).toBe(false)
    expect(w.find('.home-search__status').exists()).toBe(false)
  })

  it('mounts an EpgEventDrawer stub with null event initially', () => {
    const w = mountCard()
    expect(w.find('.drawer-stub').attributes('data-event-id')).toBe('')
  })

  it('does not show the empty-state for short queries (<3 chars)', () => {
    mockSearch.query.value = 'ho'
    const w = mountCard()
    expect(w.find('.home-search__status').exists()).toBe(false)
    expect(w.find('.home-search__results').exists()).toBe(false)
  })
})

describe('HomeSearchCard — results card', () => {
  it('renders one row per event with title + channel + time', () => {
    mockSearch.events.value = [
      makeEvent({ eventId: 1, title: 'House M.D.', channelName: 'BBC One' }),
      makeEvent({ eventId: 2, title: 'House Hunters', channelName: 'HGTV' }),
    ]
    mockSearch.totalCount.value = 2
    const w = mountCard()

    const rows = w.findAll('.home-search__row')
    expect(rows).toHaveLength(2)
    expect(rows[0].find('.home-search__title').text()).toBe('House M.D.')
    expect(rows[0].find('.home-search__meta').text()).toContain('BBC One')
    expect(rows[1].find('.home-search__title').text()).toBe('House Hunters')
    expect(rows[1].find('.home-search__meta').text()).toContain('HGTV')
  })

  it('renders an "Untitled" fallback when an event has no title', () => {
    mockSearch.events.value = [makeEvent({ eventId: 1, title: undefined })]
    mockSearch.totalCount.value = 1
    const w = mountCard()
    expect(w.find('.home-search__title').text()).toBe('Untitled')
  })

  it('hides the overflow line when totalCount equals events.length', () => {
    mockSearch.events.value = [makeEvent({ eventId: 1 })]
    mockSearch.totalCount.value = 1
    const w = mountCard()
    expect(w.find('.home-search__overflow').exists()).toBe(false)
  })

  it('shows the overflow line when totalCount exceeds the returned count', () => {
    mockSearch.events.value = Array.from({ length: 100 }, (_, i) =>
      makeEvent({ eventId: i + 1, title: `Event ${i + 1}` }),
    )
    mockSearch.totalCount.value = 1247
    const w = mountCard()
    const overflow = w.find('.home-search__overflow')
    expect(overflow.exists()).toBe(true)
    expect(overflow.text()).toContain('100')
    expect(overflow.text()).toContain('1247')
  })
})

describe('HomeSearchCard — empty / loading / error states', () => {
  it('shows the empty-state when query is 3+ chars and no results', () => {
    mockSearch.query.value = 'xyz'
    const w = mountCard()
    expect(w.find('.home-search__status').text()).toContain(
      'No upcoming events',
    )
    expect(w.find('.home-search__results').exists()).toBe(false)
  })

  it('shows the loading line while loading=true (takes priority over events)', () => {
    mockSearch.loading.value = true
    mockSearch.events.value = [makeEvent()]
    mockSearch.totalCount.value = 1
    const w = mountCard()
    expect(w.find('.home-search__status').text()).toContain('Searching')
    /* Results card hidden while loading — loading takes precedence. */
    expect(w.find('.home-search__results').exists()).toBe(false)
  })

  it('shows the error line when error is set (takes priority over events)', () => {
    mockSearch.error.value = new Error('network down')
    mockSearch.events.value = [makeEvent()]
    const w = mountCard()
    const status = w.find('.home-search__status--error')
    expect(status.exists()).toBe(true)
    expect(status.text()).toContain('network down')
    expect(w.find('.home-search__results').exists()).toBe(false)
  })
})

describe('HomeSearchCard — drawer interaction', () => {
  it('clicking a row passes that event to the drawer', async () => {
    const ev = makeEvent({ eventId: 42, title: 'House M.D.' })
    mockSearch.events.value = [ev]
    mockSearch.totalCount.value = 1
    const w = mountCard()

    expect(w.find('.drawer-stub').attributes('data-event-id')).toBe('')

    await w.find('.home-search__row').trigger('click')
    await flushPromises()

    expect(w.find('.drawer-stub').attributes('data-event-id')).toBe('42')
  })

  it('row is a native <button> so the browser handles keyboard activation', async () => {
    /* The row used to be a `<li role="button">` with explicit
     * `@keydown.enter` / `@keydown.space` handlers — replaced by a
     * real `<button>` element so the browser's own button-
     * activation machinery (Enter and Space both dispatch a
     * `click`) takes over without us shipping duplicate handlers.
     * Asserting the element type is the right level of coverage:
     * the actual key→click conversion is a browser primitive, and
     * the click handler is already covered by the click test
     * above. */
    const ev = makeEvent({ eventId: 42 })
    mockSearch.events.value = [ev]
    mockSearch.totalCount.value = 1
    const w = mountCard()
    const row = w.find('.home-search__row')
    expect(row.exists()).toBe(true)
    expect(row.element.tagName).toBe('BUTTON')
    expect(row.attributes('type')).toBe('button')
  })

  it('drawer @close clears the event prop; results stay visible', async () => {
    const ev = makeEvent({ eventId: 42 })
    mockSearch.events.value = [ev]
    mockSearch.totalCount.value = 1
    const w = mountCard()

    await w.find('.home-search__row').trigger('click')
    await flushPromises()
    expect(w.find('.drawer-stub').attributes('data-event-id')).toBe('42')

    await w.find('.drawer-close').trigger('click')
    await flushPromises()

    expect(w.find('.drawer-stub').attributes('data-event-id')).toBe('')
    /* Results card still rendered — closing the drawer doesn't
     * clear the search state. */
    expect(w.findAll('.home-search__row')).toHaveLength(1)
  })
})
