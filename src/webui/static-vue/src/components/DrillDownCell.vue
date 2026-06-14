<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DrillDownCell — text cell with a trailing chevron-right
 * "go to" icon that opens the referenced entity's editor in
 * the AppShell-mounted drill-down drawer.
 *
 * Read-mode only. The chevron is hidden when:
 *   - the row lacks the UUID companion field (graceful absence
 *     for old-server wire shapes), OR
 *   - the user doesn't have the access flag declared on the
 *     column (`col.targetAccessKey`), since clicking through
 *     to an editor the user can't open is a teaser for a path
 *     they can't take.
 *
 * Hover reveals the chevron on desktop; touch devices show it
 * always (`@media (hover: none)`). Click stops propagation so
 * the row's selection state doesn't toggle, then summons the
 * singleton entity editor via `useEntityEditor`.
 *
 * Inline-edit interaction: the cell only renders in read mode
 * because IdnodeGrid swaps in the inline editor for cells that
 * support edit mode. No extra guard needed here.
 */
import { computed, inject, type Ref } from 'vue'
import { ChevronRight } from 'lucide-vue-next'
import type { ColumnDef } from '@/types/column'
import type { BaseRow } from '@/types/grid'
import { useAccessStore } from '@/stores/access'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { useI18n } from '@/composables/useI18n'

const props = defineProps<{
  /* Standard cellComponent contract — DataGrid passes value/row/col. */
  value?: unknown
  row: BaseRow
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

/* Display text: prefer the explicit value DataGrid passed in;
 * fall back to row[col.field] for completeness. Whatever we
 * render here was rendered before — DrillDownCell is a wrapper,
 * not a transformer. */
const displayText = computed<string>(() => {
  const v = props.value
  if (v == null || v === '') {
    const fromRow = props.row[props.col.field]
    return fromRow == null ? '' : String(fromRow)
  }
  return String(v)
})

const targetUuid = computed<string | null>(() => {
  const field = props.col.targetUuidField
  if (!field) return null
  const v = props.row[field]
  return typeof v === 'string' && v ? v : null
})

const allowed = computed<boolean>(() => {
  const key = props.col.targetAccessKey
  if (!key) return true
  return !!access.data?.[key]
})

const showChevron = computed(
  () => !gridActivelyEditing?.value && targetUuid.value !== null && allowed.value,
)

function onClick(event: MouseEvent): void {
  event.stopPropagation()
  if (targetUuid.value === null) return
  entityEditor.open(targetUuid.value)
}
</script>

<template>
  <span class="drill-cell">
    <span class="drill-cell__text">{{ displayText }}</span>
    <button
      v-if="showChevron"
      type="button"
      class="drill-cell__icon"
      :title="t('Open in editor')"
      :aria-label="t('Open in editor')"
      @click="onClick"
    >
      <ChevronRight :size="14" aria-hidden="true" />
    </button>
  </span>
</template>

<style scoped>
/* Inline-flex (not block-level flex) so the hover zone matches
 * the visible content width — not the cell's full padded width.
 * Block-level flex would cause the chevron to linger when the
 * mouse moves through cell padding outside the actual text. */
.drill-cell {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  min-width: 0;
  max-width: 100%;
}

.drill-cell__text {
  flex: 1 1 auto;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.drill-cell__icon {
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

.drill-cell:hover .drill-cell__icon,
.drill-cell__icon:focus-visible {
  opacity: 1;
}

.drill-cell__icon:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.drill-cell__icon:focus-visible {
  outline: 2px solid var(--tvh-focus);
  outline-offset: 2px;
}

/* Touch devices: no hover state, show the icon always. */
@media (hover: none) {
  .drill-cell__icon {
    opacity: 1;
  }
}
</style>
