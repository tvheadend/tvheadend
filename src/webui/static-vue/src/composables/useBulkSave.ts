// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useBulkSave — set the same fields on every selected row with one
 * request.
 *
 * The sibling of useBulkAction for toolbar actions that change a
 * property instead of calling a bulk endpoint (Rescan on Muxes,
 * Enable / Disable on Services and Muxes). It posts `idnode/save`
 * with `node={uuid: [...], <fields>}`, the "Foreach" shape that
 * `api_idnode_save` (`src/api/api_idnode.c`) applies to every uuid
 * in the list. Same handle as useBulkAction, so a view treats both
 * alike: an in-flight flag for the button and a `run(selection,
 * clear)` that clears the selection on success.
 *
 * The change itself is only visible once Comet updates the rows,
 * so a success toast confirms it right away.
 */
import { ref } from 'vue'
import { apiCall } from '@/api/client'
import { useToastNotify } from '@/composables/useToastNotify'
import type { BulkActionHandle } from '@/composables/useBulkAction'
import type { BaseRow } from '@/types/grid'

export interface BulkSaveConfig {
  /** Fields set on every row, e.g. `{ enabled: false }`. */
  fields: Record<string, unknown>
  /** Prefix for the failure toast, the error message follows after `: `. */
  failPrefix: string
  /** Success toast text for the number of rows the request was sent for. */
  successText: (count: number) => string
}

export function useBulkSave(config: BulkSaveConfig): BulkActionHandle {
  const inflight = ref(false)
  const toast = useToastNotify()

  async function run(selected: BaseRow[], clear: () => void) {
    const uuid = selected.map((r) => r.uuid).filter((u): u is string => !!u)
    if (uuid.length === 0) return
    inflight.value = true
    try {
      await apiCall('idnode/save', { node: JSON.stringify({ uuid, ...config.fields }) })
      clear()
      toast.success(config.successText(uuid.length))
    } catch (err) {
      toast.error(`${config.failPrefix}: ${err instanceof Error ? err.message : String(err)}`)
    } finally {
      inflight.value = false
    }
  }

  return { inflight, run }
}
