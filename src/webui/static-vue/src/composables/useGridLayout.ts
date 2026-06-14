// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useGridLayout — shared grid-layout persistence for `<StatusGrid>`
 * and `<IdnodeGrid>`.
 *
 * Owns four axes of per-grid layout state, persisted to a single
 * localStorage key:
 *   - sort     ({ field, dir: 'ASC' | 'DESC' })
 *   - columns  (Record<field, { hidden?, width? }>)
 *   - order    (string[] of field names, user-chosen sequence)
 *
 * View-level filtering (PO_ADVANCED / PO_EXPERT) stays in IdnodeGrid
 * — Status rows have no level axis and the composable doesn't model
 * it. Filter persistence stays in IdnodeGrid too: Status has no
 * filters in scope, and IdnodeGrid keeps its existing filter
 * machinery untouched as a layer alongside this composable.
 *
 * Default-aware writes: when the user picks a sort matching the
 * caller's `defaultSort`, the persisted slot is dropped instead of
 * being written. Same for `order` matching the source column
 * sequence. Keeps the blob lean and lets a future change to a
 * default propagate to existing users without being silently
 * overridden by a stale "default match" persisted blob.
 *
 * Optional CSS width injector: when `gridKey` is supplied, the
 * composable installs a `<style>` element on mount that emits
 * `width: Xpx !important` rules keyed by
 * `[data-grid-key="<gridKey>"] th/td[data-field="<field>"]`.
 * Needed because PrimeVue's auto-layout doesn't honour inline
 * widths on remount. Skip the injector by leaving `gridKey`
 * undefined (e.g. for unit tests that mount a harness without
 * a DataGrid).
 */

import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import type { ColumnDef } from '@/types/column'
import { readStoredJson, removeStoredKey, writeStoredJson } from '@/utils/storage'

export interface LayoutBlob {
  sort?: { field: string; dir: 'ASC' | 'DESC' }
  cols?: Record<string, { hidden?: boolean; width?: number }>
  order?: string[]
  /* Active group-by field. Unset (= no grouping) is the implicit
   * default; presence persists across reloads alongside sort and
   * columns. Cleared by the Reset to defaults footer along with
   * the other axes. */
  groupField?: string
  /* Cluster sort direction. Tracked SEPARATELY from the regular
   * `sort` field so the user can sort rows WITHIN clusters by
   * column A while still flipping cluster ORDER via the group-
   * column header — clicking the group column updates this slot
   * rather than overwriting the secondary sort. Default ASC when
   * unset. Persisted across reloads. Cleared by Reset to
   * defaults. */
  groupOrder?: 'ASC' | 'DESC'
}

export interface UseGridLayoutOptions {
  /* localStorage key for the persisted blob. */
  storageKey: string
  /* Reactive getter for the column array. Re-evaluated by every
   * computed that depends on it; pass a getter so Vue tracks the
   * dependency. */
  columns: () => ColumnDef[]
  /* When the user picks a sort matching this, the persisted slot
   * is dropped. Also drives `sort.value` when nothing's persisted.
   * Optional — without it, the composable returns `null` for an
   * unset sort and writes every user pick. */
  defaultSort?: { field: string; dir: 'ASC' | 'DESC' }
  /* When set, install a `<style>` element that emits column-width
   * `!important` rules scoped to
   * `[data-grid-key="<gridKey>"] th/td[data-field="..."]`. Skip
   * by leaving undefined. */
  gridKey?: string
  /* Fallback width applied to columns that have no per-column
   * persisted width AND no `col.width` hint. Default 160. Used by
   * the CSS injector to keep PrimeVue's `table-layout: fixed`
   * mode from collapsing un-widthed columns to zero. */
  fallbackColWidthPx?: number
}

const DEFAULT_FALLBACK_COL_WIDTH_PX = 160

/* Shape guard for the persisted blob: any non-null object passes.
 * Rejects a stored literal "null" / scalar so `blob.value.sort`
 * style reads never land on a non-object. */
function isLayoutBlob(v: unknown): v is LayoutBlob {
  return typeof v === 'object' && v !== null && !Array.isArray(v)
}

function loadBlob(storageKey: string): LayoutBlob {
  /* Corrupt blob shouldn't brick the grid — silently reset. */
  return readStoredJson(storageKey, isLayoutBlob) ?? {}
}

