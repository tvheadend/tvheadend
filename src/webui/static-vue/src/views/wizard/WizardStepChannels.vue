<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardStepChannels — the final summary step. Renders the
 * server-emitted icon + Markdown description (which thanks the
 * user + warns about the default-admin-entry removal when an
 * admin was created via the wizard, per src/wizard.c:1179-1208
 * choosing between `channels` and `channels2` doc variants).
 *
 * Finish flow:
 *   1. POST wizard/channels/save — triggers the C-side
 *      `channels_changed` callback (src/wizard.c:1157) which
 *      removes the wide-open default access entry when a
 *      wizard-created admin exists.
 *   2. POST wizard/cancel — clears `config.wizard` so the
 *      wizard auto-trigger doesn't re-fire on next session.
 *   3. Navigate to `/gui/` (the EPG route) — the user is dropped
 *      into the regular UI.
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
import { markSetupComplete } from '@/utils/setupGreeting'
import { addExternalLinkAttrs, renderMarkdown, rewriteStaticUrls } from '@/utils/markdown'
import type { IdnodeProp } from '@/types/idnode'

const router = useRouter()
const wizard = useWizardStore()

const iconUrl = ref<string>('')
const descriptionHtml = ref<string>('')
const finishing = ref(false)

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
  try {
    /* Sequence per ADR 0015 §10. The server-side
     * channels_changed callback fires on the save POST and
     * removes the default wide-open access entry; the cancel
     * POST then clears config.wizard. Doing this in two
     * sequential requests (not parallel) so the access-entry
     * removal completes before cancel teardown. */
    await apiCall('wizard/channels/save', { node: JSON.stringify({}) })
    await wizard.cancel()
  } catch (e) {
    console.warn('[wizard] finish flow failed:', e)
  } finally {
    finishing.value = false
  }
  /* Page reload, not router.push — mirrors ExtJS at
   * static/app/wizard.js:154. The finish path is the END of the
   * wizard, not a step transition: (1) the channels_changed
   * callback may have just removed the wide-open default
   * access entry that the user was authenticated via, so the
   * browser needs a fresh auth round-trip; (2) `accessUpdate`
   * isn't re-broadcast when config.wizard clears (server only
   * emits at mailbox creation, per src/webui/comet.c:300) so a
   * soft router.push would land back on /wizard/hello via the
   * stale-cursor guard. Reloading at /gui/ forces a fresh WS
   * connection that carries the now-empty wizard field; the
   * regular UI mounts cleanly. ADR 0015 §4's soft-refresh
   * decision applies to in-wizard transitions, not to the
   * exit. Runs even if one of the POSTs threw — the user
   * explicitly chose to finish; trapping them on the channels
   * step is worse than slight server-side inconsistency
   * (recoverable from the Configuration UI). */
  /* Flag the Home dashboard's "Setup complete" greeting for after
   * the reload — sessionStorage survives the full reload, in-memory
   * state does not. The Home reads + clears it once. */
  markSetupComplete()
  globalThis.location.assign('/gui/')
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
