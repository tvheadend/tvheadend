<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * HomeView — the task-oriented Home dashboard (ADR 0017, step 1).
 *
 * A role-aware dashboard: its content is a function of the install's
 * state and the logged-in user's capabilities (see useHomeState).
 * The card registry (homeCards.ts) is filtered against that pair and
 * laid out in tiers — a guidance band, then the TV tier, then the
 * admin-only Server tier.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { CircleCheck, X } from 'lucide-vue-next'
import { apiCall } from '@/api/client'
import { useI18n } from '@/composables/useI18n'
import { useToastNotify } from '@/composables/useToastNotify'
import { useHomeState } from '@/composables/useHomeState'
import { consumeSetupGreeting } from '@/utils/setupGreeting'
import { useAccessStore } from '@/stores/access'
import { cometClient } from '@/api/comet'
import { useWizardStore } from '@/stores/wizard'
import {
  refreshEpg as refreshEpgAction,
  scanAllNetworks,
  startSetupWizard,
} from '@/commands/actionHandlers'
import { useCommandPalette } from '@/composables/useCommandPalette'
import { homeCards, type HomeCard } from './homeCards'
import HomeCardComponent from './HomeCard.vue'
import RecordingNow from './RecordingNow.vue'
import ThisWeekStrip from './ThisWeekStrip.vue'
import RecentlyRecorded from './RecentlyRecorded.vue'
import HealthLine from './HealthLine.vue'
import HomeSearchCard from './HomeSearchCard.vue'

const { t } = useI18n()
const router = useRouter()
const toast = useToastNotify()
const wizard = useWizardStore()

const { state, capabilities, loading, error } = useHomeState()
const palette = useCommandPalette()
const access = useAccessStore()

/* Touch-only signal — `(hover: none)` matches phones, tablets,
 * and touch-only laptops. A laptop with a mouse AND a
 * touchscreen reports `hover: hover` (the primary input still
 * has hover), so it correctly counts as non-touch and gets the
 * keyboard-shortcut variant of the palette-discovery tile.
 *
 * Reactive so the tile flips if the user adds / removes a
 * pointing device while Home is open (rare but cheap to
 * support). Initialised eagerly so the first render already
 * has the right value. */
const touchOnly = ref(false)
let hoverMql: MediaQueryList | undefined
function syncTouchOnly(): void {
  if (hoverMql) touchOnly.value = hoverMql.matches
}
if (typeof globalThis.matchMedia === 'function') {
  hoverMql = globalThis.matchMedia('(hover: none)')
  syncTouchOnly()
  hoverMql.addEventListener('change', syncTouchOnly)
}
onBeforeUnmount(() => {
  hoverMql?.removeEventListener('change', syncTouchOnly)
})

/* "Setup complete" greeting — shown once, right after the setup
 * wizard finishes (it flags sessionStorage before its full reload;
 * consumeSetupGreeting reads + clears that flag, so this is a
 * one-shot and a normal load never triggers it). */
const showGreeting = ref(consumeSetupGreeting())

/* Enabled-channel count for the greeting — probed only when the
 * greeting shows. The wizard auto-enables every channel it creates
 * (channels.c: ch_enabled defaults to 1), so this is "channels
 * ready to watch"; filtering to enabled keeps it honest even if the
 * wizard was re-run over previously-disabled channels. */
const greetingChannelCount = ref<number | null>(null)
onMounted(async () => {
  if (!showGreeting.value) return
  try {
    const resp = await apiCall<{ total?: number; totalCount?: number }>(
      'channel/grid',
      {
        start: 0,
        limit: 0,
        filter: JSON.stringify([{ field: 'enabled', type: 'boolean', value: true }]),
      },
    )
    greetingChannelCount.value = resp.total ?? resp.totalCount ?? 0
  } catch {
    /* Count unavailable — the greeting falls back to count-less text. */
  }
})

const greetingText = computed(() => {
  const n = greetingChannelCount.value
  if (n === null) return t('Setup complete')
  if (n === 1) return t('Setup complete — 1 channel ready')
  return t('Setup complete — {0} channels ready', n)
})

