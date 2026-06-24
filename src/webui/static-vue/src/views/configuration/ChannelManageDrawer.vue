<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ChannelManageDrawer — the dedicated reorganise surface for
 * Configuration → Channels.
 *
 * Opens as a modal drawer with its OWN IdnodeGrid instance
 * (separate `store-key`, separate filter / sort / column
 * persistence) so the user's regular Channels view state is
 * untouched. Inside: a slimmed-down fixed-column grid (Enabled,
 * Name, Number, Tag), fixed sort (number ASC), no filter UI, and
 * always-on edit mode — the user opened this drawer specifically
 * to mutate.
 *
 * Three editing affordances:
 *   1. Drag-to-reorder rows by their drag-handle column → commits
 *      slot-based number reassignments via useInlineEdit.
 *   2. Tag chip painter cells (EditableTagChipCell) — click × to
 *      remove a tag, click + to pick an additional tag. Each click
 *      commits per-row, preserving each channel's other tags.
 *   3. Bulk toolbar actions on the multi-select — Add Tag ▾ /
 *      Remove Tag ▾ / Enable / Disable. Same per-row "paint" not
 *      "replace" semantics.
 *
 * All edits accumulate in the host IdnodeGrid's inline-edit dirty
 * store; the grid's own Save / Cancel buttons in the toolbar
 * flush or revert.
 *
 * Closing the drawer with unsaved changes loses them — mirrors
 * cell-edit's nav-away semantics (which only catches Vue-router
 * navigation, not drawer-close). A confirm-on-close guard would
 * be a reasonable follow-up if dropping unsaved edits proves
 * surprising in practice.
 */
import { computed, onBeforeUnmount, onMounted, provide, ref, watch } from 'vue'
import Drawer from 'primevue/drawer'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import EditableTagChipCell from '@/components/EditableTagChipCell.vue'
import EditableNumberCell from '@/components/EditableNumberCell.vue'
import BooleanCell from '@/components/BooleanCell.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import SettingsPopover from '@/components/SettingsPopover.vue'
import { useResizableDrawerWidth } from '@/composables/useResizableDrawerWidth'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useI18n } from '@/composables/useI18n'
import {
  fetchDeferredEnum,
  type Option,
} from '@/components/idnode-fields/deferredEnum'
import {
  renumberAsIntegers,
  renumberPreservingMinors,
} from '@/utils/slotReorder'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'

const props = defineProps<{ visible: boolean }>()
const emit = defineEmits<{ 'update:visible': [value: boolean] }>()

const { t } = useI18n()
const confirm = useConfirmDialog()

/* Channel-tag enum — fetched once on first mount, used by the
 * bulk Add Tag / Remove Tag toolbar action's children. The inline
 * EditableTagChipCell uses its own fetchDeferredEnum on the same
 * cache key, so this is essentially a coalesced shared fetch. */
const allTagOptions = ref<Option[]>([])
onMounted(() => {
  fetchDeferredEnum({
    type: 'api',
    uri: 'channeltag/list',
    params: { all: 1 },
  })
    .then((opts) => {
      allTagOptions.value = opts
    })
    .catch(() => {
      /* Empty options → Add Tag menu shows no children; chip
       * picker fails the same way. Deferred-enum cache logs the
       * underlying error once. */
      allTagOptions.value = []
    })
})

/* Hardcoded column set — minimal for the reorganise workflow.
 * sortable / filterType deliberately omitted so PrimeVue won't
 * surface sort / filter affordances on the header. */
