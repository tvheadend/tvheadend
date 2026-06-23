<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardStepGeneric — generic step component used for every
 * wizard step whose UX is "load idnode metadata → render fields
 * → Save & Next". That covers hello, login, network, muxes, and
 * mapping. The status step (poll + auto-advance) and channels
 * step (final summary + Finish-to-cleanup) have their own
 * components.
 *
 * Layout:
 *   - Step header (icon + description, both server-emitted as
 *     fields on the wizard idnode at `src/wizard.c:SPECIAL_PROP`).
 *   - IdnodeConfigForm — reuses the existing form renderer with
 *     its `loadEndpoint` / `saveEndpoint` props pointing at
 *     `/api/wizard/<step>/load` and `/api/wizard/<step>/save`.
 *     Toolbar hidden (`hideToolbar`) because the wizard's own
 *     WizardFooter owns the Save / Previous / Cancel actions.
 *   - WizardFooter — uniform action bar.
 *
 * Why `alwaysDirty`: the wizard's Save & Next button is the only
 * way forward, so it must POST even when the user didn't touch
 * a field (idempotent save: server re-runs the step's
 * `ic_changed` callback with the same values; matches ExtJS
 * behaviour where Save & Next always fires).
 *
 * Save → navigate flow:
 *   1. User clicks Save & Next in WizardFooter.
 *   2. `handleSave()` invokes `formRef.value.save()` via the
 *      form's exposed imperative handle.
 *   3. The form POSTs to `/api/wizard/<step>/save`. On success,
 *      it emits `saved`.
 *   4. Our `@saved="handleSaved"` listener navigates to the
 *      route for `nextStepAfter(step)`. The server's accessUpdate
 *      will arrive moments later with the new wizard cursor; the
 *      route guard would also have redirected, but we navigate
 *      eagerly for snappier UX.
 *   5. On save failure, the form sets its own error state. We
 *      stay on the step — no navigation.
 *
 * Icon + description extraction: IdnodeConfigForm now emits
 * `loaded` with the params array after every load. We scan for
 * `id === 'icon'` and `id === 'description'`. Both are
 * PO_RDONLY | PO_NOUI on the server (`src/wizard.c:SPECIAL_PROP`)
 * so they don't render as form fields — the form just emits them
 * to us and we render the header chrome here.
 */
import { computed, ref } from 'vue'
import { useRouter } from 'vue-router'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import WizardFooter from './WizardFooter.vue'
import { useWizardStore, type WizardStepName } from '@/stores/wizard'
import { addExternalLinkAttrs, renderMarkdown, rewriteStaticUrls } from '@/utils/markdown'
import type { IdnodeProp } from '@/types/idnode'

/*
 * Wizard password-field workaround scope — see handleLoaded
 * below. Module-scope Set for O(1) lookup; matched in the
 * loaded-emit patch loop. Deletable once
 * src/wizard.c:434, 452 add `.opts = PO_PASSWORD,`.
 */
const WIZARD_PASSWORD_IDS = new Set(['admin_password', 'password'])

interface Props {
  /* Canonical step name. Drives the load/save endpoints and the
   * prev/next route resolution. Route components pass this as a
   * static prop. */
  step: WizardStepName
  /* Optional async hook that fires AFTER a successful save and
   * BEFORE the navigation to the next step. The hello step's
   * wrapper uses this to detect a `ui_lang` change and refetch
   * `/locale.js` (soft refresh) before letting navigation
   * proceed. Receives the values that were just POSTed (still
   * in `currentValues` at this point — `saved` emits before
   * the post-save load refresh).
   *
   * Default: no-op. Awaited — throwing or rejecting will let
   * the error propagate to the click handler, leaving the user
   * on the current step. */
  beforeNext?: (savedValues: Record<string, unknown>) => void | Promise<void>
}

const props = defineProps<Props>()

const emit = defineEmits<{
  /* Re-emit of the form's `loaded` event so wrappers
   * (WizardStepHello) can capture initial values like the
   * `ui_lang` baseline for comparison after save. */
  loaded: [params: IdnodeProp[]]
}>()

const router = useRouter()
const wizard = useWizardStore()

const formRef = ref<InstanceType<typeof IdnodeConfigForm> | null>(null)

/* Saving state for the footer's Save button feedback. Tracks our
 * own invocation of `formRef.save()` — the form has its own
 * internal `saving` ref but we expose a cleaner one here so the
 * footer doesn't need to reach through the template ref. */
const saving = ref(false)

/* Server-emitted chrome. Bound from the `loaded` emit. Falls
 * back to empty so the header just collapses when nothing's
 * loaded yet. `descriptionHtml` is the Markdown-rendered HTML
 * shape — every wizard description is Markdown (mirrors ExtJS
 * which runs the same field through marked() before injecting
 * into a div). Image paths inside the rendered HTML are
 * normalised to root-absolute so relative URLs like
 * `static/img/opencollective.png` resolve correctly under the
 * Vue route prefix. */
