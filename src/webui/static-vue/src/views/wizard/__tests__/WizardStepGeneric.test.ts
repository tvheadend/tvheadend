// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* eslint-disable vue/one-component-per-file -- Test file mocks
 * several child components inline (IdnodeConfigForm + WizardFooter
 * stubs); each is a small throwaway harness, not a real component. */

/*
 * WizardStepGeneric component test. Pins:
 *   - Renders the icon + description chrome when the form's
 *     `loaded` emit delivers those server props.
 *   - Passes `wizard/<step>/load` + `wizard/<step>/save` endpoints
 *     to IdnodeConfigForm with `alwaysDirty` + `hideToolbar`.
 *   - Re-emits `loaded` upward so wrappers can capture baseline
 *     values.
 *   - WizardFooter prev-step reflects canonical order.
 *   - On `saved` from the form, navigates to the next step's
 *     route — and invokes the `beforeNext` hook first if
 *     provided.
 *   - Cancel calls `wizard.cancel()` then navigates to /epg.
 *   - Previous navigates to the canonical previous step.
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, ref } from 'vue'
import { mount, flushPromises } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import type { IdnodeProp } from '@/types/idnode'

const pushMock = vi.fn()
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: pushMock }),
}))

const { cancelMock } = vi.hoisted(() => ({
  cancelMock: vi.fn().mockResolvedValue(undefined),
}))
vi.mock('@/stores/wizard', () => {
  const STEP_ORDER = [
    'hello',
    'login',
    'network',
    'muxes',
    'status',
    'mapping',
    'channels',
  ]
  return {
    WIZARD_STEPS: STEP_ORDER as readonly string[],
    useWizardStore: () => ({
      cancel: cancelMock,
      prevStepBefore: (s: string) => {
        const i = STEP_ORDER.indexOf(s)
        return i > 0 ? STEP_ORDER[i - 1] : null
      },
      nextStepAfter: (s: string) => {
        const i = STEP_ORDER.indexOf(s)
        return i === -1 || i === STEP_ORDER.length - 1 ? null : STEP_ORDER[i + 1]
      },
    }),
  }
})

vi.mock('@/components/IdnodeConfigForm.vue', () => ({
  default: defineComponent({
    name: 'IdnodeConfigForm',
    props: {
      loadEndpoint: { type: String, default: undefined },
      saveEndpoint: { type: String, default: undefined },
      alwaysDirty: { type: Boolean },
      hideToolbar: { type: Boolean },
    },
    emits: ['loaded', 'saved'],
    setup(_, { emit, expose }) {
      const currentValues = ref<Record<string, unknown>>({ ui_lang: 'eng' })
      const loading = ref(false)
      const saving = ref(false)
      const save = vi.fn(() => {
        emit('saved')
        return Promise.resolve()
      })
      /* Vue's defineExpose auto-unwraps refs on consumer access.
       * The expose({ ... }) helper in setup is the same machinery,
       * so consumers see plain values. */
      expose({ save, loading, saving, currentValues })
      return () =>
        h('div', { class: 'idnode-config-form-stub' }, [
          h(
            'button',
            {
              class: 'fire-loaded',
              onClick: () =>
                emit('loaded', [
                  { id: 'icon', value: '/img/wizard-network.png' },
                  { id: 'description', value: 'Pick a tuner network.' },
                  { id: 'ui_lang', value: 'eng' },
                ] as IdnodeProp[]),
            },
            'fire-loaded',
          ),
          h(
            'button',
            {
              /* Fire a wizard-login-shaped payload that
               * deliberately omits PO_PASSWORD on the password
               * fields — the WizardStepGeneric workaround
               * should patch them on the way through. */
              class: 'fire-loaded-login',
              onClick: () =>
                emit('loaded', [
                  { id: 'admin_password', type: 'str', value: '' },
                  { id: 'password', type: 'str', value: '' },
                  /* A non-password str field with id "username"
                   * to verify the workaround doesn't over-match. */
                  { id: 'username', type: 'str', value: '' },
                  /* A numeric "password" field would be weird
                   * but defensive: verify type guard skips it. */
                  { id: 'password', type: 'int', value: 0 },
                ] as IdnodeProp[]),
            },
            'fire-loaded-login',
          ),
          h(
            'button',
            { class: 'fire-saved', onClick: () => save() },
            'fire-saved',
          ),
        ])
    },
  }),
}))

