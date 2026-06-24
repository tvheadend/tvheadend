<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardLayout — full-viewport chrome for the setup wizard. Pre-
 * empts the regular UI: when the user is on a `/wizard/*` route,
 * AppShell's NavRail / TopBar are NOT rendered — this layout owns
 * everything from header to body.
 *
 * Two regions:
 *   - Header: logo + "Setup Wizard" title + WizardProgress strip.
 *     The strip is uniform across every step (always reflects the
 *     current step from the store), so it lives in the layout
 *     directly.
 *   - Body: <router-view /> — the active step component. Each step
 *     owns its own internal layout including its WizardFooter
 *     action bar (button configuration is per-step: Previous off
 *     on hello, Skip on mapping, Finish on channels, etc.).
 *
 * Responsive: the desktop layout flips to a single-column phone
 * shape at < 768 px. Header collapses.
 *
 * i18n: every user-facing chrome string in this component goes
 * through `t()`. Strings reuse the ExtJS wizard's literals where
 * possible (translations free); novel strings are marked with
 * `/* i18n: new string *\/`.
 */
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import ConfirmDialog from 'primevue/confirmdialog'
import { HelpCircle } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import WizardProgress from './WizardProgress.vue'
import WizardHelpDock from './WizardHelpDock.vue'

const { t } = useI18n()
const route = useRoute()

/*
 * Key passed to the inner router-view to force a full
 * unmount/remount on every wizard route change. See the
 * template comment for the reasoning. Computed instead of
 * inlining `$route.fullPath` so unit tests can mount this
 * component without installing the full router — the `useRoute`
 * mock returns a deterministic value and `routeKey` reads from
 * it via the composable rather than the template's auto-
 * exposed `$route`.
 */
const routeKey = computed(() => route.fullPath ?? '')

/*
 * Cross-UI logo reference. Bound as an expression (not a literal
 * `src`) so Vite's Vue plugin doesn't try to bundle the asset —
 * the URL is served at runtime by tvheadend's existing /static
 * handler from the ExtJS bundle.
 */
const logoUrl = '/static/img/logo.png'
</script>

<template>
  <div class="wizard-layout">
    <header class="wizard-layout__header">
      <img :src="logoUrl" alt="" class="wizard-layout__logo" />
      <h1 class="wizard-layout__title">{{ t('Setup Wizard') }}</h1>
      <div class="wizard-layout__progress">
        <WizardProgress />
      </div>
    </header>
    <main class="wizard-layout__body">
      <!--
        Keying the router-view by the current route's fullPath
        forces a full unmount/remount on every wizard route
        change. Without it, Vue Router reuses the same
        `WizardStepGeneric` instance across login → network →
        muxes → mapping (all four are the same component class,
        only the `step` prop differs), and `IdnodeConfigForm`'s
        load runs only from `onMounted()` — so the new
        `loadEndpoint` prop never refetches. Result is a one-
        step content lag (URL + progress strip advance, body
        still shows the previous step).

        This is a workaround. The proper fix lives in
        `IdnodeConfigForm` — it should `watch(() =>
        props.loadEndpoint, () => load())` so prop changes
        trigger a fresh fetch without forcing a tree remount.
        Once that reactive-reload behaviour lands (pending),
        this `:key` can come off; the wizard step components
        will update in-place rather than thrashing the DOM.
      -->
      <router-view :key="routeKey" class="wizard-layout__step" />
      <!--
        Help dock — desktop split-view sibling of the step;
        flips to a phone-modal full-screen overlay below the
        768 px breakpoint via its own scoped CSS. Open / close
        state lives in `useHelp` (module-level singleton)
        so it survives step navigation; WizardFooter's Help
        button toggles via the same composable.
      -->
      <WizardHelpDock />
    </main>
    <!--
      Singleton ConfirmDialog mount for wizard routes. The wizard
      bypasses AppShell, where the main ConfirmDialog instance
      lives, so we need our own here for WizardFooter's
      cancel-confirmation prompt. Matches AppShell's icon-slot
      shape (Lucide HelpCircle for visual parity).
    -->
    <ConfirmDialog>
      <template #icon>
        <HelpCircle :size="22" :stroke-width="2" />
      </template>
    </ConfirmDialog>
  </div>
</template>

<style scoped>
.wizard-layout {
  position: fixed;
  inset: 0;
  display: flex;
  flex-direction: column;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  z-index: 100;
}

.wizard-layout__header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  background: var(--tvh-bg-surface);
  border-bottom: 1px solid var(--tvh-border);
  flex-shrink: 0;
  /* Padding picks up the safe-area envelopes on the leading +
   * trailing edges + top. Resolves to plain
   * `var(--tvh-space-3) var(--tvh-space-6)` in a regular
   * browser tab; in PWA / fullscreen modes (macOS rounded
   * window corners, iOS notch, iPadOS Stage Manager) the
   * insets push the chrome inward so the logo + title don't
   * clip on the rounded edges. */
  padding-top: calc(var(--tvh-space-3) + env(safe-area-inset-top));
  padding-bottom: var(--tvh-space-3);
  padding-left: calc(var(--tvh-space-6) + env(safe-area-inset-left));
  padding-right: calc(var(--tvh-space-6) + env(safe-area-inset-right));
}

.wizard-layout__logo {
  height: 28px;
  width: auto;
  flex-shrink: 0;
}

.wizard-layout__title {
  margin: 0;
  font-size: var(--tvh-text-xl); /* @snap-from: 17px */
  font-weight: 600;
  letter-spacing: -0.01em;
  flex-shrink: 0;
}

.wizard-layout__progress {
  flex: 1;
  display: flex;
  justify-content: center;
  min-width: 0;
}

.wizard-layout__body {
  flex: 1 1 auto;
  display: flex;
  /* Horizontal row on desktop so the WizardHelpDock can sit
   * beside the step as a split-view sibling. The step itself
   * still stacks header / scroll / footer vertically (its own
   * scoped `flex-direction: column` inside `.wizard-step`). */
  flex-direction: row;
  min-height: 0;
}

.wizard-layout__step {
  /* Flex-shrink-friendly so the step body narrows when the
   * help dock takes its 400 px slice. `min-width: 0` is the
   * standard flex escape — without it, intrinsic content
   * width keeps the step from shrinking below its widest
   * line. */
  flex: 1 1 auto;
  min-width: 0;
  display: flex;
  flex-direction: column;
}

@media (max-width: 767px) {
  .wizard-layout__header {
    flex-wrap: wrap;
    gap: var(--tvh-space-2);
    padding-top: calc(var(--tvh-space-2) + env(safe-area-inset-top));
    padding-bottom: var(--tvh-space-2);
    padding-left: calc(var(--tvh-space-3) + env(safe-area-inset-left));
    padding-right: calc(var(--tvh-space-3) + env(safe-area-inset-right));
  }

  .wizard-layout__title {
    font-size: var(--tvh-text-xl); /* @snap-from: 15px */
  }

  .wizard-layout__progress {
    /* Phone: progress strip wraps to its own row below the
     * logo + title, full-width. */
    flex-basis: 100%;
    justify-content: flex-start;
  }
}
</style>