const iconUrl = ref<string>('')
const descriptionHtml = ref<string>('')

const prevStep = computed<WizardStepName | null>(() =>
  wizard.prevStepBefore(props.step),
)

const nextStep = computed<WizardStepName | null>(() =>
  wizard.nextStepAfter(props.step),
)

const loadEndpoint = computed(() => `wizard/${props.step}/load`)
const saveEndpoint = computed(() => `wizard/${props.step}/save`)

function handleLoaded(params: IdnodeProp[]): void {
  /* The wizard idnode shape: icon and description are
   * PO_RDONLY | PO_NOUI server props (src/wizard.c). The form
   * delivers them in the params array; pull them out for the
   * header chrome. Default-empty so the header just renders
   * blank if a step happens to ship without them. */
  const iconProp = params.find((p) => p.id === 'icon')
  const descProp = params.find((p) => p.id === 'description')
  const rawIcon = typeof iconProp?.value === 'string' ? iconProp.value : ''
  iconUrl.value = normalizeStaticUrl(rawIcon)
  const rawDesc = typeof descProp?.value === 'string' ? descProp.value : ''
  /* Render Markdown -> HTML, then rewrite any bare relative
   * static/* URLs inside <img src> and <a href> attributes —
   * the channels-step description, for example, embeds
   * `static/img/opencollective.png` which would otherwise
   * resolve under `/gui/wizard/channels/...`. Same root cause
   * as the wizard icon URL fix. */
  descriptionHtml.value = addExternalLinkAttrs(rewriteStaticUrls(renderMarkdown(rawDesc)))
  /* Client-side overlay: flip `password: true` on the wizard's
   * admin_password + password fields so IdnodeFieldString masks
   * them and renders the show/hide toggle. The C-side wizard
   * (src/wizard.c:434, 452) declares both as plain PT_STR
   * without `.opts = PO_PASSWORD`, so the server returns them
   * tagged as regular strings. Other consumers (access.c:2314,
   * cclient.c:1386) already declare PO_PASSWORD correctly, so
   * this scoped patch in the wizard wrapper is safe; it doesn't
   * affect any non-wizard surface. `params` is the same reactive
   * array IdnodeConfigForm stored in its `fieldProps` ref a
   * moment ago — mutating the prop objects in place propagates
   * through Vue's deep reactivity and the field renderer
   * re-evaluates `isPassword`. The overlay is redundant once
   * `src/wizard.c` adds `.opts = PO_PASSWORD,` to both prop
   * declarations (pending). */
  for (const p of params) {
    if (WIZARD_PASSWORD_IDS.has(p.id) && p.type === 'str') {
      p.password = true
    }
  }
  /* Re-emit upward so wrappers can capture baseline values. */
  emit('loaded', params)
}

/*
 * Server emits the wizard icon path as a bare relative string
 * like `static/img/logobig.png` (src/wizard.c:icon_get). The
 * ExtJS UI is served from the document root so the browser
 * resolves the relative path to `/static/img/...`; the Vue UI
 * is mounted under `/gui/...` so the same string resolves to
 * `/gui/wizard/<step>/static/...` and 404s. Coerce to a root-
 * absolute URL whenever the server hands us a bare path. Leaves
 * full URLs (http/https) and already-absolute paths alone.
 */
