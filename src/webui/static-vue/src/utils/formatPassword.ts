// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * fmtPassword — grid-cell mask for password fields (PO_PASSWORD).
 * The grid endpoints still return the stored value, so a cell must
 * never render it. The classic UI's idnode renderer
 * (`src/webui/static/app/idnode.js:362-366`) always shows asterisks;
 * here an unset value stays blank so an entry without a password
 * reads as such.
 *
 * Views that know a column holds a password set it as the column's
 * `format` so the mask is there from the first render, and
 * IdnodeGrid applies it again to every field the class metadata
 * flags `password`.
 */
export const PASSWORD_MASK = '********'

export function fmtPassword(value: unknown): string {
  return value === null || value === undefined || value === '' ? '' : PASSWORD_MASK
}
