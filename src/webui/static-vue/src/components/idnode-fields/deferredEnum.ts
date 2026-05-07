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

export function fetchDeferredEnum(d: DeferredEnum): Promise<Option[]> {
  const key = `${d.uri}|${JSON.stringify(d.params ?? {})}`
  const existing = enumFetchCache.get(key)
  if (existing) return existing
  const promise = apiCall<{ entries?: Option[] }>(d.uri, d.params ?? {})
    .then((res) => res.entries ?? [])
    .catch((err) => {
      /* Don't poison the cache — let the next open retry. */
      enumFetchCache.delete(key)
      throw err
    })
  enumFetchCache.set(key, promise)
  return promise
}
