// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/* Pure helper for building the multi-edit save payload that
 * `idnode/save`'s uuid-array branch (Case 2 at
 * `src/api/api_idnode.c:408-429`) accepts. The server iterates
 * the `uuid` array and applies the SAME field set to every
 * matching node — partial success (some perm-denied / some
 * not-found) is treated as success.
 *
 * Walks `currentValues` and picks only the keys whose
 * `applyChecked[id]` is `true`. The `uuid` key is always set
 * to the supplied list. Mirrors Classic's
 * `getFieldValues` override at
 * `static/app/idnode.js:1081-1092` — the user's per-field
 * apply-checkbox is the gate.
 *
 * Same testing posture as `cloneIdnode.ts` / `idnodeMove.ts` /
 * `timeshiftDisable.ts` — pure, framework-free, easy to unit-
 * test in isolation. The host editor wires the apiCall + toast
 * + drawer-close around this. */

export interface MultiEditPayload {
  /** Array of uuids the same field set will be applied to. */
  uuid: string[]
  /** Each ticked field's value, keyed by field id. */
  [fieldName: string]: unknown
}

export function buildMultiEditPayload(
  uuids: readonly string[],
  currentValues: Readonly<Record<string, unknown>>,
  applyChecked: Readonly<Record<string, boolean>>,
): MultiEditPayload {
  const payload: MultiEditPayload = { uuid: [...uuids] }
  for (const id of Object.keys(currentValues)) {
    if (applyChecked[id]) {
      payload[id] = currentValues[id]
    }
  }
  return payload
}
