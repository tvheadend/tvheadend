<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * DVR Smart autorecs view — the expression-bearing half of the
 * autorec class. Same idnode class as AutorecsView
 * (`dvrautorec`), different server-side grid: `dvr/autorec/grid_smart`
 * returns only entries with a non-empty `expression`; the default
 * grid returns the rest. Membership in a view is how a user tells
 * smart from flat.
 *
 * The editor carries the expression editor, the preview and the
 * settings fields only. The flat predicate fields are deliberately
 * absent: on a smart entry they are PO_RDONLY no-constraint ballast,
 * and offering them alongside an expression is the contradictory
 * state the view split exists to make unrepresentable.
 *
 * Rules arrive here via Convert on the flat Autorecs view, or via
 * Add (a new rule born smart: name + expression + settings).
 */
import { computed } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import SmartExpressionEditor from '@/components/idnode-fields/SmartExpressionEditor.vue'
import type { ColumnDef } from '@/types/column'
import { apiCall } from '@/api/client'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useErrorDialog } from '@/composables/useErrorDialog'
import { useDvrRulesView } from '@/composables/useDvrRulesView'
import { AUTOREC_FIELDS } from './dvrFieldDefs'
import { adminAwareEditList } from './dvrToolbarHelpers'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()
const confirmDialog = useConfirmDialog()
const errorDialog = useErrorDialog()

/* The expression edits in the drawer via the smart-expression
 * editor (JSONC with a branch tree, per-branch skip toggles and the
 * server-side match preview) — the same fieldGroups hook the flat
 * view uses for its start-window picker. */
const SMART_AUTOREC_FIELD_GROUPS = [
  {
    keys: ['expression'] as const,
    component: SmartExpressionEditor,
  },
] as const

/* Settings columns only — the flat predicate columns would render
 * empty on every row here. `expression` is visible by default in
 * this view (it IS the rule), overriding the field-def default that
 * hides it as a code blob in mixed contexts. */
const cols: ColumnDef[] = [
  { field: 'enabled', ...AUTOREC_FIELDS.enabled, editable: true },
  { field: 'name', ...AUTOREC_FIELDS.name, editable: true },
  { field: 'expression', ...AUTOREC_FIELDS.expression, hiddenByDefault: false },

  /* Advanced — server's PO_ADVANCED filter gates these on basic users */
  { field: 'record', ...AUTOREC_FIELDS.record, editable: true },
  { field: 'pri', ...AUTOREC_FIELDS.pri, editable: true },
  { field: 'directory', ...AUTOREC_FIELDS.directory, editable: true },
  { field: 'config_name', ...AUTOREC_FIELDS.config_name, editable: true },

  /* Expert */
  { field: 'owner', ...AUTOREC_FIELDS.owner, editable: true },
  { field: 'creator', ...AUTOREC_FIELDS.creator, editable: true },
  { field: 'comment', ...AUTOREC_FIELDS.comment, editable: true },
]

/* Editor field list: expression + the settings subset (settings
 * keep their normal meaning on a smart entry). No flat predicates,
 * not even read-only. */
const EDITOR_LIST_BASE = 'name,expression,record,directory,config_name,pri,comment'

const editList = adminAwareEditList({
  head: 'enabled,start_extra,stop_extra',
  base: EDITOR_LIST_BASE,
  adminExtra: 'owner,creator',
  tail: 'retention,removal,maxcount,maxsched',
})

const {
  editingUuid,
  editingUuids,
  creatingBase,
  gridRef,
  editorLevel,
  editorList,
  openEditor,
  closeEditor,
  flipToEdit,
  buildActions,
} = useDvrRulesView({
  createBase: 'dvr/autorec',
  editList,
  createList: EDITOR_LIST_BASE,
  entityNoun: t('smart autorec entries'),
  addTooltip: t('Add a new smart autorec entry'),
})

/*
 * Multi-edit excludes the expression: it is the rule's whole
 * identity, so bulk-writing the same expression onto several rules
 * is near-certainly a mistake — and the multi-edit path skips the
 * per-save preview warning below, so allowing it would also bypass
 * the voluntary match-all check. Settings fields (priority,
 * retention, profile, ...) stay bulk-editable, which is what
 * multi-select is actually for.
 */
const smartEditorList = computed(() => {
  if ((editingUuids.value?.length ?? 0) >= 2)
    return editorList.value
      .split(',')
      .filter((f) => f !== 'expression')
      .join(',')
  return editorList.value
})

