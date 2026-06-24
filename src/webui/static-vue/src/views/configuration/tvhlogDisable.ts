// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Pure helper for the Configuration → Debugging → Configuration
 * conditional-disable predicate. Mirrors the legacy ExtJS
 * onchange handler at `static/app/tvhlog.js:3-13`:
 *
 *   - syslog disabled when enable_syslog is false.
 *
 * Pure UX: the server doesn't reject saves with mismatched flag
 * combinations — it just silently ignores the downstream values
 * when the gating flag is off. The disabled state warns the
 * user that their input would have no effect.
 *
 * Classic also gated `tracesubs` disabled-when-`!trace`. The Vue
 * port renders `tracesubs` via a custom subsystem table (not the
 * form's auto-renderer), so the predicate doesn't fire for it
 * regardless — and the table cells stay editable on purpose so
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
