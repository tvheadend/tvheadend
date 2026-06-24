// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * cloneIdnode — generic "clone this entity" helper for any
 * idnode-class master-detail view that wants a Clone toolbar
 * action.
 *
 * Loads a source entity by uuid, flattens its config to a
 * key/value map, mutates a name field with a suffix to
 * disambiguate, then POSTs to the class's create endpoint.
 * Returns the new entity's uuid + name so the caller can
 * select it in the master-detail layout.
 *
 * Stream Profiles is the first consumer (`profile/create`),
 * but the helper is class-agnostic — future master-detail
 * grids that want Clone just point `createEndpoint` at their
 * own create handler.
 *
 * The class id is read from the source's `class` field
 * (configurable via `classField`) and passed as a top-level
 * `class` param of the create POST — most idnode create
 * handlers accept this shape (Profile, Network, Mux all do).
 *
 * Caller is responsible for surfacing errors (duplicate-name
 * server rejections, ref-count limits, network failures) via
 * a toast. The helper just throws the original Error.
 */
import { apiCall } from './client'

interface IdnodeProp {
  id: string
  value?: unknown
}

interface LoadResponse {
  entries?: Array<{
    uuid?: string
    /** Top-level class id, emitted by `idnode_serialize0`
     *  (`src/idnode.c:1550`) for every idnode-class type. Always
     *  present on a normal load response. */
    class?: string
    params?: IdnodeProp[]
  }>
}

interface CreateResponse {
  uuid?: string
}

export interface CloneIdnodeOptions {
  /** Endpoint to POST the new entity to. e.g. `'profile/create'`. */
  createEndpoint: string
  /** Field on the source whose value selects the create class.
   *  Profile uses `class`. Default `'class'`. */
  classField?: string
  /** Field to mutate on the cloned entity. Default `'name'`. */
  nameField?: string
  /** Suffix appended to the name. Default `' (copy)'`. */
  nameSuffix?: string
}

export interface CloneIdnodeResult {
  uuid: string
  name: string
}

export async function cloneIdnode(
  srcUuid: string,
  opts: CloneIdnodeOptions
): Promise<CloneIdnodeResult> {
  const classField = opts.classField ?? 'class'
  const nameField = opts.nameField ?? 'name'
  const nameSuffix = opts.nameSuffix ?? ' (copy)'

  /* 1. Load source. `idnode/load` without `meta` keeps the
   * response small (just the params array, no class metadata). */
  const loaded = await apiCall<LoadResponse>('idnode/load', { uuid: srcUuid })
  const entry = loaded.entries?.[0]
  if (!entry?.params) {
    throw new Error(`Failed to load source entity ${srcUuid}`)
  }

  /* 2. Flatten params → conf. The server's `prop_serialize0` emits
   * each property as `{ id, value, … }`; we only need id+value
   * for the create round-trip. */
  const conf: Record<string, unknown> = {}
  for (const p of entry.params) {
    conf[p.id] = p.value
  }

  /* 3. Extract class id. Prefer the top-level `entry.class`
   * field (always emitted by `idnode_serialize0` server-side);
   * fall back to a `class`-named property in the flattened
   * params for classes that also expose it as a property
   * (profile_class does this — see `src/profile.c:301-310`).
   * Single-class types like `dvr_config_class` only have the
   * top-level field, so the fallback alone wouldn't find it.
   * Strip the field from the conf body either way — class goes
   * in the top-level POST arg, not inside the wrapped conf
   * payload. uuid is server-assigned; never include it. */
  const classId =
    typeof entry.class === 'string' && entry.class
      ? entry.class
      : conf[classField]
  if (typeof classId !== 'string' || !classId) {
    throw new Error(
      `Source entity ${srcUuid} missing required class field "${classField}"`
    )
  }
  delete conf[classField]
  delete conf.uuid

  /* 4. Mutate name. Append the suffix. If the source had no
   * name (or a non-string), fall back to a generic placeholder
   * so the create payload is still valid (the server's
   * uniqueness check kicks in either way). */
  const rawName = conf[nameField]
  const srcName = typeof rawName === 'string' ? rawName : ''
  const newName = (srcName || 'unnamed') + nameSuffix
  conf[nameField] = newName

  /* 5. POST create. Returns `{ uuid }` for the new entity. */
  const created = await apiCall<CreateResponse>(opts.createEndpoint, {
    class: classId,
    conf: JSON.stringify(conf),
  })
  if (!created.uuid) {
    throw new Error(
      `Create endpoint ${opts.createEndpoint} returned no uuid`
    )
  }

  return { uuid: created.uuid, name: newName }
}
