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
}

export function dvrEntryColumns(
  kodiFmt: (v: unknown) => string,
  opts: DvrEntryColumnsOpts = {},
): ColumnDef[] {
  /* Single declarative spread — conditional segments expand to []
   * when their flag is off. Keeps the array build linear so the
   * Basic / Advanced / Expert grouping reads top-to-bottom. */
  return [
    /* Basic */
    { field: 'disp_title', ...DVR_FIELDS.disp_title, format: kodiFmt },
    { field: 'disp_extratext', ...DVR_FIELDS.disp_extratext, format: kodiFmt },
    { field: 'episode_disp', ...DVR_FIELDS.episode_disp },
    { field: 'channelname', ...DVR_FIELDS.channelname },
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
}