const cols = computed<ColumnDef[]>(() => [
  {
    field: 'enabled',
    label: t('Enabled'),
    width: 100,
    cellComponent: BooleanCell,
    editable: true,
    minVisible: 'phone',
  },
  {
    field: 'name',
    label: t('Name'),
    width: 280,
    editable: true,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
  {
    field: 'number',
    label: t('Number'),
    width: 110,
    cellComponent: EditableNumberCell,
    editable: true,
    minVisible: 'phone',
  },
  {
    field: 'tags',
    label: t('Tags'),
    width: 360,
    cellComponent: EditableTagChipCell,
    enumSource: {
      type: 'api',
      uri: 'channeltag/list',
      params: { all: 1 },
    },
    editable: true,
  },
])

/* IdnodeGrid exposes via defineExpose. Vue 3 auto-unwraps
 * TOP-LEVEL refs on the component proxy — so
 * `gridRef.value.effectiveEntries` (exposed as a computed
 * ref) is the plain Row[] — no `.value` indirection.
 * `inlineEdit` is exposed as a plain object whose PROPERTIES
 * include refs (dirtyMap is `Ref<Map>`); those nested refs
 * are NOT auto-unwrapped and still need `.value`. */
const gridRef = ref<{
  inlineEdit?: {
    commitCell: (uuid: string, field: string, value: unknown) => void
    cellValue: (uuid: string, field: string, fallback: unknown) => unknown
    dirtyMap: { value: Map<string, Map<string, unknown>> }
  } | null
  effectiveEntries?: BaseRow[]
  /* Exposed by IdnodeGrid so the drawer's "View options" popover can
   * disable Reset when the layout already matches defaults, and call
   * the same reset path the in-grid GridSettingsMenu uses. */
  isAtDefaults?: boolean
  resetGridPrefs?: () => void
} | null>(null)

/* Defaults predicate for the drawer-header SettingsPopover. Reads
 * through the IdnodeGrid ref so the disabled state mirrors the
 * in-grid GridSettingsMenu without duplicating the predicate.
 * Defaults to true (Reset disabled) when the grid isn't mounted
 * yet so the button isn't briefly enabled during drawer open.
 *
 * AND-composed with `drawerWidth.isAtDefault` so the Reset button
 * lights up when EITHER the grid layout OR the user-resized
 * drawer width deviates from defaults. */
const drawerWidth = useResizableDrawerWidth({
  storageKey: 'channel-manage-drawer:width',
  defaultPx: 1100,
  minPx: 480,
  maxPx: 1600,
})
const layoutAtDefaults = computed(
  () => (gridRef.value?.isAtDefaults ?? true) && drawerWidth.isAtDefault.value,
)

/* Single Reset entry-point — clears BOTH the grid layout (column
 * order / widths / sort / filters) AND the resized drawer width.
 * Same affordance covers every per-drawer preference. The
 * power-user shortcut for width-only reset is double-clicking the
 * drag handle on the drawer's inside-left edge (see template). */
function onResetView(): void {
  gridRef.value?.resetGridPrefs?.()
  drawerWidth.reset()
}

/* Ref + lifecycle wiring for the drag handle. attachHandle
 * returns a teardown fn we call on unmount so document-level
 * mousemove/touchmove listeners can't leak past the drawer's
 * lifetime if the user happens to be mid-drag when navigating
 * away. */
const resizeHandleEl = ref<HTMLElement | null>(null)
let detachResizeHandle: (() => void) | null = null

watch(resizeHandleEl, (el) => {
  if (detachResizeHandle) {
    detachResizeHandle()
    detachResizeHandle = null
  }
  if (el) detachResizeHandle = drawerWidth.attachHandle(el)
})

function onResizeHandleDblclick(): void {
  drawerWidth.reset()
}

onBeforeUnmount(() => {
  if (detachResizeHandle) {
    detachResizeHandle()
    detachResizeHandle = null
  }
})

/* Discard-on-close guard. Same pattern IdnodeEditor uses
 * (`IdnodeEditor.vue:1497-1516`) — wrap visibility through a
 * writable computed so PrimeVue's "close drawer" intent (Esc,
 * click-outside, X button) routes through `attemptClose`. If the
 * inline-edit dirty store has any unsaved cells, prompt; on
 * "Keep editing" the setter simply doesn't propagate, the
 * computed re-reads `props.visible` (still true), and PrimeVue
 * keeps the drawer open.
 *
 * `dirtyMap` is exposed by IdnodeGrid as a Vue Ref<Map<...>>
 * (nested refs in defineExpose are NOT auto-unwrapped per Vue's
 * docs), so we read its `.value.size`. */
const visibleProxy = computed<boolean>({
  get: () => props.visible,
  set: (v) => {
    if (v) {
      emit('update:visible', true)
      return
    }
    void attemptClose()
  },
})

async function attemptClose(): Promise<void> {
  const dirtySize = gridRef.value?.inlineEdit?.dirtyMap?.value?.size ?? 0
  if (dirtySize > 0) {
    const ok = await confirm.ask(t('Discard unsaved channel changes?'), {
      header: t('Reorganize channels'),
      severity: 'danger',
      acceptLabel: t('Discard'),
      rejectLabel: t('Keep editing'),
    })
    if (!ok) return
  }
  emit('update:visible', false)
}

/* Enabled-only filter default. Most reorganise sessions are
 * about the channels the user actually watches — the noise of
 * scanned-but-disabled services would crowd the working set.
 * The checkbox in the toolbar lets the rare "include
 * disabled" case opt in.
 *
 * Filtering is CLIENT-SIDE (not via the gridStore's server
 * filter) so it can respect dirty `enabled` toggles: a
 * channel the user just dirtied to enabled appears
 * immediately even though the server still has it disabled,
 * and vice-versa. The grid still pulls all channels
 * (extraParams `all: 1`); the predicate below decides what
 * actually renders. */
const includeDisabled = ref(false)

function effectiveEnabledFilter(
  row: BaseRow,
  dirtyForRow: ReadonlyMap<string, unknown> | undefined,
): boolean {
  if (includeDisabled.value) return true
  const effective = dirtyForRow?.has('enabled')
    ? dirtyForRow.get('enabled')
    : row.enabled
  return !!effective
}

function commitCell(uuid: string, field: string, value: unknown): void {
  gridRef.value?.inlineEdit?.commitCell(uuid, field, value)
}

/* Duplicate-number tracker — Map<stringified number, count of
 * rows holding it>. Provided to descendant EditableNumberCell
 * instances so each cell can light its warning badge when its
 * value isn't unique.
 *
 * Reactivity: depends on the grid's effectiveEntries (post-
 * filter, post-dirty-aware-sort) AND the dirtyMap. A user
 * dirtying a row to a number another row already holds lights
 * up BOTH rows' badges on the next tick. Unnumbered rows (0 /
 * null / non-finite) are excluded — twenty just-scanned
 * channels mustn't all flag each other.
 *
 * Computed eagerly per change; provide as a Ref so injected
 * cells track only the .value, not us, and the parent's
 * re-render cost stays scoped to the cell. */
const numberDuplicateCounts = computed<Map<string, number>>(() => {
  const counts = new Map<string, number>()
  const grid = gridRef.value
  if (!grid?.effectiveEntries) return counts
  const dirty = grid.inlineEdit?.dirtyMap?.value
  for (const row of grid.effectiveEntries) {
    if (typeof row.uuid !== 'string') continue
    /* Effective number: dirty wins over server value (mirrors
     * cellModelValue's read). */
    const dirtyForRow = dirty?.get(row.uuid)
    const effective =
      dirtyForRow?.has('number') ? dirtyForRow.get('number') : row.number
    const n = Number(effective)
    if (!Number.isFinite(n) || n === 0) continue /* unnumbered → skip */
    /* Key by the same string the cell uses to render — that's
     * what the cell will look up. Integer collapses to "5"; the
     * dotted value stays "5.1". */
    const key = Number.isInteger(n) ? String(n) : String(effective)
    counts.set(key, (counts.get(key) ?? 0) + 1)
  }
  return counts
})
provide('numberDuplicateCounts', numberDuplicateCounts)

/* Renumber actions — operate on the CURRENTLY VISIBLE rows in
 * their current display order. Both flavours flow through
 * inlineEdit.commitCell so they accumulate in the dirty store
 * just like drag-reorder; user reviews + clicks Save to commit
 * the lot to the server. */
function renumberVisibleAsIntegers(): void {
  const grid = gridRef.value
  if (!grid?.effectiveEntries || !grid.inlineEdit) return
  renumberAsIntegers(
    grid.effectiveEntries,
    'number',
    1,
    grid.inlineEdit.commitCell,
  )
}

function renumberVisiblePreservingMinors(): void {
  const grid = gridRef.value
  if (!grid?.effectiveEntries || !grid.inlineEdit) return
  renumberPreservingMinors(
    grid.effectiveEntries,
    'number',
    1,
    grid.inlineEdit.commitCell,
  )
}

function buildRenumberActions(): ActionDef[] {
  return [
    {
      id: 'manage-renumber',
      label: t('Renumber'),
      tooltip: t('Reassign channel numbers across all visible rows'),
      children: [
        {
          id: 'manage-renumber-integers',
          label: t('As integers (1, 2, 3, …)'),
          tooltip: t(
            'Assign 1 to the first row, 2 to the second, and so on — sub-channel suffixes (.1) are flattened.',
          ),
          onClick: renumberVisibleAsIntegers,
        },
        {
          id: 'manage-renumber-preserve',
          label: t('Preserving sub-channels (1, 1.1, 2, …)'),
          tooltip: t(
            'Renumber major channels sequentially while keeping each sub-channel\'s .N suffix attached.',
          ),
          onClick: renumberVisiblePreservingMinors,
        },
      ],
    },
  ]
}

/* Bulk action helpers — per-row "paint" semantics. Adding HD to
 * 5 channels preserves each channel's existing tags. */
function bulkAddTag(tagUuid: string, selection: BaseRow[]): void {
  for (const row of selection) {
    if (typeof row.uuid !== 'string') continue
    const current = Array.isArray(row.tags)
      ? (row.tags as unknown[]).filter(
          (u): u is string => typeof u === 'string',
        )
      : []
    if (current.includes(tagUuid)) continue
    commitCell(row.uuid, 'tags', [...current, tagUuid])
  }
}

function bulkRemoveTag(tagUuid: string, selection: BaseRow[]): void {
  for (const row of selection) {
    if (typeof row.uuid !== 'string') continue
    const current = Array.isArray(row.tags)
      ? (row.tags as unknown[]).filter(
          (u): u is string => typeof u === 'string',
        )
      : []
    if (!current.includes(tagUuid)) continue
    commitCell(row.uuid, 'tags', current.filter((u) => u !== tagUuid))
  }
}

function bulkSetEnabled(value: boolean, selection: BaseRow[]): void {
  for (const row of selection) {
    if (typeof row.uuid !== 'string') continue
    commitCell(row.uuid, 'enabled', value)
  }
}

/* Collect tag uuids present on any selected row, so the "Remove
 * tag" submenu only offers tags the user can actually remove.
 * Empty-selection → full tag list (lets the menu still expand
 * for affordance discovery). */
function tagsInSelection(selection: BaseRow[]): string[] {
  if (selection.length === 0) {
    return allTagOptions.value.map((o) => String(o.key))
  }
  const set = new Set<string>()
  for (const row of selection) {
    if (!Array.isArray(row.tags)) continue
    for (const u of row.tags as unknown[]) {
      if (typeof u === 'string') set.add(u)
    }
  }
  return [...set]
}

function tagLabel(uuid: string): string {
  const hit = allTagOptions.value.find((o) => String(o.key) === uuid)
  return hit ? hit.val : uuid
}

function buildBulkActions(selection: BaseRow[]): ActionDef[] {
  const noSelection = selection.length === 0
  const addChildren: ActionDef[] = allTagOptions.value.map((o) => ({
    id: `manage-add-tag-${o.key}`,
    label: o.val,
    onClick: () => bulkAddTag(String(o.key), selection),
  }))
  const removeUuids = tagsInSelection(selection)
  const removeChildren: ActionDef[] = removeUuids.map((u) => ({
    id: `manage-remove-tag-${u}`,
    label: tagLabel(u),
    onClick: () => bulkRemoveTag(u, selection),
  }))
  return [
    {
      id: 'manage-add-tag',
      label: t('Add tag'),
      tooltip: t(
        'Add a tag to every selected channel (preserves existing tags)',
      ),
      disabled: noSelection || addChildren.length === 0,
      children: addChildren,
    },
    {
      id: 'manage-remove-tag',
      label: t('Remove tag'),
      tooltip: t('Remove a tag from every selected channel'),
      disabled: noSelection || removeChildren.length === 0,
      children: removeChildren,
    },
    {
      id: 'manage-enable',
      label: t('Enable'),
      tooltip: t('Enable every selected channel'),
      disabled: noSelection,
      onClick: () => bulkSetEnabled(true, selection),
    },
    {
      id: 'manage-disable',
      label: t('Disable'),
      tooltip: t('Disable every selected channel'),
      disabled: noSelection,
      onClick: () => bulkSetEnabled(false, selection),
    },
  ]
}
</script>

<template>
  <Drawer
    v-model:visible="visibleProxy"
    position="right"
    class="channel-manage-drawer"
    :style="drawerWidth.widthStyle.value"
    :pt="{ root: { class: 'channel-manage-drawer__root' } }"
  >
    <!--
      Resize handle — thin vertical strip on the drawer's
      inside-left edge. Mouse drag and touch drag both supported
      via the composable; double-click resets the width only
      (column tweaks stay). The handle is absolutely positioned
      so it doesn't perturb the drawer's flow layout.
    -->
    <div
      ref="resizeHandleEl"
      class="channel-manage-drawer__resize-handle"
      role="separator"
      aria-orientation="vertical"
      :title="t('Drag to resize · double-click to reset')"
      @dblclick="onResizeHandleDblclick"
    />
    <template #header>
      <div class="channel-manage-drawer__header">
        <span class="channel-manage-drawer__title">{{ t('Reorganize channels') }}</span>
      </div>
    </template>

    <div class="channel-manage-drawer__body">
      <IdnodeGrid
        ref="gridRef"
        endpoint="channel/grid"
        entity-class="channel"
        :columns="cols"
        store-key="config-channel-manage"
        isolated-store
        :default-sort="{ key: 'number', dir: 'ASC' }"
        :extra-params="{ all: 1 }"
        :virtual-scroller-options="{ itemSize: 36, lazy: false }"
        :count-label="t('channels')"
        edit-mode="cell"
        always-edit
        reorderable-rows
        reorder-field="number"
        reorder-preserve-minor
        dirty-aware-sort
        :column-actions="{}"
        :client-filter="effectiveEnabledFilter"
        class="channel-manage-drawer__grid"
      >
        <template #empty>
          <p class="channel-manage-drawer__empty">
            {{ t('No channels to reorganise yet — create or map some first.') }}
          </p>
        </template>
        <template #editingActions="{ selection }">
          <label class="channel-manage-drawer__include-disabled">
            <input
              v-model="includeDisabled"
              type="checkbox"
            />
            {{ t('Include disabled') }}
          </label>
          <!--
            Single ActionMenu hosts both Renumber + bulk actions so
            they share one width-aware overflow row (ActionMenu's
            built-in `…` fallback) instead of each being a
            standalone button that never overflows together. Order:
            Renumber first (always available), bulk actions after
            (selection-gated).
          -->
          <ActionMenu :actions="[...buildRenumberActions(), ...buildBulkActions(selection)]" />
          <SettingsPopover
            class="channel-manage-drawer__view-options"
            :defaults-active="layoutAtDefaults"
            @reset="onResetView"
          />
        </template>
      </IdnodeGrid>
    </div>
  </Drawer>
</template>

<style scoped>
.channel-manage-drawer__header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

/* Push the View options popover to the right edge of the header
 * so it sits opposite the title — same convention IdnodeGrid uses
 * in its toolbar. */
.channel-manage-drawer__view-options {
  margin-left: auto;
}

.channel-manage-drawer__title {
  font-weight: 600;
  font-size: var(--tvh-text-xl);
}

.channel-manage-drawer__body {
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0;
}

.channel-manage-drawer__grid {
  flex: 1 1 auto;
  min-height: 0;
}

/* Drag handle for resizing the drawer width. Pinned to the
 * inside-left edge of the drawer panel via `:deep()` since the
 * handle is rendered inside PrimeVue's Drawer root which sits
 * outside our scoped-class subtree.
 *
 * Hover-only highlight keeps the affordance discoverable without
 * being visually noisy at rest. Hidden on phone — drawer is full-
 * viewport-width there, no axis to resize. */
:deep(.channel-manage-drawer__root) {
  position: relative;
}

.channel-manage-drawer__resize-handle {
  position: absolute;
  inset-block: 0;
  inset-inline-start: 0;
  width: 6px;
  cursor: col-resize;
  z-index: 10;
  background: transparent;
  transition: background 120ms ease;
  touch-action: none;
}

.channel-manage-drawer__resize-handle:hover {
  background: color-mix(in srgb, var(--tvh-primary) 30%, transparent);
}

@media (max-width: 767px) {
  .channel-manage-drawer__resize-handle {
    display: none;
  }
}

.channel-manage-drawer__empty {
  color: var(--tvh-text-muted);
  text-align: center;
  padding: var(--tvh-space-6);
}

/* "Include disabled" checkbox in the editing-actions toolbar
 * cluster — sits alongside the bulk-action menu. Muted text,
 * small click target — reads as a control, not a value. */
.channel-manage-drawer__include-disabled {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
  cursor: pointer;
}
</style>
