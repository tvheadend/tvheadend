/*
 * Shared option-list resolution for the three idnode enum-field
 * variants (single, multi, multi-ordered). The C-side `prop.enum`
 * arrives in one of two shapes:
 *
 *   1. Inline array: `[{ key, val }, …]` or a flat
 *      `[string|number, …]` (when the .list callback emits values
 *      via htsmsg_add_str / _u32 with a NULL key — e.g.
 *      dvr_autorec.c:787-799 for the time-of-day picklist). The
 *      flat shape is normalised to `{ key, val }` so consumers
 *      don't have to branch.
 *   2. Deferred ref: `{ type: 'api', uri, params }`. Resolved on
 *      mount via the shared `fetchDeferredEnum` cache; the same
 *      cache is consulted by every consumer of the same descriptor
 *      so each option list is fetched once per session.
 *
 * Returns the resolved options as a computed (synchronous when
 * inline, async-populated when deferred — empty until the fetch
 * lands). Callers render directly off `options.value` and rely on
 * the empty-array initial state to avoid flicker (synthetic
 * "current value" option / empty checkbox group, etc.).
 */
import { computed, onMounted, ref, type ComputedRef } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { fetchDeferredEnum, isDeferredEnum, type Option } from './deferredEnum'

export interface UseEnumOptionsResult {
  options: ComputedRef<Option[]>
}

export function useEnumOptions(getProp: () => IdnodeProp): UseEnumOptionsResult {
  const fetchedOptions = ref<Option[] | null>(null)

  const options = computed<Option[]>(() => {
    const e = getProp().enum as unknown
    if (Array.isArray(e)) {
      return (e as unknown[]).map((item) =>
        typeof item === 'string' || typeof item === 'number'
          ? { key: item, val: String(item) }
          : (item as Option)
      )
    }
    if (isDeferredEnum(e)) return fetchedOptions.value ?? []
    return []
  })

  onMounted(async () => {
    const e = getProp().enum as unknown
    if (isDeferredEnum(e)) {
      fetchedOptions.value = await fetchDeferredEnum(e)
    }
  })

  return { options }
}