function normalizeStaticUrl(u: string): string {
  if (!u) return ''
  if (u.startsWith('/') || /^https?:\/\//i.test(u)) return u
  return `/${u}`
}

async function handleSave(): Promise<void> {
  if (!formRef.value || saving.value) return
  saving.value = true
  try {
    await formRef.value.save()
  } finally {
    /* Always release saving — success path navigates away in
     * handleSaved (so the flag is irrelevant); failure path
     * keeps us on the step with the form's error visible. */
    saving.value = false
  }
}

async function handleSaved(): Promise<void> {
  /* Snapshot the just-POSTed values BEFORE the post-save
   * load() refresh overwrites them. `saved` emits between the
   * apiCall resolve and the load() — currentValues still holds
   * what was sent. Vue's defineExpose auto-unwraps refs, so
   * `formRef.value.currentValues` is the plain object here. */
  const exposedValues = formRef.value?.currentValues
  const savedValues =
    exposedValues && typeof exposedValues === 'object'
      ? { ...(exposedValues as Record<string, unknown>) }
      : {}
  if (props.beforeNext) {
    /* If the hook throws, swallow + log so the wizard doesn't
     * get stuck on a step. The user's data is already saved
     * server-side at this point — refusing to navigate would
     * be the worse failure mode. */
    try {
      await props.beforeNext(savedValues)
    } catch (e) {
      console.error('[wizard] beforeNext hook failed:', e)
    }
  }
  const next = nextStep.value
  if (next === null) {
    /* Channels is the last step — but channels uses
     * WizardStepChannels, not Generic. Generic should never see
     * a save with no next. Defensive: stay put. */
    return
  }
  await router.push({ name: `wizard-${next}` })
}

async function handlePrevious(): Promise<void> {
  const prev = prevStep.value
  if (prev === null) return
  await router.push({ name: `wizard-${prev}` })
}

async function handleCancel(): Promise<void> {
  await wizard.cancel()
  /* Server clears config.wizard and broadcasts accessUpdate.
   * Route guard would redirect on the next tick, but navigate
   * eagerly for snappier UX. */
  await router.push({ name: 'epg' })
}

/* Expose the form's template ref so step-specific wrappers
 * (WizardStepHello) can reach IdnodeConfigForm's imperative
 * surface — currentValues for inspection, save() / reload()
 * for the language-change live-preview orchestration. */
defineExpose({ formRef })
</script>

<template>
  <div class="wizard-step">
    <div class="wizard-step__scroll">
      <header
        v-if="iconUrl || descriptionHtml"
        class="wizard-step__header"
        :class="{ 'wizard-step__header--logo-right': step === 'hello' }"
      >
        <img v-if="iconUrl" :src="iconUrl" alt="" class="wizard-step__icon" />
        <!--
          Server-emitted Markdown description rendered through
          `marked` and v-html'd. Safe by trust: source is
          compiled-in `docs/wizard/*.md`, not user input.
          Matches ExtJS at `static/app/wizard.js:106`.
        -->
        <!-- eslint-disable-next-line vue/no-v-html -->
        <div v-if="descriptionHtml" class="wizard-step__description" v-html="descriptionHtml"></div>
      </header>
      <div class="wizard-step__form">
        <IdnodeConfigForm
          ref="formRef"
          :load-endpoint="loadEndpoint"
          :save-endpoint="saveEndpoint"
          :always-dirty="true"
          :hide-toolbar="true"
          @loaded="handleLoaded"
          @saved="handleSaved"
        />
      </div>
    </div>
    <WizardFooter
      :prev-step="prevStep"
      :saving="saving"
      @previous="handlePrevious"
      @cancel="handleCancel"
      @save="handleSave"
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
  /* Padding picks up the safe-area envelopes on the leading +
   * trailing edges. Resolves to plain var(--tvh-space-6) in a
   * regular browser tab; in PWA / fullscreen modes (macOS
   * rounded window corners, iOS notch/dynamic-island, iPadOS
   * Stage Manager) the insets push content inward so it
   * doesn't clip on the rounded edges. */
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

/* Hello step only — mirror the Classic UI's welcome-page
 * layout where the wizard logo sits on the trailing edge and
 * the introductory text reads from the leading edge.
 * `row-reverse` swaps visual order without changing the source
 * order (so screen readers still encounter the image first
 * as a decorative pre-header element). */
.wizard-step__header--logo-right {
  flex-direction: row-reverse;
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

/* Markdown-rendered shapes inside the description. Keep
 * paragraphs compact, lists left-aligned, links visually
 * distinct, and embedded images bounded so the OpenCollective
 * donate banner on the channels step doesn't blow up the
 * step header. */
.wizard-step__description :deep(p) {
  margin: 0 0 var(--tvh-space-2);
}
.wizard-step__description :deep(p:last-child) {
  margin-bottom: 0;
}
.wizard-step__description :deep(ul),
.wizard-step__description :deep(ol) {
  margin: 0 0 var(--tvh-space-2);
  /* `list-style-position: outside` (the default) hangs the
   * bullet marker in the padding-inline-start gutter. 1.5em
   * is roughly the browser default scaled to our 14px text;
   * smaller values clip the bullets against the description's
   * left edge. */
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

/* External-link marker — a small "opens in new tab" glyph
 * appended to any link `addExternalLinkAttrs` has stamped with
 * target="_blank". Tells the user up-front which links navigate
 * away (vs. internal markdown cross-links handled in-page).
 *
 * Implementation: CSS mask-image keeps the Lucide `ExternalLink`
 * shape rendered at the link's own colour (`currentColor`), so
 * the glyph inherits the primary-tinted link colour and any
 * future theme switch (e.g. dark mode) automatically tracks. */
.wizard-step__description :deep(a[target="_blank"])::after {
  content: '';
  display: inline-block;
  width: 0.85em;
  height: 0.85em;
  margin-left: 0.2em;
  vertical-align: -0.05em;
  background-color: currentColor;
  -webkit-mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='black' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='M15 3h6v6'/%3E%3Cpath d='M10 14 21 3'/%3E%3Cpath d='M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h6'/%3E%3C/svg%3E") no-repeat center / contain;
  mask: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='black' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='M15 3h6v6'/%3E%3Cpath d='M10 14 21 3'/%3E%3Cpath d='M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h6'/%3E%3C/svg%3E") no-repeat center / contain;
}
.wizard-step__description :deep(img) {
  max-width: 100%;
  height: auto;
}

.wizard-step__form {
  /* Form fills available width; IdnodeConfigForm owns its own
   * internal field grid layout. */
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
    /* Phone: drop the icon entirely. Vertical real estate is at
     * a premium and the description carries the same context. */
    display: none;
  }
}
</style>
