<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardStepHello — wraps WizardStepGeneric for the hello step
 * with two language-change behaviours:
 *
 * 1. LIVE PREVIEW (on dropdown change, before Save & Next).
 *    When the user picks a different `ui_lang` value, we save
 *    the form's current values in place (no navigation), then
 *    refetch /locale.js + the step's idnode metadata so the
 *    hello-page chrome (form field labels, group headers,
 *    description Markdown, footer buttons) flips to the new
 *    language without a page reload. The user sees the wizard
 *    speak their language immediately on the same screen —
 *    better than ExtJS which does a full
 *    `window.location.reload()` (wizard.js:147) after save.
 *
 *    Debounced 300 ms so a quick scroll through the dropdown
 *    options doesn't fire multiple save round-trips.
 *
 * 2. SAVE & NEXT (via `beforeNext` hook into WizardStepGeneric).
 *    Same orchestration as live-preview but fires after the
 *    explicit Save click, just before navigation to the next
 *    step. If live-preview already ran (user picked a new
 *    language, saw the preview, then clicked Save & Next), the
 *    new language is already in place — `beforeNext` becomes a
 *    no-op because `initialLang` matches the saved value.
 *
 * Soft refresh, not page reload: ADR 0015 §4 — locked. Both
 * paths keep the wizard chrome mounted and update locale via
 * a fresh `/locale.js` `<script>` tag.
 */
import { onBeforeUnmount, ref, watch } from 'vue'
import WizardStepGeneric from './WizardStepGeneric.vue'
import { loadLocale } from '@/composables/useI18n'
import { apiCall } from '@/api/client'
import type { IdnodeProp } from '@/types/idnode'

const stepRef = ref<InstanceType<typeof WizardStepGeneric> | null>(null)

/* Snapshot of `ui_lang` after the most recent successful load
 * (either initial mount or a live-preview refresh). The live-
 * preview watcher fires when the form's current `ui_lang`
 * diverges from this snapshot. */
const initialLang = ref<string>('')

/* Flips true on first `loaded` event — gates the watcher so it
 * doesn't fire during the initial load's currentValues
 * population (where currentValues.ui_lang transitions from
 * undefined to the loaded value). */
const loaded = ref(false)

function handleLoaded(params: IdnodeProp[]): void {
  const langProp = params.find((p) => p.id === 'ui_lang')
  initialLang.value = typeof langProp?.value === 'string' ? langProp.value : ''
  loaded.value = true
}

/* --- Live preview --- */

let debounceTimer: ReturnType<typeof globalThis.setTimeout> | null = null
let refreshing = false

/* Watch the form's currentValues.ui_lang via the exposed
 * template-ref chain. Vue's reactivity walks the optional-
 * chained accesses, so this fires whenever the refs populate
 * AND whenever the dropdown value changes. */
watch(
  () => stepRef.value?.formRef?.currentValues?.ui_lang,
  (newLang) => {
    if (!loaded.value || refreshing) return
    if (typeof newLang !== 'string' || !newLang) return
    if (newLang === initialLang.value) return
    if (debounceTimer !== null) globalThis.clearTimeout(debounceTimer)
    debounceTimer = globalThis.setTimeout(() => {
      debounceTimer = null
      void runLivePreview(newLang)
    }, 300)
  },
)

async function runLivePreview(newLang: string): Promise<void> {
  const form = stepRef.value?.formRef
  if (!form?.currentValues) return
  refreshing = true
  try {
    /* POST current form values so config.language_ui takes
     * the new value server-side. We invoke apiCall directly
     * (not form.save()) to avoid firing the `saved` emit,
     * which would trigger WizardStepGeneric's handleSaved →
     * router.push to the next step — we want to stay on
     * hello. */
    await apiCall('wizard/hello/save', {
      node: JSON.stringify(form.currentValues),
    })
    /* Refresh JS-side translation catalog so the footer's
     * Save & Next / Cancel / Help labels + the WizardProgress
     * pills (whatever's translated) flip too. Failure is
     * non-fatal — the form will still refresh below. */
    try {
      await loadLocale()
    } catch (e) {
      console.warn('[wizard] /locale.js refetch failed:', e)
    }
    /* Refresh the form's idnode metadata so its labels +
     * group headers + the description Markdown all flip to
     * the new language. */
    await form.reload()
    /* Sync the snapshot so the watcher doesn't re-fire on the
     * same value after the reload populates currentValues. */
    initialLang.value = newLang
  } catch (e) {
    console.warn('[wizard] live-preview language refresh failed:', e)
  } finally {
    refreshing = false
  }
}

onBeforeUnmount(() => {
  if (debounceTimer !== null) globalThis.clearTimeout(debounceTimer)
})

/* --- Save & Next --- */

async function handleLangChange(savedValues: Record<string, unknown>): Promise<void> {
  const newLang = typeof savedValues.ui_lang === 'string' ? savedValues.ui_lang : ''
  if (newLang === initialLang.value) {
    /* No change since the last snapshot — either the user
     * didn't touch the language, or live-preview already
     * ran. Fast path. */
    return
  }
  try {
    await loadLocale()
  } catch (e) {
    console.warn('[wizard] /locale.js refetch failed:', e)
  }
}
</script>

<template>
  <WizardStepGeneric
    ref="stepRef"
    step="hello"
    :before-next="handleLangChange"
    @loaded="handleLoaded"
  />
</template>
