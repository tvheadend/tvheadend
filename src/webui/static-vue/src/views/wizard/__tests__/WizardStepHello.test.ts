// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * WizardStepHello component test. Pins:
 *
 *   1. SAVE & NEXT path (`beforeNext` hook):
 *      - On loaded, captures the initial ui_lang.
 *      - On save, the beforeNext hook compares submitted
 *        ui_lang to that baseline; if different, refetches
 *        /locale.js + invalidates the wizard metadata cache.
 *        If unchanged, it does nothing.
 *      - Soft-refresh: never calls window.location.reload —
 *        relies on loadLocale to bump the reactive locale
 *        counter.
 *      - Failure of loadLocale is swallowed (non-fatal);
 *        navigation still proceeds.
 *
 *   2. LIVE-PREVIEW path (watcher on formRef.currentValues.ui_lang):
 *      - After loaded, a user-driven change to ui_lang triggers
 *        (after a 300 ms debounce) a save+refresh round-trip:
 *        apiCall('wizard/hello/save', ...), loadLocale, form.reload().
 *      - Initial-load population does NOT trigger.
 *      - Same-value re-pick after a refresh doesn't trigger.
 *      - Rapid sequential picks within the debounce window
 *        collapse to a single round-trip on the final value.
 *      - Errors are non-fatal (logged and swallowed).
 *      - Unmount during debounce cancels the pending timer.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, reactive, ref } from 'vue'
import { mount, flushPromises, type VueWrapper } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'

const loadLocaleMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/composables/useI18n', () => ({
  loadLocale: () => loadLocaleMock(),
  useI18n: () => ({ t: (s: string) => s, currentLang: () => '' }),
  t: (s: string) => s,
}))

const apiCallMock = vi.fn().mockResolvedValue(undefined)
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiCallMock(...args),
}))

/* Stub WizardStepGeneric. Exposes a `formRef` shape that
 * mirrors what IdnodeConfigForm exposes through
 * defineExpose, so the live-preview watcher in WizardStepHello
 * can observe currentValues changes + verify `reload()` is
 * invoked. */
type LoadedPayload = { id: string; value?: unknown }[]
const capturedProps = ref<{
  step?: string
  beforeNext?: (v: Record<string, unknown>) => Promise<void> | void
}>({})
const emitLoaded = ref<((p: LoadedPayload) => void) | null>(null)
const stubReloadMock = vi.fn().mockResolvedValue(undefined)
const stubCurrentValues = reactive<Record<string, unknown>>({})

vi.mock('../WizardStepGeneric.vue', () => ({
  default: defineComponent({
    name: 'WizardStepGeneric',
    props: {
      step: { type: String, default: undefined },
      beforeNext: { type: Function, default: undefined },
    },
    emits: ['loaded'],
    setup(props, { emit, expose }) {
      capturedProps.value = {
        step: props.step as string,
        beforeNext: props.beforeNext as
          | ((v: Record<string, unknown>) => Promise<void> | void)
          | undefined,
      }
      emitLoaded.value = (p: LoadedPayload) => emit('loaded', p)
      const formRef = {
        currentValues: stubCurrentValues,
        reload: stubReloadMock,
      }
      expose({ formRef })
      return () => h('div', { class: 'wizard-step-generic-stub' })
    },
  }),
}))

import WizardStepHello from '../WizardStepHello.vue'

/* Track every mounted wrapper so afterEach can unmount. Without
 * this, the previous test's `WizardStepHello` watcher stays
 * subscribed to the shared `stubCurrentValues` reactive object,
 * so the next test's `stubCurrentValues.ui_lang = ...` fires
 * watchers from every prior mount — apiCallMock counts
 * accumulate across tests. */
const wrappers: VueWrapper[] = []
function mountHello() {
  const w = mount(WizardStepHello)
  wrappers.push(w)
  return w
}

beforeEach(() => {
  setActivePinia(createPinia())
  loadLocaleMock.mockClear()
  loadLocaleMock.mockResolvedValue(undefined)
  apiCallMock.mockClear()
  apiCallMock.mockResolvedValue(undefined)
  stubReloadMock.mockClear()
  stubReloadMock.mockResolvedValue(undefined)
  for (const k of Object.keys(stubCurrentValues)) delete stubCurrentValues[k]
  capturedProps.value = {}
  emitLoaded.value = null
  vi.useFakeTimers()
})

afterEach(() => {
  vi.useRealTimers()
  while (wrappers.length) wrappers.pop()?.unmount()
})

describe('WizardStepHello — mounts WizardStepGeneric with hello step', () => {
  it('passes step="hello" + a beforeNext callback', () => {
    mountHello()
    expect(capturedProps.value.step).toBe('hello')
    expect(typeof capturedProps.value.beforeNext).toBe('function')
  })
})

