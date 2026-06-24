<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * PlayCell — per-row inline Play icon for grid action columns.
 *
 * Mirrors Classic's `playLink()` shape (`static/app/tvheadend.js:856-861`)
 * but renders a bare Lucide icon-button instead of an `<img>` anchor.
 * One click opens `/play/ticket/<col.playPath>/<row.uuid>` in a new
 * tab via `openPlay()` from `utils/playUrl.ts`, which the OS MIME
 * handler routes to the user's external media player.
 *
 * Cell reads three optional fields off the ColumnDef
 * (`src/webui/static-vue/src/types/column.ts`):
 *   - `playPath`    — REQUIRED. Stream-path prefix without the uuid
 *                     (e.g. 'stream/channel', 'dvrfile').
 *   - `playTitle`   — optional builder for the `?title=` URL query
 *                     param (Classic decorates this for m3u track
 *                     display in the external player).
 *   - `playEnabled` — optional predicate; defaults true. DVR
 *                     Finished sets `r => r.filesize > 0` so deleted-
 *                     on-disk rows render disabled.
 *
 * Click is locally handled — the underlying row's selection state
 * does not change because PrimeVue's row-click handler fires on the
 * row body, not on cell content with its own click handlers
 * (confirmed via the Subsystem-table precedent at
 * `ConfigDebuggingConfigView.vue:299-310`).
 */
import { computed } from 'vue'
import { Play } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import { useI18n } from '@/composables/useI18n'
import { openPlay } from '@/utils/playUrl'

const props = defineProps<{
  /* value / row / col — the standard cellComponent prop shape
   * DataGrid forwards. We ignore `value` (synthetic column has no
   * row field) and read everything off `col`. */
  value?: unknown
  row: BaseRow
  col: ColumnDef
}>()

const { t } = useI18n()

const enabled = computed(() => {
  if (typeof props.row.uuid !== 'string' || !props.row.uuid) return false
  if (!props.col.playPath) return false
  if (props.col.playEnabled) return props.col.playEnabled(props.row)
  return true
})

function play(event: MouseEvent): void {
  /* Stop propagation so the click doesn't reach the row-click
   * handler (which would toggle row selection). */
  event.stopPropagation()
  if (!enabled.value) return
  const uuid = props.row.uuid as string
  const prefix = props.col.playPath as string
  const titleFn = props.col.playTitle
  const title = titleFn ? titleFn(props.row) : ''
  const query = title ? `?title=${encodeURIComponent(title)}` : ''
  openPlay(`${prefix}/${uuid}${query}`)
}
</script>

<template>
  <button
    type="button"
    class="play-cell"
    :class="{ 'play-cell--disabled': !enabled }"
    :disabled="!enabled"
    :title="t('Play this stream')"
    :aria-label="t('Play this stream')"
    @click="play"
  >
    <Play :size="16" class="play-cell__icon" aria-hidden="true" />
  </button>
</template>

<style scoped>
.play-cell {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 28px;
  height: 28px;
  padding: 0;
  background: none;
  border: none;
  color: var(--tvh-primary);
  cursor: pointer;
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition);
}

.play-cell:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.play-cell:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

.play-cell--disabled {
  color: var(--tvh-text-muted);
  cursor: not-allowed;
}

.play-cell__icon {
  flex: none;
}
</style>
