// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { nextTick, ref, type Ref } from 'vue'
import type { BaseRow } from '@/types/grid'
import { apiCall } from '@/api/client'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'
import {
  canMoveSelection,
  pickScrollUuid,
  sortUuidsForMove,
  type MoveDirection,
} from '@/views/configuration/idnodeMove'

/* Grid template ref shape — `store.entries` + `store.fetch()` for
 * the post-move refetch, and `scrollToUuid()` to keep the moved row
 * visible. Matches what `IdnodeGrid.vue` exposes via defineExpose. */
type GridHandle = {
  store?: { entries: BaseRow[]; fetch: () => Promise<void> }
  scrollToUuid?: (uuid: string, opts?: { behavior?: ScrollBehavior }) => boolean
} | null

/* Wires Move Up / Move Down for an idnode grid. Pure logic lives
 * in `idnodeMove.ts`; this composable does the apiCall + refetch +
 * scroll-into-view + error-toast wiring all idnode views share. */
export function useIdnodeMove(gridRef: Ref<unknown>) {
  const toast = useToastNotify()
  const { t } = useI18n()
  const moveInflight = ref(false)

  function getGrid(): GridHandle {
    return gridRef.value as GridHandle
  }

  async function moveSelected(direction: MoveDirection, selection: BaseRow[]): Promise<void> {
    if (selection.length === 0) return
    const grid = getGrid()
    const entries = grid?.store?.entries ?? []
    const uuids = sortUuidsForMove(selection, entries, direction)
    if (uuids.length === 0) return
    moveInflight.value = true
    try {
      await apiCall(`idnode/move${direction}`, { uuid: JSON.stringify(uuids) })
      await grid?.store?.fetch()
      await nextTick()
      const scrollUuid = pickScrollUuid(uuids)
      if (scrollUuid) grid?.scrollToUuid?.(scrollUuid)
    } catch (e) {
      toast.error(e instanceof Error ? e.message : String(e), {
        summary: t('Failed to move {0}', direction),
      })
    } finally {
      moveInflight.value = false
    }
  }

  function canMove(selection: BaseRow[], direction: MoveDirection): boolean {
    const grid = getGrid()
    return canMoveSelection(selection, grid?.store?.entries ?? [], direction)
  }

  return { moveInflight, moveSelected, canMove }
}
