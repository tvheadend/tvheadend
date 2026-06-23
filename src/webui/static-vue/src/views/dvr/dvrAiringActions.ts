// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * dvrAiringActions — DVR write helpers behind the per-row Record /
 * Switch actions on the Related-broadcasts / Alternative-showings
 * dialog.
 *
 * Kept as plain functions (rather than inlined in UpcomingView) so the
 * record-then-cancel sequencing is unit-testable without mounting the
 * whole view. The caller owns the confirm prompt and the toast feedback.
 */
import { apiCall } from '@/api/client'

/* Schedule a recording for a single EPG event. The empty `config_uuid`
 * tells the server to apply the requesting user's default DVR profile
 * (`dvr_config_find_by_list`). Rejects if the create fails. */
export async function recordAiring(eventId: number): Promise<void> {
  await apiCall('dvr/entry/create_by_event', {
    event_id: eventId,
    config_uuid: '',
  })
}

/* Result of switchToAiring. `'cancel-failed'` means the new recording
 * was created but the original entry could not be cancelled, so both
 * are now scheduled — the caller should tell the user. */
export type SwitchOutcome = 'ok' | 'cancel-failed'

/*
 * Switch a DVR entry to a different airing: record `eventId`, then
 * cancel the original upcoming entry.
 *
 * Order is create-then-cancel on purpose. A failed create rejects
 * before anything is cancelled — the caller aborts and nothing is
 * lost. A failed cancel, after the create already succeeded, resolves
 * to `'cancel-failed'` rather than rejecting: the new recording
 * exists, so the worst case is a visible, user-removable duplicate
 * instead of a silently lost recording.
 */
export async function switchToAiring(
  eventId: number,
  originalDvrUuid: string,
): Promise<SwitchOutcome> {
  await recordAiring(eventId)
  try {
    await apiCall('dvr/entry/cancel', { uuid: JSON.stringify([originalDvrUuid]) })
  } catch {
    return 'cancel-failed'
  }
  return 'ok'
}
