// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * createRailPreference — factory for the L1 NavRail and L2 sub-rail
 * collapse-state composables. Both rails behave identically (manual
 * preference persisted in localStorage, auto-collapse rule running
 * alongside, transient per-visit override that lets the chevron stay
 * effective even when auto wants compact); only the storage key and
 * the auto-rule trigger surface differ.
 *
 * Three pieces of shared state, all module-level singletons inside
 * the returned composable so every call site reads the same
 * instances:
 *
 *   manualCollapsed   boolean, persisted in localStorage under the
 *                     caller-supplied key. The user's standing
 *                     preference. Toggled via the chevron when no
 *                     auto rule is active.
 *
 *   autoActive        boolean, in-memory only. Mirror of "the auto-
 *                     compact rule is firing right now" — caller
 *                     writes it (AppShell for L1, L2Sidebar for L2)
 *                     via a watchEffect / resize listener; the
 *                     toggle function reads it.
 *
 *   autoOverride      boolean | null, in-memory only. Transient
 *                     per-view / per-visit override that lets the
 *                     chevron stay effective even when the auto rule
 *                     would otherwise force compact. `null` means
 *                     "no override, defer to manual || auto." A
 *                     non-null value wins over both flags. Reset to
 *                     null on every route change so re-entering an
 *                     auto-active route re-fires the auto rule —
 *                     Configuration always auto-collapses on entry.
 *
 * Effective compact rule (computed in the consumer):
 *
 *   if (viewport < 768)         → false (phone has its own affordance)
 *   if (autoOverride !== null)  → autoOverride
 *   otherwise                   → manualCollapsed || autoActive
 *
 * Click semantics (the `toggle` function below):
 *
 *   if (autoActive)             → first click sets override (auto
 *                                 wants compact, the user's click is
 *                                 meaningfully "show me the rail").
 *                                 Subsequent clicks toggle.
 *   otherwise                   → clear stale override and flip
 *                                 manualCollapsed; the change writes
 *                                 through to localStorage.
 *
 * Walking the load-bearing case: user on EPG (manual=false), navigates
 * to Configuration at mid width. Auto fires → compact. They click the
 * chevron — autoOverride goes null→false, rail expands. Navigate back
 * to EPG: route change clears autoOverride, manual||auto = false,
 * rail stays expanded (their stored preference). Re-enter
 * Configuration: autoOverride is null again, auto fires → compact.
 * Each visit gets the auto behaviour fresh; per-visit overrides are
 * scoped to that visit.
 */
import { ref, watch } from 'vue'

export interface RailPreferenceApi {
  manualCollapsed: ReturnType<typeof ref<boolean>>
  autoActive: ReturnType<typeof ref<boolean>>
  autoOverride: ReturnType<typeof ref<boolean | null>>
  toggle: () => void
  clearAutoOverride: () => void
}

export function createRailPreference(storageKey: string): () => RailPreferenceApi {
  function loadFromStorage(): boolean {
    try {
      return globalThis.window.localStorage.getItem(storageKey) === 'collapsed'
    } catch {
      return false
    }
  }

  const manualCollapsed = ref<boolean>(loadFromStorage())
  const autoActive = ref<boolean>(false)
  const autoOverride = ref<boolean | null>(null)

  watch(manualCollapsed, (v) => {
    try {
      globalThis.window.localStorage.setItem(
        storageKey,
        v ? 'collapsed' : 'expanded',
      )
    } catch {
      /* In-memory state still applies for this session; the next
       * session won't remember the choice. Acceptable degradation. */
    }
  })

  function toggle() {
    if (autoActive.value) {
      autoOverride.value =
        autoOverride.value === null ? false : !autoOverride.value
    } else {
      autoOverride.value = null
      manualCollapsed.value = !manualCollapsed.value
    }
  }

  function clearAutoOverride() {
    autoOverride.value = null
  }

  return () => ({
    manualCollapsed,
    autoActive,
    autoOverride,
    toggle,
    clearAutoOverride,
  })
}
