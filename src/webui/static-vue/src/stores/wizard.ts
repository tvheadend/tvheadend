// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Wizard store — coordinates the seven-step setup flow.
 *
 * Server-side state (`config.wizard` string in `src/config.h:42`)
 * is the source of truth for the active step; it's pushed via
 * Comet `accessUpdate.wizard` and surfaced through the access
 * store. This store doesn't mirror or cache step metadata —
 * `IdnodeConfigForm` handles its own load/save state per mount,
 * so each step navigation triggers a fresh fetch automatically.
 *
 * What this store does own:
 *   - Step ordering / next-prev helpers (the canonical sequence
 *     defined here is the only place the seven step names live)
 *   - Lifecycle actions (`start`, `cancel`)
 *   - Progress polling for the status step (`startPolling` /
 *     `stopPolling` / `pollProgress`)
 *
 * Server endpoints (all `ACCESS_ADMIN`,
 * `src/api/api_wizard.c:110-131`):
 *   POST /api/wizard/start             — sets config.wizard = "hello"
 *   POST /api/wizard/cancel            — clears the wizard
 *   POST /api/wizard/<step>/load       — idnode metadata + values
 *   POST /api/wizard/<step>/save       — runs ic_changed callback
 *   POST /api/wizard/status/progress   — { progress, muxes, services }
 *
 * Per-step metadata load/save is delegated to `IdnodeConfigForm`
 * via its existing `loadEndpoint` / `saveEndpoint` props (see
 * `IdnodeConfigForm.vue:48-52`); this store doesn't proxy those.
 */
import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { apiCall } from '@/api/client'
import { useAccessStore } from './access'

/*
 * Canonical step order — declaration order in `src/wizard.c`. The
 * server doesn't enforce order on save (skipping is allowed), but
 * the client uses this for prev/next navigation and the progress
 * indicator's "Step N of 7" rendering.
 */
export const WIZARD_STEPS = [
  'hello',
  'login',
  'network',
  'muxes',
  'status',
  'mapping',
  'channels',
] as const

export type WizardStepName = (typeof WIZARD_STEPS)[number]

/* The status-step polling endpoint's response shape. */
export interface WizardProgress {
  progress: number
  muxes: number
  services: number
}

/* Polling cadence — matches ExtJS at `static/app/wizard.js:182`. */
const PROGRESS_POLL_MS = 1000

/* ---- Module-scope pure helpers (Sonar S7721) ----
 *
 * These don't reference store-internal state, so they live at
 * module scope. The store re-exports them in its return shape so
 * `wizard.foo()` call sites can keep using the store handle, and
 * consumers that don't need the store can import them directly. */

export function nextStepAfter(name: string): WizardStepName | null {
  const idx = WIZARD_STEPS.indexOf(name as WizardStepName)
  if (idx === -1 || idx === WIZARD_STEPS.length - 1) return null
  return WIZARD_STEPS[idx + 1]
}

export function prevStepBefore(name: string): WizardStepName | null {
  const idx = WIZARD_STEPS.indexOf(name as WizardStepName)
  if (idx <= 0) return null
  return WIZARD_STEPS[idx - 1]
}

/* Lifecycle actions — POST against the wizard endpoints.
 *
 * After the POST resolves the server has set the wizard cursor
 * AND scheduled an `accessUpdate` comet broadcast carrying the
 * new value. The comet message arrives some milliseconds later
 * — by then a caller that synchronously router.push'd onto a
 * wizard route would already have tripped the router's
 * beforeEach guard with the stale (empty) cursor and been
 * redirected back to the default tab.
 *
 * ExtJS sidesteps this by hard-reloading the page on success
 * (`static/app/config.js:19-21`) so the access state is fetched
 * fresh during cold-load. The Vue stores do the SPA equivalent:
 * mirror the just-acked state into the access store via
 * `setWizardCursor`. The next `accessUpdate` from comet
 * wholesale-replaces `access.data` with the server view; in the
 * normal case it carries the same cursor we wrote and the
 * optimistic update is a no-op confirmation. */

export async function start(): Promise<void> {
  await apiCall('wizard/start')
  useAccessStore().setWizardCursor('hello')
}

export async function cancel(): Promise<void> {
  /* Server clears `config.wizard`; partial config from prior
   * step saves is NOT rolled back (matches ADR 0015 §5 and
   * ExtJS behaviour). */
  await apiCall('wizard/cancel')
  useAccessStore().setWizardCursor('')
}

export const useWizardStore = defineStore('wizard', () => {
  const access = useAccessStore()

  /*
   * Current active step from the server's perspective. Empty
   * string when wizard is inactive. Mirrors `access.data.wizard`
   * directly so any change pushed via Comet `accessUpdate`
   * propagates without extra wiring.
   */
  const currentStep = computed<string>(() => access.data?.wizard ?? '')

  /* True when the server says the wizard is active for this user. */
  const isActive = computed(() => currentStep.value !== '')

  /* ---- Status-step progress polling (closes over local state) ---- */

  const progress = ref<WizardProgress | null>(null)
  const polling = ref(false)
  const error = ref<string | null>(null)
  let pollHandle: ReturnType<typeof globalThis.setInterval> | null = null

  async function pollProgress(): Promise<void> {
    try {
      progress.value = await apiCall<WizardProgress>('wizard/status/progress')
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  function startPolling(): void {
    if (polling.value) return
    polling.value = true
    /* Fire immediately for the first reading; subsequent fires
     * happen on the interval. Caller doesn't need to await. */
    void pollProgress()
    pollHandle = globalThis.setInterval(() => {
      void pollProgress()
    }, PROGRESS_POLL_MS)
  }

  function stopPolling(): void {
    if (pollHandle !== null) {
      globalThis.clearInterval(pollHandle)
      pollHandle = null
    }
    polling.value = false
  }

  return {
    /* state */
    progress,
    polling,
    error,
    /* derived */
    currentStep,
    isActive,
    /* helpers (re-exported from module scope; see Sonar S7721
     * note above for why they live outside the setup) */
    nextStepAfter,
    prevStepBefore,
    /* actions */
    start,
    cancel,
    pollProgress,
    startPolling,
    stopPolling,
  }
})
