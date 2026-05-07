/*
 * ColumnDef — caller-supplied per-column configuration for IdnodeGrid.
 *
 * `minVisible` drives the responsive behavior:
 *   'desktop' — only visible on desktop (>=1024px) layouts.
 *   'tablet'  — visible on tablet (768-1023px) and desktop.
 *   'phone'   — visible everywhere; also picked up by the default
 *               phone-card layout (cards show only phone-marked columns).
 *
 * The phone-card layout iterates phone-marked columns as Label: Value
 * pairs. For more elaborate phone layouts, callers provide a #phoneCard
 * slot which receives the full row.
 */

import type { Component } from 'vue'
import type { BaseRow } from './grid'
import type { EnumSource } from '@/components/idnode-fields/deferredEnum'

export type Breakpoint = 'desktop' | 'tablet' | 'phone'

/*
 * ColumnDef is non-generic to match IdnodeGrid's non-generic shape.
 * `format` receives the raw cell value (`unknown`) and the BaseRow;
 * callers can narrow inside the formatter if they need typed access.
 */
export interface ColumnDef {
  /* Property key on the row (matches the server-emitted field name). */
  field: string
  /*
   * Optional explicit label override. The grid resolves the displayed
   * header in this order:
   *   1. server-localized caption from the idnode-class metadata
   *      (`prop.caption` for the matching `field`),
   *   2. this `label` if set (synthetic/computed columns or deliberate
   *      overrides),
   *   3. the field name itself as a last-resort fallback.
   * Most callers leave this unset so server localization drives the
   * column header, the search aria-label, the phone sort dropdown,
   * and the filter input placeholders without extra plumbing.
   */
  label?: string
  /* Whether the user can sort by this column. */
  sortable?: boolean
  /* If set, enables column-header filter input of this type. */
  filterType?: 'string' | 'numeric' | 'boolean'
  /* Optional formatter for the cell value (e.g. timestamps -> dates). */
  format?: (value: unknown, row: BaseRow) => string
  /*
   * Optional Vue component for rendering the cell. Receives `value`,
   * `row`, and `col` as props. When set, takes precedence over both
   * `format` and the grid's default text rendering. Use for cells
   * that need a non-text visual (icons, pills, progress bars).
   */
  cellComponent?: Component
  /* Whether the column starts hidden (user can re-enable from the column-toggle menu). */
  hiddenByDefault?: boolean
  /* Initial width in pixels; user can resize and the value is persisted. */
  width?: number
  /* Lowest breakpoint at which the column is visible. Defaults to 'tablet'. */
  minVisible?: Breakpoint
  /*
   * Optional enum descriptor for cells that hold a key from an enum.
   * Either:
   *   - a deferred reference (`{ type: 'api', uri, params? }`) that
   *     `fetchDeferredEnum` resolves once per session, or
   *   - an inline static `Option[]` list for small fixed enums.
   * Used by `EnumNameCell` (and any future cell renderer that needs
   * to map keys → labels) to render the resolved label in place of
   * the raw key.
   */
  enumSource?: EnumSource
}
