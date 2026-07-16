<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * LiveTvButton — a toolbar launcher that opens the in-browser player
 * with no channel preselected. Gives every EPG view a way to start
 * playback that does NOT depend on an EPG event existing (parity with
 * the classic UI's "Watch TV" button), so channels with no programme
 * data are still reachable (issue #2183). It's the primary play path
 * in the Table view, which has no per-channel row to click, and a
 * universal fallback in Timeline / Magazine.
 *
 * Channel selection lives inside VideoPlayerDialog (its Channel
 * dropdown), so this button is just the launcher — no channel fetch,
 * no picker. The `/stream/` route enforces ACCESS_STREAMING
 * server-side, so no client-side access check here.
 */
import { useI18n } from '@/composables/useI18n'
import { useVideoPlayer } from '@/composables/useVideoPlayer'

const { t } = useI18n()
const player = useVideoPlayer()
</script>

<template>
  <button
    type="button"
    class="live-tv-btn"
    :title="t('Live TV')"
    :aria-label="t('Live TV')"
    @click="player.open()"
  >
    {{ t('Live TV') }}
  </button>
</template>

<style scoped>
.live-tv-btn {
  display: inline-flex;
  align-items: center;
  height: 32px;
  padding: 0 12px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-weight: 500;
  cursor: pointer;
  white-space: nowrap;
}

.live-tv-btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), var(--tvh-bg-page));
}

.live-tv-btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
