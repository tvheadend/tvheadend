// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrEntryColumns — column-array factory for the DVR entry-list
 * views (Finished / Failed / Removed; Upcoming has its own
 * shape with `start` / `stop` instead of `start_real` / `stop_real`,
 * and includes `enabled` + `pri`, so it doesn't share this builder).
 *
 * The three views differ only in which optional columns they
 * include:
 *
 *   Finished  → filesize, playcount, filename
 *   Failed    → filesize, playcount, filename, status
 *   Removed   → status   (no filesize / playcount / filename
 *                         because the file is gone)
 *
 * The Basic / Advanced / Expert grouping (controlled server-side
 * via PO_ADVANCED / PO_EXPERT) and the per-field config
 * (sortable / filterType / width / minVisible / format) come
 * from the shared DVR_FIELDS map.
 */
import type { ColumnDef } from '@/types/column'
import { DVR_FIELDS } from './dvrFieldDefs'

export interface DvrEntryColumnsOpts {
  status?: boolean
  filesize?: boolean
  playcount?: boolean
  filename?: boolean
  /** Field IDs to upgrade from `minVisible: 'desktop'` to
   *  `'phone'`. Used by views that want to surface a
   *  desktop-default field on the phone-card (e.g. FinishedView
   *  pulling `filesize` and `duration` into the card so the user
   *  sees recording size at a glance, FailedView promoting
   *  `status` for the failure-reason trailer). The shared
   *  `phoneOrder` defaults in `DVR_FIELDS` already place each
   *  field correctly; this just toggles visibility. */
  phoneFields?: string[]
}

export function dvrEntryColumns(
  kodiFmt: (v: unknown) => string,
  opts: DvrEntryColumnsOpts = {},
): ColumnDef[] {
  /* Single declarative spread — conditional segments expand to []
   * when their flag is off. Keeps the array build linear so the
   * Basic / Advanced / Expert grouping reads top-to-bottom. */
  const cols: ColumnDef[] = [
    /* Basic */
    { field: 'disp_title', ...DVR_FIELDS.disp_title, format: kodiFmt },
    { field: 'disp_extratext', ...DVR_FIELDS.disp_extratext, format: kodiFmt },
    { field: 'episode_disp', ...DVR_FIELDS.episode_disp },
    /* Use the UUID-bearing `channel` column (not the snapshotted
     * `channelname`) so drill-down + clustering work for live
     * channels; EnumNameCell's `fallbackField: 'channelname'`
     * preserves orphan recordings' display when the source
     * channel has been deleted. */
    { field: 'channel', ...DVR_FIELDS.channel },
    { field: 'start_real', ...DVR_FIELDS.start_real },
    { field: 'stop_real', ...DVR_FIELDS.stop_real },
    { field: 'duration', ...DVR_FIELDS.duration },
    ...(opts.filesize ? [{ field: 'filesize', ...DVR_FIELDS.filesize }] : []),
    ...(opts.status ? [{ field: 'status', ...DVR_FIELDS.status }] : []),
    { field: 'sched_status', ...DVR_FIELDS.sched_status },
    { field: 'comment', ...DVR_FIELDS.comment },

    /* Advanced — server-gated by PO_ADVANCED on basic users */
    ...(opts.playcount ? [{ field: 'playcount', ...DVR_FIELDS.playcount }] : []),
    { field: 'config_name', ...DVR_FIELDS.config_name },
    { field: 'copyright_year', ...DVR_FIELDS.copyright_year },

    /* Expert — server-gated by PO_EXPERT */
    { field: 'owner', ...DVR_FIELDS.owner },
    { field: 'creator', ...DVR_FIELDS.creator },
    { field: 'errors', ...DVR_FIELDS.errors },
    { field: 'data_errors', ...DVR_FIELDS.data_errors },
    ...(opts.filename ? [{ field: 'filename', ...DVR_FIELDS.filename }] : []),
  ]
  if (!opts.phoneFields?.length) return cols
  const upgradeSet = new Set(opts.phoneFields)
  return cols.map((c) =>
    upgradeSet.has(c.field) ? { ...c, minVisible: 'phone' } : c,
  )
}
