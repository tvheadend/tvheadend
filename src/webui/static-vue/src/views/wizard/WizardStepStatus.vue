<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardStepStatus — the scan-monitor step. The wizard's
 * "scanning" page polls `/api/wizard/status/progress` at 1 Hz
 * (cadence matches `static/app/wizard.js:182`) and renders:
 *   - the step's icon + Markdown description (one-shot fetch
 *     from `wizard/status/load`);
 *   - a progress bar driven by the polled `progress` value
 *     (0.0 → 1.0);
 *   - live counts of muxes + services found so far.
 *
 * Forward navigation: user-driven only. The footer's Skip
 * button advances to mapping immediately — its label flips to
 * "Next" once the scan reaches 100 % so it reads as a
 * continuation rather than an opt-out. The server keeps
 * scanning in the background if the user skips early; the
 * wizard cursor moves on, and the user can come back to a
 * completed scan later via the configured services.
 *
 * The Skip button is the only forward path on this step; the
 * scan progress bar never navigates on its own. Matches
 * Classic's `wizard.js:75-93`, which also only updates the
 * bar.
 *
 * Unlike other form-driven steps, this one does NOT use
 * `IdnodeConfigForm` — the C-side props (`muxes`, `services`,
 * `progress`) are placeholders the form would render as
 * disabled inputs. The real values come from the dedicated
 * polling endpoint, and a progress bar reads better than three
 * read-only text rows.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import ProgressBar from 'primevue/progressbar'
import WizardFooter from './WizardFooter.vue'
import { useWizardStore } from '@/stores/wizard'
import { apiCall } from '@/api/client'
import { addExternalLinkAttrs, renderMarkdown, rewriteStaticUrls } from '@/utils/markdown'
import type { IdnodeProp } from '@/types/idnode'

const router = useRouter()
const wizard = useWizardStore()

const iconUrl = ref<string>('')
const descriptionHtml = ref<string>('')
/* Server-emitted captions for the count rows. Fallbacks are
 * marked novel for the i18n audit since they ship in this Vue
 * source file and aren't in `static/app/*.js`. */
/* i18n: new string */ const muxesLabel = ref<string>('Muxes')
/* i18n: new string */ const servicesLabel = ref<string>('Services')

const progress = computed(() => wizard.progress)

/* Progress bar value as percent (PrimeVue ProgressBar expects
 * 0-100). Reads from the polled progress; defaults to 0 before
 * the first poll resolves. */
const progressPercent = computed(() => {
  const p = progress.value?.progress
  if (typeof p !== 'number' || !Number.isFinite(p)) return 0
  return Math.min(100, Math.max(0, Math.round(p * 100)))
})

const muxesCount = computed(() => progress.value?.muxes ?? 0)
const servicesCount = computed(() => progress.value?.services ?? 0)

interface WizardLoadResponse {
  entries?: { params?: IdnodeProp[] }[]
}

onMounted(async () => {
  /* One-shot load to capture chrome (icon + description +
   * server-translated captions for the count labels). The
   * actual scan-state values come from the polling endpoint —
   * this fetch is just for the static metadata. */
  try {
    const res = await apiCall<WizardLoadResponse>('wizard/status/load', { meta: 1 })
    const params = res.entries?.[0]?.params ?? []
    const iconProp = params.find((p) => p.id === 'icon')
    const descProp = params.find((p) => p.id === 'description')
    const muxesProp = params.find((p) => p.id === 'muxes')
    const servicesProp = params.find((p) => p.id === 'services')
    if (typeof iconProp?.value === 'string') {
      iconUrl.value = normalizeStaticUrl(iconProp.value)
    }
    if (typeof descProp?.value === 'string') {
      descriptionHtml.value = addExternalLinkAttrs(
        rewriteStaticUrls(renderMarkdown(descProp.value)),
      )
    }
    if (typeof muxesProp?.caption === 'string' && muxesProp.caption) {
      muxesLabel.value = muxesProp.caption
    }
    if (typeof servicesProp?.caption === 'string' && servicesProp.caption) {
      servicesLabel.value = servicesProp.caption
    }
  } catch (e) {
    console.warn('[wizard] status/load failed:', e)
  }
  /* Always start polling, even if the metadata fetch failed —
   * the user still benefits from progress feedback. */
  wizard.startPolling()
})

onBeforeUnmount(() => {
  wizard.stopPolling()
})

/* Footer Skip-button label flips from "Skip" to "Next" once the
 * scan finishes. Same button, same emit; only the label
 * changes so the user reads a continuation rather than an
 * opt-out when the scan is genuinely complete. */
const skipLabel = computed(() => (progressPercent.value >= 100 ? 'Next' : 'Skip'))

