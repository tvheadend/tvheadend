// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Wires the per-class field rules (components/idnode-fields/
 * fieldRules.ts) into an editing form: `overrides` re-evaluates as
 * the user edits, `overlay(p)` gives the prop a field component should
 * render, and a watcher writes any pinned `value` back into the form
 * so what is saved matches what is shown.
 */

import { computed, watch, type Ref } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import {
  overlayProp,
  resolveFieldOverrides,
  type FieldOverrides,
} from '@/components/idnode-fields/fieldRules'

export function useFieldRules(
  classKey: Ref<string | null>,
  values: Ref<Record<string, unknown>>,
  enabled: Ref<boolean> = computed(() => true)
) {
  const overrides = computed<FieldOverrides>(() =>
    enabled.value ? resolveFieldOverrides(classKey.value, values.value) : {}
  )

  watch(
    overrides,
    (next) => {
      for (const [id, o] of Object.entries(next)) {
        if (!('value' in o) || !(id in values.value)) continue
        if (values.value[id] !== o.value) values.value[id] = o.value
      }
    },
    { immediate: true }
  )

  function overlay(p: IdnodeProp): IdnodeProp {
    return overlayProp(p, overrides.value[p.id])
  }

  /* Rule-level messages, keyed by field id. */
  function ruleErrors(): Map<string, string> {
    const map = new Map<string, string>()
    for (const [id, o] of Object.entries(overrides.value)) {
      if (o.error) map.set(id, o.error)
    }
    return map
  }

  return { overrides, overlay, ruleErrors }
}
