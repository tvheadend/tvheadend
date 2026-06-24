// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Pure helper for the Configuration → Recording → Timeshift
 * conditional-disable predicate. Mirrors the legacy ExtJS
 * onchange handler at `static/app/timeshift.js:3-14`:
 *
 *   - max_period disabled when unlimited_period is true.
 *   - max_size  disabled when unlimited_size || ram_only is true.
 *
 * The `ram_only` branch on max_size matches server-side behaviour
 * too — `timeshift_conf_class_changed` (`src/timeshift.c:118-122`)
 * overwrites `max_size = ram_size` when `ram_only` is true, so
 * the disabled state isn't just UX cosmetic; it warns the user
 * that any value they type would be silently overwritten.
 *
 * Extracted from the view for unit testability — the view wires
 * `<IdnodeConfigForm :disabled-for="timeshiftDisable">` and the
 * predicate stays a pure function with no Vue dependencies. */

export function timeshiftDisable(
  id: string,
  vals: Record<string, unknown>
): boolean {
  if (id === 'max_period') return Boolean(vals.unlimited_period)
  if (id === 'max_size') return Boolean(vals.unlimited_size) || Boolean(vals.ram_only)
  return false
}