/*
 * The voluntary half of the match-all safeguard: the server
 * hard-gates CREATES only, so the
 * editor warns before an edit save whose dirty expression matches
 * most of the EPG. The threshold lives server-side; the preview
 * response's `matchall` flag carries the verdict, so no constant is
 * duplicated here. Advisory only: an unreachable preview or an
 * invalid expression falls through to the normal save (an invalid
 * expression stores flagged and force-disabled, the flat idiom).
 */
/*
 * Born-smart guarantee: a rule created here must carry an
 * expression. Without one the server would happily create a flat
 * rule with no selectors — inert, and listed in the flat Autorecs
 * view rather than this one, so Save would appear to do nothing.
 */
async function smartAutorecPreCreate(
  conf: Record<string, unknown>): Promise<boolean> {
  const expression = conf.expression
  if (typeof expression === 'string' && expression.trim() !== '') return true
  errorDialog.show({
    title: t('Expression required'),
    message: t('A smart autorec needs an expression. Write one, or use Convert on a rule in the Autorecs view.'),
  })
  return false
}

async function smartAutorecPreSave(
  dirty: Record<string, unknown>, uuid: string): Promise<boolean> {
  const expression = dirty.expression
  if (typeof expression !== 'string') return true
  /* Stays-smart guarantee, the edit-side mirror of pre-create:
   * saving an empty expression would turn the rule flat server-side
   * and strand its row in this grid (the Comet patch reloads rows
   * by uuid; it never evicts rows that stop matching the endpoint).
   * Disabling or deleting is the way to retire a smart rule. */
  if (expression.trim() === '') {
    errorDialog.show({
      title: t('Expression required'),
      message: t('A smart autorec needs an expression. Disable the rule to stop it matching, or delete it.'),
    })
    return false
  }
  try {
    const r = await apiCall<{ matchall?: number; matched?: number; scanned?: number; error?: string }>(
      'dvr/autorec/preview',
      { conf: JSON.stringify({ ...dirty, uuid }), limit: 1 })
    if (!r.matchall) return true
    return await confirmDialog.ask(
      t('The expression matches most of the EPG ({0} of {1} events). Save anyway?')
        .replace('{0}', String(r.matched ?? 0))
        .replace('{1}', String(r.scanned ?? 0)),
      {
        header: t('Confirm save'),
        acceptLabel: t('Save anyway'),
        rejectLabel: t('Cancel'),
        severity: 'danger',
      })
  } catch (e) {
    /* the gate could not run: say so instead of waving the save
     * through in silence */
    return await confirmDialog.ask(
      t('The match-all check could not run ({0}). Save anyway?')
        .replace('{0}', apiErrorMessage(e)),
      {
        header: t('Confirm save'),
        acceptLabel: t('Save anyway'),
        rejectLabel: t('Cancel'),
        severity: 'danger',
      })
  }
}
</script>

<template>
  <IdnodeGrid
    ref="gridRef"
    endpoint="dvr/autorec/grid_smart"
    help-page="dvr_smartautorec"
    :columns="cols"
    store-key="dvr-autorec-smart"
    :default-sort="{ key: 'name', dir: 'ASC' }"
    :virtual-scroller-options="{ itemSize: 36, lazy: false }"
    count-label="smart autorecs"
    edit-mode="cell"
    class="smart-autorec__grid"
    @row-dblclick="(row) => openEditor([row])"
  >
    <template #empty>
      <p class="smart-autorec__empty">
        {{ t('No smart autorecs. Convert a rule from the Autorecs view, or click Add to write an expression from scratch.') }}
      </p>
    </template>
    <template #toolbarActions="{ selection, clearSelection }">
      <ActionMenu :actions="buildActions(selection, clearSelection)" />
    </template>
  </IdnodeGrid>
  <IdnodeEditor
    :uuid="editingUuid"
    :uuids="editingUuids"
    :create-base="creatingBase"
    :level="editorLevel"
    :list="smartEditorList"
    :title="editingUuid ? t('Edit Smart Autorec') : t('Add Smart Autorec')"
    :field-groups="SMART_AUTOREC_FIELD_GROUPS"
    :pre-save="smartAutorecPreSave"
    :pre-create="smartAutorecPreCreate"
    wide
    @close="closeEditor"
    @created="flipToEdit"
  />
</template>

<style scoped>
.smart-autorec__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.smart-autorec__empty {
  color: var(--tvh-text-muted);
}
</style>
