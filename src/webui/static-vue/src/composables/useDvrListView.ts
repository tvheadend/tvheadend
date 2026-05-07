/*
 * useDvrListView — shared script-setup glue for the DVR
 * entry-list views (Finished / Failed / Removed; Upcoming uses
 * the same shape but its create-aware editor lives outside this
 * composable).
 *
 * Each consumer view differs only in column set + action handles
 * + endpoint + editList. The setup work everyone shares — kodi
 * text formatter wired to the access flag, editor-drawer state
 * via useEditorMode — collapses to one call.
 *
 * Returns the editor state spread directly so consumers reach
 * `editingUuid`, `openEditor`, etc. without an extra `.editor`
 * level. `kodiFmt` is the per-cell formatter the column array
 * passes to text columns. The view declares its own access store
 * binding (separate `useAccessStore()` call) — needed for the
 * admin-aware editList computed AND avoids a destructure-order
 * trap, since the editList factory often closes over `access`
 * and that closure runs synchronously inside this composable.
 */
import type { ComputedRef, Ref } from 'vue'
import { useAccessStore } from '@/stores/access'
import { useEditorMode } from './useEditorMode'
import { makeKodiPlainFmt } from '@/views/epg/kodiText'

export function useDvrListView(opts: {
  editList: Ref<string> | ComputedRef<string>
  urlSync?: boolean
}) {
  const access = useAccessStore()
  const kodiFmt = makeKodiPlainFmt(() => !!access.data?.label_formatting)
  const editor = useEditorMode({
    editList: opts.editList,
    urlSync: opts.urlSync ?? false,
  })
  return {
    kodiFmt,
    ...editor,
  }
}
