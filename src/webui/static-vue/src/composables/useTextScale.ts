// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { ref, watch } from 'vue'
import { useIsPhone } from './useIsPhone'

/*
 * Reactive read of `--tvh-text-scale` from the document root.
 *
 * Most code that scales with the text-size token system does it
 * implicitly via CSS — components reference `var(--tvh-text-md)` and
 * the calc()-product re-resolves whenever the active scale changes.
 * But some metrics live in JS-side coordinate math: per-pixel-per-
 * minute event positioning on the EPG, channel-row height for scroll
 * bucketing, magazine column widths. Those have no CSS variable to
 * piggy-back on, so they read the scale into a reactive Ref and
 * multiply directly.
 *

 * Reactivity sources:
 *   - MutationObserver on document.documentElement's `data-theme`
 *     attribute — catches every theme change. The server's
 *     accessUpdate watcher writes the dataset attribute directly.
 *   - the shared phone-breakpoint flag (useIsPhone) — themes that
 *     declare a different scale at @media (max-width: 767px)
 *     (today: only Access) need the ref to update when the
 *     viewport crosses the breakpoint in either direction.
 *
 * Singleton design: one shared Ref + one set of observers no matter
 * how many components call useTextScale(). Initialised on first use;
 * cleanup is intentionally omitted because the listeners are
 * document-level and live as long as the SPA does.
 *
 * Fallback to 1 when the property is missing or unparseable — keeps
 * the EPG layout sensible even if tokens.css hasn't loaded yet (a
 * brief window during cold boot) or a future edit removes the
 * variable.
 */
const scale = ref(1)
let initialized = false

function readScale(): void {
  if (typeof document === 'undefined') return
  const raw = getComputedStyle(document.documentElement)
    .getPropertyValue('--tvh-text-scale')
    .trim()
  const parsed = Number.parseFloat(raw)
  scale.value = Number.isFinite(parsed) && parsed > 0 ? parsed : 1
}

function initialize(): void {
  if (initialized) return
  initialized = true
  if (globalThis.window === undefined || globalThis.document === undefined) return

  readScale()

  const observer = new MutationObserver(readScale)
  observer.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['data-theme'],
  })

  /* Module-level watch — intentionally never stopped; the
   * singleton lives as long as the SPA does. */
  watch(useIsPhone(), readScale)
}

export function useTextScale() {
  initialize()
  return scale
}
