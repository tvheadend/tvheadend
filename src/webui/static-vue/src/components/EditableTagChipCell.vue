<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EditableTagChipCell — Manage-mode-only chip painter for the
 * Channels view's `tags` column. Renders the row's current tag
 * array as removable chips, plus a `+` button that opens a small
 * picker of unused tags. Each chip add / remove commits to the
 * grid's inline-edit dirty store via the injected commit handle,
 * accumulating with any drag-reorder changes; the host grid's
 * Save button flushes the lot.
 *
 * When the grid is NOT in Manage mode, this component delegates
 * to the read-only `EnumNameCell` so cells switch chrome cleanly
 * with the mode toggle and the consuming view doesn't have to
 * fork its column descriptor.
 *
 * Generic in shape — works for any `PT_STR | islist` column whose
 * enumSource resolves uuid → name via `fetchDeferredEnum`. The
 * Channels `tags` column is the first consumer; future bulk-tag
 * surfaces (epggrab channels, access entries) could reuse it.
 */
import {
  computed,
  inject,
  onMounted,
  onBeforeUnmount,
  ref,
  type Ref,
} from 'vue'
import { Plus, X } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import EnumNameCell from './EnumNameCell.vue'
import {
  fetchDeferredEnum,
  isInlineEnum,
  type Option,
} from './idnode-fields/deferredEnum'
import { resolveChannelListDescriptor } from '@/utils/channelListDescriptor'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  value: unknown
  row?: BaseRow
  col: ColumnDef
}>()

const { t } = useI18n()

/* Provided by IdnodeGrid when the grid is in cell-edit mode (the
 * pencil toggle is on, or `alwaysEdit` consumers like
 * ChannelManageDrawer force it). Falls back to a static-false
 * inject outside an IdnodeGrid ancestor so the cell still mounts
 * — it just stays in delegated read-only mode. */
const isActivelyEditing = inject<Ref<boolean> | undefined>(
  'idnodeGridActivelyEditing',
  undefined,
)
const isManaging = computed(() => isActivelyEditing?.value ?? false)

/* Provided by IdnodeGrid alongside the Manage flag. Mutations
 * funnel into the same `useInlineEdit` dirty store the cell-edit
 * mode uses. Null fallback keeps the cell from crashing if mounted
 * outside an inline-edit-enabled grid. */
const commit = inject<
  ((uuid: string, field: string, value: unknown) => void) | null
>('idnodeGridInlineCommit', null)

/* Read counterpart: lets currentTags pull the LIVE dirty value
 * directly from the inlineEdit store instead of relying on the
 * `value` prop, which only refreshes when PrimeVue re-runs the
 * body slot. With virtual-scroller / stable-ref optimisations,
 * the slot is elided on dirty-only changes, leaving props.value
 * frozen at its initial server value. Reading via this inject
 * tracks dirtyMap as a reactive dep on the cell itself, so the
 * chip display updates the instant commit() lands — even if
 * PrimeVue never re-invokes the slot. */
const cellValueFn = inject<
  ((uuid: string, field: string, fallback: unknown) => unknown) | null
>('idnodeGridCellValue', null)

/* Tag option cache — resolves the column's enumSource the same way
 * EnumNameCell does, so the chip labels match the read-only view
 * exactly and we share the fetch with sibling cells. */
const options = ref<Option[] | null>(null)
onMounted(() => {
  const src = resolveChannelListDescriptor(props.col.enumSource)
  if (!src) return
  if (isInlineEnum(src)) {
    options.value = src
    return
  }
  fetchDeferredEnum(src)
    .then((res) => {
      options.value = res
    })
    .catch(() => {
      options.value = []
    })
})

function labelFor(key: string): string {
  if (options.value === null) return key
  const hit = options.value.find((o) => String(o.key) === key)
  return hit ? hit.val : key
}

/* Normalise the row's current value to a string[] regardless of
 * whether the wire shape is an array or a single uuid string.
 * Prefer the live dirty-aware accessor when available so the
 * chip display reactively tracks commits; fall back to props.
 * value when the cell is mounted outside an inline-edit grid.
 *
 * Sort by RESOLVED LABEL so chip display is consistent across
 * rows — the server emits tags in attach-order, which differs
 * per channel ("HD, News" on one row vs "News, HD" on another).
 * Sorting at the display layer is safe because islist values
 * are sets (see valuesMatch). When the enum options haven't
 * resolved yet, `labelFor` returns the uuid — that's still a
 * stable order, just not the eventual alphabetical one. */
