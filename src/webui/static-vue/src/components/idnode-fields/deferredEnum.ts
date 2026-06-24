// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Deferred-enum fetch + cache, shared between IdnodeFieldEnum and
 * IdnodeFieldEnumMulti.
 *
 * Background: enum-shaped properties arrive on the wire in two forms
 * (see prop_serialize_value in src/prop.c:547-552 and per-class .list
 * callbacks):
 *
 *   1. Inline array — `[{ key, val }, …]`. Static option lists are
 *      serialised directly. Each component handles this synchronously.
 *
 *   2. Deferred reference — `{ type: 'api', uri, event?, params? }`.
 *      For dynamic option sets (channel pickers, network pickers, etc.)
 *      the server tells the client *where* to fetch options, not the
 *      options themselves. ExtJS handles this via
 *      `tvheadend.idnode_enum_store` (static/app/idnode.js:263); we
 *      mirror it here. The fetched response shape is
 *      `{ entries: [{ key, val }, …] }` — same convention used by
 *      api/idnode/load and api/channel/list (api_channel.c:71-72).
 *
 * The cache is module-level so that opening five rows that all
 * reference `channel/list` does one fetch, not five — and so that an
 * IdnodeFieldEnumMulti and an IdnodeFieldEnum on the same page that
 * both reference `mpegts/input/network_list` share the result.
 *
 * Cached values are `Promise<Option[]>` so concurrent in-flight
 * requests share the same fetch instead of racing.
 */
import { apiCall } from '@/api/client'

export interface Option {
  key: string | number
  val: string
}

export interface DeferredEnum {
  type: 'api'
  uri: string
  params?: Record<string, unknown>
  /** Comet notification class that invalidates this enum's cached
   *  options. Emitted server-side (e.g. `service` for
   *  `src/service_mapper.c:566`). Consumers subscribe to this
   *  class via `useEnumOptions` and refetch on each notification.
   *  Optional — many static lists don't change at runtime and
   *  omit it. */
  event?: string
}

/*
 * EnumSource = either a server-fetched deferred reference (existing
 * `DeferredEnum` shape) OR an inline static option list. Inline lists
 * cover the small fixed enums whose labels are stable across the
 * session (e.g. mpegts_mux's tri-state Enable/Disable/Ignore) and
 * skip the round-trip + cache plumbing of the deferred path.
 */
export type EnumSource = DeferredEnum | Option[]

export function isDeferredEnum(v: unknown): v is DeferredEnum {
  return (
    !!v &&
    typeof v === 'object' &&
    !Array.isArray(v) &&
    (v as { type?: unknown }).type === 'api' &&
    typeof (v as { uri?: unknown }).uri === 'string'
  )
}

export function isInlineEnum(v: unknown): v is Option[] {
  return Array.isArray(v)
}

const enumFetchCache = new Map<string, Promise<Option[]>>()

/* Synchronous mirror of the resolved fetch result. Populated by
 * `fetchDeferredEnum` when the network call returns; cleared on
 * `invalidateDeferredEnum`. Lets sync read paths
 * (e.g. IdnodeGrid's editable-cell display, which needs to
 * convert a freshly-picked enum key into its label without
 * awaiting) reuse the same options the editor saw. Returns
 * null when the fetch hasn't landed yet — callers should fall
 * back to displaying the raw value, which gets corrected on
 * the next render after the promise resolves. */
const enumResolvedCache = new Map<string, Option[]>()

/* Cache key — `event` is intentionally NOT part of the key. Two
 * descriptors that point at the same `uri` + `params` share the
 * fetch result regardless of whether one carries an `event` hint
 * (every consumer ends up with the same options anyway). */
function cacheKey(d: DeferredEnum): string {
  return `${d.uri}|${JSON.stringify(d.params ?? {})}`
}

export function fetchDeferredEnum(d: DeferredEnum): Promise<Option[]> {
  const key = cacheKey(d)
  const existing = enumFetchCache.get(key)
  if (existing) return existing
  const promise = apiCall<{ entries?: Option[] }>(d.uri, d.params ?? {})
    .then((res) => {
      const opts = res.entries ?? []
      enumResolvedCache.set(key, opts)
      return opts
    })
    .catch((err) => {
      /* Don't poison the cache — let the next open retry. */
      enumFetchCache.delete(key)
      throw err
    })
  enumFetchCache.set(key, promise)
  return promise
}

/* Synchronous accessor for resolved options. Returns null when
 * the fetch hasn't landed (or when the descriptor was never
 * fetched). Used by render paths that can't await — e.g. the
 * editable-cell label resolver. */
export function getResolvedDeferredEnum(d: DeferredEnum): Option[] | null {
  return enumResolvedCache.get(cacheKey(d)) ?? null
}

/* Drop the cache entry for a deferred enum so the next call to
 * `fetchDeferredEnum` re-hits the network. Called from
 * `useEnumOptions` whenever a Comet notification of the
 * descriptor's `event` class arrives — the server has signalled
 * that the underlying option set has changed. */
export function invalidateDeferredEnum(d: DeferredEnum): void {
  const key = cacheKey(d)
  enumFetchCache.delete(key)
  enumResolvedCache.delete(key)
}