function saveBlob(storageKey: string, blob: LayoutBlob): void {
  writeStoredJson(storageKey, blob)
}

function removeBlob(storageKey: string): void {
  removeStoredKey(storageKey)
}

export function useGridLayout(opts: UseGridLayoutOptions) {
  const blob = ref<LayoutBlob>(loadBlob(opts.storageKey))

  function persist(): void {
    saveBlob(opts.storageKey, blob.value)
  }

  /* ---- Sort ---- */

  const sort = computed<{ field: string; order: 1 | -1 } | null>(() => {
    const s = blob.value.sort
    if (s) return { field: s.field, order: s.dir === 'DESC' ? -1 : 1 }
    if (opts.defaultSort) {
      return {
        field: opts.defaultSort.field,
        order: opts.defaultSort.dir === 'DESC' ? -1 : 1,
      }
    }
    return null
  })

  function setSort(field: string, dir: 'ASC' | 'DESC'): void {
    const isDefault =
      opts.defaultSort?.field === field && opts.defaultSort?.dir === dir
    if (isDefault) {
      if (blob.value.sort !== undefined) {
        const next: LayoutBlob = { ...blob.value }
        delete next.sort
        blob.value = next
        persist()
      }
      return
    }
    blob.value = { ...blob.value, sort: { field, dir } }
    persist()
  }

  function clearSort(): void {
    if (blob.value.sort === undefined) return
    const next: LayoutBlob = { ...blob.value }
    delete next.sort
    blob.value = next
    persist()
  }

  /* ---- Column visibility ----
   *
   * `isHidden(col)` resolves the cascade:
   *   1. User pref (blob.cols.<field>.hidden) — wins when set.
   *   2. col.hiddenByDefault — used when (1) is unset.
   *   3. false — visible.
   *
   * Callers that also need to consult server-side `prop.hidden`
   * (IdnodeGrid) layer that check on top — the composable stays
   * idnode-agnostic. */
  function isHidden(col: ColumnDef): boolean {
    const pref = blob.value.cols?.[col.field]?.hidden
    if (pref !== undefined) return pref
    return col.hiddenByDefault ?? false
  }

  /* Low-level accessor returning the user's hidden preference (or
   * `undefined` if no preference is recorded). Wrappers that need
   * to layer extra state on top of the composable's cascade — e.g.
   * IdnodeGrid checks the idnode class's `prop.hidden` AFTER the
   * user pref, AFTER col.hiddenByDefault — read this to decide
   * when to fall through to their extra check. */
  function getHiddenPref(field: string): boolean | undefined {
    return blob.value.cols?.[field]?.hidden
  }

  /* Low-level accessor returning the user's persisted sort (or
   * `undefined` if none is recorded). Wrappers that re-feed the
   * sort to a store on mount need to distinguish "user picked
   * this sort" from "composable is falling back to defaultSort"
   * — only the former should be threaded into the store's
   * initial fetch params. */
  function getSortPref(): { field: string; dir: 'ASC' | 'DESC' } | undefined {
    return blob.value.sort
  }

  function setColumnHidden(field: string, hidden: boolean): void {
    const cols = blob.value.cols ?? {}
    const prev = cols[field] ?? {}
    blob.value = {
      ...blob.value,
      cols: { ...cols, [field]: { ...prev, hidden } },
    }
    persist()
  }

  /* ---- Column widths ---- */

  const columnWidths = computed<Map<string, number>>(() => {
    const out = new Map<string, number>()
    for (const [field, pref] of Object.entries(blob.value.cols ?? {})) {
      if (typeof pref?.width === 'number') out.set(field, pref.width)
    }
    return out
  })

  function isWidthCustom(field: string): boolean {
    return blob.value.cols?.[field]?.width !== undefined
  }

  function setColumnWidth(field: string, px: number): void {
    if (!Number.isFinite(px) || px <= 0) return
    const cols = blob.value.cols ?? {}
    const prev = cols[field] ?? {}
    blob.value = {
      ...blob.value,
      cols: { ...cols, [field]: { ...prev, width: px } },
    }
    persist()
  }

  function clearColumnWidth(field: string): void {
    const cols = blob.value.cols ?? {}
    const prev = cols[field]
    if (prev?.width === undefined) return
    const next = { ...prev }
    delete next.width
    let updatedCols: Record<string, { hidden?: boolean; width?: number }>
    if (Object.keys(next).length === 0) {
      const rest = { ...cols }
      delete rest[field]
      updatedCols = rest
    } else {
      updatedCols = { ...cols, [field]: next }
    }
    blob.value = { ...blob.value, cols: updatedCols }
    persist()
  }

  /* ---- Column order ----
   *
   * `orderedColumns` resolves `opts.columns()` through the user's
   * persisted `order` (a list of field names). Columns named in
   * `order` come first, in that sequence; columns not present in
   * `order` get appended in their source array order. Stale
   * entries in `order` (fields that no longer exist in the source
   * array) are silently dropped — keeps the grid robust against
   * schema evolution.
   *
   * When `order` is unset or empty, returns the source array
   * directly — no allocation, no re-render churn for grids that
   * never customise.
   */
  const orderedColumns = computed<ColumnDef[]>(() => {
    const source = opts.columns()
    const saved = blob.value.order
    if (!saved || saved.length === 0) return source
    const byField = new Map(source.map((c) => [c.field, c]))
    const out: ColumnDef[] = []
    for (const f of saved) {
      const c = byField.get(f)
      if (c) {
        out.push(c)
        byField.delete(f)
      }
    }
    for (const c of source) {
      if (byField.has(c.field)) out.push(c)
    }
    return out
  })

  function setColumnOrder(fields: string[]): void {
    const source = opts.columns().map((c) => c.field)
    const matchesSource =
      fields.length === source.length &&
      fields.every((f, i) => f === source[i])
    if (matchesSource) {
      if (blob.value.order !== undefined) {
        const next: LayoutBlob = { ...blob.value }
        delete next.order
        blob.value = next
        persist()
      }
      return
    }
    blob.value = { ...blob.value, order: [...fields] }
    persist()
  }

  /* Move-column-by-arrow handler — swaps a field with its
   * adjacent sibling among `excluding`-filtered fields (mirrors
   * GridSettingsMenu's `visibleColumns` semantics: master-detail
   * layouts exclude the synthetic `uuid` column from the menu's
   * reorder rows, so the swap must respect that filter to keep
   * the swap targets matching what the user sees).
   *
   * `excluding` defaults to `['uuid']` — the convention every
   * existing grid uses. Pass `[]` to disable the exclusion. */
  function moveColumn(
    field: string,
    dir: 'up' | 'down',
    excluding: string[] = ['uuid']
  ): void {
    const excluded = new Set(excluding)
    const fullFields = orderedColumns.value.map((c) => c.field)
    const visibleFields = fullFields.filter((f) => !excluded.has(f))
    const idx = visibleFields.indexOf(field)
    if (idx < 0) return
    const target = dir === 'up' ? idx - 1 : idx + 1
    if (target < 0 || target >= visibleFields.length) return
    const swapped = [...visibleFields]
    ;[swapped[idx], swapped[target]] = [swapped[target], swapped[idx]]
    let vIdx = 0
    const newFull: string[] = []
    for (const f of fullFields) {
      if (excluded.has(f)) {
        newFull.push(f)
      } else {
        newFull.push(swapped[vIdx])
        vIdx++
      }
    }
    setColumnOrder(newFull)
  }

  /* ---- Group-by field + cluster sort direction ---- */

  const groupField = computed<string | null>(
    () => blob.value.groupField ?? null,
  )

  function setGroupField(field: string | null): void {
    if (field === null) {
      if (blob.value.groupField === undefined) return
      const next: LayoutBlob = { ...blob.value }
      delete next.groupField
      blob.value = next
      persist()
      return
    }
    blob.value = { ...blob.value, groupField: field }
    persist()
  }

  const groupOrder = computed<'ASC' | 'DESC'>(
    () => blob.value.groupOrder ?? 'ASC',
  )

  function setGroupOrder(dir: 'ASC' | 'DESC'): void {
    if (dir === 'ASC') {
      if (blob.value.groupOrder === undefined) return
      const next: LayoutBlob = { ...blob.value }
      delete next.groupOrder
      blob.value = next
      persist()
      return
    }
    blob.value = { ...blob.value, groupOrder: dir }
    persist()
  }

  /* ---- isAtDefaults ----
   *
   * True when nothing's persisted — every axis matches the
   * caller's defaults. Drives the GridSettingsMenu's footer
   * "Reset to defaults" disabled state.
   */
  const isAtDefaults = computed(
    () =>
      blob.value.sort === undefined &&
      blob.value.order === undefined &&
      blob.value.groupField === undefined &&
      /* `groupOrder` is INTENTIONALLY not checked here. Cluster
       * direction has no visible effect when grouping is off, so
       * flagging a stale DESC value as non-default would light up
       * the Reset link for state the user can't see. With
       * `groupField === undefined` already required above,
       * checking groupOrder here would be redundant for the
       * grouping-off case AND would be unreachable for the
       * grouping-on case (which already fails the previous
       * clause). The groupOrder setting is still persisted and
       * restored when grouping turns back on later. */
      Object.keys(blob.value.cols ?? {}).length === 0
  )

  function reset(): void {
    blob.value = {}
    removeBlob(opts.storageKey)
  }

  /* ---- Width CSS injection (optional) ----
   *
   * When `gridKey` is supplied, install a `<style>` element on
   * mount. Each width pref emits a CSS rule that targets the
   * DataGrid's `[data-grid-key]` root with the field's data
   * attribute. Re-runs whenever the blob mutates. The fallback
   * rule (FALLBACK_COL_WIDTH_PX) covers columns with no width
   * pref AND no `col.width` hint — keeps PrimeVue's
   * `table-layout: fixed` from collapsing them to zero. */
  const fallbackPx = opts.fallbackColWidthPx ?? DEFAULT_FALLBACK_COL_WIDTH_PX
  let styleEl: HTMLStyleElement | null = null

  function buildWidthCss(): string {
    if (!opts.gridKey) return ''
    const widthsByField = new Map<string, number>()
    const source = opts.columns()
    for (const col of source) {
      if (typeof col.width === 'number') widthsByField.set(col.field, col.width)
    }
    for (const [field, w] of columnWidths.value) {
      widthsByField.set(field, w)
    }
    const rules: string[] = []
    for (const [field, w] of widthsByField) {
      rules.push(
        `[data-grid-key="${opts.gridKey}"] th[data-field="${field}"],` +
          `[data-grid-key="${opts.gridKey}"] td[data-field="${field}"] { ` +
          `width: ${w}px !important; max-width: ${w}px !important; }`
      )
    }
    for (const col of source) {
      if (widthsByField.has(col.field)) continue
      rules.push(
        `[data-grid-key="${opts.gridKey}"] th[data-field="${col.field}"],` +
          `[data-grid-key="${opts.gridKey}"] td[data-field="${col.field}"] { ` +
          `width: ${fallbackPx}px !important; ` +
          `max-width: ${fallbackPx}px !important; }`
      )
    }
    return rules.join('\n')
  }

  function applyWidthCss(): void {
    if (!opts.gridKey || !styleEl) return
    styleEl.textContent = buildWidthCss()
  }

  if (opts.gridKey !== undefined) {
    onMounted(() => {
      if (typeof document === 'undefined') return
      styleEl = document.createElement('style')
      styleEl.dataset.tvhGridLayout = opts.gridKey
      document.head.appendChild(styleEl)
      applyWidthCss()
    })

    onBeforeUnmount(() => {
      if (styleEl) {
        styleEl.remove()
        styleEl = null
      }
    })

    /* React to blob mutations (column width changes, resets, etc.)
     * and to columns prop changes (e.g. a fresh column added to the
     * source array). Both should re-emit the style rules. */
    watch(blob, applyWidthCss, { deep: true })
  }

  return {
    /* Reactive state */
    sort,
    columnWidths,
    orderedColumns,
    groupField,
    groupOrder,
    isAtDefaults,
    /* Predicates */
    isHidden,
    isWidthCustom,
    getHiddenPref,
    getSortPref,
    /* Mutations */
    setSort,
    clearSort,
    setColumnHidden,
    setColumnWidth,
    clearColumnWidth,
    setColumnOrder,
    moveColumn,
    setGroupField,
    setGroupOrder,
    reset,
  }
}

export type UseGridLayoutReturn = ReturnType<typeof useGridLayout>
