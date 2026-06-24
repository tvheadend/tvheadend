<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardFooter — the uniform action bar at the bottom of every
 * wizard step. Renders the standard set of wizard buttons and
 * emits intent events upward; the step component (or store)
 * decides what each event actually does.
 *
 * Buttons (left-to-right):
 *   - Cancel       — always rendered. Confirms with the user
 *                    before firing `cancel` (partial config
 *                    sticks; matches ADR 0015's "no rollback").
 *   - Previous     — rendered when `prevStep` is non-null.
 *                    Emits `previous` (parent navigates back).
 *   - Skip         — rendered when `showSkip` is true (status
 *                    + optional mapping). Emits `skip`.
 *   - Save & Next  — primary action. Label flips to "Finish" on
 *                    the last (channels) step. Disabled while
 *                    `saving` is true. Emits `save`.
 *
 * Cancel-confirmation copy follows ADR 0015 § "no rollback" —
 * the user is told partial config persists on cancel so they
 * have one chance to back out.
 *
 * i18n: every label goes through `t()`. "Previous", "Cancel",
 * "Save & Next", "Finish" — all reused verbatim from the
 * ExtJS wizard (`src/webui/static/app/wizard.js`), so
 * translations are immediately available.
 */
import { computed } from 'vue'
import Button from 'primevue/button'
import { useI18n } from '@/composables/useI18n'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useHelp } from '@/composables/useHelp'

interface Props {
  /* Step name of the previous step in canonical order. Render
   * the Previous button iff this is non-null. */
  prevStep: string | null
  /* When true, the Save button shows "Finish" rather than
   * "Save & Next" (channels step only). */
  isFinal?: boolean
  /* When true, render the Skip button (status step polling, or
   * mapping if the user opts out of auto-mapping). */
  showSkip?: boolean
  /* Disables the Save button (e.g. while a save POST is in
   * flight). */
  saving?: boolean
  /* Markdown page name passed to `useHelp().toggle()` for the
   * Help button. ExtJS wizard uses `firstconfig` for every step
   * (`wizard.js:165`); we match. Set to null to hide the Help
   * button.
   *
   * Clicking Help toggles the `WizardHelpDock` panel (docked
   * beside the step on desktop, full-screen modal on phone)
   * with the rendered markdown plus breadcrumb-based navigation
   * between cross-linked help pages. Mirrors the
   * `tvheadend.mdhelp` shape from Classic. */
  helpPage?: string | null
  /* When true, the primary Save / Save & Next / Finish button
   * is not rendered. Used by the status step where there's
   * nothing to save — just the polling + user-driven Skip /
   * Next advance. */
  hideSave?: boolean
  /* Label for the Skip button. Defaults to 'Skip'; the status
   * step swaps to 'Next' once the scan reaches 100 % so the
   * button reads as a continuation rather than an opt-out.
   * Caller passes the English string; the footer t()s it for
   * i18n consistency with the other labels here. */
  skipLabel?: string
  /* When true, the Skip button promotes from outlined-secondary
   * to the primary filled style (same look as Save & Next /
   * Finish). Used by the status step at 100 % progress so the
   * "continue from here" affordance pulls the user's eye the
   * moment the scan completes — paired with the skipLabel flip
   * from 'Skip' to 'Next'. Default false: Skip stays outlined
   * (opt-out semantics). */
  skipEmphasized?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  isFinal: false,
  showSkip: false,
  saving: false,
  helpPage: 'firstconfig',
  hideSave: false,
  skipLabel: 'Skip',
  skipEmphasized: false,
})

const emit = defineEmits<{
  previous: []
  cancel: []
  skip: []
  save: []
}>()

const { t } = useI18n()
const { ask } = useConfirmDialog()
const help = useHelp()

const saveLabel = computed(() =>
  /* Reused from ExtJS wizard.js line 160 — translations free. */
  props.isFinal ? t('Finish') : t('Save & Next'),
)

/* `aria-pressed` + active-state class on the Help button reflect
 * whether the dock is open. The toggle semantics close the dock
 * regardless of which page is showing (so a user who has
 * navigated deeper inside the dock sees "Help is on" until they
 * close it), which means this footer's button stays lit even if
 * the user is currently reading a cross-linked sub-page rather
 * than the wizard's default `firstconfig`. */
const helpOpen = computed(() => help.isOpen.value)

function handleHelp(): void {
  if (!props.helpPage) return
  /* Toggle the in-app WizardHelpDock — desktop split-view dock
   * to the right of the step, phone modal full-screen overlay.
   * State lives in `useHelp` (module singleton) so the
   * panel survives wizard step navigation. A second click on
   * Help (with the same page already open) closes the dock. */
  help.toggle(props.helpPage).catch(() => {})
}

