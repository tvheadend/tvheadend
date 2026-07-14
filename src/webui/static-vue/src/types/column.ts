// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * ColumnDef — caller-supplied per-column configuration for IdnodeGrid.
 *
 * `minVisible` drives the responsive behavior. Two modes:
 *   'desktop' — visible at non-phone widths (>=768px). Renders in
 *               the table layout. Defaults to this when unset.
 *   'phone'   — visible everywhere; also picked up by the
 *               phone-card layout (cards show only phone-marked
 *               columns).
 *
 * The phone-card layout iterates phone-marked columns as Label: Value
 * pairs. For more elaborate phone layouts, callers provide a #phoneCard
 * slot which receives the full row.
 *
 * Historical note: a third 'tablet' breakpoint existed at 768-1023px
 * with a media-query rule that force-hid desktop columns there.
 * That hid columns the user had explicitly toggled visible in the
 * settings popover — paternalistic. We collapsed to phone/desktop:
 * at non-phone widths the user gets every column they enabled and
 * scrolls horizontally if the total exceeds the viewport (which is
 * how desktop already behaves when narrowed). Tag values from
 * before the collapse migrated to 'desktop' since the runtime
 * effect (visible on non-phone) is the same.
 */

import type { Component } from 'vue'
import type { BaseRow } from './grid'
import type { EnumSource, Option } from '@/components/idnode-fields/deferredEnum'
import type { PermissionKey } from './access'

