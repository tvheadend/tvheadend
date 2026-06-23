// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * HealthLine tests — the Server-tier disk-health widget: an
 * all-clear affirmation when healthy, a warning when storage runs
 * low, and the disk-usage bar.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { enableAutoUnmount, mount } from '@vue/test-utils'
import HealthLine from '../HealthLine.vue'

vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({
    t: (s: string, ...args: Array<string | number>) =>
      s.replace(/\{(\d+)\}/g, (m, i) => String(args[Number(i)] ?? m)),
  }),
}))

/* Controllable access-store disk figures — set per test before mount. */
const accessData: { freediskspace?: number; totaldiskspace?: number } = {}
let mockIsAdmin = false
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    data: accessData,
    has: (key: string) => (key === 'admin' ? mockIsAdmin : false),
  }),
}))

/* Controllable status/inputs entries — one row per tuner input;
 * `subs` is its subscription count (0 = an idle placeholder row). */
const statusEntries: Array<{ uuid: string; subs: number }> = []
vi.mock('@/stores/status', () => ({
  useStatusStore: () => ({
    entries: statusEntries,
    fetch: vi.fn(() => Promise.resolve()),
  }),
}))

const TIB = 1024 ** 4

enableAutoUnmount(afterEach)
beforeEach(() => {
  accessData.freediskspace = undefined
  accessData.totaldiskspace = undefined
  statusEntries.length = 0
  mockIsAdmin = false
})

/* Stub vue-router so <router-link> mounts without a real
 * router instance. The component renders as <a> with the
 * resolved target on the `data-to` attribute so tests can
 * verify the destination. */
const routerLinkStub = {
  name: 'RouterLink',
  props: ['to'],
  template: '<a :data-to="JSON.stringify(to)"><slot /></a>',
}
const globalMountOpts = {
  global: { stubs: { 'router-link': routerLinkStub } },
}

describe('HealthLine', () => {
  it('shows the all-clear affirmation on a healthy disk', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = TIB /* 50% free */
    const w = mount(HealthLine, globalMountOpts)
    expect(w.text()).toContain("Everything's running smoothly")
    expect(w.classes()).not.toContain('health-line--warn')
  })

  it('warns when free space is low', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = 0.05 * 2 * TIB /* 5% free */
    const w = mount(HealthLine, globalMountOpts)
    expect(w.text()).toContain('Storage is running low')
    expect(w.classes()).toContain('health-line--warn')
  })

  it('fills the disk bar by used proportion', () => {
    accessData.totaldiskspace = 1000
    accessData.freediskspace = 250 /* 75% used */
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__bar-fill').attributes('style')).toContain('width: 75%')
  })

  it('shows the free and total amounts', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = TIB
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__disk-text').text()).toContain('1.0 TiB free of 2.0 TiB')
  })

  it('hides the disk bar and stays calm when there is no disk data', () => {
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__bar').exists()).toBe(false)
    expect(w.classes()).not.toContain('health-line--warn')
  })

  it('sums subscriptions across tuners and ignores idle ones', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = TIB
    statusEntries.push(
      { uuid: 'tuner-a', subs: 2 } /* two channels off one transponder */,
      { uuid: 'tuner-b', subs: 1 },
      { uuid: 'tuner-idle', subs: 0 } /* idle "empty status" placeholder */,
    )
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__streams').text()).toBe('3 active streams')
  })

  it('uses the singular for a single active stream', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = TIB
    statusEntries.push({ uuid: 'tuner-a', subs: 1 })
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__streams').text()).toBe('1 active stream')
  })

  it('reads "Idle" when only idle tuners are present', () => {
    accessData.totaldiskspace = 2 * TIB
    accessData.freediskspace = TIB
    statusEntries.push({ uuid: 'tuner-idle', subs: 0 })
    const w = mount(HealthLine, globalMountOpts)
    expect(w.find('.health-line__streams').text()).toBe('Idle')
  })

  describe('active-streams link to status/subscriptions', () => {
    it('wraps the streams count in a router-link to status-subscriptions for admins with active streams', () => {
      accessData.totaldiskspace = 2 * TIB
      accessData.freediskspace = TIB
      mockIsAdmin = true
      statusEntries.push({ uuid: 'tuner-a', subs: 3 })
      const w = mount(HealthLine, globalMountOpts)
      const link = w.find('.health-line__streams-link')
      expect(link.exists()).toBe(true)
      expect(link.attributes('data-to')).toContain('status-subscriptions')
      expect(link.text()).toBe('3 active streams')
    })

    it('renders the streams count as plain text (no link) for non-admin users', () => {
      accessData.totaldiskspace = 2 * TIB
      accessData.freediskspace = TIB
      mockIsAdmin = false
      statusEntries.push({ uuid: 'tuner-a', subs: 3 })
      const w = mount(HealthLine, globalMountOpts)
      expect(w.find('.health-line__streams-link').exists()).toBe(false)
      expect(w.find('.health-line__streams').text()).toBe('3 active streams')
    })

    it('renders Idle as plain text (no link) even for admins — nothing to drill into', () => {
      accessData.totaldiskspace = 2 * TIB
      accessData.freediskspace = TIB
      mockIsAdmin = true
      statusEntries.push({ uuid: 'tuner-idle', subs: 0 })
      const w = mount(HealthLine, globalMountOpts)
      expect(w.find('.health-line__streams-link').exists()).toBe(false)
      expect(w.find('.health-line__streams').text()).toBe('Idle')
    })
  })
})
