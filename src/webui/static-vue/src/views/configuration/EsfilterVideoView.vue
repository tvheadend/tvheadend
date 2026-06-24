<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { videoColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/video"
    entity-class="esfilter_video"
    store-key="config-stream-video"
    :columns="videoColumns"
    :entity-label="t('Video Stream Filter')"
    :count-label="t('filters')"
  />
</template>
