// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * HomeView tests — the role-aware dashboard renders the right card
 * tiers for a given install state x capabilities.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { ref } from 'vue'
import { enableAutoUnmount, flushPromises, mount, type VueWrapper } from '@vue/test-utils'
import { createMemoryHistory, createRouter } from 'vue-router'
import HomeView from '../HomeView.vue'
import type { HomeCapabilities, InstallState } from '@/composables/useHomeState'

/* Pure i18n stub — single substitution helper reused for both the
 * static `t` export (consumed by `actionHandlers.ts`) and the
 * reactive `useI18n().t` (consumed by setup contexts). vi.mock is
 * hoisted to the top of the file by Vitest, so any reference inside
 * the factory must go through vi.hoisted (which runs before mocks). */
const { tStub } = vi.hoisted(() => ({
  tStub: (s: string, ...args: Array<string | number>) =>
    s.replace(/\{(\d+)\}/g, (m, i) => String(args[Number(i)] ?? m)),
}))
vi.mock('@/composables/useI18n', () => ({
  useI18n: () => ({ t: tStub }),
  t: tStub,
}))

/* Controllable useHomeState — set state / caps per test, then mount. */
const mockHomeState = {
  state: ref<InstallState>('healthy'),
  capabilities: ref<HomeCapabilities>({ configure: true, record: true, watch: true }),
  channelCount: ref<number | null>(40),
  loading: ref(false),
  error: ref<Error | null>(null),
  refresh: vi.fn(),
}
vi.mock('@/composables/useHomeState', () => ({
  useHomeState: () => mockHomeState,
}))

const mockWizardStart = vi.fn()
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({ start: mockWizardStart }),
}))
/* Stable toast spies — the v1 actions (scan, EPG refresh) fire
 * positive feedback through success / info as well as the original
 * error path the wizard handler uses. Per-method refs let each test
 * assert on whatever the handler called without leaking between
 * runs (reset in beforeEach). */
const toastSuccess = vi.fn()
const toastError = vi.fn()
const toastInfo = vi.fn()
const toastWarn = vi.fn()
vi.mock('@/composables/useToastNotify', () => ({
  useToastNotify: () => ({
    success: toastSuccess,
    error: toastError,
    info: toastInfo,
    warn: toastWarn,
  }),
}))

/* Controllable setup-greeting flag — true stands in for "the
 * wizard just finished" (consumeSetupGreeting is read-and-clear). */
let mockGreeting = false
vi.mock('@/utils/setupGreeting', () => ({
  consumeSetupGreeting: () => mockGreeting,
}))

/* apiCall — HomeView probes the enabled-channel count for the
 * greeting; the rest of its data comes via the mocked useHomeState. */
const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Access store stub — `authMode` drives whether the Sign-in
 * guidance card renders. Default 'authenticated' so existing
 * role-aware tests (which already mock `caps`) don't see the
 * unrelated Sign-in card unless they explicitly opt in. */
const mockAuthMode: 'pre-auth' | 'noacl' | 'anonymous-admin' | 'anonymous' | 'authenticated' =
  'authenticated'
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    get authMode() {
      return mockAuthMode
    },
  }),
}))

/* Comet stub — only `reset()` is touched by the Sign-in action
 * path; nothing else in HomeView uses the client. Indirected
 * through a setter because vi.mock is hoisted above the spy
 * declaration. */
const cometResetMock = vi.fn()
vi.mock('@/api/comet', () => ({
  cometClient: { reset: (...args: unknown[]) => cometResetMock(...args) },
}))

const RouteStub = { template: '<div />' }
function makeRouter() {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/', name: 'home', component: RouteStub },
      { path: '/epg', name: 'epg', component: RouteStub },
      { path: '/dvr', name: 'dvr', component: RouteStub },
      { path: '/cfg-epg', name: 'config-channel-epg', component: RouteStub },
      { path: '/cfg-dvb', name: 'config-dvb-networks', component: RouteStub },
      { path: '/cfg-channels', name: 'config-channel-channels', component: RouteStub },
      { path: '/wizard/hello', name: 'wizard-hello', component: RouteStub },
    ],
  })
}