describe('WizardStepHello — Save & Next (beforeNext) orchestration', () => {
  it('does nothing when ui_lang did not change', async () => {
    mountHello()
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()
    await capturedProps.value.beforeNext?.({ ui_lang: 'eng' })
    expect(loadLocaleMock).not.toHaveBeenCalled()
  })

  it('refetches /locale.js when ui_lang changed', async () => {
    mountHello()
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()
    await capturedProps.value.beforeNext?.({ ui_lang: 'deu' })
    expect(loadLocaleMock).toHaveBeenCalledTimes(1)
  })

  it('handles loadLocale failure non-fatally', async () => {
    loadLocaleMock.mockRejectedValueOnce(new Error('network down'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    mountHello()
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()
    await expect(
      capturedProps.value.beforeNext?.({ ui_lang: 'deu' }),
    ).resolves.toBeUndefined()
    expect(loadLocaleMock).toHaveBeenCalledTimes(1)
    warnSpy.mockRestore()
  })

  it('treats absent initial ui_lang as empty string', async () => {
    mountHello()
    emitLoaded.value?.([{ id: 'icon', value: '/img/hello.png' }])
    await flushPromises()
    await capturedProps.value.beforeNext?.({ ui_lang: 'eng' })
    expect(loadLocaleMock).toHaveBeenCalledTimes(1)
  })
})

describe('WizardStepHello — live-preview language refresh', () => {
  it('triggers save+reload after debounce on user-picked language change', async () => {
    mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()

    stubCurrentValues.ui_lang = 'deu'

    await vi.advanceTimersByTimeAsync(100)
    expect(apiCallMock).not.toHaveBeenCalled()
    expect(stubReloadMock).not.toHaveBeenCalled()

    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()

    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(apiCallMock.mock.calls[0][0]).toBe('wizard/hello/save')
    const body = apiCallMock.mock.calls[0][1] as { node: string }
    expect(JSON.parse(body.node)).toMatchObject({ ui_lang: 'deu' })

    expect(loadLocaleMock).toHaveBeenCalledTimes(1)
    expect(stubReloadMock).toHaveBeenCalledTimes(1)
  })

  it('does NOT fire on initial load (currentValues populated by load itself)', async () => {
    mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()
    await vi.advanceTimersByTimeAsync(500)
    await flushPromises()
    expect(apiCallMock).not.toHaveBeenCalled()
    expect(stubReloadMock).not.toHaveBeenCalled()
  })

  it('does NOT fire when the user re-picks the same value', async () => {
    mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()

    stubCurrentValues.ui_lang = 'deu'
    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()
    expect(apiCallMock).toHaveBeenCalledTimes(1)

    /* After the refresh, initialLang is 'deu'. Re-picking 'deu'
     * is a no-op. */
    stubCurrentValues.ui_lang = 'deu'
    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()
    expect(apiCallMock).toHaveBeenCalledTimes(1)
  })

  it('collapses rapid sequential picks into one round-trip on the final value', async () => {
    mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()

    stubCurrentValues.ui_lang = 'deu'
    await vi.advanceTimersByTimeAsync(50)
    stubCurrentValues.ui_lang = 'spa'
    await vi.advanceTimersByTimeAsync(50)
    stubCurrentValues.ui_lang = 'fre'
    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()

    expect(apiCallMock).toHaveBeenCalledTimes(1)
    const body = apiCallMock.mock.calls[0][1] as { node: string }
    expect(JSON.parse(body.node)).toMatchObject({ ui_lang: 'fre' })
  })

  it('swallows apiCall errors so the user can keep editing', async () => {
    apiCallMock.mockRejectedValueOnce(new Error('500'))
    const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {})
    mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()

    stubCurrentValues.ui_lang = 'deu'
    await vi.advanceTimersByTimeAsync(300)
    await flushPromises()

    expect(apiCallMock).toHaveBeenCalledTimes(1)
    expect(loadLocaleMock).not.toHaveBeenCalled()
    expect(stubReloadMock).not.toHaveBeenCalled()
    warnSpy.mockRestore()
  })

  it('cancels the pending debounce timer on unmount', async () => {
    const wrapper = mountHello()
    stubCurrentValues.ui_lang = 'eng'
    emitLoaded.value?.([{ id: 'ui_lang', value: 'eng' }])
    await flushPromises()

    stubCurrentValues.ui_lang = 'deu'
    await vi.advanceTimersByTimeAsync(100)
    wrapper.unmount()
    /* Pop from wrappers tracker so afterEach doesn't double-unmount. */
    const idx = wrappers.indexOf(wrapper)
    if (idx >= 0) wrappers.splice(idx, 1)

    await vi.advanceTimersByTimeAsync(500)
    await flushPromises()
    expect(apiCallMock).not.toHaveBeenCalled()
  })
})