vi.mock('../WizardFooter.vue', () => ({
  default: defineComponent({
    name: 'WizardFooter',
    props: {
      prevStep: { type: String, default: undefined },
      isFinal: { type: Boolean },
      showSkip: { type: Boolean },
      saving: { type: Boolean },
    },
    emits: ['previous', 'cancel', 'skip', 'save'],
    setup(props, { emit }) {
      return () =>
        h('div', { class: 'wizard-footer-stub' }, [
          h(
            'button',
            { class: 'wf-previous', disabled: !props.prevStep, onClick: () => emit('previous') },
            'previous',
          ),
          h('button', { class: 'wf-cancel', onClick: () => emit('cancel') }, 'cancel'),
          h('button', { class: 'wf-save', onClick: () => emit('save') }, 'save'),
          h('span', { class: 'wf-prev-step' }, String(props.prevStep ?? '')),
        ])
    },
  }),
}))

import WizardStepGeneric from '../WizardStepGeneric.vue'

beforeEach(() => {
  setActivePinia(createPinia())
  pushMock.mockReset()
  cancelMock.mockClear()
})

const DEFAULT_MOUNT_PROPS: { step: string; beforeNext?: unknown } = {
  step: 'network',
}
function mountStep(props: { step: string; beforeNext?: unknown } = DEFAULT_MOUNT_PROPS) {
  return mount(WizardStepGeneric, {
    props: props as never,
  })
}

describe('WizardStepGeneric — chrome rendering', () => {
  it('renders no header until the loaded event arrives', () => {
    const wrapper = mountStep()
    expect(wrapper.find('.wizard-step__header').exists()).toBe(false)
  })

  it('renders icon + description after loaded fires', async () => {
    const wrapper = mountStep()
    await wrapper.find('.fire-loaded').trigger('click')
    await nextTick()
    const header = wrapper.find('.wizard-step__header')
    expect(header.exists()).toBe(true)
    expect(header.find<HTMLImageElement>('.wizard-step__icon').attributes('src')).toBe(
      '/img/wizard-network.png',
    )
    expect(header.find('.wizard-step__description').text()).toBe('Pick a tuner network.')
  })

  it('re-emits loaded upward for wrapper capture', async () => {
    const wrapper = mountStep()
    await wrapper.find('.fire-loaded').trigger('click')
    expect(wrapper.emitted('loaded')).toHaveLength(1)
    const payload = wrapper.emitted('loaded')![0][0] as IdnodeProp[]
    expect(payload.find((p) => p.id === 'icon')?.value).toBe('/img/wizard-network.png')
  })
})

describe('WizardStepGeneric — wizard password-field workaround', () => {
  /*
   * The wizard's `admin_password` + `password` PT_STR props in
   * src/wizard.c don't set PO_PASSWORD, so the server returns
   * them tagged as regular strings. WizardStepGeneric.handleLoaded
   * patches `password: true` in place on those two ids so
   * IdnodeFieldString masks them. This block pins:
   *   - the two known ids get patched
   *   - non-password ids (username) are left alone
   *   - non-str types with id="password" (defensive) are skipped
   *   - the patch happens before re-emitting upward
   */
  it('patches password:true on admin_password and password (str type only)', async () => {
    const wrapper = mountStep({ step: 'login' })
    await wrapper.find('.fire-loaded-login').trigger('click')
    const payload = wrapper.emitted('loaded')![0][0] as IdnodeProp[]
    const adminPwd = payload.find(
      (p) => p.id === 'admin_password' && p.type === 'str',
    )
    const userPwd = payload.find((p) => p.id === 'password' && p.type === 'str')
    const username = payload.find((p) => p.id === 'username')
    const intPassword = payload.find((p) => p.id === 'password' && p.type === 'int')
    expect(adminPwd?.password).toBe(true)
    expect(userPwd?.password).toBe(true)
    /* Non-password fields stay untouched. */
    expect(username?.password).toBeUndefined()
    /* Defensive: non-str fields whose id happens to be "password"
     * (none exist today, but the workaround's type guard pins
     * this contract) are skipped. */
    expect(intPassword?.password).toBeUndefined()
  })
})

