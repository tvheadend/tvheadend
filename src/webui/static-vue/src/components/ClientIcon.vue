<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ClientIcon — the mark of the client a viewer is (clientIcons): Kodi,
 * Jellyfin, Emby, Plex, VLC, Firefox, Chrome, Safari, Opera or Vivaldi in
 * its own colour, else a phone, a globe for another browser (Edge among
 * them) or a screen for anything else, in the text colour.
 * `size` is in pixels.  The name it stands for is its label, so that a
 * screen reader says it.
 */
import { computed } from 'vue'
import { Globe, Monitor, Smartphone } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { BRAND_ICONS, clientKind } from '@/utils/clientIcons'

const props = withDefaults(defineProps<{ client?: string | null; size?: number }>(), {
  client: null,
  size: 12,
})

const { t } = useI18n()
const kind = computed(() => clientKind(props.client))
const brand = computed(() =>
  kind.value in BRAND_ICONS ? BRAND_ICONS[kind.value as keyof typeof BRAND_ICONS] : null
)
const generic = computed(() =>
  kind.value === 'tvhclient'
    ? Smartphone
    : kind.value === 'browser' || kind.value === 'edge'
      ? Globe
      : Monitor
)
const label = computed(() =>
  brand.value
    ? brand.value.name
    : kind.value === 'tvhclient'
      ? 'TVHClient'
      : kind.value === 'edge'
        ? 'Edge'
        : kind.value === 'browser'
          ? t('Web browser')
          : t('Client')
)
</script>

<template>
  <svg
    v-if="brand"
    class="client-icon"
    :class="`client-icon--${kind}`"
    viewBox="0 0 24 24"
    :width="size"
    :height="size"
    role="img"
    :aria-label="label"
  >
    <path :d="brand.path" :fill="brand.color" />
  </svg>
  <component
    :is="generic"
    v-else
    class="client-icon"
    :class="`client-icon--${kind}`"
    :size="size"
    role="img"
    :aria-label="label"
  />
</template>

<style scoped>
.client-icon {
  display: inline-block;
  flex: 0 0 auto;
  vertical-align: -0.15em;
}
</style>
