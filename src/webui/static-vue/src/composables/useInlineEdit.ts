// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useInlineEdit — per-row, per-cell dirty store + batch save for
 * the admin grids that opt into inline cell editing
 * (`editMode: 'cell'` on IdnodeGrid).
 *
 * Architectural note: PrimeVue's `<DataTable editMode="cell">`
 * tracks its own internal `editingMeta`, but it CLEARS that
 * structure on every sort / filter / pagination change
 * (`primevue/datatable/DataTable.vue:524, 595, 693`). Classic
 * ExtJS's `EditorGridPanel`, by contrast, keeps dirty values
 * across sort changes. Mirroring Classic — and giving us batch
 * Save / Undo semantics for free — means we keep our own dirty
 * store ABOVE PrimeVue's edit state. We listen for
 * `cell-edit-complete` purely as a signal to copy the new value
 * into our `dirtyMap`; cell rendering reads back from our map,
 * so display values survive any PrimeVue-internal re-render.
 *
 * Save shape mirrors Classic exactly:
 *   POST idnode/save  node=[{uuid, field1:v1}, {uuid, field2:v2}, …]
 *   - per-row partial objects, only changed fields
 *   - server's `api_idnode_save` (`api_idnode.c:410-429`,
 *     ADR 0004's "Foreach" path) iterates the array and applies
 *     each row's diff individually
 *
 * Lifecycle:
 *   - `commitCell(uuid, field, value)` — called from a cell's
 *     editor on `cell-edit-complete`. No server round-trip.
 *   - `revertAll()` — discards every dirty cell. Called from
 *     the toolbar's Undo button.
 *   - `save()` — POSTs the diff. On success, clears the dirty
 *     store; the comet listener for the entity class will
 *     refresh the grid. On failure, dirty store is preserved
 *     so the user can retry without re-typing.
 */
import { computed, ref, type ComputedRef, type Ref } from 'vue'
import { apiCall } from '@/api/client'
import { useErrorDialog } from '@/composables/useErrorDialog'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { t } from '@/composables/useI18n'
import type { BaseRow } from '@/types/grid'

export interface UseInlineEditOptions<Row extends BaseRow> {
  /** Reactive view of the current grid rows. Used at save time
   *  to filter out dirty entries whose uuid no longer exists
   *  (e.g. deleted by another session while edits were pending). */
  entries: Readonly<Ref<readonly Row[]>>
  /** Endpoint for the batch save POST. Defaults to
   *  `'idnode/save'`. Override only if the consumer talks to a
   *  custom save endpoint. */
  saveEndpoint?: string
  /** Optional per-cell edit gate. Returns:
   *    - `true` to allow the edit,
   *    - `false` to block silently,
   *    - a string to block AND surface as a tooltip / toast.
   *  Used by DVR Upcoming to block edits on rows whose
   *  `sched_status === 'recording'`. */
  beforeEdit?: (row: Row, field: string) => boolean | string
}

