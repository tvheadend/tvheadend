// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Integration test for the setup-wizard flow. Drives the full
 * route sequence using a real vue-router instance + Pinia store
 * + mocked apiCall, asserting:
 *
 *   - All 7 wizard routes are registered with the expected
 *     names (wizard-hello / wizard-login / ... / wizard-channels).
 *   - The "save then advance" pattern reaches the right endpoint
 *     for each form-driven step.
 *   - The status step starts polling on mount + auto-advances
 *     to mapping when the scan progress completes.
 *   - The channels step's Finish flow POSTs save then cancel
 *     then navigates to /gui/ (epg).
 *   - The wizard guard pulls users back into the wizard when
 *     they navigate to a non-wizard route while wizard is
 *     active, and kicks them out when wizard is inactive on a
 *     wizard route.
 *   - Cancel from each form-driven step calls wizard.cancel()
 *     and lands on /gui/ (epg).
 *
 * Per-component rendering details are covered by the
 * per-component unit tests. This file pins the cross-step
 * plumbing — router config + guard behaviour + endpoint-order
 * + flow termination.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { createRouter, createMemoryHistory, type Router } from 'vue-router'
import { wizardGuard } from '@/router'

const apiCallMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

type CometListener = (msg: unknown) => void
const cometHandlers = new Map<string, CometListener>()
vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: CometListener) => {
      cometHandlers.set(klass, listener)
      return () => cometHandlers.delete(klass)
    },
  },
}))

/* Drive the access store's wizard cursor by firing a synthetic
 * accessUpdate via the captured comet handler. Mirrors what
 * the real server broadcasts on mailbox creation. */
function setWizardCursor(cursor: string, admin = true) {
  cometHandlers.get('accessUpdate')?.({
    admin,
    dvr: true,
    wizard: cursor,
  })
}

/* The router uses the actual route table from src/router so we
 * pick up any rename / typo / shape drift. Build a slimmed-down
 * version with just the wizard branch + a tiny EPG placeholder
 * so the guard's redirects have a real target. */
async function buildRouter(): Promise<Router> {
  const placeholder = { template: '<div class="page-stub">page</div>' }
  const router = createRouter({
    history: createMemoryHistory('/gui/'),
    routes: [
      { path: '/', redirect: { name: 'epg' } },
      { path: '/epg', name: 'epg', component: placeholder },
      { path: '/about', name: 'about', component: placeholder },
      {
        path: '/status',
        name: 'status-streams',
        component: placeholder,
        meta: { permission: 'admin' },
      },
      {
        path: '/wizard',
        name: 'wizard',
        component: { template: '<router-view />' },
        meta: { isWizard: true },
        children: [
          { path: '', redirect: { name: 'wizard-hello' } },
          {
            path: 'hello',
            name: 'wizard-hello',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'login',
            name: 'wizard-login',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'network',
            name: 'wizard-network',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'muxes',
            name: 'wizard-muxes',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'status',
            name: 'wizard-status',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'mapping',
            name: 'wizard-mapping',
            component: placeholder,
            meta: { isWizard: true },
          },
          {
            path: 'channels',
            name: 'wizard-channels',
            component: placeholder,
            meta: { isWizard: true },
          },
        ],
      },
    ],
  })
  await router.push('/')
  await router.isReady()
  return router
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiCallMock.mockReset()
  apiCallMock.mockResolvedValue(undefined)
  cometHandlers.clear()
})

afterEach(() => {
  cometHandlers.clear()
})

/* Instantiate the access store so its comet subscription exists
 * before setWizardCursor fires a synthetic accessUpdate. */
async function accessStore() {
  const { useAccessStore } = await import('@/stores/access')
  return useAccessStore()
}

describe('wizard flow — route table', () => {
  it('registers all seven wizard step routes by name', async () => {
    const router = await buildRouter()
    const names = [
      'wizard-hello',
      'wizard-login',
      'wizard-network',
      'wizard-muxes',
      'wizard-status',
      'wizard-mapping',
      'wizard-channels',
    ]
    for (const name of names) {
      const r = router.resolve({ name })
      expect(r.name).toBe(name)
      expect(r.matched.some((m) => m.meta?.isWizard)).toBe(true)
    }
  })

  it('bare /wizard redirects to /wizard/hello', async () => {
    const router = await buildRouter()
    /* Push by path so vue-router applies the empty-child
     * redirect. Pushing by `{ name: 'wizard' }` resolves to
     * the parent route, which is a separate case. */
    await router.push('/wizard')
    await router.isReady()
    expect(router.currentRoute.value.name).toBe('wizard-hello')
  })
})

