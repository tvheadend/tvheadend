<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { caColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/ca"
    entity-class="esfilter_ca"
    store-key="config-stream-ca"
    :columns="caColumns"
    :entity-label="t('CA Stream Filter')"
    :count-label="t('filters')"
  />
</template>