export interface UseInlineEditApi<Row extends BaseRow> {
  /** Reactive ref into the dirty store. Outer-map keyed by
   *  uuid, inner-map keyed by field id. */
  dirtyMap: Ref<Map<string, Map<string, unknown>>>
  /** True when at least one row has at least one dirty cell. */
  hasDirty: ComputedRef<boolean>
  /** Number of rows with any dirty cell. Useful for toolbar
   *  copy ("3 rows have unsaved changes"). */
  dirtyRowCount: ComputedRef<number>
  /** Whether a save round-trip is currently in flight. */
  inflight: Ref<boolean>
  /** Returns true if `(uuid, field)` is in the dirty store. */
  isCellDirty: (uuid: string, field: string) => boolean
  /** Returns the dirty value for the cell, or the supplied
   *  fallback if not dirty. The cell renderer passes the
   *  row's original value as the fallback. */
  cellValue: (uuid: string, field: string, fallback: unknown) => unknown
  /** Returns true when the cell is dirty AND the row's underlying
   *  server value has changed since the user first dirtied this
   *  cell (i.e., a comet refresh delivered a new value while the
   *  user has unsaved local edits — a conflict). The caller passes
   *  the row's CURRENT value (post-refresh); we compare against
   *  the captured baseline. Used by IdnodeGrid to render the
   *  warning-orange pulsing marker on conflict cells. */
  isCellServerPending: (uuid: string, field: string, currentRowValue: unknown) => boolean
  /** Whether `(row, field)` is currently allowed to enter edit
   *  mode. `true` if no `beforeEdit` predicate is configured;
   *  otherwise delegates to it. */
  canEdit: (row: Row, field: string) => boolean | string
  /** Records a new dirty value for `(uuid, field)`. Pass
   *  `{ skipSmartClear: true }` from per-keystroke commits so
   *  a transient match against the server value mid-edit
   *  doesn't clear the dirty marker (e.g. backspacing 200 →
   *  20 → 2 on the way to 210 would otherwise clear-then-re-
   *  dirty on every step). Callers using this option should
   *  call `evaluateSmartClear` once on cell-edit-complete. */
  commitCell: (
    uuid: string,
    field: string,
    value: unknown,
    options?: { skipSmartClear?: boolean },
  ) => void
  /** Manual smart-clear pass for callers that committed with
   *  `skipSmartClear`. If the cell's current dirty value
   *  matches the server value, drops the dirty + baseline
   *  entry. No-op otherwise. */
  evaluateSmartClear: (uuid: string, field: string) => void
  /** Discards every dirty cell. */
  revertAll: () => void
  /** Sends the batch save POST. Resolves on success (dirty
   *  store cleared) or rejects on failure (dirty store
   *  preserved). No-op if `hasDirty` is false. */
  save: () => Promise<void>
}

/* Compare a candidate cell value against the row's original
 * (server) value. Coerces bool 0/1 ↔ true/false and null /
 * undefined / '' to a single equivalence class — both sides
 * of those pairs land in the wire format inconsistently
 * across idnode classes (PT_BOOL emits 0/1, but the cell
 * editor commits booleans), and treating them as different
 * would leave a "ghost dirty" cell when the user toggles
 * back to the visible original. Plain strings / numbers
 * compare with `===`. */
function valuesMatch(a: unknown, b: unknown): boolean {
  if (typeof a === 'boolean' || typeof b === 'boolean') {
    return Boolean(a) === Boolean(b)
  }
  const aEmpty = a === null || a === undefined || a === ''
  const bEmpty = b === null || b === undefined || b === ''
  if (aEmpty && bEmpty) return true
  /* Array fields (islist columns such as `tags`) emit fresh
   * array references on every dirty mutation — chip add/remove
   * builds a new array each click. Reference equality would
   * never match the original, so add-then-remove of the same
   * tag would keep the row marked dirty even though the
   * resulting set is semantically identical.
   *
   * Compare as SETS (sorted contents): islist values in
   * tvheadend's idnode model are unordered membership lists,
   * not ordered sequences. The server-emitted order varies
   * by channel (whichever order the tags were attached in),
   * so insisting on positional equality would falsely mark
   * "{HD, News}" dirty just because the user removed and
   * re-added a tag — same set, different position. If a
   * future field needs order-significant comparison, add a
   * per-field opt-out rather than reverting this. */
  if (Array.isArray(a) && Array.isArray(b)) {
    if (a.length !== b.length) return false
    const cmp = (x: string, y: string) => x.localeCompare(y)
    const sortedA = [...a].map(String).sort(cmp)
    const sortedB = [...b].map(String).sort(cmp)
    for (let i = 0; i < sortedA.length; i++) {
      if (sortedA[i] !== sortedB[i]) return false
    }
    return true
  }
  /* Number ↔ numeric-string equivalence: 2 ≡ "2", 5.1 ≡ "5.1".
   * Some editors emit strings for numeric props (IdnodeFieldIntSplit
   * for channel-number's major.minor wire shape — `"5."` and
   * trailing dots can't round-trip through Number) while the
   * server-stored row holds an actual number. Without coercion,
   * typing "2 → 200 → 2" in a numeric cell leaves dirty stuck
   * at "2" because "2" !== 2, so the cell stays flagged even
   * though the user is back at the server value. */
  if (
    (typeof a === 'number' && typeof b === 'string') ||
    (typeof a === 'string' && typeof b === 'number')
  ) {
    const aNum = Number(a)
    const bNum = Number(b)
    if (Number.isFinite(aNum) && Number.isFinite(bNum) && aNum === bNum) {
      return true
    }
  }
  return a === b
}

