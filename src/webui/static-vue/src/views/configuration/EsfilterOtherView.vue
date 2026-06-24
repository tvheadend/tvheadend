<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { otherColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/other"
    entity-class="esfilter_other"
    store-key="config-stream-other"
    :columns="otherColumns"
    :entity-label="t('Other Stream Filter')"
    :count-label="t('filters')"
  />
</template>
