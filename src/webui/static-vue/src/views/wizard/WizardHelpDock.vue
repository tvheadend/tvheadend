<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * WizardHelpDock — thin wrapper around `HelpPanelInner` that
 * gives the wizard help its dock chrome: a flex-sibling aside
 * inside `.wizard-layout__body` on desktop, flipping to a
 * position-fixed full-screen overlay on phone. Slide-in /
 * slide-out transition on open / close.
 *
 * The shared `HelpPanelInner` owns the header (breadcrumb +
 * back + close), the body rendering, and the click handler
 * for in-panel navigation. This wrapper only handles the
 * outer shell and the Escape-to-close key binding.
 *
 * Mode flip is reactive — `isPhone` tracks the shared breakpoint
 * so a browser drag across it re-renders into the right shape
 * without losing scroll position.
 */
import { onBeforeUnmount, onMounted } from 'vue'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'
import { useHelp } from '@/composables/useHelp'
import HelpPanelInner from '@/components/HelpPanelInner.vue'

const { t } = useI18n()
const help = useHelp()

const isPhone = useIsPhone()

function onKey(ev: KeyboardEvent) {
  if (ev.key === 'Escape' && help.isOpen.value) help.close()
}

onMounted(() => {
  document.addEventListener('keydown', onKey)
})

onBeforeUnmount(() => {
  document.removeEventListener('keydown', onKey)
})
</script>

<template>
  <Transition name="dock-slide">
    <aside
      v-if="help.isOpen.value"
      class="wizard-help-dock"
      :class="{ 'wizard-help-dock--phone': isPhone }"
      :aria-label="t('Help')"
    >
      <HelpPanelInner />
    </aside>
  </Transition>
</template>

<style scoped>
.wizard-help-dock {
  /* Desktop default: flex sibling of the wizard step inside
   * .wizard-layout__body. Takes a fixed slot; step content
   * fills the rest. */
  flex: 0 0 400px;
  display: flex;
  flex-direction: column;
  min-height: 0;
  background: var(--tvh-bg-surface);
  border-left: 1px solid var(--tvh-border);
  overflow: hidden;
}

.wizard-help-dock--phone {
  /* Phone: modal full-screen overlay. Detaches from the flex
   * row so the step body isn't squeezed; positioned above
   * everything inside the wizard layout. */
  position: fixed;
  inset: 0;
  z-index: 200;
  flex: none;
  border-left: none;
}

/* Slide-in / slide-out transition. Animates `transform` so the
 * panel feels anchored to its right edge whether it's the
 * desktop dock or the phone overlay. */
.dock-slide-enter-active,
.dock-slide-leave-active {
  transition: transform 180ms ease-out, opacity 180ms ease-out;
}

.dock-slide-enter-from,
.dock-slide-leave-to {
  transform: translateX(100%);
  opacity: 0;
}
</style>
