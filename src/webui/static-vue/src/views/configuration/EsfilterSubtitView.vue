<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { subtitColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/subtit"
    entity-class="esfilter_subtit"
    store-key="config-stream-subtit"
    :columns="subtitColumns"
    :entity-label="t('Subtitle Stream Filter')"
    :count-label="t('filters')"
  />
</template>
