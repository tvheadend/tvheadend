// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * settingsSource — dynamic command palette source for singleton
 * config fields. Indexes every editable property on the six
 * Configuration → … pages whose body is a single idnode editor
 * (Base, Image cache, SAT>IP Server, Timeshift, Debugging, EPG
 * Grabber). Per-entity dataset fields (channel name, mux
 * frequency, recording title) are deliberately out of scope —
 * dataset lookup is already covered by the Channels / Recordings /
 * Autorecs sources and would otherwise explode the result count
 * (one "name" entry per N rows). Master-detail pages (Stream
 * Profiles, Codec Profiles, EPG Grabber Modules) are out for the
 * same reason — they're singletons per-instance but rendered as a
 * list-of-instances UX.
 *
 * Permission gate: all six singletons are admin-only at the route
 * level. We short-circuit at the source so non-admins pay zero
 * network cost.
 *
 * Action on Enter: navigates to the field's page with a
 * `#field=<id>` hash. `IdnodeConfigForm` watches the hash and
 * scrolls + highlights the matching row + auto-promotes the UI
 * level if the field needs Advanced or Expert. Hash-based so deep
 * links work (paste-and-share a URL pointing at a specific field).
 */
import { ref, type Ref } from 'vue'
import { Sliders } from 'lucide-vue-next'
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import { t } from '@/composables/useI18n'
import type { IdnodeProp, PropertyGroup } from '@/types/idnode'
import type { useAccessStore } from '@/stores/access'
import type { Command } from './commandRegistry'

/* Same TTL as the other dynamic sources (channels, recordings,
 * autorecs). 60 s is long enough to keep a typing session
 * fluent and short enough that adding a tuner / changing a
 * permission elsewhere reflects on the next palette open. */
const CACHE_TTL_MS = 60_000

interface PageDef {
  /* Route name in `router/index.ts` — used as `router.push({ name })`. */
  route: string
  /* Singleton-config load endpoint. Server responds with the same
   * `{ entries: [{ params, meta: { groups } }] }` shape
   * `IdnodeConfigForm` consumes (`meta: 1` requests the metadata
   * alongside current values). */
  loadEndpoint: string
  /* Human-readable trail shown as the result description, e.g.
   * "General — Base". English strings here are passed through
   * `t()` at command-build time — the server caption is also
   * pre-localised but won't capture the breadcrumb path. */
  pageTitle: string
}

const PAGES: readonly PageDef[] = [
  { route: 'config-general-base', loadEndpoint: 'config/load', pageTitle: 'General — Base' },
  { route: 'config-general-image-cache', loadEndpoint: 'imagecache/config/load', pageTitle: 'General — Image cache' },
  { route: 'config-general-satip-server', loadEndpoint: 'satips/config/load', pageTitle: 'General — SAT>IP Server' },
  { route: 'config-recording-timeshift', loadEndpoint: 'timeshift/config/load', pageTitle: 'Recording — Timeshift' },
  { route: 'config-debugging-config', loadEndpoint: 'tvhlog/config/load', pageTitle: 'Debugging' },
  { route: 'config-channel-epg-grabber', loadEndpoint: 'epggrab/config/load', pageTitle: 'Channel/EPG — EPG Grabber' },
] as const

interface LoadResponse {
  entries?: Array<{
    params?: IdnodeProp[]
    meta?: { groups?: PropertyGroup[] }
  }>
}

export interface SettingsSourceDeps {
  router: Router
  access: ReturnType<typeof useAccessStore>
}

const commands = ref<Command[]>([])
let lastFetchAt = 0
let inflight: Promise<void> | null = null

export function getSettingsCommands(): Ref<Command[]> {
  return commands
}

/*
 * Trigger a fetch if the cache is cold or stale. Idempotent —
 * concurrent calls collapse onto the same `inflight` promise.
 * Non-admins early-return; the empty commands ref is what gates
 * the source's visibility in the palette.
 */
export async function ensureSettingsLoaded(deps: SettingsSourceDeps): Promise<void> {
  if (!deps.access.has('admin')) return
  if (inflight) return inflight
  if (commands.value.length > 0 && Date.now() - lastFetchAt < CACHE_TTL_MS) return
  inflight = doFetch(deps)
  return inflight
}

async function doFetch(deps: SettingsSourceDeps): Promise<void> {
  try {
    const results = await Promise.all(PAGES.map((page) => fetchPage(page)))
    commands.value = results.flatMap(({ page, params, groupByNumber }) =>
      params.filter(searchable).map((prop) => buildSettingCommand(prop, page, groupByNumber, deps)),
    )
    lastFetchAt = Date.now()
  } finally {
    inflight = null
  }
}

async function fetchPage(
  page: PageDef,
): Promise<{ page: PageDef; params: IdnodeProp[]; groupByNumber: Map<number, string> }> {
  try {
    const resp = await apiCall<LoadResponse>(page.loadEndpoint, { meta: 1 })
    const entry = resp?.entries?.[0]
    const params = entry?.params ?? []
    const groups = entry?.meta?.groups ?? []
    const map = new Map<number, string>()
    for (const g of groups) {
      if (typeof g.number === 'number' && typeof g.name === 'string') {
        map.set(g.number, g.name)
      }
    }
    return { page, params, groupByNumber: map }
  } catch {
    /* Silent fail — one disabled module (e.g. satips on a build
     * without `--enable-satip_server`) shouldn't wipe results
     * for the other five pages. Empty params array means this
     * page just contributes nothing to the source. */
    return { page, params: [], groupByNumber: new Map() }
  }
}

/* Server-side opt-outs:
 *   `noui`    — never expose to the UI (form skips it entirely)
 *   `phidden` — permanently hidden, no toggle to show it
 * A field with neither caption nor id is corrupt and unrepresentable,
 * skip defensively. */
function searchable(p: IdnodeProp): boolean {
  if (p.noui || p.phidden) return false
  if (!p.caption && !p.id) return false
  return true
}

function buildSettingCommand(
  prop: IdnodeProp,
  page: PageDef,
  groupByNumber: Map<number, string>,
  deps: SettingsSourceDeps,
): Command {
  const groupName =
    typeof prop.group === 'number' ? groupByNumber.get(prop.group) ?? null : null
  const description = groupName ? `${page.pageTitle} · ${groupName}` : page.pageTitle
  /* Keywords carry the synonym surface for fuse:
   *   - prop.id boosts queries that use the internal field name
   *     ("uilevel", "language_ui") even when the caption is a
   *     pretty translation.
   *   - prop.description boosts conceptual matches ("how often"
   *     for an interval field, "minutes" for a duration). */
  const keywords: string[] = [prop.id]
  if (prop.description) keywords.push(prop.description)
  return {
    id: `settings:${page.route}.${prop.id}`,
    label: prop.caption ?? prop.id,
    description,
    section: 'Settings',
    icon: Sliders,
    keywords,
    requires: 'admin',
    actionLabel: t('Open setting'),
    action: () => {
      /* Hash format `#field=<id>` is parsed by IdnodeConfigForm's
       * hash-focus watcher. `.catch` swallows NavigationFailure
       * (router-guard redirect / already on the page with no hash
       * change) so an aborted navigation doesn't surface as an
       * unhandled rejection. */
      deps.router
        .push({ name: page.route, hash: `#field=${prop.id}` })
        .catch(() => undefined)
    },
  }
}

/* Test helper — drop the module-level cache so a leaked
 * fetch result from one test doesn't bleed into another.
 * Not part of the public surface. */
export function __resetSettingsSourceForTests(): void {
  commands.value = []
  lastFetchAt = 0
  inflight = null
}
