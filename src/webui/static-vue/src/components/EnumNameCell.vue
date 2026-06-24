<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EnumNameCell — grid-cell renderer for fields that hold enum
 * keys. Auto-detects between scalar and array shape via
 * `Array.isArray(value)`:
 *
 *   value: 'uuid-1'                → "Channel One HD"
 *   value: ['uuid-1', 'uuid-2']    → "Pass, MKV"
 *
 * Both shapes consume the same column descriptor
 * (`enumSource: …` — either a deferred-fetch descriptor
 * `{ type: 'api', uri, params? }` for dynamic / large option
 * lists, or an inline `Option[]` for small fixed enums like a
 * tri-state). Deferred sources flow through `fetchDeferredEnum`'s
 * shared cache so the option list is fetched once per session
 * no matter how many rows / columns reference the same source;
 * inline sources skip the fetch entirely.
 *
 * Rendering policy (scalar):
 *   - empty / null / '': '—'
 *   - in-flight: raw key (so the cell isn't empty during the
 *     brief fetch window)
 *   - resolved + key found: localized name
 *   - resolved + key not found: '—'
 *
 * Rendering policy (array):
 *   - empty / null / non-array: '—'
 *   - in-flight: raw keys joined with ', '
 *   - resolved + key found: include the localized name in the join
 *   - resolved + key not found: include '—' for THAT entry,
 *     preserving cardinality so the user sees that one of the N
 *     references is stale
 *   - resolved + every key missing: collapse to a single '—' so
 *     the cell isn't a forest of dashes
 */
import { computed, inject, onMounted, ref, type Ref } from 'vue'
import { ChevronRight } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import {
  fetchDeferredEnum,
  isInlineEnum,
  type Option,
} from './idnode-fields/deferredEnum'
import { resolveChannelListDescriptor } from '@/utils/channelListDescriptor'
import { useAccessStore } from '@/stores/access'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  value: unknown
  row?: BaseRow
  col: ColumnDef
}>()

const access = useAccessStore()
const entityEditor = useEntityEditor()
const { t } = useI18n()

/* Provided by IdnodeGrid when the grid is in inline-edit
 * mode. The chevron hides during edit so the user isn't
 * tempted to navigate away mid-edit. Undefined when used
 * outside an IdnodeGrid ancestor — treated as "not editing". */
const gridActivelyEditing = inject<Ref<boolean> | undefined>(
  'idnodeGridActivelyEditing',
  undefined,
)

const options = ref<Option[] | null>(null)

onMounted(() => {
  /* Merge user's chname_num / chname_src prefs into channel/list
   * descriptors so the cell label matches what the editor
   * dropdown renders for the same field. Pass-through for any
   * other deferred URI. */
  const src = resolveChannelListDescriptor(props.col.enumSource)
  if (!src) return
  if (isInlineEnum(src)) {
    /* Inline static enum — populate synchronously, skip the
     * deferred-fetch path entirely. No in-flight phase, no
     * cache key, no network round-trip. */
    options.value = src
    return
  }
  fetchDeferredEnum(src)
    .then((res) => {
      options.value = res
    })
    .catch(() => {
      /* Leave options empty; the rendered fallback (raw value /
       * em-dash) is already a sensible degradation. The fetch
       * error is surfaced once via deferredEnum's own catch. */
      options.value = []
    })
})

/* Read the sibling-field snapshot for orphan handling. Empty
 * string when the column doesn't declare `fallbackField` OR the
 * row doesn't carry the sibling value. Consumers use this to
 * keep historic display readable even when the live UUID
 * resolution has nothing to point at (DVR Finished's `channel`
 * column ↔ wire-side `channelname` is the canonical case). */
function fallbackValue(): string {
  const field = props.col.fallbackField
  if (!field || !props.row) return ''
  const v = props.row[field]
  return typeof v === 'string' ? v : ''
}

function labelFor(key: string): string {
  const fb = fallbackValue()
  if (options.value === null) {
    /* In-flight — prefer the sibling-field snapshot if available
     * (reads naturally for the user while the deferred-enum
     * fetch completes), else show the raw key as before. */
    return fb || key
  }
  const hit = options.value.find((o) => String(o.key) === key)
  if (hit) return hit.val
  /* Resolved but key not found — orphan reference. Fall back to
   * the snapshot if the column declared one; otherwise '—'. */
  return fb || '—'
}

/* Resolves the row value to a `{ display, full }` pair:
 *   - `display` is the visible cell text (compressed for
 *     multi-element arrays so the grid line stays readable),
 *   - `full` is the comma-joined full list for multi-element
 *     arrays, used as the cell's `title` tooltip so hover
 *     reveals every referenced entity. For scalar / single /
 *     empty values, `full` is undefined (no extra tooltip
 *     beyond what the browser would natively show).
 *
 * Multi-element compression: "Channel One, +2 more" mirrors the
 * pattern several admin UIs use for multi-ref grid cells. The
 * single-element chevron drill (see below) handles the case
 * where the cell maps to exactly one entity; the multi-element
 * case stays read-only here and defers to a future popover
 * affordance.
 *
 * Sort order: the deferred-enum's options list arrives in the
 * server's canonical sort (per `<class>_class_get_list` —
 * channels by their server-side sort, profiles alphabetically,
 * etc.). Sorting the row's items by their position in that
 * list makes the displayed "first" name deterministic and
 * matches what the user would see at the top of any picker for
 * the same class. Items missing from the options (stale
 * references) sink to the end so the user still sees a real
 * label in the cell, not an em-dash. While options is still
 * in-flight (`options.value === null`), there's nothing to sort
 * against — we render in wire order and let the next render
 * pick up the canonical order once the fetch resolves. */
