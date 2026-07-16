// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Grid-pagination constants shared across grid/list fetches.
 */

/*
 * The server's "All" page-size sentinel (`page_size_ui`, `access.c:1518`):
 * a limit this large tells tvheadend's grid handler (`api_idnode.c`) to
 * return every row. A fetch that wants the full result set passes this
 * as its `limit`.
 */
export const GRID_LIMIT_ALL = 999_999_999
