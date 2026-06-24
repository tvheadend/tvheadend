// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

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
 *   2. Deferred ref: `{ type: 'api', uri, event?, params? }`.
 *      Resolved on mount via the shared `fetchDeferredEnum` cache;
 *      the same cache is consulted by every consumer of the same
 *      descriptor so each option list is fetched once per session.
 *
 * Live freshness via Comet: when a deferred descriptor carries an
 * `event` hint (e.g. `service` for `src/service_mapper.c:566`),
 * we subscribe to that Comet class on mount. Each notification
 * invalidates the descriptor's cache entry and refetches; the
 * fetched array replaces `fetchedOptions.value`, which propagates
 * to every renderer reading off `options`. Mirrors Classic ExtJS
 * `static/app/idnode.js:48-52`.
 *
 * Burst behaviour: server-side mux scans emit one Comet message
 * per newly-discovered service, often dozens-to-hundreds in
 * quick succession. Debounce the refetch trigger by 250 ms so a
 * burst collapses into one network round-trip rather than N.
 *
 * Returns the resolved options as a computed (synchronous when
 * inline, async-populated when deferred — empty until the fetch
 * lands). Callers render directly off `options.value` and rely on
 * the empty-array initial state to avoid flicker (synthetic
 * "current value" option / empty checkbox group, etc.).
 */
import {
  computed,
  onBeforeUnmount,
  onMounted,
  ref,
  type ComputedRef,
} from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { cometClient } from '@/api/comet'
import { createDebounce } from '@/utils/debounce'
import {
  fetchDeferredEnum,
  getResolvedDeferredEnum,
  invalidateDeferredEnum,
  isDeferredEnum,
  type DeferredEnum,
  type Option,
} from './deferredEnum'

export interface UseEnumOptionsResult {
  options: ComputedRef<Option[]>
}

const REFETCH_DEBOUNCE_MS = 250

export function useEnumOptions(getProp: () => IdnodeProp): UseEnumOptionsResult {
  const fetchedOptions = ref<Option[] | null>(null)

  const options = computed<Option[]>(() => {
    const e = getProp().enum as unknown
    if (Array.isArray(e)) {
      /* Inline enums carry server-defined ordering — priority
       * levels go High/Normal/Low intentionally, tooltip modes
       * go Off/Always/Short, etc. The C side controls the order
       * by the sequence of htsmsg_add_msg() calls and we respect
       * it. No client-side sort here. */
      return (e as unknown[]).map((item) =>
        typeof item === 'string' || typeof item === 'number'
          ? { key: item, val: String(item) }
          : (item as Option)
      )
    }
    if (isDeferredEnum(e)) {
      /* Deferred-list enums (channel.services, channel.tags, DVR
       * config refs, etc.) are entities — the server emits them
       * in `idnode_find_all` insertion order, which has no
       * meaning for the user. Sort alphabetically by display
       * label for parity with Classic ExtJS
       * (`static/app/idnode.js:2277-2278` defaults `sortInfo` to
       * `{ field: 'val', direction: 'ASC' }`). localeCompare for
       * accent/case-aware ordering. Clone before sort so the
       * cached array stays in fetch order — the computed re-runs
       * on each read but never mutates the source. */
      const raw = fetchedOptions.value
      if (!raw) return []
      return [...raw].sort((a, b) =>
        String(a.val ?? '').localeCompare(String(b.val ?? ''))
      )
    }
    return []
  })

  /* Holds the Comet unsubscribe handle so onBeforeUnmount can
   * tear it down cleanly alongside the debounce. */
  let unsubscribe: (() => void) | null = null

  const scheduleRefetch = createDebounce((d: DeferredEnum) => {
    invalidateDeferredEnum(d)
    fetchDeferredEnum(d)
      .then((opts) => {
        fetchedOptions.value = opts
      })
      .catch(() => {
        /* Failed refetch leaves the previous options in place;
         * a follow-up Comet notification or a fresh mount will
         * trigger another attempt. */
      })
  }, REFETCH_DEBOUNCE_MS)

  /* Synchronous warm-cache check runs before the async path
   * so the FIRST render already has resolved options when
   * possible — important for the inline-cell-edit path
   * where the user clicks a cell, the editor mounts with
   * focus, and the dropdown might be opened immediately.
   * The async window of `await fetchDeferredEnum(...)`
   * meant the dropdown's first paint showed only the
   * synthetic "current value" option (the raw key,
   * typically an int or UUID) until the next microtask.
   * Cached fetches now populate before the first paint;
   * the async path below still covers cold caches. */
  const initialEnum = getProp().enum as unknown
  if (isDeferredEnum(initialEnum)) {
    const cached = getResolvedDeferredEnum(initialEnum)
    if (cached !== null) fetchedOptions.value = cached
  }

  onMounted(async () => {
    const e = getProp().enum as unknown
    if (!isDeferredEnum(e)) return
    if (fetchedOptions.value === null) {
      fetchedOptions.value = await fetchDeferredEnum(e)
    } else {
      /* Already populated from the synchronous fast path —
       * still fire the fetch (cache hit, returns existing
       * promise) so the Comet event-subscription wiring
       * below has a settled descriptor to attach to. */
      void fetchDeferredEnum(e)
    }
    if (e.event) {
      unsubscribe = cometClient.on(e.event, () => scheduleRefetch(e))
    }
  })

  onBeforeUnmount(() => {
    if (unsubscribe) {
      unsubscribe()
      unsubscribe = null
    }
    scheduleRefetch.cancel()
  })

  return { options }
}
