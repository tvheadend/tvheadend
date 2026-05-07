/*
 * Test helpers shared between IdnodePickClassDialog.test.ts and
 * IdnodePickEntityDialog.test.ts. Both pick-dialog test files
 * have identical mock + scaffolding needs around the
 * Dialog component and the `apiCall` mock; collected here.
 *
 * `vi.mock` calls cannot live inside a helper (they must be
 * module-scoped + are hoisted by Vitest), so each test file
 * still declares its own `apiCall` mock. This module owns the
 * pieces that DO compose: the Dialog stub object and the
 * mock-reset hook registration.
 */
import { afterEach, beforeEach, type Mock } from 'vitest'

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
