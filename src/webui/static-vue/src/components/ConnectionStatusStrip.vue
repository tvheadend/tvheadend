<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ConnectionStatusStrip — a thin, non-modal notice under the top of
 * the app shell while the Comet connection to the server is lost,
 * then "Reconnected" for a few seconds. The timing lives in the
 * comet store. The live region stays in the DOM (empty when all is
 * well) so screen readers announce the text as it appears. The texts
 * are the Classic msgids the store also writes to the Log.
 */
import { useCometStore } from '@/stores/comet'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const comet = useCometStore()
</script>

<template>
  <div class="connection-strip" role="status" aria-live="polite">
    <p
      v-if="comet.connectionLost"
      class="connection-strip__text connection-strip__text--lost"
    >
      {{
        t(
          'There seems to be a problem with the live update feed from Tvheadend. Trying to reconnect...',
        )
      }}
    </p>
    <p
      v-else-if="comet.reconnected"
      class="connection-strip__text connection-strip__text--back"
    >
      {{ t('Reconnected to Tvheadend') }}
    </p>
  </div>
</template>

<style scoped>
.connection-strip {
  flex-shrink: 0;
}

.connection-strip__text {
  margin: 0;
  padding: 6px var(--tvh-space-4);
  font-size: var(--tvh-text-md);
  text-align: center;
  color: var(--tvh-text);
  border-bottom: 1px solid var(--tvh-border);
}

.connection-strip__text--lost {
  background: color-mix(in srgb, var(--tvh-warning) 18%, var(--tvh-bg-surface));
  border-bottom-color: var(--tvh-warning);
}

.connection-strip__text--back {
  background: color-mix(in srgb, var(--tvh-success) 15%, var(--tvh-bg-surface));
  border-bottom-color: var(--tvh-success);
}
</style>