/* Filter the registry against the current state x capabilities, then
 * split by tier. The state x role matrix is asserted in the registry
 * test; here it is just `Array.filter`. */
const visibleCards = computed<HomeCard[]>(() =>
  homeCards.filter((c) =>
    c.visible({
      state: state.value,
      caps: capabilities.value,
      seenPalette: palette.seenPalette.value,
      touchOnly: touchOnly.value,
      authMode: access.authMode,
    }),
  ),
)
const guidanceCards = computed(() => visibleCards.value.filter((c) => c.tier === 'guidance'))
const tvCards = computed(() => visibleCards.value.filter((c) => c.tier === 'tv'))
const serverCards = computed(() => visibleCards.value.filter((c) => c.tier === 'server'))

/* The TV-tier activity widgets (recordings) show once channels exist
 * and the user can record. Each widget self-hides when its own data
 * is empty. */
const showActivity = computed(
  () =>
    capabilities.value.record &&
    (state.value === 'epg-missing' || state.value === 'healthy'),
)

/* The Server-tier health line shows for an admin once the install
 * has moved past `fresh` — the same gate as the Server cards. */
const showHealth = computed(
  () => capabilities.value.configure && state.value !== 'fresh',
)

/* Action-card double-click guard. A handler that fires a real
 * mutation (scan, EPG re-grab) takes ~50-300ms before the toast
 * lands; this ref keeps the card inert during that window so a
 * double-click coalesces into a single fire. Cleared in finally.
 * Per-action, so a slow scan doesn't block the user from also
 * kicking off an EPG refresh. */
const inflightAction = ref<string | null>(null)

async function onCardAction(actionId: string): Promise<void> {
  if (inflightAction.value === actionId) return
  if (actionId === 'open-palette') {
    /* Discovery tile — opens the palette and self-dismisses (the
     * open call writes the seenPalette flag, the card's visible()
     * predicate flips false, the tile disappears). No inflight
     * guard needed; it's a local UI action. */
    palette.open()
    return
  }
  if (actionId === 'start-wizard') {
    await startSetupWizard(wizard, router, toast)
    return
  }
  if (actionId === 'sign-in') {
    /* Same flow as NavRail's Login button: hit the server's
     * /login (which 401s + Digest-challenges for anonymous
     * callers), let the browser prompt and cache credentials,
     * then drop the comet mailbox and reconnect so the new
     * accessUpdate carries the authed identity. The wizard
     * watcher in main.ts auto-launches the setup wizard if the
     * server has one pending. See `NavRail.vue` for the long
     * rationale. */
    try {
      const res = await fetch('/login', {
        method: 'GET',
        credentials: 'include',
        cache: 'no-store',
      })
      if (res.ok) cometClient.reset()
    } catch {
      /* Network error or browser-cancelled prompt — the card
       * stays available for another try. */
    }
    return
  }
  if (actionId === 'scan-all-networks') {
    inflightAction.value = actionId
    try {
      await scanAllNetworks(toast)
    } finally {
      inflightAction.value = null
    }
    return
  }
  if (actionId === 'refresh-epg') {
    inflightAction.value = actionId
    try {
      await refreshEpgAction(toast)
    } finally {
      inflightAction.value = null
    }
  }
}
</script>