export function useInlineEdit<Row extends BaseRow>(
  opts: UseInlineEditOptions<Row>,
): UseInlineEditApi<Row> {
  const dirtyMap = ref<Map<string, Map<string, unknown>>>(new Map())
  /* `baselineMap` captures the row's server value at the moment the
   * user FIRST dirtied a cell. It's the reference point for the
   * "did the server change this field after I started editing?"
   * comparison — if the row's current (post-refresh) value differs
   * from this captured baseline, we have a conflict and the cell
   * gets the warning marker. Same shape as dirtyMap; entries are
   * created/cleared in lockstep with the dirty store. */
  const baselineMap = ref<Map<string, Map<string, unknown>>>(new Map())
  const inflight = ref(false)

  const hasDirty = computed(() => {
    for (const fieldMap of dirtyMap.value.values()) {
      if (fieldMap.size > 0) return true
    }
    return false
  })

  const dirtyRowCount = computed(() => {
    let count = 0
    for (const fieldMap of dirtyMap.value.values()) {
      if (fieldMap.size > 0) count++
    }
    return count
  })

  function isCellDirty(uuid: string, field: string): boolean {
    return dirtyMap.value.get(uuid)?.has(field) ?? false
  }

  function cellValue(uuid: string, field: string, fallback: unknown): unknown {
    const m = dirtyMap.value.get(uuid)
    if (m?.has(field)) return m.get(field)
    return fallback
  }

  function canEdit(row: Row, field: string): boolean | string {
    if (!opts.beforeEdit) return true
    return opts.beforeEdit(row, field)
  }

  function clearBaseline(uuid: string, field: string): void {
    const bm = baselineMap.value.get(uuid)
    if (!bm?.has(field)) return
    bm.delete(field)
    if (bm.size === 0) baselineMap.value.delete(uuid)
    baselineMap.value = new Map(baselineMap.value)
  }

  function commitCell(
    uuid: string,
    field: string,
    value: unknown,
    options: { skipSmartClear?: boolean } = {},
  ): void {
    /* Smart dirty: if the new value equals the row's original
     * (server) value, the cell isn't actually dirty — drop it
     * from the map. Mirrors VS Code's modified-indicator
     * pattern (Ctrl+Z back to save point clears the dot) and
     * avoids the user having to click Undo to clear a
     * touched-then-restored cell.
     *
     * `skipSmartClear` defers that check. Callers that fire
     * per-keystroke (cell editor's @input → commit) should set
     * it so a transient match mid-typing — e.g. backspacing
     * 200 down to 2 while heading to 210 — doesn't drop the
     * dirty mid-edit. The caller then re-evaluates smart-clear
     * once at cell-edit-complete via `evaluateSmartClear`
     * below. */
    const row = opts.entries.value.find(
      (r) => (r as { uuid?: string }).uuid === uuid,
    )
    const original = row
      ? (row as Record<string, unknown>)[field]
      : undefined

    if (!options.skipSmartClear && valuesMatch(value, original)) {
      const m = dirtyMap.value.get(uuid)
      if (!m?.has(field)) return
      m.delete(field)
      if (m.size === 0) dirtyMap.value.delete(uuid)
      dirtyMap.value = new Map(dirtyMap.value)
      /* Cell reverted to server value — drop the conflict
       * baseline too. Without this, a subsequent dirty + clear
       * cycle would keep showing the conflict marker against
       * a stale baseline. */
      clearBaseline(uuid, field)
      return
    }

    let m = dirtyMap.value.get(uuid)
    if (!m) {
      m = new Map()
      dirtyMap.value.set(uuid, m)
    }
    /* Capture the baseline at FIRST dirty — the row's value at
     * the moment the user started editing this cell. Subsequent
     * keystrokes (cell already dirty) keep the original
     * baseline; only the dirty value updates. This is the
     * reference point for the "server changed under me" check
     * in `isCellServerPending`. */
    if (!m.has(field)) {
      let bm = baselineMap.value.get(uuid)
      if (!bm) {
        bm = new Map()
        baselineMap.value.set(uuid, bm)
      }
      bm.set(field, original)
      baselineMap.value = new Map(baselineMap.value)
    }
    m.set(field, value)
    /* Reassign the outer ref so Vue's reactivity picks the
     * change up — Map mutations don't trigger reactivity on a
     * `ref<Map>` by default. The inner Map is preserved by
     * reference; only the outer ref is replaced. */
    dirtyMap.value = new Map(dirtyMap.value)
  }

  function isCellServerPending(
    uuid: string,
    field: string,
    currentRowValue: unknown,
  ): boolean {
    if (!isCellDirty(uuid, field)) return false
    const bm = baselineMap.value.get(uuid)
    if (!bm?.has(field)) return false
    return !valuesMatch(currentRowValue, bm.get(field))
  }

  function revertAll(): void {
    dirtyMap.value = new Map()
    baselineMap.value = new Map()
  }

  /* Re-evaluates smart-clear for one (uuid, field). Used by
   * callers that committed with `skipSmartClear` per-keystroke
   * to defer the original-equality check until the user
   * actually finishes editing (cell-edit-complete). If the
   * cell's current dirty value matches the row's server value,
   * drop the dirty + baseline entry. No-op when there's no
   * dirty entry for that cell. */
  function evaluateSmartClear(uuid: string, field: string): void {
    const m = dirtyMap.value.get(uuid)
    if (!m?.has(field)) return
    const row = opts.entries.value.find(
      (r) => (r as { uuid?: string }).uuid === uuid,
    )
    const original = row
      ? (row as Record<string, unknown>)[field]
      : undefined
    if (!valuesMatch(m.get(field), original)) return
    m.delete(field)
    if (m.size === 0) dirtyMap.value.delete(uuid)
    dirtyMap.value = new Map(dirtyMap.value)
    clearBaseline(uuid, field)
  }

  async function save(): Promise<void> {
    if (!hasDirty.value || inflight.value) return
    /* Filter out dirty entries whose uuid is no longer present
     * in the current entries (deleted by another session,
     * filter dropped them, etc.). Sending a stale uuid would
     * either be a no-op server-side or surface as a
     * misleading error. */
    const liveUuids = new Set(opts.entries.value.map((r) => r.uuid))
    const out: Array<Record<string, unknown>> = []
    for (const [uuid, fieldMap] of dirtyMap.value) {
      if (fieldMap.size === 0) continue
      if (!liveUuids.has(uuid)) continue
      const row: Record<string, unknown> = { uuid }
      for (const [field, value] of fieldMap) {
        row[field] = value
      }
      out.push(row)
    }
    if (out.length === 0) {
      dirtyMap.value = new Map()
      return
    }
    inflight.value = true
    try {
      await apiCall(opts.saveEndpoint ?? 'idnode/save', {
        node: JSON.stringify(out),
      })
      /* Server accepted the batch. Clear the dirty store + the
       * conflict baseline; the comet listener for the entity
       * class will refresh the grid's underlying entries via
       * the existing channel-subscription path. */
      dirtyMap.value = new Map()
      baselineMap.value = new Map()
    } catch (e) {
      /* Surface the failure via the global error dialog and
       * swallow — the dirty state stays intact so the user can
       * correct the offending field and retry. Re-throwing
       * here would force every caller (most of which don't
       * have a catch today) to handle the rejection, defeating
       * the point of a singleton error surface. */
      useErrorDialog().show({
        title: t('Edit save failed'),
        message: apiErrorMessage(e),
      })
    } finally {
      inflight.value = false
    }
  }

  return {
    dirtyMap,
    hasDirty,
    dirtyRowCount,
    inflight,
    isCellDirty,
    cellValue,
    isCellServerPending,
    canEdit,
    commitCell,
    evaluateSmartClear,
    revertAll,
    save,
  }
}
