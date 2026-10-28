// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * DVR schedule-state tokens → display kind and label.
 *
 * The server reports a DVR entry's state as a fixed token
 * (`dvr_entry_schedstatus`, `src/dvr/dvr_db.c:703-737`): the
 * `sched_status` property of dvr_entry rows and the `dvrState`
 * field of EPG event rows. Classic turns the token straight into
 * an icon CSS class (`ext.css` `.scheduled`, `.recording`, …).
 * DvrStateCell (EPG Table, DVR grids) and DvrOverlayBar (Timeline,
 * Magazine) share this mapping so every surface names a state the
 * same way.
 */
import { t as translate } from '@/composables/useI18n'

export type DvrStateKind =
  | 'scheduled'
  | 'recording'
  | 'recordingError'
  | 'completed'
  | 'completedError'
  | 'completedWarning'
  | 'completedRerecord'

const KINDS = new Set<string>([
  'scheduled',
  'recording',
  'recordingError',
  'completed',
  'completedError',
  'completedWarning',
  'completedRerecord',
])

/* The server emits the tokens verbatim, so an exact match is
 * enough. Anything else ('unknown', '', absent) has no kind. */
export function dvrStateKind(value: unknown): DvrStateKind | null {
  return typeof value === 'string' && KINDS.has(value) ? (value as DvrStateKind) : null
}

/* Short generic name for a state, used where the row has no
 * `status` text of its own (EPG rows, the Timeline / Magazine
 * overlay). The DVR grids name the icon with the row's `status`
 * instead, which says why (see DvrStateCell).
 *
 * `t` is the translator. A component passes `useI18n().t` so its
 * computed label follows a runtime language change, which the
 * static `t` doesn't track. The parameter is called `t` because
 * `make intl` extracts the literals of `t('…')` calls. */
export function dvrStateLabel(
  kind: DvrStateKind | null,
  t: (englishText: string) => string = translate,
): string {
  switch (kind) {
    case 'scheduled':
      return t('Scheduled for recording')
    case 'recording':
      return t('Recording')
    case 'recordingError':
      return t('Recording (errors)')
    case 'completed':
      return t('Completed OK')
    case 'completedError':
      return t('Failed')
    case 'completedWarning':
      return t('Time missed')
    case 'completedRerecord':
      return t('Re-record')
    default:
      return ''
  }
}
