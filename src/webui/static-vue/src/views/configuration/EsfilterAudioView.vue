<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { audioColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/audio"
    entity-class="esfilter_audio"
    store-key="config-stream-audio"
    :columns="audioColumns"
    :entity-label="t('Audio Stream Filter')"
    :count-label="t('filters')"
  />
</template>