async function mountHome(): Promise<VueWrapper> {
  const router = makeRouter()
  router.push('/')
  await router.isReady()
  return mount(HomeView, {
    global: {
      plugins: [router],
      /* The TV-tier activity widgets fetch DVR data of their own —
       * stub them here; HomeView's test covers tier composition, the
       * widgets have their own tests. */
      stubs: {
        RecordingNow: true,
        ThisWeekStrip: true,
        RecentlyRecorded: true,
        HealthLine: true,
        HomeSearchCard: true,
      },
    },
  })
}

function cardTitles(wrapper: VueWrapper): string[] {
  return wrapper.findAll('.home-card__title').map((n) => n.text())
}
function tierTitles(wrapper: VueWrapper): string[] {
  return wrapper.findAll('.home__tier-title').map((n) => n.text())
}

enableAutoUnmount(afterEach)

/* Pre-flip the palette's seenPalette flag so the "Try the command
 * palette" discoverability tile doesn't appear in the matrix tests
 * below — they assert specific tier-card lists and the tile would
 * otherwise pollute every "healthy" case. The tile's own visibility
 * logic is asserted directly in homeCards.test.ts (the
 * discover-palette describe block). */
import { useCommandPalette } from '@/composables/useCommandPalette'

beforeEach(() => {
  mockHomeState.state.value = 'healthy'
  mockHomeState.capabilities.value = { configure: true, record: true, watch: true }
  mockHomeState.loading.value = false
  mockHomeState.error.value = null
  mockWizardStart.mockReset()
  mockGreeting = false
  apiMock.mockReset()
  apiMock.mockResolvedValue({ total: 0 })
  toastSuccess.mockReset()
  toastError.mockReset()
  toastInfo.mockReset()
  toastWarn.mockReset()
  /* Mark the palette as "already seen" so the discoverability
   * tile is suppressed by default in these matrix tests. */
  useCommandPalette().seenPalette.value = true
})

describe('HomeView — role-aware tiers', () => {
  it('fresh install, admin: the Set up live TV action', async () => {
    mockHomeState.state.value = 'fresh'
    const wrapper = await mountHome()
    expect(cardTitles(wrapper)).toEqual(['Set up live TV'])
  })

  it('fresh install, non-admin: the honest not-ready notice', async () => {
    mockHomeState.state.value = 'fresh'
    mockHomeState.capabilities.value = { configure: false, record: false, watch: true }
    const wrapper = await mountHome()
    expect(cardTitles(wrapper)).toEqual(['No channels yet'])
    expect(wrapper.text()).toContain('needs administrator access')
  })

  it('healthy install, admin: TV and Server tiers', async () => {
    const wrapper = await mountHome()
    expect(tierTitles(wrapper)).toEqual(['TV', 'Server'])
    expect(cardTitles(wrapper)).toEqual([
      'TV Guide',
      'Recordings',
      'Scan for channels',
      'Refresh TV guide',
      'Reorganize channels',
    ])
  })

  it('healthy install, viewer: only the TV Guide, no Server tier', async () => {
    mockHomeState.capabilities.value = { configure: false, record: false, watch: true }
    const wrapper = await mountHome()
    expect(tierTitles(wrapper)).toEqual(['TV'])
    expect(cardTitles(wrapper)).toEqual(['TV Guide'])
  })

  it('shows a loading state', async () => {
    mockHomeState.loading.value = true
    const wrapper = await mountHome()
    expect(wrapper.text()).toContain('Loading')
  })

  it('a start-wizard action card triggers wizard.start()', async () => {
    mockHomeState.state.value = 'fresh'
    const wrapper = await mountHome()
    await wrapper.find('button.home-card').trigger('click')
    expect(mockWizardStart).toHaveBeenCalled()
  })

  it('healthy install, recorder: the TV activity widgets are shown', async () => {
    const wrapper = await mountHome()
    expect(wrapper.find('.home__activity').exists()).toBe(true)
  })

  it('healthy install, viewer: no activity widgets (cannot record)', async () => {
    mockHomeState.capabilities.value = { configure: false, record: false, watch: true }
    const wrapper = await mountHome()
    expect(wrapper.find('.home__activity').exists()).toBe(false)
  })

  it('healthy install, admin: the Server health line is shown', async () => {
    const wrapper = await mountHome()
    expect(wrapper.find('.home__health').exists()).toBe(true)
  })

  it('healthy install, viewer: no Server health line (not an admin)', async () => {
    mockHomeState.capabilities.value = { configure: false, record: false, watch: true }
    const wrapper = await mountHome()
    expect(wrapper.find('.home__health').exists()).toBe(false)
  })

  it('fresh install, admin: no Server health line yet', async () => {
    mockHomeState.state.value = 'fresh'
    const wrapper = await mountHome()
    expect(wrapper.find('.home__health').exists()).toBe(false)
  })
})

