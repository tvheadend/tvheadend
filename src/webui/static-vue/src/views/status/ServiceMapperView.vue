<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Status → Service Mapper.
 *
 * Live monitor for the in-flight service-mapping job. Counters
 * + progress bar + currently-checking service + Stop button
 * when active, "Idle" with last-run summary otherwise.
 *
 * Updates ride the `'servicemapper'` Comet notification class
 * via `useServiceMapperStore`. Initial state pulls from
 * `service/mapper/status` on mount so a refresh mid-job lands
 * on the current view (Comet only delivers post-connect events).
 *
 * The configure-and-trigger form does NOT live here. It opens
 * as a modal dialog (`ServiceMapperDialog`) from the Channels
 * and DVB Services grid pages, mirroring Classic's UX where
 * mapping is started from the same grids that produced the
 * services in the first place. Splitting status (this page)
 * from action (the dialog) keeps the Status section's role
 * pure read-only monitoring.
 */
import { onMounted } from 'vue'
import { HelpCircle } from 'lucide-vue-next'
import { useServiceMapperStore } from '@/stores/serviceMapper'
import { useI18n } from '@/composables/useI18n'
import { useHelp } from '@/composables/useHelp'

const { t } = useI18n()
const help = useHelp()

const store = useServiceMapperStore()

const HELP_PAGE = 'status_service_mapper'

function onHelpClick(): void {
  help.toggle(HELP_PAGE).catch(() => {})
}

onMounted(() => {
  /* Initial snapshot. Comet will take over once the first push
   * arrives — usually within 1s of any state change. */
  store.fetchInitial().catch(() => {
    /* fetchInitial swallows errors internally; the .catch is
     * here to satisfy lint without using `void`. */
  })
})
</script>

<template>
  <div class="service-mapper">
    <section class="service-mapper__card">
      <header class="service-mapper__card-header">
        <h2 class="service-mapper__card-title">{{ t('Service Mapping Status') }}</h2>
        <div class="service-mapper__card-header-actions">
          <button
            v-if="store.isActive"
            type="button"
            class="service-mapper__btn service-mapper__btn--stop"
            :disabled="store.stopping"
            @click="store.stop()"
          >
            {{ store.stopping ? t('Stopping…') : t('Stop') }}
          </button>
          <!-- Help — matches the icon-button shape the grid surfaces
               use so the affordance reads consistently across the
               Status section. -->
          <button
            v-tooltip.bottom="t('Help')"
            type="button"
            class="service-mapper__help"
            :aria-label="t('Help')"
            :aria-pressed="help.isOpen.value"
            @click="onHelpClick"
          >
            <HelpCircle :size="16" :stroke-width="2" />
          </button>
        </div>
      </header>
      <div class="service-mapper__status-body">
        <p v-if="!store.isActive" class="service-mapper__idle">
          <strong>{{ t('Idle.') }}</strong>
          <template v-if="store.status.total > 0">
            {{ t('Last run mapped {0} of {1} services', store.status.ok, store.status.total) }}
            <template v-if="store.status.fail > 0">{{
              t(', {0} failed', store.status.fail)
            }}</template>
            <template v-if="store.status.ignore > 0">{{
              t(', {0} ignored', store.status.ignore)
            }}</template>.
          </template>
          <template v-else>
            {{ t('Open Configuration → Channel/EPG → Channels or Configuration → DVB Inputs → Services to start mapping.') }}
          </template>
        </p>
        <template v-else>
          <p class="service-mapper__active">
            {{ t('Mapping in progress — currently checking') }}
            <strong>{{ store.activeServiceName ?? store.status.active }}</strong>
          </p>
          <div class="service-mapper__progress">
            <div class="service-mapper__progress-track">
              <div
                class="service-mapper__progress-fill"
                :style="{ width: (store.progressFraction * 100).toFixed(1) + '%' }"
              />
            </div>
            <p class="service-mapper__progress-text">
              {{ t('{0} / {1} processed ({2}%)', store.processed, store.status.total, Math.round(store.progressFraction * 100)) }}
            </p>
          </div>
        </template>
        <dl class="service-mapper__counters">
          <div class="service-mapper__counter">
            <dt>{{ t('Mapped') }}</dt>
            <dd>{{ store.status.ok }}</dd>
          </div>
          <div class="service-mapper__counter">
            <dt>{{ t('Failed') }}</dt>
            <dd>{{ store.status.fail }}</dd>
          </div>
          <div class="service-mapper__counter">
            <dt>{{ t('Ignored') }}</dt>
            <dd>{{ store.status.ignore }}</dd>
          </div>
          <div class="service-mapper__counter">
            <dt>{{ t('Total') }}</dt>
            <dd>{{ store.status.total }}</dd>
          </div>
        </dl>
        <p v-if="store.error" class="service-mapper__error">{{ store.error }}</p>
      </div>
    </section>
  </div>
</template>

<style scoped>
.service-mapper {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-4);
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
  padding: var(--tvh-space-2);
}

.service-mapper__card {
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  padding: var(--tvh-space-4);
}

.service-mapper__card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--tvh-space-3);
  margin-bottom: var(--tvh-space-3);
}

.service-mapper__card-title {
  margin: 0;
  font-size: var(--tvh-text-lg);
  font-weight: 600;
  color: var(--tvh-text);
}

.service-mapper__status-body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
}

.service-mapper__idle,
.service-mapper__active {
  margin: 0;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
  line-height: 1.5;
}

.service-mapper__error {
  margin: 0;
  font-size: var(--tvh-text-md);
  color: var(--tvh-error);
}

.service-mapper__progress-track {
  width: 100%;
  height: 8px;
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: 999px;
  overflow: hidden;
}

.service-mapper__progress-fill {
  height: 100%;
  background: var(--tvh-primary);
  /* Smooth the fraction tick — comet emits ~1 update/sec
   * during a job, the eye prefers a sliding bar to a
   * stepped one. */
  transition: width 250ms ease-out;
}

.service-mapper__progress-text {
  margin: var(--tvh-space-2) 0 0;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.service-mapper__counters {
  margin: 0;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: var(--tvh-space-2);
}

.service-mapper__counter {
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-2) var(--tvh-space-3);
}

.service-mapper__counter > dt {
  margin: 0 0 2px;
  font-size: var(--tvh-text-xs);
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
}

.service-mapper__counter > dd {
  margin: 0;
  font-size: var(--tvh-text-2xl);
  font-weight: 600;
  color: var(--tvh-text);
  font-variant-numeric: tabular-nums;
}

.service-mapper__btn {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px var(--tvh-space-3);
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.service-mapper__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.service-mapper__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.service-mapper__btn--stop {
  border-color: var(--tvh-error);
  color: var(--tvh-error);
}

.service-mapper__btn--stop:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-error) var(--tvh-hover-strength), transparent);
}

@media (max-width: 767px) {
  .service-mapper {
    padding: var(--tvh-space-1);
    gap: var(--tvh-space-3);
  }
  .service-mapper__card {
    padding: var(--tvh-space-3);
  }
}

/* Card-header right cluster — groups the Stop button (when
 * active) and the Help icon button so they sit together at the
 * trailing edge of the header strip. */
.service-mapper__card-header-actions {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

/* Help button — same 32 px icon-button shape the grid surfaces
 * use, so the help affordance reads consistently across the
 * Status section. */
.service-mapper__help {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.service-mapper__help:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-page)
  );
}

.service-mapper__help:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.service-mapper__help[aria-pressed='true'] {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
}
</style>