const currentTags = computed<string[]>(() => {
  let v: unknown = props.value
  if (cellValueFn && props.row?.uuid) {
    v = cellValueFn(props.row.uuid, props.col.field, props.value)
  }
  let normalized: string[]
  if (Array.isArray(v)) {
    normalized = v.filter((x): x is string => typeof x === 'string')
  } else if (typeof v === 'string' && v) {
    normalized = [v]
  } else {
    normalized = []
  }
  return [...normalized].sort((u1, u2) =>
    labelFor(u1).localeCompare(labelFor(u2)),
  )
})

/* Tags available in the picker — every option NOT already on the
 * row. Sorted by label for predictable order. */
const availableTags = computed<Option[]>(() => {
  if (options.value === null) return []
  const taken = new Set(currentTags.value)
  return options.value
    .filter((o) => !taken.has(String(o.key)))
    .sort((a, b) => a.val.localeCompare(b.val))
})

function removeTag(uuid: string): void {
  if (!commit || !props.row?.uuid) return
  const next = currentTags.value.filter((u) => u !== uuid)
  commit(props.row.uuid, props.col.field, next)
}

function addTag(uuid: string): void {
  if (!commit || !props.row?.uuid) return
  if (currentTags.value.includes(uuid)) return /* defensive, picker already filters */
  const next = [...currentTags.value, uuid]
  commit(props.row.uuid, props.col.field, next)
  pickerOpen.value = false
}

/* Picker open / close — anchored to the `+` button. Outside-click
 * closes; clicking the same `+` toggles. Keyboard Escape closes.
 *
 * The picker is teleported to <body> so it escapes the PrimeVue
 * table cell's `overflow: hidden`, which would otherwise clip the
 * dropdown to the cell's bounds (only the first row would be
 * visible, and overflowing rows hidden). Position is computed
 * from the + button's bounding rect on open. */
const pickerOpen = ref(false)
const wrapperRef = ref<HTMLElement | null>(null)
const addBtnRef = ref<HTMLButtonElement | null>(null)
const pickerRef = ref<HTMLElement | null>(null)
const pickerPos = ref<{ top: number; left: number }>({ top: 0, left: 0 })

function positionPicker(): void {
  const btn = addBtnRef.value
  if (!btn) return
  const r = btn.getBoundingClientRect()
  pickerPos.value = { top: r.bottom + 4, left: r.left }
}

function togglePicker(): void {
  pickerOpen.value = !pickerOpen.value
  if (pickerOpen.value) positionPicker()
}

function onDocumentClick(e: MouseEvent): void {
  if (!pickerOpen.value) return
  const target = e.target as Node
  if (wrapperRef.value?.contains(target)) return
  if (pickerRef.value?.contains(target)) return
  pickerOpen.value = false
}

function onKeydown(e: KeyboardEvent): void {
  if (e.key === 'Escape' && pickerOpen.value) pickerOpen.value = false
}

/* Reposition on scroll/resize while open — keeps the picker
 * anchored to the + button when the table or window moves. */
function onReposition(): void {
  if (pickerOpen.value) positionPicker()
}

onMounted(() => {
  document.addEventListener('click', onDocumentClick)
  document.addEventListener('keydown', onKeydown)
  window.addEventListener('scroll', onReposition, true)
  window.addEventListener('resize', onReposition)
})
onBeforeUnmount(() => {
  document.removeEventListener('click', onDocumentClick)
  document.removeEventListener('keydown', onKeydown)
  window.removeEventListener('scroll', onReposition, true)
  window.removeEventListener('resize', onReposition)
})
</script>

