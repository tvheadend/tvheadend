<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * RailInfoArea — shared live-status footer/strip used by both
 * NavRail (vertical, in the rail's footer at desktop / tablet)
 * and TopBar (horizontal, at the right end at phone width).
 *
 * `config.info_area` (`src/config.c:2045-2052`) is a CSV of up to
 * three keys from { 'login', 'storage', 'time' } controlling
 * which items appear and in what order. Default is all three.
 * The setting arrives via the `accessUpdate` Comet payload
 * (`comet.c:204-205`); a fresh value lands either at WS connect
 * or after the General → Save reload. Storage values stay live
 * via the separate `diskspaceUpdate` notification (~30 s server
 * cadence, wired in stores/access.ts).
 *
 * Two visual forms picked via the `compact` prop:
 *
 *   compact=false (default) — full-width row per item with icon
 *     + descriptive label + values (used at expanded rail width).
 *   compact=true            — stacked icon + short value pill
 *     (used at compact rail width AND at phone width in the
 *     TopBar's right slot).
 *
 * Layout direction is the consumer's responsibility — this
 * component just renders its children in document order. NavRail
 * wraps it in a `flex-direction: column` footer; TopBar wraps it
 * in a `flex-direction: row` strip.
 */
import { computed } from 'vue'
import { Clock, HardDrive, UserCircle2 } from 'lucide-vue-next'
import { useAccessStore } from '@/stores/access'
import { useNowCursor } from '@/composables/useNowCursor'
import { formatBytes } from '@/utils/formatBytes'

withDefaults(
  defineProps<{
    /** Stacked icon + short value pill when true; full row when false. */
    compact?: boolean
  }>(),
  { compact: false },
)

const access = useAccessStore()

/* ---- info_area ordering ---- */
const KNOWN_INFO_ITEMS = new Set(['login', 'storage', 'time'])
const DEFAULT_INFO_ITEMS = ['login', 'storage', 'time']

const infoItems = computed<string[]>(() => {
  const raw = access.data?.info_area
  if (!raw) return DEFAULT_INFO_ITEMS
  const parsed = raw
    .split(',')
    .map((s) => s.trim())
    .filter((s) => KNOWN_INFO_ITEMS.has(s))
  return parsed.length > 0 ? parsed : DEFAULT_INFO_ITEMS
})

/* ---- Login ----
 * `authMode` (from the access store) classifies the identity state
 * into five distinguishable buckets — see `types/access.ts` for
 * the rationale. The previous single "—" glyph collapsed all five
 * onto one display, making "I'm not logged in" indistinguishable
 * from "the server has no auth backend" or "the wizard prompt
 * fired but the rail hasn't caught up yet". `loginLabel` /
 * `loginTooltip` here pick a state-appropriate label; the compact
 * one-letter `usernameInitial` falls through to `·` for any state
 * that doesn't have a name to show. */
const username = computed(() => access.data?.username ?? '')
const usernameInitial = computed(() => (username.value[0] ?? '·').toUpperCase())
const loginLabel = computed<string>(() => {
  switch (access.authMode) {
    case 'pre-auth':
      return 'Connecting…'
    case 'noacl':
      return 'Authentication disabled'
    case 'anonymous-admin':
      return 'Anonymous (admin)'
    case 'anonymous':
      return 'Not logged in'
    case 'authenticated':
      return username.value
  }
  return ''
})
const loginTooltip = computed<string>(() => {
  switch (access.authMode) {
    case 'authenticated':
      return `Logged in as ${username.value}`
    case 'noacl':
      return 'Server started with --noacl — all access-control checks are bypassed and every request is treated as admin'
    case 'anonymous-admin':
      return 'No credentials presented — anonymous wildcard grants admin'
    default:
      return loginLabel.value
  }
})

/* ---- Storage ---- */

/* Compact-mode footer abbreviation — single-letter suffix, no
 * decimal except for TiB where the digit matters (the difference
 * between 1.2T and 1.8T is 600 GiB). */
function formatBytesAbbr(b: number): string {
  if (b >= 1024 ** 4) return `${(b / 1024 ** 4).toFixed(1)}T`
  if (b >= 1024 ** 3) return `${Math.round(b / 1024 ** 3)}G`
  if (b >= 1024 ** 2) return `${Math.round(b / 1024 ** 2)}M`
  if (b >= 1024) return `${Math.round(b / 1024)}K`
  return `${b}B`
}

function fmtMaybe(v: number | undefined, fmt: (b: number) => string): string {
  return typeof v === 'number' && v >= 0 ? fmt(v) : '—'
}

const storageWide = computed(() => ({
  free: fmtMaybe(access.data?.freediskspace, formatBytesAbbr),
  used: fmtMaybe(access.data?.useddiskspace, formatBytesAbbr),
  total: fmtMaybe(access.data?.totaldiskspace, formatBytesAbbr),
}))

const freePct = computed<string>(() => {
  const f = access.data?.freediskspace
  const t = access.data?.totaldiskspace
  if (typeof f !== 'number' || typeof t !== 'number' || t <= 0) return '—'
  return `${Math.round((f / t) * 100)}%`
})

const storageTooltip = computed(() => {
  const free = fmtMaybe(access.data?.freediskspace, formatBytes)
  const used = fmtMaybe(access.data?.useddiskspace, formatBytes)
  const total = fmtMaybe(access.data?.totaldiskspace, formatBytes)
  return `Storage — Free: ${free} · Used: ${used} · Total: ${total}`
})

/* ---- Time ----
 * Reuses `useNowCursor` (already aligned to wall-clock :00 / :30
 * ticks). HH:MM display only visibly changes at :00 of each
 * minute — :30 ticks are silent, which is fine. */
const { now } = useNowCursor()

const dayTimeWide = computed(() => {
  const d = new Date(now.value * 1000)
  return {
    day: new Intl.DateTimeFormat(undefined, { weekday: 'short' }).format(d),
    time: new Intl.DateTimeFormat(undefined, {
      hour: '2-digit',
      minute: '2-digit',
      hour12: false,
    }).format(d),
    tooltip: new Intl.DateTimeFormat(undefined, {
      dateStyle: 'full',
      timeStyle: 'short',
    }).format(d),
  }
})
</script>

<template>
  <template v-for="item in infoItems" :key="item">
    <!-- LOGIN -->
    <template v-if="item === 'login'">
      <div v-if="!compact" class="info-row" :title="loginTooltip">
        <UserCircle2 v-if="!access.userGlyph" :size="14" :stroke-width="2" />
        <span v-else class="info-glyph">{{ access.userGlyph }}</span>
        <span class="info-row__text">
          <template v-if="access.authMode === 'authenticated'">
            Logged in as <strong>{{ loginLabel }}</strong>
          </template>
          <span v-else class="info-row__text--muted">{{ loginLabel }}</span>
        </span>
      </div>
      <div v-else class="info-stack" :title="loginTooltip">
        <UserCircle2 v-if="!access.userGlyph" :size="16" :stroke-width="2" />
        <span v-else class="info-glyph info-glyph--lg">{{ access.userGlyph }}</span>
        <span class="info-stack__value">{{ usernameInitial }}</span>
      </div>
    </template>

    <!-- STORAGE -->
    <template v-else-if="item === 'storage'">
      <div v-if="!compact" class="info-row" :title="storageTooltip">
        <HardDrive :size="14" :stroke-width="2" />
        <span class="info-row__text">
          Free <strong>{{ storageWide.free }}</strong> Used
          <strong>{{ storageWide.used }}</strong> Total
          <strong>{{ storageWide.total }}</strong>
        </span>
      </div>
      <div v-else class="info-stack" :title="storageTooltip">
        <HardDrive :size="16" :stroke-width="2" />
        <span class="info-stack__value">{{ freePct }}</span>
      </div>
    </template>

    <!-- TIME -->
    <template v-else-if="item === 'time'">
      <div v-if="!compact" class="info-row" :title="dayTimeWide.tooltip">
        <Clock :size="14" :stroke-width="2" />
        <span class="info-row__text">
          {{ dayTimeWide.day }} <strong>{{ dayTimeWide.time }}</strong>
        </span>
      </div>
      <div v-else class="info-stack" :title="dayTimeWide.tooltip">
        <Clock :size="16" :stroke-width="2" />
        <span class="info-stack__value">{{ dayTimeWide.time }}</span>
      </div>
    </template>
  </template>
</template>

<style scoped>
.info-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  /* Match `.info-stack` natural height (icon + 2 px gap + small
   * line) so each row in expanded mode occupies the same vertical
   * real estate as a collapsed-mode stack. Lets the consumer
   * (NavRail's footer) keep the same total height across collapse
   * and expand. */
  min-height: 32px;
  /* Single-line clip — long usernames or stretched storage values
   * shouldn't push the layout. The tooltip carries the full text. */
  overflow: hidden;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
}

.info-row__text {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-variant-numeric: tabular-nums;
}

.info-row strong {
  color: var(--tvh-text);
  font-weight: 600;
}

/* Muted variant for the non-authenticated identity labels
 * (Not logged in / Anonymous (admin) / No authentication /
 * Connecting…) — distinguishes "state caption" from the
 * authenticated "Logged in as <name>" line at a glance. */
.info-row__text--muted {
  color: var(--tvh-text-muted);
  font-style: italic;
}

.info-stack {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  font-size: var(--tvh-text-xs);
  font-variant-numeric: tabular-nums;
  /* Same min-height as `.info-row` so collapsed stacks and
   * expanded rows occupy the same vertical real estate when the
   * consumer is column-flowed. */
  min-height: 32px;
  color: var(--tvh-text-muted);
}

.info-stack__value {
  /* The stack value picks up the muted colour from the parent —
   * separate hook here so consumers can override (e.g. the
   * TopBar's at-a-glance strip might want full-strength text on
   * a denser background). */
}

.info-glyph {
  font-size: var(--tvh-text-lg);
  line-height: 1;
}

.info-glyph--lg {
  font-size: var(--tvh-text-xl);
}
</style>
