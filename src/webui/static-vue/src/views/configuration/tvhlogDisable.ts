// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Pure helper for the Configuration → Debugging → Configuration
 * conditional-disable predicate:
 *
 *   - syslog disabled when enable_syslog is false.
 *
 * Pure UX: the server doesn't reject saves with mismatched flag
 * combinations — it just silently ignores the downstream values
 * when the gating flag is off. The disabled state warns the
 * user that their input would have no effect.
 *
 * `tracesubs` is not disabled when `!trace`: it renders via a
 * custom subsystem table (not the form's auto-renderer), so the
 * predicate doesn't fire for it — and the table cells stay
 * editable on purpose so
 * users can curate trace subsystems before flipping the umbrella
 * `trace` flag.
 *
 * Extracted from the view for unit testability — same shape as
 * `timeshiftDisable.ts`. */

export function tvhlogDisable(
  id: string,
  vals: Record<string, unknown>
): boolean {
  if (id === 'syslog') return !vals.enable_syslog
  return false
}