<template>
  <article class="view home">
    <header class="view__header">
      <h1>{{ t('Home') }}</h1>
    </header>

    <!-- One-shot "Setup complete" greeting, right after the wizard. -->
    <output v-if="showGreeting" class="home__greeting">
      <CircleCheck :size="20" class="home__greeting-icon" aria-hidden="true" />
      <span class="home__greeting-text">{{ greetingText }}</span>
      <button
        type="button"
        class="home__greeting-dismiss"
        :aria-label="t('Dismiss')"
        @click="showGreeting = false"
      >
        <X :size="16" aria-hidden="true" />
      </button>
    </output>

    <p v-if="loading" class="home__status">{{ t('Loading…') }}</p>
    <p v-else-if="error" class="home__status">
      {{ t('Could not load the dashboard.') }}
    </p>

    <template v-else>
      <!-- Guidance band — the recommended next step (or an honest
           explanation for a user who cannot act on it). -->
      <section v-if="guidanceCards.length" class="home__guidance">
        <HomeCardComponent
          v-for="card in guidanceCards"
          :key="card.id"
          :card="card"
          :primary="card.kind === 'action'"
          @action="onCardAction"
        />
      </section>

      <!-- TV tier — everyday viewing, for every user. -->
      <section v-if="tvCards.length || showActivity" class="home__tier">
        <h2 class="home__tier-title">{{ t('TV') }}</h2>
        <!-- EPG title search — scoped to TV programming, so it
             belongs with the other TV-tier surfaces (and is naturally
             hidden on fresh / channels-missing installs where the
             tier itself doesn't render). The very-top of Home is
             reserved for a future universal-search affordance. -->
        <HomeSearchCard />
        <div v-if="showActivity" class="home__activity">
          <RecordingNow />
          <ThisWeekStrip />
          <RecentlyRecorded />
        </div>
        <div v-if="tvCards.length" class="home__cards">
          <HomeCardComponent
            v-for="card in tvCards"
            :key="card.id"
            :card="card"
            @action="onCardAction"
          />
        </div>
      </section>

      <!-- Server tier — secondary, admin-only. -->
      <section v-if="serverCards.length || showHealth" class="home__tier">
        <h2 class="home__tier-title">{{ t('Server') }}</h2>
        <HealthLine v-if="showHealth" class="home__health" />
        <div v-if="serverCards.length" class="home__cards">
          <HomeCardComponent
            v-for="card in serverCards"
            :key="card.id"
            :card="card"
            @action="onCardAction"
          />
        </div>
      </section>

      <!-- Never-empty fallback — the registry guarantees at least one
           card for every state x capability, but guard anyway. -->
      <p v-if="!visibleCards.length" class="home__status">
        {{ t('Nothing to show here yet.') }}
      </p>
    </template>
  </article>
</template>

<style scoped>
.view__header {
  margin-bottom: var(--tvh-space-4);
}

.view h1 {
  font-size: var(--tvh-text-3xl);
}

.home {
  max-width: 960px;
  margin: 0 auto;
}

.home__status {
  padding: var(--tvh-space-4) 0;
  color: var(--tvh-text-muted);
}

/* Setup-complete greeting — a calm success-tinted banner. */
.home__greeting {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  margin-bottom: var(--tvh-space-4);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  background: color-mix(in srgb, var(--tvh-success) 12%, var(--tvh-bg-surface));
  border: 1px solid color-mix(in srgb, var(--tvh-success) 40%, var(--tvh-border));
  border-radius: var(--tvh-radius-md);
}

.home__greeting-icon {
  flex-shrink: 0;
  color: var(--tvh-success);
}

.home__greeting-text {
  flex: 1;
  font-weight: 600;
}

.home__greeting-dismiss {
  flex-shrink: 0;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: none;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition);
}

.home__greeting-dismiss:hover {
  background: color-mix(in srgb, var(--tvh-success) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.home__greeting-dismiss:focus-visible {
  outline: 2px solid var(--tvh-success);
  outline-offset: 1px;
}

/* Guidance band — full-width cards; one is usually visible at a time,
 * and it stays prominent on every screen size. */
.home__guidance {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  margin-bottom: var(--tvh-space-6);
}

.home__tier {
  margin-bottom: var(--tvh-space-6);
}

.home__tier-title {
  margin: 0 0 var(--tvh-space-3);
  font-size: var(--tvh-text-xl);
  font-weight: 600;
}

/*
 * Card grid — `auto-fill` + a min track width makes it responsive
 * with no media queries: one column on a phone, more as the viewport
 * widens.
 */
.home__cards {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: var(--tvh-space-3);
}

/* TV-tier activity widgets — a vertical stack above the nav cards. */
.home__activity {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  margin-bottom: var(--tvh-space-3);
}

/* Server-tier health line — spaced off the nav cards below it. */
.home__health {
  margin-bottom: var(--tvh-space-3);
}
</style>
