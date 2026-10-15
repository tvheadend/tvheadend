// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Deferred-enum fetch + cache, shared by the enum editor fields
 * (IdnodeFieldEnum, IdnodeFieldEnumMulti) and the grid cells that
 * show names from a list (EnumNameCell).
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
 *
 * Lists that change at runtime carry an `event`, the Comet class the
 * server notifies when one of their entities is created, changed or
 * deleted (`channel` for channel/list, see channel_class_get_list in
 * src/channels.c). `subscribeDeferredEnum` refetches such a list after
 * each batch of these notifications and hands it to every subscriber,
 * so all the grid cells and editor fields showing it share one
 * subscription and one refetch.
 */
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import { createDebounce } from '@/utils/debounce'

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
   *  class via `subscribeDeferredEnum`, which refetches the list
   *  after each batch of notifications. Optional — many static
   *  lists don't change at runtime and omit it. */
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

/* Synchronous mirror of the resolved fetch result, with the number
 * of the fetch it came from. Populated by `fetchDeferredEnum` when
 * the network call returns and replaced when a newer fetch lands,
 * so it keeps the last list meanwhile. Lets sync read paths
 * (e.g. IdnodeGrid's editable-cell display, which needs to
 * convert a freshly-picked enum key into its label without
 * awaiting) reuse the same options the editor saw. Returns
 * null when the fetch hasn't landed yet — callers should fall
 * back to displaying the raw value, which gets corrected on
 * the next render after the promise resolves. */
const enumResolvedCache = new Map<string, { fetchNo: number; opts: Option[] }>()

/* Fetches are numbered as they start, so a fetch that lands after
 * a newer one can tell that its list is the older one. */
let fetchCount = 0

/* Cache key — `event` is intentionally NOT part of the key. Two
 * descriptors that point at the same `uri` + `params` share the
 * fetch result regardless of whether one carries an `event` hint
 * (every consumer ends up with the same options anyway). */
function cacheKey(d: DeferredEnum): string {
  return `${d.uri}|${JSON.stringify(d.params ?? {})}`
}

export function fetchDeferredEnum(d: DeferredEnum): Promise<Option[]> {
  const key = cacheKey(d)
  return enumFetchCache.get(key) ?? startFetch(key, d)
}

/* Fetch the list and make it the cached fetch for `key`, replacing
 * one still in flight. Each fetch hands out its list as soon as it
 * lands. One that lands after a newer fetch settles with the newer
 * list instead, so an older answer that arrives late cannot
 * overwrite the newer list, in the cache or in a component that
 * awaits it. */
function startFetch(key: string, d: DeferredEnum): Promise<Option[]> {
  const fetchNo = ++fetchCount
  const promise: Promise<Option[]> = apiCall<{ entries?: Option[] }>(d.uri, d.params ?? {})
    .then((res) => {
      const resolved = enumResolvedCache.get(key)
      if (resolved && resolved.fetchNo > fetchNo) return resolved.opts
      const opts = res.entries ?? []
      enumResolvedCache.set(key, { fetchNo, opts })
      return opts
    })
    .catch((err: unknown) => {
      /* Don't poison the cache — let the next open retry. A newer
       * fetch stays cached. */
      if (enumFetchCache.get(key) === promise) enumFetchCache.delete(key)
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
  return enumResolvedCache.get(cacheKey(d))?.opts ?? null
}

/* Delay of the refetch a notification starts. The server sends its
 * notifications in batches, at most one batch every 500 ms
 * (notify_thread in src/notify.c). The debounce turns one batch into
 * one refetch. It does not merge separate batches: while a list is
 * shown, every batch with its event class fetches it again. */
const REFETCH_DEBOUNCE_MS = 250

interface EnumSubscription {
  listeners: Set<(opts: Option[]) => void>
  /* For the test reset only. */
  stop: () => void
}

/* Keyed by cache key and event class. */
const enumSubscriptions = new Map<string, EnumSubscription>()

/*
 * Keep the list of a deferred enum fresh while a component shows it.
 * After each batch of Comet notifications of the descriptor's `event`
 * class, the list is fetched again and passed to `onOptions`.
 * Subscribers of the same descriptor share one Comet subscription and
 * one debounced refetch, so a grid column costs one request per batch
 * however many cells it has. A failed refetch keeps the previous list
 * until the next notification.
 *
 * The Comet subscription is kept for the life of the page. A grid
 * replaces all its cells when it redraws them, so a subscription that
 * ended with the last cell would end on every redraw, and the list,
 * or the fetch still running for it, would be dropped and fetched
 * again. While nobody shows the list, a notification only drops the
 * cached fetch. The next component to show the list fetches it again,
 * and `getResolvedDeferredEnum` returns the last list until then.
 *
 * Returns the unsubscribe function. A descriptor without an `event`
 * gets a no-op, and its list stays cached for the life of the page.
 */
export function subscribeDeferredEnum(
  d: DeferredEnum,
  onOptions: (opts: Option[]) => void
): () => void {
  const event = d.event
  if (!event) return () => {}
  const key = cacheKey(d)
  const subKey = `${key}|${event}`
  let sub = enumSubscriptions.get(subKey)
  if (!sub) {
    const listeners = new Set<(opts: Option[]) => void>()
    /* Fetches the list again, or only drops the cached fetch while
     * nobody shows the list. */
    const refetch = createDebounce(() => {
      if (listeners.size === 0) {
        /* Nobody shows the list. Fetch it when one does. */
        enumFetchCache.delete(key)
        return
      }
      startFetch(key, d)
        .then((opts) => {
          for (const listener of listeners) listener(opts)
        })
        .catch(() => {
          /* Keep the previous list, see above. */
        })
    }, REFETCH_DEBOUNCE_MS)
    const unsubscribe = cometClient.on(event, () => refetch())
    sub = {
      listeners,
      stop: () => {
        unsubscribe()
        refetch.cancel()
      },
    }
    enumSubscriptions.set(subKey, sub)
  }
  const { listeners } = sub
  /* A wrapper of its own, so each subscriber takes one place in the
   * set even when two of them pass the same function. */
  const listener = (opts: Option[]): void => onOptions(opts)
  listeners.add(listener)
  return () => {
    listeners.delete(listener)
  }
}

/* Test helper — drop the cached lists and the subscriptions so one
 * test does not see another's. Not part of the public surface. */
export function __resetDeferredEnumForTests(): void {
  for (const sub of enumSubscriptions.values()) sub.stop()
  enumSubscriptions.clear()
  enumFetchCache.clear()
  enumResolvedCache.clear()
}