describe('HomeView — setup-complete greeting', () => {
  it('shows the greeting with the enabled-channel count after the wizard', async () => {
    mockGreeting = true
    apiMock.mockResolvedValue({ total: 42 })
    const wrapper = await mountHome()
    await flushPromises()
    expect(wrapper.find('.home__greeting').exists()).toBe(true)
    expect(wrapper.find('.home__greeting-text').text()).toBe(
      'Setup complete — 42 channels ready',
    )
  })

  it('counts only enabled channels for the greeting', async () => {
    mockGreeting = true
    apiMock.mockResolvedValue({ total: 42 })
    await mountHome()
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith(
      'channel/grid',
      expect.objectContaining({
        filter: JSON.stringify([{ field: 'enabled', type: 'boolean', value: true }]),
      }),
    )
  })

  it('shows no greeting on a normal load', async () => {
    const wrapper = await mountHome()
    expect(wrapper.find('.home__greeting').exists()).toBe(false)
    /* No greeting → no channel-count probe. */
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('the greeting can be dismissed', async () => {
    mockGreeting = true
    apiMock.mockResolvedValue({ total: 42 })
    const wrapper = await mountHome()
    await flushPromises()
    await wrapper.find('.home__greeting-dismiss').trigger('click')
    expect(wrapper.find('.home__greeting').exists()).toBe(false)
  })
})

/* Helper to locate a specific action card by title. The healthy
 * admin layout has four cards in document order — TV Guide,
 * Recordings, Scan for channels, Refresh TV guide — but indexing
 * by title is robust to registry reordering. Hoisted to module
 * scope so the inner test functions don't reallocate it. */
function clickCard(wrapper: VueWrapper, title: string): Promise<void> {
  const cards = wrapper.findAll('button.home-card')
  const match = cards.find((c) => c.find('.home-card__title').text() === title)
  if (!match) throw new Error(`No action card titled "${title}"`)
  return match.trigger('click')
}

describe('HomeView — Server-tier one-click actions', () => {

  it('scan-channels: fetches enabled networks, POSTs scan, success toast names the count', async () => {
    /* Two-step api dance: grid → uuids, then scan. mockResolvedValueOnce
     * twice so the order is deterministic. */
    apiMock
      .mockResolvedValueOnce({
        entries: [{ uuid: 'net-1' }, { uuid: 'net-2' }],
      })
      .mockResolvedValueOnce({})
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Scan for channels')
    await flushPromises()

    /* Grid probe filters to enabled — disabled networks shouldn't
     * have their tuners spun up by a one-click action. */
    expect(apiMock).toHaveBeenNthCalledWith(
      1,
      'mpegts/network/grid',
      expect.objectContaining({
        filter: JSON.stringify([{ field: 'enabled', type: 'boolean', value: true }]),
      }),
    )
    expect(apiMock).toHaveBeenNthCalledWith(2, 'mpegts/network/scan', {
      uuid: JSON.stringify(['net-1', 'net-2']),
    })
    expect(toastSuccess).toHaveBeenCalledWith('Scan started on 2 networks.')
  })

  it('scan-channels: 1-network install uses the singular toast', async () => {
    apiMock
      .mockResolvedValueOnce({ entries: [{ uuid: 'only-net' }] })
      .mockResolvedValueOnce({})
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Scan for channels')
    await flushPromises()
    expect(toastSuccess).toHaveBeenCalledWith('Scan started on 1 network.')
  })

  it('scan-channels: empty network list → info toast, no scan POST', async () => {
    apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Scan for channels')
    await flushPromises()
    expect(toastInfo).toHaveBeenCalledWith('No enabled networks to scan.')
    /* Only the grid probe ran — no scan POST. */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock).toHaveBeenCalledWith(
      'mpegts/network/grid',
      expect.objectContaining({
        filter: JSON.stringify([{ field: 'enabled', type: 'boolean', value: true }]),
      }),
    )
  })

  it('scan-channels: api failure surfaces via error toast', async () => {
    apiMock.mockRejectedValueOnce(new Error('network down'))
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Scan for channels')
    await flushPromises()
    expect(toastError).toHaveBeenCalledWith(
      'network down',
      expect.objectContaining({ summary: 'Could not start the scan' }),
    )
  })

  it('refresh-epg: POSTs both internal AND ota grabbers, toasts success', async () => {
    apiMock.mockResolvedValue({})
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Refresh TV guide')
    await flushPromises()
    expect(apiMock).toHaveBeenCalledWith('epggrab/internal/rerun', { rerun: 1 })
    expect(apiMock).toHaveBeenCalledWith('epggrab/ota/trigger', { trigger: 1 })
    expect(toastSuccess).toHaveBeenCalledWith('TV guide refresh started.')
  })

  it('refresh-epg: success toast fires when only ONE of the two endpoints succeeds', async () => {
    /* Realistic mixed outcome — a server with no OTA grabbers
     * enabled returns an error from epggrab/ota/trigger but
     * internal/rerun succeeds. The user click should still feel
     * successful because the guide actually did refresh from the
     * internal grabbers. */
    apiMock
      .mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error('no ota muxes'))
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Refresh TV guide')
    await flushPromises()
    expect(toastSuccess).toHaveBeenCalledWith('TV guide refresh started.')
    expect(toastError).not.toHaveBeenCalled()
  })

  it('refresh-epg: error toast fires only when BOTH endpoints fail', async () => {
    apiMock
      .mockRejectedValueOnce(new Error('internal failed'))
      .mockRejectedValueOnce(new Error('ota failed'))
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Refresh TV guide')
    await flushPromises()
    expect(toastSuccess).not.toHaveBeenCalled()
    expect(toastError).toHaveBeenCalledWith(
      'internal failed',
      expect.objectContaining({ summary: 'Could not refresh the TV guide' }),
    )
  })

  it('refresh-epg: double-click while inflight coalesces to one click', async () => {
    /* Keep both calls pending so the second click hits the
     * inflight-guard branch. */
    let resolveInternal: (v: unknown) => void
    let resolveOta: (v: unknown) => void
    const internalCall = new Promise((r) => {
      resolveInternal = r
    })
    const otaCall = new Promise((r) => {
      resolveOta = r
    })
    apiMock.mockReturnValueOnce(internalCall).mockReturnValueOnce(otaCall)
    const wrapper = await mountHome()
    await clickCard(wrapper, 'Refresh TV guide')
    await clickCard(wrapper, 'Refresh TV guide')
    /* Two POSTs from the first click; the second click is
     * inflight-suppressed before it can fire either endpoint. */
    expect(apiMock).toHaveBeenCalledTimes(2)
    resolveInternal!({})
    resolveOta!({})
    await flushPromises()
    expect(toastSuccess).toHaveBeenCalledTimes(1)
  })

  it('refresh-epg: hidden in epg-missing for non-admin', async () => {
    mockHomeState.state.value = 'epg-missing'
    mockHomeState.capabilities.value = { configure: false, record: true, watch: true }
    const wrapper = await mountHome()
    expect(cardTitles(wrapper)).not.toContain('Refresh TV guide')
  })

  it('refresh-epg: visible in epg-missing for admin (most actionable state)', async () => {
    mockHomeState.state.value = 'epg-missing'
    const wrapper = await mountHome()
    expect(cardTitles(wrapper)).toContain('Refresh TV guide')
  })
})
