// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Shared types for the multi-entry picker drawer (ADR 0017, Home
 * dashboard). A picker presents a compact single-select table of N
 * entities; selecting a row drives the IdnodeEditor below it.
 *
 * Kept in a standalone module so the composable (useEntityEditor) and
 * the components (EntityPickerTable, IdnodeEditor) share one
 * definition without a composable -> .vue import.
 */

/* One row of a picker table. `uuid` identifies the entity; the
 * remaining keys are the column values the caller supplies. */
export interface PickerRow {
  uuid: string
  [key: string]: unknown
}

/* One column of a picker table. `field` indexes into the row;
 * `label` is the already-translated header; `format` optionally
 * renders the raw value for display (e.g. an epoch -> a date). */
export interface PickerColumn {
  field: string
  label: string
  format?: (value: unknown, row: PickerRow) => string
}