function normalizeStaticUrl(u: string): string {
  if (!u) return ''
  if (u.startsWith('/') || /^https?:\/\//i.test(u)) return u
  return `/${u}`
}

async function handlePrevious(): Promise<void> {
  await router.push({ name: 'wizard-muxes' })
}

async function handleSkip(): Promise<void> {
  /* Skip moves the URL forward; the server keeps scanning. The
   * wizard cursor will advance once the mapping step's load
   * fires (cursor-advances-on-load semantics — see
   * src/api/api_wizard.c:49). */
  await router.push({ name: 'wizard-mapping' })
}

async function handleCancel(): Promise<void> {
  await wizard.cancel()
  await router.push({ name: 'epg' })
}
</script>

<template>
  <div class="wizard-step">
    <div class="wizard-step__scroll">
      <header v-if="iconUrl || descriptionHtml" class="wizard-step__header">
        <img v-if="iconUrl" :src="iconUrl" alt="" class="wizard-step__icon" />
        <!-- Server-emitted Markdown description rendered through
             `marked` and v-html'd. Safe by trust: source is
             compiled-in `docs/wizard/*.md`, not user input.
             Matches ExtJS at `static/app/wizard.js:106`. -->
        <!-- eslint-disable-next-line vue/no-v-html -->
        <div v-if="descriptionHtml" class="wizard-step__description" v-html="descriptionHtml"></div>
      </header>
      <div class="wizard-status">
        <ProgressBar :value="progressPercent" />
        <dl class="wizard-status__counts">
          <div class="wizard-status__count">
            <dt class="wizard-status__count-label">{{ muxesLabel }}</dt>
            <dd class="wizard-status__count-value">{{ muxesCount }}</dd>
          </div>
          <div class="wizard-status__count">
            <dt class="wizard-status__count-label">{{ servicesLabel }}</dt>
            <dd class="wizard-status__count-value">{{ servicesCount }}</dd>
          </div>
        </dl>
      </div>
    </div>
    <WizardFooter
      :prev-step="'muxes'"
      :show-skip="true"
      :skip-label="skipLabel"
      :skip-emphasized="progressPercent >= 100"
      :hide-save="true"
      @previous="handlePrevious"
      @cancel="handleCancel"
      @skip="handleSkip"
    />
  </div>
</template>

<style scoped>
.wizard-step {
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}

.wizard-step__scroll {
  flex: 1 1 auto;
  overflow-y: auto;
  padding-top: var(--tvh-space-6);
  padding-bottom: var(--tvh-space-6);
  padding-left: calc(var(--tvh-space-6) + env(safe-area-inset-left));
  padding-right: calc(var(--tvh-space-6) + env(safe-area-inset-right));
}

.wizard-step__header {
  display: flex;
  align-items: flex-start;
  gap: var(--tvh-space-3);
  margin-bottom: var(--tvh-space-4);
  padding-bottom: var(--tvh-space-3);
  border-bottom: 1px solid var(--tvh-border);
}

.wizard-step__icon {
  width: 96px;
  height: 96px;
  object-fit: contain;
  flex-shrink: 0;
}

.wizard-step__description {
  flex: 1;
  min-width: 0;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
  line-height: 1.5;
}

.wizard-step__description :deep(p) {
  margin: 0 0 var(--tvh-space-2);
}
.wizard-step__description :deep(p:last-child) {
  margin-bottom: 0;
}
.wizard-step__description :deep(ul),
.wizard-step__description :deep(ol) {
  margin: 0 0 var(--tvh-space-2);
  padding-left: 1.5em;
}
.wizard-step__description :deep(li) {
  margin-bottom: 4px;
}
.wizard-step__description :deep(strong) {
  color: var(--tvh-text);
  font-weight: 600;
}
.wizard-step__description :deep(a) {
  color: var(--tvh-primary);
  text-decoration: underline;
}
.wizard-step__description :deep(img) {
  max-width: 100%;
  height: auto;
}

/* Scan-state panel — progress bar + count rows. */
.wizard-status {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-4);
  max-width: 480px;
  margin: 0 auto;
}

.wizard-status__counts {
  display: flex;
  gap: var(--tvh-space-6);
  margin: 0;
  padding: 0;
  justify-content: center;
}

.wizard-status__count {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

.wizard-status__count-label {
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
  margin: 0;
}

.wizard-status__count-value {
  font-size: var(--tvh-text-display);
  font-weight: 600;
  color: var(--tvh-text);
  margin: 0;
  font-variant-numeric: tabular-nums;
}

@media (max-width: 767px) {
  .wizard-step__scroll {
    padding-top: var(--tvh-space-3);
    padding-bottom: var(--tvh-space-3);
    padding-left: calc(var(--tvh-space-3) + env(safe-area-inset-left));
    padding-right: calc(var(--tvh-space-3) + env(safe-area-inset-right));
  }

  .wizard-step__header {
    flex-direction: column;
    gap: var(--tvh-space-2);
  }

  .wizard-step__icon {
    display: none;
  }
}
</style>