<template>
  <!-- Outside Manage mode: delegate to the standard read-only enum
       cell so the column looks identical to every other view of it.
       Pass the sorted tag list so every row renders the same set in
       the same order — server attach-order varies per channel. -->
  <EnumNameCell
    v-if="!isManaging"
    :value="currentTags"
    :row="row"
    :col="col"
  />

  <!-- Manage mode: chips + add button + (conditional) picker. -->
  <span v-else ref="wrapperRef" class="tag-chip-cell">
    <span
      v-for="uuid in currentTags"
      :key="uuid"
      class="tag-chip-cell__chip"
    >
      <span class="tag-chip-cell__label">{{ labelFor(uuid) }}</span>
      <button
        type="button"
        class="tag-chip-cell__remove"
        :title="t('Remove tag')"
        :aria-label="t('Remove tag {0}', labelFor(uuid))"
        @click.stop="removeTag(uuid)"
      >
        <X :size="11" :stroke-width="2.5" aria-hidden="true" />
      </button>
    </span>

    <span class="tag-chip-cell__add-wrap">
      <button
        ref="addBtnRef"
        type="button"
        class="tag-chip-cell__add"
        :title="t('Add tag')"
        :aria-label="t('Add tag')"
        :disabled="availableTags.length === 0"
        @click.stop="togglePicker"
      >
        <Plus :size="12" :stroke-width="2.5" aria-hidden="true" />
      </button>
    </span>
  </span>

  <!-- Picker teleported to <body> to escape the PrimeVue table
       cell's overflow:hidden. Position is computed from the +
       button's rect on open + window scroll/resize. Each option
       is a real <button> so screen readers + keyboard navigation
       get native semantics — avoids the listbox/option ARIA
       pitfalls (aria-selected required, non-interactive elements
       with interactive roles) for a small unordered menu where
       single-selection-vs-multi-selection isn't meaningful. -->
  <Teleport to="body">
    <div
      v-if="isManaging && pickerOpen && availableTags.length > 0"
      ref="pickerRef"
      class="tag-chip-cell__picker"
      :style="{
        position: 'fixed',
        top: `${pickerPos.top}px`,
        left: `${pickerPos.left}px`,
      }"
    >
      <button
        v-for="opt in availableTags"
        :key="opt.key"
        type="button"
        class="tag-chip-cell__picker-item"
        @click.stop="addTag(String(opt.key))"
      >
        {{ opt.val }}
      </button>
    </div>
  </Teleport>
</template>

<style scoped>
.tag-chip-cell {
  display: inline-flex;
  align-items: center;
  flex-wrap: wrap;
  gap: var(--tvh-space-1);
  max-width: 100%;
}

.tag-chip-cell__chip {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px 4px 2px 8px;
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
  border: 1px solid color-mix(
    in srgb,
    var(--tvh-primary) 30%,
    var(--tvh-border)
  );
  border-radius: var(--tvh-radius-sm);
  font-size: var(--tvh-text-sm);
  line-height: 1.2;
}

.tag-chip-cell__label {
  white-space: nowrap;
}

.tag-chip-cell__remove {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 16px;
  height: 16px;
  padding: 0;
  background: none;
  border: 0;
  color: var(--tvh-text-muted);
  cursor: pointer;
  border-radius: 999px;
  transition: background var(--tvh-transition), color var(--tvh-transition);
}

.tag-chip-cell__remove:hover,
.tag-chip-cell__remove:focus-visible {
  background: var(--tvh-error);
  color: var(--tvh-bg-surface);
  outline: none;
}

.tag-chip-cell__add-wrap {
  position: relative;
  display: inline-flex;
}

.tag-chip-cell__add {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 20px;
  height: 20px;
  padding: 0;
  background: var(--tvh-bg-surface);
  border: 1px dashed var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  cursor: pointer;
  transition: border-color var(--tvh-transition), color var(--tvh-transition);
}

.tag-chip-cell__add:hover:not(:disabled),
.tag-chip-cell__add:focus-visible {
  border-color: var(--tvh-primary);
  color: var(--tvh-primary);
  outline: none;
}

.tag-chip-cell__add:disabled {
  cursor: not-allowed;
  opacity: 0.4;
}

/* Picker is teleported to <body> and positioned via inline
 * `position: fixed` + top/left from the + button's rect. The
 * static rules below provide the chrome (size cap, surface,
 * border, scroll); positioning is supplied inline.
 *
 * z-index sits ABOVE PrimeVue's overlay/modal stack — Drawer
 * + Dialog + Popover all sit in the 1100-1300 range with
 * dynamic ZIndex bumping. 2000 puts us reliably above; bump
 * if a future component goes higher (none today). */
.tag-chip-cell__picker {
  z-index: 2000;
  min-width: 160px;
  max-height: 240px;
  margin: 0;
  padding: var(--tvh-space-1) 0;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
  list-style: none;
  overflow-y: auto;
}

.tag-chip-cell__picker-item {
  display: block;
  width: 100%;
  padding: var(--tvh-space-1) var(--tvh-space-3);
  text-align: left;
  background: none;
  border: 0;
  font: inherit;
  color: inherit;
  font-size: var(--tvh-text-sm);
  cursor: pointer;
}

.tag-chip-cell__picker-item:hover,
.tag-chip-cell__picker-item:focus-visible {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
  outline: none;
}
</style>