export type Breakpoint = 'desktop' | 'phone'

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
  /*
   * Suppress the label text in the grid's column header (the
   * `<th>` body) while preserving it everywhere else: the
   * column-picker menu, the screen-reader name, and the
   * hover-to-read `title` attribute on the `<th>` itself. Use
   * for icon-only action columns (e.g. the per-row Play icon)
   * where the cell content is self-explanatory and a textual
   * header would just add visual noise. Default false: header
   * renders the label as usual.
   */
  hideHeaderLabel?: boolean
  /* Whether the user can sort by this column. */
  sortable?: boolean
  /* If set, enables column-header filter input of this type.
   *   - 'string'  — plain text input, server matches via
   *     `idnode_filter_add_str` (regex against the rendered
   *     display value).
   *   - 'numeric' — operator + value (or Between range) UI via
   *     `NumericFilterControls`.
   *   - 'boolean' — Any / Yes / No select.
   *   - 'enum'    — single-select dropdown of the column's
   *     `enumSource` options via `EnumFilterControl`. The
   *     column MUST carry `enumSource`. Wire shape is a
   *     `{type: 'numeric', comparison: 'eq', value: <key>}`
   *     entry — exact int match against the raw enum key, so
   *     the server bypasses label-resolution entirely (clean,
   *     locale-stable). Multi-select awaits an upstream PR that
   *     grows the generic idnode filter machinery into OR-compose
   *     semantics on the same field. */
  filterType?: 'string' | 'numeric' | 'boolean' | 'enum'
  /* Optional formatter for the cell value (e.g. timestamps -> dates). */
  format?: (value: unknown, row: BaseRow) => string
  /*
   * Optional Vue component for rendering the cell. Receives `value`,
   * `row`, and `col` as props. When set, takes precedence over both
   * `format` and the grid's default text rendering. Use for cells
   * that need a non-text visual (icons, pills, progress bars).
   */
  cellComponent?: Component
  /*
   * Optional value-deriver. When set, the column reads
   * `computeValue(row)` instead of `row[field]` for both cell
   * rendering AND client-side filter / sort matching. Useful when
   * the wire shape doesn't fit the desired display + filter
   * semantics — e.g. the EPG Grabber Modules grid where the server
   * emits the strtab string `'epggrabmodEnabled'` /
   * `'epggrabmodNone'` but the user wants a Yes / No filter:
   * `computeValue: (row) => row.status === 'epggrabmodEnabled'`.
   * Lets the column reuse the standard BooleanCell + boolean
   * filter UI without per-view glue.
   *
   * Server-side (lazy) filter mode bypasses computed values — the
   * server doesn't know about them. Use in combination with
   * IdnodeGrid's `clientSideFilter` prop (or any non-lazy DataGrid
   * caller).
   */
  computeValue?: (row: BaseRow) => unknown
  /*
   * Optional custom comparator for client-side sorting of this
   * column. Direction-agnostic ascending: return <0 when a sorts
   * before b; the grid applies the sort direction. Set it when the
   * column's wire value doesn't sort correctly by default — e.g.
   * channelNumber's "major.minor" string, which neither
   * String.localeCompare nor a decimal parse orders right.
   * Consumed by DataGrid's grouped within-cluster secondary sort
   * and by callers that own a client-side sort (the EPG Table
   * view). Ignored in server-side (lazy) sort — the server owns it.
   */
  sortComparator?: (a: unknown, b: unknown) => number
  /* Whether the column starts hidden (user can re-enable from the column-toggle menu). */
  hiddenByDefault?: boolean
  /* Initial width in pixels; user can resize and the value is persisted. */
  width?: number
  /* Lowest breakpoint at which the column is visible. Defaults
   * to 'desktop' (visible at non-phone widths). Only set 'phone'
   * to opt the column into the phone-card layout. */
  minVisible?: Breakpoint
  /*
   * Phone-card emphasis. Only meaningful when `minVisible` is
   * `'phone'`.
   *   - `'primary'` — the field renders as the card's headline:
   *     full-width row, no label, larger / bolder font. At most
   *     one column per view should set this; if more than one do,
   *     the first wins and the rest fall back to secondary.
   *   - `'secondary'` (default) — the field renders in a two-up
   *     label-value pairing with the other secondaries. An odd
   *     trailing secondary gets a full-width row to itself, which
   *     is the natural surface for fields with long values
   *     (e.g. mux strings).
   */
  phoneRole?: 'primary' | 'secondary'
  /*
   * Phone-only secondary-pair ordering. When set, the column
   * sorts at this position among the secondaries (lowest first).
   * Unset columns keep their source-array index. Useful when the
   * desktop column order differs from the order you want on the
   * phone card — e.g. Services lists `enabled` (parent class)
   * before `network` (subclass) on desktop, but the phone card
   * reads more naturally with network first.
   */
  phoneOrder?: number
  /*
   * Force the column to render on its own row in the phone card
   * (no 2-up pairing). Default false; when true, the framework
   * skips pairing this column with any sibling and emits a
   * single-column row for it. Useful when a column's value is
   * naturally long (long titles, full timestamps, free-form
   * text) and the 50%-width slot would truncate awkwardly.
   * Surrounding 2-up packing continues normally around the
   * full-width row.
   */
  phoneFullWidth?: boolean
  /*
   * Optional enum descriptor for cells that hold a key from an enum.
   * Either:
   *   - a deferred reference (`{ type: 'api', uri, params? }`) that
   *     `fetchDeferredEnum` resolves once per session, or
   *   - an inline static `Option[]` list for small fixed enums.
   * Drives label resolution (rendered label in place of the raw
   * key) for cell renderers that need a key→label mapping.
   */
  enumSource?: EnumSource
  /*
   * Optional grouping hook for `filterType: 'enum'`. When set,
   * the enum-filter popover renders the options as a
   * PrimeVue Select with `optionGroupLabel` /
   * `optionGroupChildren` bound — group headers (non-
   * selectable) cluster related options. The hook is called
   * once per option, returning the bucket identifier that
   * option belongs to; options sharing a key land under the
   * same header. Returns `null` to opt this option out of
   * grouping (rendered in a trailing "Other" bucket).
   *
   * Group LABELS are derived inside the filter component:
   * for each unique group key, the option whose own `key`
   * matches the group key supplies the header label (its
   * `val`). For the EIT content_type case this works because
   * major-group codes (`0x10`, `0x20`, ...) appear as
   * options in their own right alongside the subtype codes
   * (`0x11`, `0x12`, ...) — major-group key === bucket key,
   * major-group val === header label.
   *
   * Only meaningful with `filterType: 'enum'` + `enumSource`.
   * Without this hook, options render as a flat list. */
  enumGroupBy?: (option: Option) => string | number | null
  /*
   * Enable PrimeVue Select's built-in type-to-filter
   * affordance in the enum-filter popover. Recommended for
   * lists of more than ~15 entries (EIT content_type, large
   * deferred enums); not needed for short discrete sets like
   * DVR Priority. Default false. */
  enumFilterable?: boolean
  /*
   * Sibling-field name to display when the enum key doesn't
   * resolve (key not found in `enumSource`'s options OR options
   * still loading). Used by EnumNameCell to handle orphaned
   * references — e.g. DVR Finished's `channel` column points at
   * a UUID, but the recording's source channel may have been
   * deleted since the row was captured. The wire response
   * carries a snapshotted `channelname` string; setting
   * `fallbackField: 'channelname'` lets the cell render that
   * snapshot instead of an em-dash. Only meaningful with
   * `enumSource` set.
   */
  fallbackField?: string
  /*
   * Inline cell editing — opt in per column. Only meaningful on
   * grids that pass `editMode="cell"` to IdnodeGrid / DataGrid.
   * When `editable === true` AND the column has a known idnode
   * field type (str / int / bool / enum / time), the cell renders
   * an editor on click and posts changes via the grid's batch
   * Save toolbar. Without this flag (or when explicitly false),
   * the cell stays read-only even on edit-mode grids — useful
   * for derived / computed / read-only columns.
   */
  editable?: boolean
  /*
   * Optional override for the inline-edit editor component.
   * Defaults to the rendererDispatch-selected field renderer
   * (IdnodeFieldString / Number / Bool / Enum / Time) in
   * `compact` mode. Override only when the column needs a custom
   * editor that doesn't match the server-side prop type — rare
   * (e.g. a custom number-with-units widget).
   */
  editorComponent?: Component
  /*
   * Optional extra props forwarded to the editor component.
   * Merged with the standard `prop` + `modelValue` + `compact`
   * props the cell wiring already supplies.
   */
  editorProps?: Record<string, unknown>
  /*
   * Inline edit pattern. Two shapes:
   *   - `true` (default): PrimeVue's cell-editor overlay
   *     swaps in the `#editor` slot's content on click. Right
   *     for str / numeric / enum / time where the display
   *     reads as plain text and the editor is a separate
   *     input widget.
   *   - `false`: the `#editableCell` slot's content IS the
   *     editor (always mounted, click-to-act). Right for bool
   *     — the checkbox itself toggles state on click without
   *     PrimeVue switching to an editor overlay (which would
   *     consume the first click for edit-mode entry instead
   *     of the toggle).
   * Only meaningful when the column is `editable: true` in a
   * grid running `editMode: 'cell'`.
   */
  inlineEditorOverlay?: boolean
  /*
   * PlayCell-specific: stream-path prefix for the `/play/ticket/...`
   * URL the per-row Play icon opens. Required when
   * `cellComponent: PlayCell`. Mirrors the four Classic shapes:
   *   - 'stream/channel' / 'stream/service' / 'stream/mux' / 'dvrfile'
   * PlayCell appends `/${row.uuid}` and forwards to `openPlay()`.
   */
  playPath?: string
  /*
   * PlayCell-specific: optional builder for the `?title=` URL
   * query param. Mirrors Classic's `playLink()` decoration
   * (`static/app/tvheadend.js:856-861`) — gives the external
   * media player a human-readable title for the m3u track.
   * When omitted, the URL has no title query param.
   */
  playTitle?: (row: BaseRow) => string
  /*
   * PlayCell-specific: optional predicate gating whether the
   * icon is clickable. Defaults to always-enabled. DVR Finished
   * passes `(r) => r.filesize > 0` so deleted-on-disk rows
   * render the icon disabled — matches the toolbar Play's prior
   * gating.
   */
  playEnabled?: (row: BaseRow) => boolean
  /*
   * InfoCell-specific: per-row click callback. Required when
   * `cellComponent: InfoCell`. The cell calls this with the
   * row on click — the host view typically uses it to open a
   * detail dialog. Same callback-from-ColumnDef pattern the
   * PlayCell uses for its `playPath` etc., kept specific so
   * each cell type stays readable in column declarations.
   */
  onInfo?: (row: BaseRow) => void
  /*
   * DrillDownCell-specific: row property name that holds the
   * UUID of the referenced entity. The cell reads
   * `row[targetUuidField]` and renders a chevron-right icon
   * that opens that entity in the AppShell-mounted drill-down
   * drawer (`useEntityEditor`). Example: EPG event rows carry
   * `channelUuid` alongside `channelName`, so a Channel column
   * uses `targetUuidField: 'channelUuid'`. DVR entry rows
   * store the UUID directly in `channel`, so the DVR Channel
   * column uses `targetUuidField: 'channel'`. When unset, or
   * when the row lacks the named field, the chevron is hidden.
   */
  targetUuidField?: string
  /*
   * DrillDownCell-specific: access flag required for the
   * chevron to appear. The cell consults `useAccessStore()`
   * and hides the icon when `access.data[targetAccessKey]`
   * is falsy. Rationale: if the user couldn't reach the
   * editor via the normal nav (e.g. Configuration → Channels
   * requires admin), the chevron is a teaser for a path they
   * can't take. Unset = no permission check.
   */
  targetAccessKey?: PermissionKey
  /*
   * Title for the multi-entry picker drawer that opens when the
   * chevron fires on a cell whose value is an array of 2+ UUIDs
   * (`EnumNameCell` → `useEntityEditor.openList(rows, columns,
   * title)`). Falls back to `col.label`. Only matters for the
   * 2+-element case — a one-row drilldown opens the single
   * entity's editor directly and the picker title is unused.
   */
  pickerTitle?: string
}