describe('WizardStepGeneric — endpoints', () => {
  it('passes wizard/<step>/load + save endpoints to the form', () => {
    const wrapper = mountStep({ step: 'login' })
    const form = wrapper.findComponent({ name: 'IdnodeConfigForm' })
    expect(form.props('loadEndpoint')).toBe('wizard/login/load')
    expect(form.props('saveEndpoint')).toBe('wizard/login/save')
    expect(form.props('alwaysDirty')).toBe(true)
    expect(form.props('hideToolbar')).toBe(true)
  })
})

describe('WizardStepGeneric — footer state', () => {
  it('passes the canonical prevStep to the footer', () => {
    const wrapper = mountStep({ step: 'network' })
    /* network -> previous is login */
    expect(wrapper.find('.wf-prev-step').text()).toBe('login')
  })

  it('passes null prevStep for hello', () => {
    const wrapper = mountStep({ step: 'hello' })
    expect(wrapper.find('.wf-prev-step').text()).toBe('')
  })
})

describe('WizardStepGeneric — save flow', () => {
  it('save click triggers the form save which emits saved → navigates next', async () => {
    const wrapper = mountStep({ step: 'network' })
    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-muxes' })
  })

  it('invokes beforeNext with the just-POSTed values BEFORE navigating', async () => {
    const beforeNext = vi.fn().mockResolvedValue(undefined)
    const wrapper = mountStep({ step: 'hello', beforeNext })
    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()
    expect(beforeNext).toHaveBeenCalledTimes(1)
    /* The stubbed form exposes currentValues with ui_lang: 'eng' */
    const arg = beforeNext.mock.calls[0][0]
    expect(arg).toMatchObject({ ui_lang: 'eng' })
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-login' })
    /* beforeNext invocation order: called BEFORE router.push. */
    const beforeNextOrder = beforeNext.mock.invocationCallOrder[0]
    const pushOrder = pushMock.mock.invocationCallOrder[0]
    expect(beforeNextOrder).toBeLessThan(pushOrder)
  })

  it('swallows beforeNext errors and proceeds with navigation', async () => {
    const beforeNext = vi.fn().mockRejectedValue(new Error('boom'))
    /* Silence the expected console.error so test output stays clean. */
    const errorSpy = vi.spyOn(console, 'error').mockImplementation(() => {})
    const wrapper = mountStep({ step: 'hello', beforeNext })
    await wrapper.find('.wf-save').trigger('click')
    await flushPromises()
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-login' })
    errorSpy.mockRestore()
  })
})

describe('WizardStepGeneric — previous + cancel flows', () => {
  it('previous click navigates to the canonical previous step', async () => {
    const wrapper = mountStep({ step: 'network' })
    await wrapper.find('.wf-previous').trigger('click')
    expect(pushMock).toHaveBeenCalledWith({ name: 'wizard-login' })
  })

  it('cancel click calls wizard.cancel() then navigates to /epg', async () => {
    const wrapper = mountStep({ step: 'network' })
    await wrapper.find('.wf-cancel').trigger('click')
    await flushPromises()
    expect(cancelMock).toHaveBeenCalledTimes(1)
    expect(pushMock).toHaveBeenCalledWith({ name: 'epg' })
  })
})
