// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Test helpers shared between IdnodeEditor / IdnodeEditor multi-edit /
 * IdnodeConfigForm / ServiceMapperDialog test files. Each of those
 * test files has identical fixture needs around the PrimeVue
 * Drawer / Dialog stubs, the tooltip directive no-op, and the
 * apiMock + Pinia reset hooks.
 *
 * `vi.mock` calls cannot live inside a helper (they must be
 * module-scoped + are hoisted by Vitest), so each test file still
 * declares its own `vi.mock('@/api/client', …)` and (where used)
 * `vi.mock('@/composables/useConfirmDialog', …)`. This module owns
 * the pieces that DO compose: the stubs and the per-test reset
 * hook registration with Pinia bootstrap.
 */
import { afterEach, beforeEach, type Mock } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'

/* PrimeVue tooltip directive no-op. The real directive isn't
 * registered when mounting bare via @vue/test-utils, so any
 * `v-tooltip` use in a child component would error without this
 * stub. */
export const TOOLTIP_DIRECTIVE_STUB = {
  mounted() {
    /* no-op */
  },
  updated() {
    /* no-op */
  },
  unmounted() {
    /* no-op */
  },
}

/* PrimeVue Drawer passthrough stub. Renders the default slot +
 * the `footer` slot when `visible` is true. The IdnodeEditor
 * variant also supplies a `header` slot — pass true to include
 * it. The real Drawer component teleports + transitions; the
 * stub keeps everything in-place so assertions can find chrome
 * without traversing the teleport boundary. */
export function makeDrawerStub(opts: { withHeader?: boolean } = {}) {
  const headerSlot = opts.withHeader ? '<slot name="header" />' : ''
  return {
    template:
      `<div class="drawer-stub" v-if="visible"><slot />${headerSlot}<slot name="footer" /></div>`,
    props: ['visible'],
  }
}

/* PrimeVue Dialog passthrough stub. Same shape as the Drawer
 * variant for the same reasons; ServiceMapperDialog uses Dialog
 * (full-screen modal) where IdnodeEditor uses Drawer (side
 * panel). */
export const DIALOG_PASSTHROUGH_STUB = {
  template:
    '<div class="dialog-stub" v-if="visible"><slot /><slot name="footer" /></div>',
  props: ['visible'],
}

/* PrimeVue Checkbox passthrough stub. The real Checkbox needs the
 * PrimeVue plugin ($primevue config), which the bare
 * @vue/test-utils mount doesn't install. The stub mirrors the real
 * component's DOM shape — a root element (carrying any fallthrough
 * class) wrapping a native `<input type="checkbox">` — so tests
 * drive it via `.find('input')` exactly as they would the real
 * component. Binary mode only. */
export const CHECKBOX_STUB = {
  template:
    `<span class="checkbox-stub"><input type="checkbox" :checked="modelValue" @change="$emit('update:modelValue', $event.target.checked)" /></span>`,
  props: ['modelValue', 'binary', 'ariaLabel'],
  emits: ['update:modelValue'],
}

/* Reset the supplied apiMock before AND after each test, plus
 * re-bootstrap Pinia per test so store state doesn't leak across
 * cases. The Pinia bootstrap is required because IdnodeEditor /
 * IdnodeConfigForm reach into `useAccessStore()` for the level-
 * picker access cap. */
export function setupApiMockReset(mock: Mock): void {
  beforeEach(() => {
    mock.mockReset()
    setActivePinia(createPinia())
  })
  afterEach(() => {
    mock.mockReset()
  })
}
