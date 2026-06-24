<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HomeCard — one card on the Home dashboard, rendering a generic
 * registry entry (homeCards.ts): a `nav` card is a RouterLink, an
 * `action` card is a button that emits its action id, a `notice`
 * card is a static block. Bespoke cards (the TV-tier activity
 * widgets, the Server health line) get their own components.
 */
import { useI18n } from '@/composables/useI18n'
import type { HomeCard } from './homeCards'

defineProps<{ card: HomeCard; primary?: boolean }>()
const emit = defineEmits<{ action: [actionId: string] }>()

const { t } = useI18n()
</script>

<template>
  <!-- nav -->
  <RouterLink
    v-if="card.kind === 'nav' && card.to"
    :to="card.to"
    class="home-card"
    :class="{ 'home-card--primary': primary }"
  >
    <component :is="card.icon" :size="22" :stroke-width="2" class="home-card__icon" />
    <span class="home-card__text">
      <span class="home-card__title">{{ t(card.title) }}</span>
      <span class="home-card__desc">{{ t(card.description) }}</span>
    </span>
  </RouterLink>

  <!-- action -->
  <button
    v-else-if="card.kind === 'action'"
    type="button"
    class="home-card"
    :class="{ 'home-card--primary': primary }"
    @click="card.action && emit('action', card.action)"
  >
    <component :is="card.icon" :size="22" :stroke-width="2" class="home-card__icon" />
    <span class="home-card__text">
      <span class="home-card__title">{{ t(card.title) }}</span>
      <span class="home-card__desc">{{ t(card.description) }}</span>
    </span>
  </button>

  <!-- notice -->
  <div v-else class="home-card home-card--notice">
    <component :is="card.icon" :size="22" :stroke-width="2" class="home-card__icon" />
    <span class="home-card__text">
      <span class="home-card__title">{{ t(card.title) }}</span>
      <span class="home-card__desc">{{ t(card.description) }}</span>
    </span>
  </div>
</template>

<style scoped>
.home-card {
  display: flex;
  align-items: flex-start;
  gap: var(--tvh-space-3);
  width: 100%;
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  color: var(--tvh-text);
  text-align: left;
  text-decoration: none;
  font: inherit;
  cursor: pointer;
  transition:
    border-color var(--tvh-transition),
    background var(--tvh-transition);
}

/* Notice cards are informational, not interactive. */
.home-card--notice {
  cursor: default;
}

a.home-card:hover,
button.home-card:hover {
  border-color: var(--tvh-primary);
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

a.home-card:focus-visible,
button.home-card:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* The recommended next step — the guidance action card. */
.home-card--primary {
  border-color: var(--tvh-primary);
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-active-strength),
    var(--tvh-bg-surface)
  );
}

.home-card__icon {
  flex-shrink: 0;
  margin-top: 1px;
  color: var(--tvh-primary);
}

.home-card--notice .home-card__icon {
  color: var(--tvh-text-muted);
}

.home-card__text {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.home-card__title {
  font-weight: 600;
}

.home-card__desc {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}
</style>
