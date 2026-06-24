<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HealthLine — the Server-tier "is everything OK?" widget on the
 * Home dashboard (ADR 0017). Shows disk runway (free space as a
 * proportion + bar), how many tuner inputs are streaming, and a
 * plain-language verdict: a calm "all clear" affirmation when
 * healthy, a warning when storage runs low.
 *
 * Disk figures come from the access store (freediskspace /
 * totaldiskspace), kept live via the diskspaceUpdate Comet
 * notification; the active-stream count comes from status/inputs,
 * kept live via input_status — so this ticks without a manual
 * refresh.
 */
import { computed, onMounted } from 'vue'
import { CircleCheck, RadioTower, TriangleAlert } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useAccessStore } from '@/stores/access'
import { useStatusStore } from '@/stores/status'
import { formatBytes } from '@/utils/formatBytes'

const { t } = useI18n()
const access = useAccessStore()

/* Tuner inputs from status/inputs. */
const inputs = useStatusStore('status/inputs', 'input_status', 'uuid')
onMounted(() => {
  /* `status/inputs` is registered with ACCESS_ADMIN
   * (`src/api/api_status.c:250`); firing it as anonymous returns
   * 401 + WWW-Authenticate which pops the browser's Digest
   * dialog before the user has interacted. The HealthLine is
   * already v-if-gated on `access.has('admin')` upstream, but a
   * child's setup runs before some parent gates settle — keep
   * the explicit skip here so the call is suppressed regardless
   * of mount-order timing. */
  if (!access.has('admin')) return
  inputs.fetch().catch(() => { /* fetch errors surface via inputs.error */ })
})

/* "Active streams" = total subscriptions across all tuner inputs
 * (each tuner's `subs` count summed — one tuner serving several
 * channels off the same transponder counts each). status/inputs
 * also emits an idle "empty status" placeholder row per
 * enabled-but-idle tuner (mpegts_input_get_streams); those carry
 * subs:0, so summing `subs` excludes them naturally. */
const activeStreams = computed(() =>
  inputs.entries.reduce((sum, row) => {
    const subs = row.subs
    return sum + (typeof subs === 'number' ? subs : 0)
  }, 0),
)
const streamsLabel = computed(() => {
  const n = activeStreams.value
  if (n === 0) return t('Idle')
  if (n === 1) return t('1 active stream')
  return t('{0} active streams', n)
})

/* The label is a router-link to Status → Subscriptions for
 * users who can actually open that page. `status/subscriptions`
 * is gated on ACCESS_ADMIN server-side (api_status.c) AND on
 * the router (router/index.ts: 'admin' requirement on the
 * leaf), so we mirror that here — non-admin users see plain
 * text instead of a link they'd land on a permission denial
 * from. Idle ("0 streams") also stays plain text — there's
 * nothing to drill into. */
const streamsLinkable = computed(
  () => activeStreams.value > 0 && access.has('admin'),
)

/* Free space below this fraction of total → the low-disk warning. */
const LOW_DISK_FRACTION = 0.1

function numericOrNull(v: unknown): number | null {
  return typeof v === 'number' && v >= 0 ? v : null
}

const total = computed(() => numericOrNull(access.data?.totaldiskspace))
const free = computed(() => numericOrNull(access.data?.freediskspace))

/* Whether there is enough to draw the disk bar. */
const hasDisk = computed(
  () => total.value !== null && total.value > 0 && free.value !== null,
)

/* Free fraction 0..1. */
const freeFraction = computed(() =>
  hasDisk.value ? Math.max(0, Math.min(1, free.value! / total.value!)) : 0,
)

/* The bar fills with USED space — a short fill means lots of room. */
const usedPct = computed(() => Math.round((1 - freeFraction.value) * 100))
const freePct = computed(() => Math.round(freeFraction.value * 100))

const low = computed(() => hasDisk.value && freeFraction.value < LOW_DISK_FRACTION)

const freeText = computed(() => (free.value === null ? '—' : formatBytes(free.value)))
const totalText = computed(() => (total.value === null ? '—' : formatBytes(total.value)))
</script>

<template>
  <section class="health-line" :class="{ 'health-line--warn': low }">
    <p class="health-line__verdict">
      <component
        :is="low ? TriangleAlert : CircleCheck"
        :size="18"
        class="health-line__icon"
        aria-hidden="true"
      />
      <span>{{
        low ? t('Storage is running low') : t("Everything's running smoothly")
      }}</span>
    </p>
    <div v-if="hasDisk" class="health-line__disk">
      <div class="health-line__bar">
        <div class="health-line__bar-fill" :style="{ width: usedPct + '%' }" />
      </div>
      <p class="health-line__disk-text">
        {{ t('{0} free of {1}', freeText, totalText) }}
        <span class="health-line__disk-pct">({{ freePct }}%)</span>
      </p>
    </div>
    <p class="health-line__streams">
      <RadioTower
        :size="14"
        class="health-line__streams-icon"
        aria-hidden="true"
      />
      <router-link
        v-if="streamsLinkable"
        :to="{ name: 'status-subscriptions' }"
        class="health-line__streams-link"
        :title="t('Open Subscriptions')"
      >
        {{ streamsLabel }}
      </router-link>
      <span v-else>{{ streamsLabel }}</span>
    </p>
  </section>
</template>

<style scoped>
.health-line {
  padding: var(--tvh-space-4);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.health-line__verdict {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin: 0;
  font-weight: 600;
}

/* Calm green tick when healthy; the warn modifier swaps it amber. */
.health-line__icon {
  flex-shrink: 0;
  color: var(--tvh-success);
}

.health-line--warn .health-line__icon {
  color: var(--tvh-warning);
}

.health-line__disk {
  margin-top: var(--tvh-space-3);
}

.health-line__bar {
  height: 8px;
  background: var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  overflow: hidden;
}

.health-line__bar-fill {
  height: 100%;
  background: var(--tvh-primary);
  transition: width var(--tvh-transition);
}

.health-line--warn .health-line__bar-fill {
  background: var(--tvh-warning);
}

.health-line__disk-text {
  margin: var(--tvh-space-2) 0 0;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.health-line__streams {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin: var(--tvh-space-3) 0 0;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.health-line__streams-icon {
  flex-shrink: 0;
}

/* Streams-count link is muted by default (matches the
 * surrounding text), accent + underline on hover/focus to
 * signal clickability. Keeps the calm-by-default look of the
 * health line while making the drill-down discoverable. */
.health-line__streams-link {
  color: inherit;
  text-decoration: none;
  border-radius: var(--tvh-radius-sm);
}

.health-line__streams-link:hover,
.health-line__streams-link:focus-visible {
  color: var(--tvh-primary);
  text-decoration: underline;
  outline: none;
}
</style>
