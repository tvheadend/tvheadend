<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Recording → Timeshift.
 *
 * Singleton config form for the `timeshift_conf_class` idnode
 * (`src/timeshift.c:181-304`, ACCESS_ADMIN, 11 fields across
 * basic / advanced / expert). Mirrors the legacy ExtJS page at
 * `static/app/timeshift.js:1-27` which uses `idnode_simple`
 * against `api/timeshift/config`.
 *
 * Conditional disable mirrors the legacy `onchange` handler at
 * `static/app/timeshift.js:3-14`:
 *   - max_period disabled when unlimited_period.
 *   - max_size  disabled when unlimited_size || ram_only.
 *
 * The `ram_only` branch on max_size matches the server-side
 * behaviour at `timeshift_conf_class_changed`
 * (`src/timeshift.c:118-122`) which overwrites
 * `max_size = ram_size` when `ram_only` is true — so the
 * disabled state warns the user that any value they type would
 * be silently overwritten on save.
 *
 * The page hosts no custom toolbar actions beyond IdnodeConfigForm's
 * Save / Undo / LevelMenu. Capability gate handled at the
 * router (`configRecordingTimeshiftGuard`) + the L3 tab strip
 * (`ConfigRecordingLayout`).
 */
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import { timeshiftDisable } from './timeshiftDisable'
</script>

<template>
  <IdnodeConfigForm
    load-endpoint="timeshift/config/load"
    help-page="class/timeshift"
    save-endpoint="timeshift/config/save"
    :disabled-for="timeshiftDisable"
  />
</template>
