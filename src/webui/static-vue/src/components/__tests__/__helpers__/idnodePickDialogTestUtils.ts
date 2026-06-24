// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Test helpers shared between IdnodePickClassDialog.test.ts and
 * IdnodePickEntityDialog.test.ts. Both pick-dialog test files
 * have identical mock + scaffolding needs around the
 * Dialog component and the `apiCall` mock; collected here.
 *
 * `vi.mock` calls cannot live inside a helper (they must be
 * module-scoped + are hoisted by Vitest), so each test file
 * still declares its own `apiCall` mock. This module owns the
 * pieces that DO compose: the Dialog stub object, the mock-
 * reset hook registration, and the four shape-identical test
 * cases (does-not-fetch / cancel / empty / error).
 */
import { afterEach, beforeEach, expect, it, type Mock } from 'vitest'
import { flushPromises, type VueWrapper } from '@vue/test-utils'

/* PrimeVue Dialog stub. Renders the default slot + the `footer`
 * slot in place when `visible` is true, so assertions can find
 * buttons and select options directly without traversing the
 * teleport boundary that the real PrimeVue Dialog introduces.
 * The `v-if="visible"` keeps the loaded-on-open behaviour
 * testable.
 */
export const DIALOG_STUB = {
  template:
    '<div v-if="visible" class="dialog-stub"><slot /><div class="dialog-stub__footer"><slot name="footer" /></div></div>',
  props: ['visible'],
}

/* Reset the supplied mock before AND after each test so a
 * leftover mock-state from one test can't leak into another. */
export function resetMockEachTest(mock: Mock): void {
  beforeEach(() => {
    mock.mockReset()
  })
  afterEach(() => {
    mock.mockReset()
  })
}

interface CommonPickDialogTestOptions {
  /* The mock — must be the same instance the test file passes
   * to vi.mock for `@/api/client`. */
  apiMock: Mock
  /* Mount the dialog with `visible` controlled by the caller.
   * Returns a VueWrapper. */
  mount: (visible?: boolean) => VueWrapper
  /* Empty-state text the dialog renders when the response has
   * zero entries — differs per dialog ("No types available." /
   * "No entries available."). */
  emptyText: string
  /* A single valid entry the empty-fixture and cancel-fixture
   * tests can use. Class dialog: `{ class: 'iptv_network' }`,
   * entity dialog: `{ uuid: 'u', networkname: 'n' }`. */
  validEntry: Record<string, unknown>
}

/* Drives the four shape-identical test cases that both
 * pick-dialog tests need to assert: visible-gating, cancel,
 * empty-state, fetch-error. Each consumer test file calls
 * this once inside its describe block. */
export function runCommonPickDialogTests(opts: CommonPickDialogTestOptions): void {
  it('does not fetch when visible is false on initial mount', async () => {
    opts.apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = opts.mount(false)
    await flushPromises()
    expect(opts.apiMock).not.toHaveBeenCalled()
    expect(wrapper.find('.dialog-stub').exists()).toBe(false)
  })

  it('emits close on Cancel', async () => {
    opts.apiMock.mockResolvedValueOnce({ entries: [opts.validEntry] })
    const wrapper = opts.mount(true)
    await flushPromises()
    const cancelBtn = wrapper
      .findAll('button')
      .find((b) => b.text() === 'Cancel')!
    await cancelBtn.trigger('click')
    expect(wrapper.emitted('close')).toBeTruthy()
  })

  it('renders the empty-state message when the response has no entries', async () => {
    opts.apiMock.mockResolvedValueOnce({ entries: [] })
    const wrapper = opts.mount(true)
    await flushPromises()
    expect(wrapper.text()).toContain(opts.emptyText)
    expect(wrapper.find('select').exists()).toBe(false)
  })

  it('renders the error message when the fetch rejects', async () => {
    opts.apiMock.mockRejectedValueOnce(new Error('boom'))
    const wrapper = opts.mount(true)
    await flushPromises()
    expect(wrapper.text()).toContain('boom')
    expect(wrapper.find('select').exists()).toBe(false)
  })
}
