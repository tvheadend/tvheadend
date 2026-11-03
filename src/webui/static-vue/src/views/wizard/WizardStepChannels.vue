<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardStepChannels — the final summary step. Renders the
 * server-emitted icon + Markdown description (which thanks the
 * user + warns about the default-admin-entry removal when an
 * admin was created via the wizard, per src/wizard.c:1179-1211
 * choosing between `channels` and `channels2` doc variants).
 *
 * Finish flow:
 *   1. POST wizard/cancel — clears `config.wizard` so the
 *      wizard auto-trigger doesn't re-fire on next session.
 *   2. POST wizard/channels/save — triggers the C-side
 *      `channels_changed` callback (src/wizard.c:1160) which
 *      removes the wide-open default access entry when a
 *      wizard-created admin exists. If it fails, POST
 *      wizard/start puts the wizard back.
 *   3. Reload at `/gui/` (the EPG route), only after a
 *      successful save. When 1 or 2 fails, the step stays,
 *      shows the error and Finish can be retried.
 *
 * No IdnodeConfigForm — the channels step has no form fields
 * (just icon + description + PREV / LAST markers). Mirroring
 * WizardStepStatus's shape: dedicated layout, one-shot
 * `wizard/channels/load` fetch for the chrome.
 */
import { onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import WizardFooter from './WizardFooter.vue'
import { useWizardStore } from '@/stores/wizard'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { markSetupComplete } from '@/utils/setupGreeting'
import { appBase } from '@/utils/base'
import { addExternalLinkAttrs, renderMarkdown, rewriteStaticUrls } from '@/utils/markdown'
import type { IdnodeProp } from '@/types/idnode'

const router = useRouter()
const wizard = useWizardStore()
const { t } = useI18n()

const iconUrl = ref<string>('')
const descriptionHtml = ref<string>('')
const finishing = ref(false)
/* Why the last Finish stopped early, shown until the next try. */
const finishError = ref<string | null>(null)

interface WizardLoadResponse {
  entries?: { params?: IdnodeProp[] }[]
}

onMounted(async () => {
  try {
    const res = await apiCall<WizardLoadResponse>('wizard/channels/load', { meta: 1 })
    const params = res.entries?.[0]?.params ?? []
    const iconProp = params.find((p) => p.id === 'icon')
    const descProp = params.find((p) => p.id === 'description')
    if (typeof iconProp?.value === 'string') {
      iconUrl.value = normalizeStaticUrl(iconProp.value)
    }
    if (typeof descProp?.value === 'string') {
      descriptionHtml.value = addExternalLinkAttrs(
        rewriteStaticUrls(renderMarkdown(descProp.value)),
      )
    }
  } catch (e) {
    console.warn('[wizard] channels/load failed:', e)
  }
})

function normalizeStaticUrl(u: string): string {
  if (!u) return ''
  if (u.startsWith('/') || /^https?:\/\//i.test(u)) return u
  return `/${u}`
}

async function handleFinish(): Promise<void> {
  if (finishing.value) return
  finishing.value = true
  finishError.value = null
  try {
    /* Cancel first, then save, the order Classic uses (the cancel
     * in presave, static/app/wizard.js). The save's
     * channels_changed callback removes the default wide-open
     * access entry once a wizard admin exists (src/wizard.c), and
     * wizard/cancel needs admin rights. Sent after the save, the
     * cancel could hit a credentials prompt and leave the wizard
     * active. A failed cancel stops here, before the save, so the
     * wizard and the wide-open entry stay as they are. */
    await wizard.cancel()
    try {
      await apiCall('wizard/channels/save', { node: JSON.stringify({}) })
    } catch (e) {
      /* Without the save the wide-open entry stays, so put the
       * wizard back. Previous and a page reload then stay in it.
       * A failed restart is only logged, the save error is the
       * one shown. The wizard stays off in that case, so Previous
       * and a reload lead to the normal UI with the default entry
       * kept. Finish still completes the setup. */
      await wizard.start().catch((err: unknown) => {
        console.warn('[wizard] could not restart after a failed save:', err)
      })
      throw e
    }
  } catch (e) {
    /* Stay on the step with the reason. Finish can be retried,
     * and Previous and Cancel work again. */
    console.warn('[wizard] finish failed:', e)
    finishError.value = t('Could not finish the setup wizard: {0}', apiErrorMessage(e))
    finishing.value = false
    return
  }
  /* Reload at /gui/ instead of router.push, and only after a
   * successful save, as Classic does in its postsave
   * (static/app/wizard.js:144-156). channels_changed may have
   * just removed the default entry the browser got in through,
   * so the browser needs a fresh auth round trip, and the new
   * comet mailbox brings the access rights that apply now.
   * ADR 0015 §4's soft refresh is for in-wizard transitions,
   * not for the exit. Flag the Home dashboard's "Setup complete"
   * greeting first. sessionStorage survives the reload, and the
   * Home reads and clears it once. `finishing` stays set, so the
   * footer buttons stay disabled until the new page loads. */
  markSetupComplete()
  globalThis.location.assign(appBase)
}

async function handlePrevious(): Promise<void> {
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
    </div>
    <p v-if="finishError" class="wizard-step__error" role="alert">{{ finishError }}</p>
    <WizardFooter
      :prev-step="'mapping'"
      :is-final="true"
      :saving="finishing"
      @previous="handlePrevious"
      @cancel="handleCancel"
      @save="handleFinish"
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

/* Finish failure, styled like the save error of the other steps
 * (IdnodeConfigForm). It sits between the scroll area and the
 * footer, so it shows right above Finish even when the long
 * description fills a short screen. The side margins follow the
 * padding of the scroll area and the footer. */
.wizard-step__error {
  flex-shrink: 0;
  margin-top: var(--tvh-space-2);
  margin-bottom: var(--tvh-space-2);
  margin-left: calc(var(--tvh-space-6) + env(safe-area-inset-left));
  margin-right: calc(var(--tvh-space-6) + env(safe-area-inset-right));
  padding: var(--tvh-space-2) var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-error) 8%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-error);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  font-size: var(--tvh-text-md);
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

  .wizard-step__error {
    margin-left: calc(var(--tvh-space-3) + env(safe-area-inset-left));
    margin-right: calc(var(--tvh-space-3) + env(safe-area-inset-right));
  }
}
</style>
