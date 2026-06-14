// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useEntityEditor — singleton "open arbitrary entity editor" handle.
 *
 * Backs a second AppShell-mounted `<IdnodeEditor>` instance that
 * overlays whatever view is currently shown. Driven by drill-down
 * chevrons on grid cells: click → open that entity's editor in a
 * drawer, user stays on the source page. Entity class is discovered
 * server-side from the UUID via `api/idnode/load`, so the caller
 * only needs to pass the UUID.
 *
 * Picker mode (`openList`) — opens the same drawer with a compact
 * single-select table on top listing N entities; `editingUuid` is
 * the selected row (the one the editor below shows). The Home
 * dashboard's "This week's recordings" strip uses it so a day-cell
 * click lets the user step through that day's recordings one at a
 * time. A one-entry list skips the table and opens that editor
 * directly.
 *
 * Distinct from `useDvrEditor` (which is the EPG-specific
 * singleton with URL sync on /epg/* routes). This composable
 * deliberately has no URL sync — the source surfaces (EPG Table,
 * DVR list views) aren't bookmarkable on a per-cell-drilldown
 * basis, and an extra query param on those routes would conflict
 * with the URL-sync each view already does for its own primary
 * editor state.
 *
 * Module-level singleton state, same pattern as useDvrEditor.
 * Replace-not-stack: opening a new entity while one is already
 * open replaces it; drawer-stack mechanics are deliberately out
 * of scope.
 */
import { computed, effectScope, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import type { PickerColumn, PickerRow } from '@/types/picker'

const editingUuid = ref<string | null>(null)
/* Picker mode — non-null when the drawer shows the entity-picker
 * table above the editor. `editingUuid` still names the selected
 * row. `pickerTitle` is the drawer title in picker mode — it
 * describes the SET (e.g. "Recordings on Mon"), not the selected
 * row. All null = plain single-entity drill-down. */
const pickerRows = ref<PickerRow[] | null>(null)
const pickerColumns = ref<PickerColumn[] | null>(null)
const pickerTitle = ref<string | null>(null)

let wired = false

function clearPicker() {
  pickerRows.value = null
  pickerColumns.value = null
  pickerTitle.value = null
}

function wireWatchers() {
  if (wired) return
  wired = true
  /* useRoute() needs the calling component's injection context, but
   * the route object it returns is app-level reactive and outlives
   * that component — safe to capture here and watch from a detached
   * scope. */
  const route = useRoute()

  /* Detached effectScope: the first caller mounts inside AppShell,
   * which App.vue v-ifs away on wizard routes. A watcher created in
   * that component's own scope would be disposed with it while
   * `wired` stayed true, leaving the remounted shell with a dead
   * watcher. The detached scope is never disposed, so the watcher
   * genuinely lives for the app's lifetime. */
  effectScope(true).run(() => {
    /* Route-change auto-close. The drill-down drawer is a modal
     * overlay over the source page; if the user navigates away
     * (back button, programmatic nav from inside the editor's save
     * handler, etc.), drop it so it doesn't follow the user to an
     * unrelated route. Mirrors useDvrEditor's behaviour. */
    watch(
      () => route.path,
      (newPath, oldPath) => {
        if (newPath === oldPath) return
        if (editingUuid.value !== null) {
          editingUuid.value = null
          clearPicker()
        }
      },
    )
  })
}

export function useEntityEditor() {
  wireWatchers()

  const isOpen = computed(() => editingUuid.value !== null)

  /* Open a single entity's editor — plain drill-down, no table. */
  function open(uuid: string) {
    clearPicker()
    editingUuid.value = uuid
  }

  /* Open a list of entities in picker mode. A single-entry list
   * skips the table and opens that editor directly; 2+ entries
   * show the picker table with the first row pre-selected.
   * `title` is the drawer title describing the set (omit for the
   * single-entry case — that drawer titles itself off the entry). */
  function openList(rows: PickerRow[], columns: PickerColumn[], title?: string) {
    if (rows.length === 0) return
    if (rows.length === 1) {
      open(rows[0].uuid)
      return
    }
    pickerRows.value = rows
    pickerColumns.value = columns
    pickerTitle.value = title ?? null
    editingUuid.value = rows[0].uuid
  }

  function close() {
    editingUuid.value = null
    clearPicker()
  }

  return {
    editingUuid,
    isOpen,
    pickerRows,
    pickerColumns,
    pickerTitle,
    open,
    openList,
    close,
  }
}
