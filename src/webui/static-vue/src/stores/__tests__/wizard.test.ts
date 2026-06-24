// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Wizard store unit tests. Pins:
 *   - Step ordering helpers (next/prev)
 *   - Lifecycle endpoints (start, cancel POST shape)
 *   - Progress polling (start/stop/clearInterval, 1 Hz cadence)
 *   - currentStep mirrors access.data.wizard
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { setActivePinia, createPinia } from 'pinia'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* The wizard store reads access.data.wizard via the access store.
 * We mock the access store's Comet subscription so tests can drive
 * it directly via setActivePinia + the access store's data ref. */
type Listener = (msg: unknown) => void
const cometHandlers = new Map<string, Listener>()

vi.mock('@/api/comet', () => ({
  cometClient: {
    on: (klass: string, listener: Listener) => {
      cometHandlers.set(klass, listener)
      return () => cometHandlers.delete(klass)
    },
  },
}))

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
  cometHandlers.clear()
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
})

async function importStores() {
  vi.resetModules()
  const wizardMod = await import('../wizard')
  const accessMod = await import('../access')
  return { wizard: wizardMod.useWizardStore(), access: accessMod.useAccessStore() }
}

describe('wizard store — step ordering', () => {
  it('exports the canonical seven-step order', async () => {
    const { WIZARD_STEPS } = await import('../wizard')
    expect(WIZARD_STEPS).toEqual([
      'hello',
      'login',
      'network',
      'muxes',
      'status',
      'mapping',
      'channels',
    ])
  })

  it('nextStepAfter returns the right next step', async () => {
    const { wizard } = await importStores()
    expect(wizard.nextStepAfter('hello')).toBe('login')
    expect(wizard.nextStepAfter('login')).toBe('network')
    expect(wizard.nextStepAfter('mapping')).toBe('channels')
  })

  it('nextStepAfter returns null past the last step', async () => {
    const { wizard } = await importStores()
    expect(wizard.nextStepAfter('channels')).toBeNull()
  })

  it('nextStepAfter returns null for unknown step names', async () => {
    const { wizard } = await importStores()
    expect(wizard.nextStepAfter('made-up')).toBeNull()
  })

  it('prevStepBefore returns the right previous step', async () => {
    const { wizard } = await importStores()
    expect(wizard.prevStepBefore('login')).toBe('hello')
    expect(wizard.prevStepBefore('channels')).toBe('mapping')
  })

  it('prevStepBefore returns null at hello (or earlier)', async () => {
    const { wizard } = await importStores()
    expect(wizard.prevStepBefore('hello')).toBeNull()
    expect(wizard.prevStepBefore('made-up')).toBeNull()
  })
})

describe('wizard store — lifecycle endpoints', () => {
  it('start() POSTs to wizard/start', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValueOnce({})
    await wizard.start()
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('wizard/start')
  })

  it('cancel() POSTs to wizard/cancel', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValueOnce({})
    await wizard.cancel()
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(apiMock.mock.calls[0][0]).toBe('wizard/cancel')
  })
})

describe('wizard store — currentStep mirrors access.data.wizard', () => {
  it('returns empty string when access not loaded', async () => {
    const { wizard } = await importStores()
    expect(wizard.currentStep).toBe('')
    expect(wizard.isActive).toBe(false)
  })

  it('reflects the wizard field once accessUpdate arrives', async () => {
    const { wizard } = await importStores()
    /* Fire a synthetic accessUpdate with the wizard field set. */
    const handler = cometHandlers.get('accessUpdate')
    expect(handler).toBeDefined()
    handler?.({ admin: true, dvr: true, wizard: 'login' })
    expect(wizard.currentStep).toBe('login')
    expect(wizard.isActive).toBe(true)
  })

  it('isActive is false when wizard field is empty string', async () => {
    const { wizard } = await importStores()
    cometHandlers.get('accessUpdate')?.({ admin: true, dvr: true, wizard: '' })
    expect(wizard.currentStep).toBe('')
    expect(wizard.isActive).toBe(false)
  })
})

describe('wizard store — progress polling', () => {
  it('startPolling fires immediately and on every 1 s tick', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValue({ progress: 0.1, muxes: 1, services: 0 })

    wizard.startPolling()
    expect(wizard.polling).toBe(true)
    expect(apiMock).toHaveBeenCalledTimes(1) /* immediate fire */
    expect(apiMock.mock.calls[0][0]).toBe('wizard/status/progress')

    /* Advance 1 s — second poll. */
    await vi.advanceTimersByTimeAsync(1000)
    expect(apiMock).toHaveBeenCalledTimes(2)

    /* Advance another 1 s — third poll. */
    await vi.advanceTimersByTimeAsync(1000)
    expect(apiMock).toHaveBeenCalledTimes(3)

    wizard.stopPolling()
  })

  it('stopPolling clears the interval and flips polling to false', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValue({ progress: 0.5, muxes: 5, services: 12 })

    wizard.startPolling()
    expect(wizard.polling).toBe(true)
    wizard.stopPolling()
    expect(wizard.polling).toBe(false)

    /* Subsequent timer ticks must NOT re-fire the poll. */
    apiMock.mockClear()
    await vi.advanceTimersByTimeAsync(5000)
    expect(apiMock).not.toHaveBeenCalled()
  })

  it('startPolling is idempotent (second call is a no-op while polling)', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValue({ progress: 0, muxes: 0, services: 0 })

    wizard.startPolling()
    apiMock.mockClear()
    /* Second start while still polling — must NOT re-fire the
     * immediate poll, must NOT install a second interval. */
    wizard.startPolling()
    expect(apiMock).not.toHaveBeenCalled()

    await vi.advanceTimersByTimeAsync(1000)
    /* Exactly one poll on the tick (single interval). */
    expect(apiMock).toHaveBeenCalledTimes(1)

    wizard.stopPolling()
  })

  it('updates the progress ref with each successful poll', async () => {
    const { wizard } = await importStores()
    apiMock.mockResolvedValueOnce({ progress: 0.25, muxes: 3, services: 7 })
    await wizard.pollProgress()
    expect(wizard.progress).toEqual({ progress: 0.25, muxes: 3, services: 7 })
  })

  it('captures errors without throwing', async () => {
    const { wizard } = await importStores()
    apiMock.mockRejectedValueOnce(new Error('boom'))
    await wizard.pollProgress()
    expect(wizard.error).toBe('boom')
  })
})

