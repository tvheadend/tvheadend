// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Single source of truth for the phone breakpoint.
 *
 * One module-level matchMedia listener feeds one shared ref — no
 * per-component resize listeners, no cleanup (the listener lives as
 * long as the SPA, same singleton design as useTextScale). Keep the
 * width in sync with the `@media (max-width: 767px)` rules used in
 * component CSS.
 *
 * Environments without window/matchMedia (unit tests that don't set
 * up a DOM, SSR-ish contexts) read as "not phone".
 */
import { readonly, ref, type Ref } from 'vue'

/* Phone is viewport width < this. */
export const PHONE_MAX_WIDTH = 768

const isPhone = ref(false)
let initialized = false

function initialize(): void {
  if (initialized) return
  initialized = true
  const w = globalThis.window
  if (w === undefined || typeof w.matchMedia !== 'function') return
  const mql = w.matchMedia(`(max-width: ${PHONE_MAX_WIDTH - 1}px)`)
  isPhone.value = mql.matches
  mql.addEventListener('change', (e) => {
    isPhone.value = e.matches
  })
}

/* Shared reactive flag — lazily wired on first use. */
export function useIsPhone(): Readonly<Ref<boolean>> {
  initialize()
  return readonly(isPhone)
}

/* Call-time snapshot for plain (non-reactive) utility code. */
export function isPhoneNow(): boolean {
  initialize()
  return isPhone.value
}