const cellRender = computed<{ display: string; full?: string }>(() => {
  const v = props.value
  if (Array.isArray(v)) {
    if (v.length === 0) return { display: '—' }
    const items = v.map((k) => {
      const key = String(k)
      const label = labelFor(key)
      const idx = options.value?.findIndex((o) => String(o.key) === key) ?? -1
      return { label, idx }
    })
    if (options.value !== null && items.every((it) => it.label === '—')) {
      return { display: '—' }
    }
    if (options.value !== null) {
      items.sort((a, b) => {
        if (a.idx === -1 && b.idx === -1) return 0
        if (a.idx === -1) return 1
        if (b.idx === -1) return -1
        return a.idx - b.idx
      })
    }
    const labels = items.map((it) => it.label)
    if (labels.length === 1) return { display: labels[0] }
    return {
      display: t('{0}, +{1} more', labels[0], labels.length - 1),
      full: labels.join(', '),
    }
  }
  if (v === null || v === undefined || v === '') return { display: '—' }
  return { display: labelFor(String(v)) }
})

const display = computed(() => cellRender.value.display)

/* Drill-down chevron support. The cell opts into drill-down
 * when the column declares `targetUuidField` (and optionally
 * `targetAccessKey`). Same contract as DrillDownCell. For DVR
 * idnode-enum fields where the value IS the UUID, point
 * `targetUuidField` at the same `col.field` — `row[col.field]`
 * resolves back to the UUID.
 *
 * Array (islist) handling: an array of 1 UUID drills straight to
 * that entity (`useEntityEditor.open`); an array of 2+ opens the
 * multi-entry picker drawer (`useEntityEditor.openList`) — a
 * single-select table of the referenced entities, click a row to
 * edit it. Same mechanism that powers the Home dashboard's
 * day-cell click. Empty arrays / unknown shapes keep no chevron. */
const targetUuids = computed<string[] | null>(() => {
  const field = props.col.targetUuidField
  if (!field || !props.row) return null
  const v = props.row[field]
  if (typeof v === 'string' && v) return [v]
  if (Array.isArray(v)) {
    const uuids = v.filter((x): x is string => typeof x === 'string' && !!x)
    return uuids.length ? uuids : null
  }
  return null
})

const allowed = computed<boolean>(() => {
  const key = props.col.targetAccessKey
  if (!key) return true
  return !!access.data?.[key]
})

const showChevron = computed(
  () => !gridActivelyEditing?.value && targetUuids.value !== null && allowed.value,
)

function onChevronClick(event: MouseEvent): void {
  event.stopPropagation()
  const uuids = targetUuids.value
  if (!uuids || uuids.length === 0) return
  /* openList collapses a 1-row list to a plain `open(uuid)`, so a
   * single code path handles both shapes; for 2+ entries it shows
   * the picker drawer with the first row pre-selected. The picker
   * title defaults to col.label when col.pickerTitle isn't set —
   * only matters for the multi-entry case. */
  const rows = uuids.map((uuid) => ({ uuid, name: labelFor(uuid) }))
  const columns = [{ field: 'name', label: t('Name') }]
  entityEditor.openList(rows, columns, props.col.pickerTitle ?? props.col.label)
}
</script>

<template>
  <span class="enum-name-cell">
    <span class="enum-name-cell__text" :title="cellRender.full">{{ display }}</span>
    <button
      v-if="showChevron"
      type="button"
      class="enum-name-cell__drill"
      :title="t('Open in editor')"
      :aria-label="t('Open in editor')"
      @click="onChevronClick"
    >
      <ChevronRight :size="14" aria-hidden="true" />
    </button>
  </span>
</template>

<style scoped>
.enum-name-cell {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  min-width: 0;
  max-width: 100%;
}

.enum-name-cell__text {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.enum-name-cell__drill {
  flex: none;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 22px;
  height: 22px;
  padding: 0;
  background: none;
  border: none;
  color: var(--tvh-primary);
  cursor: pointer;
  border-radius: var(--tvh-radius-sm);
  opacity: 0;
  transition: opacity var(--tvh-transition), background var(--tvh-transition);
}

.enum-name-cell:hover .enum-name-cell__drill,
.enum-name-cell__drill:focus-visible {
  opacity: 1;
}

.enum-name-cell__drill:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.enum-name-cell__drill:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

@media (hover: none) {
  .enum-name-cell__drill {
    opacity: 1;
  }
}
</style>
