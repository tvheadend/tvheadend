<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardProgress — step indicator strip for the wizard layout.
 *
 * Two visual modes:
 *
 *   Desktop (>= 768 px): seven named pills, one per step, with
 *     completed / current / pending states.
 *   Phone   (< 768 px):  compact "Step N of 7 — <name>" text.
 *
 * Reads the active step from the current route name (e.g.
 * `wizard-network` → `network`). Source of truth for the
 * highlight is the URL the user is looking at. The wizard
 * store's `currentStep` (which mirrors `access.data.wizard`)
 * would be the more correct source — except that the server
 * only emits `accessUpdate` once at comet-mailbox creation
 * (`comet.c:300`), so the cursor goes stale after the first
 * step. Reverting to the server cursor as source of truth is
 * pending live-propagation of cursor changes server-side.
 *
 * Step labels match the C-side `ic_caption` strings (`wizard.c`
 * — `N_("Welcome")`, etc.) so once `xgettext` is extended to
 * scan `static-vue/` (pending), these strings get extracted
 * automatically and the Transifex round-trip provides
 * translations. Until then they fall back to English on every
 * locale.
 */
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { WIZARD_STEPS, type WizardStepName } from '@/stores/wizard'
import { useI18n } from '@/composables/useI18n'

const route = useRoute()
const { t } = useI18n()

/*
 * Progress reflects the user's current URL route, not the
 * server's `config.wizard` cursor. The cursor IS the source of
 * truth for "which step is active" but the server only emits
 * `accessUpdate` once at comet-mailbox creation
 * (`comet.c:300`); subsequent cursor changes don't broadcast,
 * so `access.data.wizard` is frozen post-initial-connect. Using
 * the route name keeps the visual progress aligned with what
 * the user sees in their address bar (and what `IdnodeConfigForm`
 * is actually loading), which is more useful than a stale
 * cursor value. A server-side live-propagation of global config
 * changes to connected sessions would let us go back to the
 * cursor as source of truth.
 */
function routeNameToStep(name: unknown): string {
  if (typeof name !== 'string') return ''
  if (!name.startsWith('wizard-')) return ''
  return name.slice('wizard-'.length)
}
const activeStep = computed<string>(() => routeNameToStep(route.name))

/*
 * Step labels mirror the C-side `ic_caption` strings declared in
 * `src/wizard.c`. These literals are NOVEL to the Vue UI's
 * client-side i18n surface today — the JS `/locale.js`
 * dictionary is generated from `static/app/*.js` only and
 * doesn't include the C-side `N_(...)` strings. They fall
 * back to English on every locale until xgettext is taught to
 * scan `static-vue/` files (pending). Each is marked
 * `/* i18n: new string *\/` so a future audit finds them. */
const STEP_LABELS: Record<WizardStepName, string> = {
  /* i18n: new string */ hello: 'Welcome',
  /* i18n: new string */ login: 'Access Control',
  /* i18n: new string */ network: 'Tuner and Network',
  /* i18n: new string */ muxes: 'Predefined Muxes',
  /* i18n: new string */ status: 'Scanning',
  /* i18n: new string */ mapping: 'Service Mapping',
  /* i18n: new string */ channels: 'Finished',
}

const currentIndex = computed<number>(() => {
  const idx = WIZARD_STEPS.indexOf(activeStep.value as WizardStepName)
  return idx === -1 ? 0 : idx
})

interface PillState {
  step: WizardStepName
  label: string
  state: 'completed' | 'current' | 'pending'
}

function pillState(idx: number, current: number): PillState['state'] {
  if (idx < current) return 'completed'
  if (idx === current) return 'current'
  return 'pending'
}

const pills = computed<PillState[]>(() =>
  WIZARD_STEPS.map((step, idx) => ({
    step,
    label: t(STEP_LABELS[step]),
    state: pillState(idx, currentIndex.value),
  })),
)

/* Phone-mode label — short prose. Counter renders 1-of-7 (not
 * 0-of-7), and the active step's name follows after an em-dash.
 * The "Step", "of", and the dash are markup-friendly so the
 * t() lookup happens once on the whole sentence (less sensitive
 * to translator conventions on connector words). */
const phoneLabel = computed(() => {
  const n = currentIndex.value + 1
  const total = WIZARD_STEPS.length
  const stepLabel = t(STEP_LABELS[activeStep.value as WizardStepName] ?? 'hello')
  /* i18n: new string — sentence template; concatenation is
   * unavoidable today without a printf-style helper, but the
   * three pieces are simple enough that translators can
   * recompose them in their target language. */
  return `${t('Step')} ${n} ${t('of')} ${total} — ${stepLabel}`
})
</script>

<template>
  <!-- Desktop: pills row -->
  <ol class="wizard-progress wizard-progress--desktop" :aria-label="t('Setup Wizard progress')">
    <li
      v-for="pill in pills"
      :key="pill.step"
      class="wizard-progress__pill"
      :class="`wizard-progress__pill--${pill.state}`"
      :aria-current="pill.state === 'current' ? 'step' : undefined"
    >
      {{ pill.label }}
    </li>
  </ol>

  <!-- Phone: compact text -->
  <div class="wizard-progress wizard-progress--phone" :aria-label="t('Setup Wizard progress')">
    <span class="wizard-progress__phone-text">{{ phoneLabel }}</span>
  </div>
</template>

<style scoped>
.wizard-progress {
  margin: 0;
  padding: 0;
}

/* Pills (desktop) — visible at >= 768 px. */
.wizard-progress--desktop {
  display: flex;
  list-style: none;
  gap: var(--tvh-space-2);
  flex-wrap: wrap;
}

.wizard-progress__pill {
  display: inline-flex;
  align-items: center;
  padding: 4px 10px;
  font-size: var(--tvh-text-md);
  font-weight: 500;
  border-radius: 999px;
  border: 1px solid var(--tvh-border);
  background: var(--tvh-bg-page);
  color: var(--tvh-text-muted);
  white-space: nowrap;
}

.wizard-progress__pill--completed {
  background: color-mix(in srgb, var(--tvh-primary) 12%, var(--tvh-bg-page));
  border-color: color-mix(in srgb, var(--tvh-primary) 30%, var(--tvh-border));
  color: var(--tvh-text);
}

.wizard-progress__pill--current {
  background: var(--tvh-primary);
  color: var(--tvh-bg-page);
  border-color: var(--tvh-primary);
  font-weight: 600;
}

.wizard-progress__pill--pending {
  /* Inherits the base muted look. */
}

/* Phone — visible at < 768 px. */
.wizard-progress--phone {
  display: none;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
}

.wizard-progress__phone-text {
  font-weight: 500;
}

@media (max-width: 767px) {
  .wizard-progress--desktop {
    display: none;
  }
  .wizard-progress--phone {
    display: block;
  }
}
</style>
