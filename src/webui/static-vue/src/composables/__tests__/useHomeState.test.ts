// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useHomeState tests — the install-state derivation (from the three
 * count probes + the setup wizard) and the capability mapping.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent } from 'vue'
import { flushPromises, mount } from '@vue/test-utils'
import { useHomeState, type UseHomeStateReturn } from '../useHomeState'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* Controllable access + wizard stubs — set before each mount. */
let mockAdmin = false
let mockDvr = false
let mockWizardActive = false
vi.mock('@/stores/access', () => ({
  useAccessStore: () => ({
    has: (k: string) => {
      if (k === 'admin') return mockAdmin
      if (k === 'dvr') return mockDvr
      return false
    },
  }),
}))
vi.mock('@/stores/wizard', () => ({
  useWizardStore: () => ({
    get isActive() {
      return mockWizardActive
    },
  }),
}))

/* Capture the recovery refetch so a test can fire it directly
 * (standing in for a Comet reconnect / long tab-away). */
let capturedRefetch: (() => void) | null = null
vi.mock('@/composables/useStaleDataRecovery', () => ({
  useStaleDataRecovery: (opts: { refetch: () => void }) => {
    capturedRefetch = opts.refetch
  },
}))

/* Answer the three count probes with the given row totals. */
function mockCounts(networks: number, channels: number, epg: number): void {
  apiMock.mockImplementation((endpoint: string) => {
    if (endpoint === 'mpegts/network/grid') return Promise.resolve({ total: networks })
    if (endpoint === 'channel/grid') return Promise.resolve({ total: channels })
    if (endpoint === 'epg/events/grid') return Promise.resolve({ totalCount: epg })
    return Promise.resolve({ total: 0 })
  })
}

/* Mount a throwaway component so the composable's onMounted fires. */
async function run(): Promise<UseHomeStateReturn> {
  let api!: UseHomeStateReturn
  mount(
    defineComponent({
      setup() {
        api = useHomeState()
        return () => null
      },
    }),
  )
  await flushPromises()
  return api
}

beforeEach(() => {
  apiMock.mockReset()
  /* Default to admin so the install-state / probe-shape tests
   * keep exercising the populated code path. The non-admin
   * skip-the-probes path has its own describe block below that
   * flips this to false. */
  mockAdmin = true
  mockDvr = false
  mockWizardActive = false
  capturedRefetch = null
})
afterEach(() => {
  apiMock.mockReset()
})

describe('useHomeState — install state', () => {
  it('fresh when there are no networks', async () => {
    mockCounts(0, 0, 0)
    expect((await run()).state.value).toBe('fresh')
  })

  it('fresh when the setup wizard is active, even with networks', async () => {
    mockWizardActive = true
    mockCounts(2, 50, 1000)
    expect((await run()).state.value).toBe('fresh')
  })

  it('channels-missing when networks exist but no channels', async () => {
    mockCounts(1, 0, 0)
    expect((await run()).state.value).toBe('channels-missing')
  })

  it('epg-missing when channels exist but no EPG events', async () => {
    mockCounts(1, 40, 0)
    expect((await run()).state.value).toBe('epg-missing')
  })

  it('healthy when channels and EPG are present', async () => {
    mockCounts(1, 40, 5000)
    expect((await run()).state.value).toBe('healthy')
  })
})

describe('useHomeState — capabilities', () => {
  it('maps admin / dvr to configure / record; watch is the baseline', async () => {
    mockAdmin = true
    mockDvr = true
    mockCounts(0, 0, 0)
    expect((await run()).capabilities.value).toEqual({
      configure: true,
      record: true,
      watch: true,
    })
  })

  it('a non-admin non-recorder still has the watch baseline', async () => {
    mockAdmin = false
    mockDvr = false
    mockCounts(0, 0, 0)
    expect((await run()).capabilities.value).toEqual({
      configure: false,
      record: false,
      watch: true,
    })
  })
})

describe('useHomeState — failure', () => {
  it('records the error and leaves loading false', async () => {
    apiMock.mockRejectedValue(new Error('network down'))
    const s = await run()
    expect(s.error.value).toBeInstanceOf(Error)
    expect(s.loading.value).toBe(false)
  })
})

describe('useHomeState — stale-data recovery', () => {
  it('re-runs the count probes and converges when recovery fires', async () => {
    mockCounts(1, 40, 5000)
    const s = await run()
    expect(s.state.value).toBe('healthy')
    /* Initial mount = the three probes. */
    expect(apiMock).toHaveBeenCalledTimes(3)

    /* Channels vanish while the tab was asleep; recovery re-probes. */
    mockCounts(1, 0, 0)
    expect(capturedRefetch).toBeTypeOf('function')
    capturedRefetch!()
    await flushPromises()

    expect(apiMock).toHaveBeenCalledTimes(6)
    expect(s.state.value).toBe('channels-missing')
  })
})

/*
 * Non-admin path — the three count probes hit admin-gated grid
 * endpoints, which return 401 + WWW-Authenticate to anonymous
 * users and make the browser pop its native Digest auth dialog
 * unprompted (the ExtJS UI deliberately doesn't trigger any auth
 * before the user clicks Login; the Vue dashboard must match).
 */
describe('useHomeState — non-admin user', () => {
  beforeEach(() => {
    mockAdmin = false
  })

  it('skips the count probes entirely', async () => {
    await run()
    expect(apiMock).not.toHaveBeenCalled()
  })

  it("defaults `state` to 'healthy' (not 'fresh') so non-admins don't see setup prompts", async () => {
    expect((await run()).state.value).toBe('healthy')
  })

  it('still skips probes even when a recovery refetch fires', async () => {
    await run()
    capturedRefetch?.()
    await flushPromises()
    expect(apiMock).not.toHaveBeenCalled()
  })
})
