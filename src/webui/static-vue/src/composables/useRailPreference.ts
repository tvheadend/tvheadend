/*
 * useRailPreference — manual collapse state for the L1 NavRail.
 *
 * Three pieces of shared state, all module-level singletons so every
 * call site (AppShell for layout, NavRail for the chevron click)
 * reads/writes the same instances:
 *
 *   manualCollapsed   boolean, persisted in localStorage
 *                     The user's standing preference. Toggled via
 *                     the chevron when no auto rule is active.
 *
 *   autoActive        boolean, in-memory only
 *                     Mirror of "AppShell's auto-compact rule is
 *                     firing right now" — i.e., the active route
 *                     declares meta.hasL2 AND the viewport sits in
 *                     the 768–1279 mid range. AppShell writes it via
 *                     a watchEffect; the toggle function reads it.
 *
 *   autoOverride      boolean | null, in-memory only
 *                     Transient per-view override that lets the
 *                     chevron stay effective even when the auto rule
 *                     would otherwise force compact. `null` means
 *                     "no override, defer to manual || auto." A
 *                     non-null value wins over both flags. Reset to
 *                     null on every route change so re-entering an
 *                     auto-active route re-fires the auto rule
 *                     (preserves the original Commit-13 spec —
 *                     Configuration always auto-collapses on entry).
 *
 * Effective compact rule (computed in AppShell):
 *
 *   if (viewport < 768)         → false (phone uses hamburger drawer)
 *   if (autoOverride !== null)  → autoOverride
 *   otherwise                   → manualCollapsed || autoActive
 *
 * Click semantics (the `toggle` function below):
 *
 *   if (autoActive)             → first click sets override to false
 *                                 (expand on auto-collapsed view);
 *                                 subsequent clicks toggle override.
 *   otherwise                   → clear any stale override and flip
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

const STORAGE_KEY = 'tvh-rail:state'

function loadFromStorage(): boolean {
  try {
    return globalThis.window.localStorage.getItem(STORAGE_KEY) === 'collapsed'
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
      STORAGE_KEY,
      v ? 'collapsed' : 'expanded'
    )
  } catch {
    /* In-memory state still applies for this session; the next
     * session won't remember the choice. Acceptable degradation. */
  }
})

function toggle() {
  if (autoActive.value) {
    /* Auto rule is firing. First click overrides to expanded
     * (autoActive=true means auto wants compact, so the user's click
     * is meaningfully "show me the rail"). Subsequent clicks toggle
     * the override between expanded and collapsed. The stored
     * `manualCollapsed` is left alone so navigating away to a
     * non-auto route restores the user's standing preference. */
    autoOverride.value =
      autoOverride.value === null ? false : !autoOverride.value
  } else {
    /* No auto in play — chevron toggles the persistent preference.
     * Clear any stale override (would only exist if autoActive just
     * went false without a route change, e.g., user resized from
     * mid to wide) so manual is in unambiguous control. */
    autoOverride.value = null
    manualCollapsed.value = !manualCollapsed.value
  }
}

function clearAutoOverride() {
  autoOverride.value = null
}

export function useRailPreference() {
  return {
    manualCollapsed,
    autoActive,
    autoOverride,
    toggle,
    clearAutoOverride,
  }
}
