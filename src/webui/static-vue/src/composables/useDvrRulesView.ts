/*
 * useDvrRulesView — shared script-setup glue for the DVR
 * rule-list views (Autorecs / Timers). Both bind to a separate
 * idnode class (`dvrautorec` / `dvrtimerec`), but the editor +
 * delete + Add/Edit/Delete toolbar wrapping is identical.
 *
 * Each consumer differs in: column set, store-key, endpoint,
 * createBase, the EDITOR_LIST_BASE strings, the editList
 * factory, the entity-noun used in the delete confirm text,
 * and the Add tooltip. All of those land here as call options;
 * the resulting handle exposes the editor state and the
 * `buildActions(selection, clearSelection)` factory the view's
 * `toolbarActions` slot drives.
 */
import type { ComputedRef, Ref } from 'vue'
import type { BaseRow } from '@/types/grid'
import type { ActionDef } from '@/types/action'
import { useBulkAction } from './useBulkAction'
import { useEditorMode } from './useEditorMode'
import { buildAddEditDeleteActions } from '@/views/dvr/dvrToolbarHelpers'

export function useDvrRulesView(opts: {
  /** Create endpoint, e.g. 'dvr/autorec'. Drives the create drawer. */
  createBase: string
  /** Reactive admin-aware edit list (typically `adminAwareEditList(...)`). */
  editList: Ref<string> | ComputedRef<string>
  /** Field-list for the Create drawer (the bare base, no admin-only fields). */
  createList: string
  /** Plural noun used in the delete-confirm text — "autorec entries", "timer entries". */
  entityNoun: string
  /** Tooltip on the Add toolbar button. */
  addTooltip: string
}) {
  const editor = useEditorMode({
    createBase: opts.createBase,
    editList: opts.editList,
    createList: opts.createList,
  })

  const remove = useBulkAction({
    endpoint: 'idnode/delete',
    confirmText: `Do you really want to delete the selected ${opts.entityNoun}?`,
    confirmSeverity: 'danger',
    failPrefix: 'Failed to delete',
  })

  function buildActions(selection: BaseRow[], clearSelection: () => void): ActionDef[] {
    return buildAddEditDeleteActions({
      selection,
      clearSelection,
      remove,
      onAdd: editor.openCreate,
      onEdit: editor.openEditor,
      addTooltip: opts.addTooltip,
    })
  }

  return {
    ...editor,
    buildActions,
  }
}
