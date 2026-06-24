<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import EsfilterGridView from './EsfilterGridView.vue'
import { teletextColumns } from './esfilterColumns'
import { t } from '@/composables/useI18n'
</script>

<template>
  <EsfilterGridView
    api-base="esfilter/teletext"
    entity-class="esfilter_teletext"
    store-key="config-stream-teletext"
    :columns="teletextColumns"
    :entity-label="t('Teletext Stream Filter')"
    :count-label="t('filters')"
  />
</template>