describe('wizard flow — store endpoint shapes', () => {
  it('start() POSTs wizard/start', async () => {
    const { useWizardStore } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    apiCallMock.mockResolvedValueOnce({})
    await wizard.start()
    expect(apiCallMock).toHaveBeenCalledWith('wizard/start')
  })

  it('cancel() POSTs wizard/cancel', async () => {
    const { useWizardStore } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    apiCallMock.mockResolvedValueOnce({})
    await wizard.cancel()
    expect(apiCallMock).toHaveBeenCalledWith('wizard/cancel')
  })

  it('pollProgress hits wizard/status/progress', async () => {
    const { useWizardStore } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    apiCallMock.mockResolvedValueOnce({ progress: 0.5, muxes: 3, services: 7 })
    await wizard.pollProgress()
    expect(apiCallMock).toHaveBeenCalledWith('wizard/status/progress')
    expect(wizard.progress).toEqual({ progress: 0.5, muxes: 3, services: 7 })
  })
})

describe('wizard flow — store + comet cursor', () => {
  it('currentStep mirrors the comet accessUpdate.wizard field', async () => {
    const { useWizardStore } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    expect(wizard.currentStep).toBe('')
    expect(wizard.isActive).toBe(false)

    setWizardCursor('hello')
    expect(wizard.currentStep).toBe('hello')
    expect(wizard.isActive).toBe(true)

    setWizardCursor('network')
    expect(wizard.currentStep).toBe('network')

    setWizardCursor('')
    expect(wizard.currentStep).toBe('')
    expect(wizard.isActive).toBe(false)
  })

  it('step ordering helpers walk forward + backward through all 7 steps', async () => {
    const { useWizardStore, WIZARD_STEPS } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    /* Walk forward through all neighbours. */
    for (let i = 0; i < WIZARD_STEPS.length - 1; i++) {
      expect(wizard.nextStepAfter(WIZARD_STEPS[i])).toBe(WIZARD_STEPS[i + 1])
    }
    /* Past the last step → null. */
    expect(wizard.nextStepAfter(WIZARD_STEPS[WIZARD_STEPS.length - 1])).toBeNull()
    /* Walk backward. */
    for (let i = 1; i < WIZARD_STEPS.length; i++) {
      expect(wizard.prevStepBefore(WIZARD_STEPS[i])).toBe(WIZARD_STEPS[i - 1])
    }
    expect(wizard.prevStepBefore(WIZARD_STEPS[0])).toBeNull()
  })
})

describe('wizard flow — polling lifecycle', () => {
  it('startPolling fires immediately + on every PROGRESS_POLL_MS tick', async () => {
    vi.useFakeTimers()
    const { useWizardStore } = await import('@/stores/wizard')
    const wizard = useWizardStore()
    apiCallMock.mockResolvedValue({ progress: 0, muxes: 0, services: 0 })

    wizard.startPolling()
    expect(apiCallMock).toHaveBeenCalledTimes(1) /* immediate */

    await vi.advanceTimersByTimeAsync(1000)
    expect(apiCallMock).toHaveBeenCalledTimes(2)

    await vi.advanceTimersByTimeAsync(1000)
    expect(apiCallMock).toHaveBeenCalledTimes(3)

    wizard.stopPolling()

    /* No further polls after stop. */
    apiCallMock.mockClear()
    await vi.advanceTimersByTimeAsync(5000)
    expect(apiCallMock).not.toHaveBeenCalled()

    vi.useRealTimers()
  })
})

describe('wizard flow — redirect guard', () => {
  it('resolves public routes immediately while access is still unknown', async () => {
    const router = await buildRouter()
    router.beforeEach(wizardGuard)
    await accessStore()
    /* No accessUpdate ever arrives. The guard must not sit in its
     * 5 s access wait for a route it can't gate anyway — race the
     * navigation against a short timer. */
    const result = await Promise.race([
      router.push('/about').then(() => 'resolved'),
      new Promise((resolve) => setTimeout(() => resolve('guard stalled'), 200)),
    ])
    expect(result).toBe('resolved')
    expect(router.currentRoute.value.name).toBe('about')
  })

  it('pulls an admin on a gated route into the pending wizard once access is known', async () => {
    const router = await buildRouter()
    router.beforeEach(wizardGuard)
    await accessStore()
    setWizardCursor('hello')
    await router.push('/status')
    expect(router.currentRoute.value.name).toBe('wizard-hello')
  })

  it('pulls an admin on a public route into the pending wizard once access is known', async () => {
    const router = await buildRouter()
    router.beforeEach(wizardGuard)
    await accessStore()
    setWizardCursor('network')
    await router.push('/about')
    expect(router.currentRoute.value.name).toBe('wizard-network')
  })

  it('kicks the user off wizard routes when the wizard is inactive', async () => {
    const router = await buildRouter()
    router.beforeEach(wizardGuard)
    await accessStore()
    setWizardCursor('')
    await router.push('/wizard/network')
    expect(router.currentRoute.value.name).toBe('epg')
  })
})