async function handleCancel(): Promise<void> {
  /* i18n: new string — cancel-confirmation prose. ExtJS wizard
   * doesn't have a cancel button (the wizard runs in a closable
   * dialog whose close-X just hides the modal without server
   * cleanup), so this prompt is novel. Falls back to English
   * until xgettext is taught to scan Vue files. */
  const ok = await ask(
    t(
      'Cancelling will exit the setup wizard, but any settings you have already saved on previous steps will remain. Continue?',
    ),
    {
      header: t('Cancel setup wizard?'),
      acceptLabel: t('Cancel wizard'),
      rejectLabel: t('Continue setup'),
    },
  )
  if (ok) emit('cancel')
}
</script>

<template>
  <div class="wizard-footer">
    <div class="wizard-footer__meta">
      <Button
        :label="t('Cancel')"
        severity="secondary"
        text
        class="wizard-footer__cancel"
        @click="handleCancel"
      />
      <Button
        v-if="helpPage"
        :label="t('Help')"
        severity="secondary"
        text
        class="wizard-footer__help"
        :class="{ 'wizard-footer__help--active': helpOpen }"
        :aria-pressed="helpOpen"
        @click="handleHelp"
      />
    </div>
    <div class="wizard-footer__primary">
      <Button
        v-if="prevStep"
        :label="t('Previous')"
        severity="secondary"
        outlined
        @click="emit('previous')"
      />
      <!-- i18n: "Skip" and "Next" are both novel strings.
           ExtJS wizard has no Skip button — we add one for the
           long-running scan step (status) so the user isn't
           stuck waiting through the polling interval; the
           status step flips the label to "Next" at 100 % so the
           button reads as a continuation. Both fall back to
           English until xgettext is taught to scan Vue files. -->
      <Button
        v-if="showSkip"
        :label="t(skipLabel)"
        :severity="skipEmphasized ? undefined : 'secondary'"
        :outlined="!skipEmphasized"
        @click="emit('skip')"
      />
      <Button
        v-if="!hideSave"
        :label="saveLabel"
        :loading="saving"
        :disabled="saving"
        class="wizard-footer__save"
        @click="emit('save')"
      />
    </div>
  </div>
</template>

<style scoped>
.wizard-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: var(--tvh-space-3);
  width: 100%;
  /* Background + top border so the footer reads as a sticky
   * action bar even when the body scroll lands flush against
   * it (the step layout puts the scroll above; the footer is
   * the bottom edge of the viewport). */
  background: var(--tvh-bg-surface);
  border-top: 1px solid var(--tvh-border);
  /* Padding picks up the safe-area envelopes on the trailing
   * edges. In a regular browser tab `env(safe-area-inset-*)`
   * resolves to 0, so this matches the prior --tvh-space-3
   * padding exactly. In a macOS PWA window (rounded corners),
   * iOS PWA fullscreen (home indicator + dynamic-island
   * insets), or iPadOS Stage Manager, the insets push the
   * actions inward so they don't clip on the rounded edges. */
  padding-top: var(--tvh-space-3);
  padding-bottom: calc(var(--tvh-space-3) + env(safe-area-inset-bottom));
  padding-left: calc(var(--tvh-space-6) + env(safe-area-inset-left));
  padding-right: calc(var(--tvh-space-6) + env(safe-area-inset-right));
  flex-shrink: 0;
}

.wizard-footer__meta {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-1);
}

.wizard-footer__primary {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

/* Help button active state — tinted primary-colour background
 * while the WizardHelpDock is open for this footer's helpPage.
 * Mirrors the visual "pressed" affordance toggle buttons use
 * elsewhere; ARIA exposes the same state via aria-pressed. The
 * :deep() reaches into PrimeVue's inner button element since
 * Button is a wrapper component and our class lands on the
 * outer host. */
.wizard-footer__help--active,
.wizard-footer__help--active:deep(.p-button) {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
}

@media (max-width: 767px) {
  .wizard-footer {
    flex-direction: column-reverse;
    align-items: stretch;
    gap: var(--tvh-space-2);
    /* Phone overrides the desktop padding to a tighter base
     * while preserving the safe-area envelope. Matches
     * `.wizard-step__scroll`'s phone padding. */
    padding-top: var(--tvh-space-3);
    padding-bottom: calc(var(--tvh-space-3) + env(safe-area-inset-bottom));
    padding-left: calc(var(--tvh-space-3) + env(safe-area-inset-left));
    padding-right: calc(var(--tvh-space-3) + env(safe-area-inset-right));
  }

  .wizard-footer__meta {
    /* On phone, keep Cancel + Help side-by-side as a single row
     * at the bottom rather than stacking — they're both
     * thumb-reachable secondary actions. */
    justify-content: space-between;
  }

  .wizard-footer__primary {
    /* On phone, primary actions become a single stacked column.
     * column-reverse so that Save & Next sits on top (closest
     * to the form), Previous / Skip below — matches the user's
     * forward-flow expectation. */
    flex-direction: column-reverse;
    align-items: stretch;
  }

  .wizard-footer__cancel {
    /* On phone, cancel becomes the bottom-most action so it's
     * out of the thumb-zone of the primary flow. */
  }
}
</style>
