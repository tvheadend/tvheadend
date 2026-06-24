<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CaStatusCell — grid-cell renderer for the CA Clients Status
 * column. Maps the four enum strings emitted by `caclient_get_status`
 * (`src/descrambler/caclient.c:389-396`) to a Lucide icon + colour
 * + plain-English tooltip.
 *
 *   caclientNone          → Circle        (gray, "Idle")
 *   caclientReady         → CircleDot     (green, "Ready")
 *   caclientConnected     → CircleCheck   (green, "Connected")
 *   caclientDisconnected  → CircleAlert   (orange, "Disconnected")
 *
 * Page-local (not shared in `components/`) because the enum
 * vocabulary is CA-specific. If a second consumer surfaces, lift
 * to `components/` with a generic `enumMap` prop.
 */
import { computed, type Component } from 'vue'
import { Circle, CircleAlert, CircleCheck, CircleDot } from 'lucide-vue-next'
import { t } from '@/composables/useI18n'

const props = defineProps<{ value: unknown }>()

interface StatusEntry {
  icon: Component
  label: string
  cls: string
}

const STATES: Record<string, StatusEntry> = {
  caclientNone: { icon: Circle, label: t('Idle'), cls: 'ca-status--idle' },
  caclientReady: { icon: CircleDot, label: t('Ready'), cls: 'ca-status--ready' },
  caclientConnected: { icon: CircleCheck, label: t('Connected'), cls: 'ca-status--connected' },
  caclientDisconnected: {
    icon: CircleAlert,
    label: t('Disconnected'),
    cls: 'ca-status--disconnected',
  },
}

const state = computed<StatusEntry | null>(() => {
  if (typeof props.value !== 'string') return null
  return STATES[props.value] ?? null
})
</script>

<template>
  <!-- Wrap in a span so PrimeVue's `v-tooltip` directive attaches
       to a regular HTML element. The native `title` attribute
       doesn't surface on Lucide's SVG output reliably across
       browsers, and the rest of the UI uses PrimeVue's tooltip
       (registered globally in `main.ts:65`) for consistent
       look-and-feel. -->
  <span v-if="state" v-tooltip.bottom="state.label" class="ca-status-wrap">
    <component
      :is="state.icon"
      :size="16"
      class="ca-status"
      :class="state.cls"
      :aria-label="state.label"
    />
  </span>
</template>

<style scoped>
.ca-status {
  display: inline-block;
  vertical-align: middle;
}

.ca-status--idle {
  color: var(--tvh-text-muted);
}

.ca-status--ready {
  color: var(--tvh-success);
}

.ca-status--connected {
  color: var(--tvh-success);
}

.ca-status--disconnected {
  color: var(--tvh-warning);
}
</style>
