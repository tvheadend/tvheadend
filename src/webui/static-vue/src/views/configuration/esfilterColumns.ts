// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Per-class column definitions for the six Configuration → Stream
 * ES-filter grids. Field set per class follows the C property
 * tables in `src/esfilter.c`:
 *
 *   - video / audio / teletext / subtit:
 *     enabled, type, language, service, sindex, pid, action, log, comment
 *   - ca:
 *     enabled, type, CAid, CAprovider, service, sindex, pid, action, log, comment
 *     (no language; adds CAid + CAprovider)
 *   - other:
 *     enabled, type, language, service, pid, action, log, comment
 *     (no sindex)
 *
 * Hidden by default everywhere: `index` (server-managed sort key
 * driving Move-Up / Move-Down) and `class` (subclass tag). Both
 * still arrive on the wire — we just don't surface them as
 * columns.
 *
 * Mirrors the `eslist = '-class,index'` exclusion in legacy
 * `static/app/esfilter.js:22` for the grid display.
 */
import BooleanCell from '@/components/BooleanCell.vue'
import EnumNameCell from '@/components/EnumNameCell.vue'
import type { ColumnDef } from '@/types/column'

/* `language` is a 3-letter ISO 639 code on the wire (e.g. `"ger"`).
 * The server's `.list` callback advertises `language/locale` as
 * the enum source. Same descriptor as Configuration → Users → Access
 * Entries; the deferred-enum cache key is `uri|JSON(params)` so
 * literals with matching shape de-dup to the same network call. */
const LANGUAGE_ENUM = {
  type: 'api' as const,
  uri: 'language/locale',
}

/* `service` is a service uuid on the wire. Server's
 * `esfilter_class_service_enum` (`src/esfilter.c:405-415`) advertises
 * `service/list` with `params: { enum: 1 }`. With `enum=1`, the
 * `api_idnode_load_by_class0` handler (`src/api/api_idnode.c:197-199`)
 * emits `{ entries: [{ key: <uuid>, val: <title> }] }` — the
 * deferred-enum shape `fetchDeferredEnum` expects. */
const SERVICE_ENUM = {
  type: 'api' as const,
  uri: 'service/list',
  params: { enum: 1 },
}

/* Phone-card emphasis: the cards iterate `minVisible: 'phone'`
 * columns. We mark `type` (the elementary-stream kind, e.g.
 * "H.264 video" or "AAC audio") as the primary headline — most
 * semantic per-row identifier. `enabled` and `action` (filter
 * verb: Use / Exclude) fill the 2-up secondary row. `service`
 * (the target service name) lands as a full-width trailer
 * since service titles are typically wider than half a phone
 * column.
 *
 * Inline-static enums on the wire (`type`, `action`) auto-resolve
 * via IdnodeGrid's `decoratedColumns` key→label map — server
 * emits the inline option list inside class metadata so the grid
 * can decode without an extra fetch. Deferred enums (`language`,
 * `service`) need explicit `cellComponent` + `enumSource` wiring
 * because the metadata only carries a deferred reference, not
 * the resolved options. CAid + CAprovider are short hex strings
 * — readable as raw values, no decoder cell needed. */

/* Common columns — present on all six classes in this exact
 * order modulo class-specific insertions handled below. Every
 * column gets `editable: true`; the framework's
 * isInlineEditable strips unsupported / rdonly types at
 * runtime. Mirrors Classic's EditorGridPanel which permits
 * every non-rdonly cell to edit by default. */
const enabledCol: ColumnDef = {
  field: 'enabled',
  sortable: true,
  filterType: 'boolean',
  width: 100,
  minVisible: 'phone',
  phoneOrder: 1,
  cellComponent: BooleanCell,
  editable: true,
}

const typeCol: ColumnDef = {
  field: 'type',
  sortable: true,
  filterType: 'string',
  width: 200,
  minVisible: 'phone',
  phoneRole: 'primary',
  editable: true,
}

const languageCol: ColumnDef = {
  field: 'language',
  sortable: true,
  filterType: 'string',
  width: 140,
  cellComponent: EnumNameCell,
  enumSource: LANGUAGE_ENUM,
  editable: true,
}

const caidCol: ColumnDef = {
  field: 'CAid',
  sortable: true,
  filterType: 'string',
  width: 140,
  editable: true,
}

const caproviderCol: ColumnDef = {
  field: 'CAprovider',
  sortable: true,
  filterType: 'string',
  width: 160,
  editable: true,
}

const serviceCol: ColumnDef = {
  field: 'service',
  sortable: true,
  filterType: 'string',
  width: 240,
  minVisible: 'phone',
  phoneOrder: 99,
  cellComponent: EnumNameCell,
  enumSource: SERVICE_ENUM,
  editable: true,
}

const sindexCol: ColumnDef = {
  field: 'sindex',
  sortable: true,
  filterType: 'numeric',
  width: 120,
  editable: true,
}

const pidCol: ColumnDef = {
  field: 'pid',
  sortable: true,
  filterType: 'numeric',
  width: 100,
  editable: true,
}

const actionCol: ColumnDef = {
  field: 'action',
  sortable: true,
  filterType: 'string',
  width: 140,
  minVisible: 'phone',
  phoneOrder: 2,
  editable: true,
}

const logCol: ColumnDef = {
  field: 'log',
  sortable: true,
  filterType: 'boolean',
  width: 80,
  cellComponent: BooleanCell,
  editable: true,
}

const commentCol: ColumnDef = {
  field: 'comment',
  sortable: true,
  filterType: 'string',
  width: 280,
  editable: true,
}

/* Per-class arrays. Field order matches the C property table
 * order so the column display reads in the same order an admin
 * sees in Classic. */

export const videoColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  languageCol,
  serviceCol,
  sindexCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]

export const audioColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  languageCol,
  serviceCol,
  sindexCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]

export const teletextColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  languageCol,
  serviceCol,
  sindexCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]

export const subtitColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  languageCol,
  serviceCol,
  sindexCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]

export const caColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  caidCol,
  caproviderCol,
  serviceCol,
  sindexCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]

export const otherColumns: ColumnDef[] = [
  enabledCol,
  typeCol,
  languageCol,
  serviceCol,
  pidCol,
  actionCol,
  logCol,
  commentCol,
]
