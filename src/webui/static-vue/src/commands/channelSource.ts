// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * channelSource — dynamic command palette source for channels.
 *
 * Lazy-fetches `channel/list` on the first palette open and caches
 * the result for `CACHE_TTL_MS`. Each channel becomes a Command
 * with three tiered actions:
 *
 *   primary   ↵     — Open in EPG (navigates to
 *                     `/epg/table?channelName=<name>`, which
 *                     TableView consumes via its existing channel-
 *                     name column filter — same UI affordance the
 *                     user gets from typing in the column funnel
 *                     manually)
 *   secondary ⌘↵    — Watch in external player. Opens
 *                     `/play/ticket/stream/channel/<uuid>` in a
 *                     new tab; tvheadend returns an .m3u playlist
 *                     authenticated via short-lived ticket, which
 *                     the OS hands off to VLC / Kodi / whichever
 *                     handler is registered. Available to anyone
 *                     with read access to the channel (server
 *                     enforces).
 *   tertiary  ⇧⌘↵   — Edit (opens the channel's idnode editor
 *                     drawer via `useEntityEditor().open(uuid)`).
 *                     Admin-only: the editor is open to read for
 *                     anyone, but channel-class save endpoints are
 *                     admin-gated server-side, so offering this to
 *                     a non-admin would surface a drawer they
 *                     can't save from. Hidden when not admin.
 *
 * Why EPG is primary: typing a channel name in the palette is most
 * commonly a "what's on?" question. Watching live is the second
 * most-common intent; editing is the rare admin path.
 *
 * The cache is module-level so multiple palette opens within
 * the TTL window reuse the same fetch result. After the TTL the
 * next open triggers a refetch — a user who added a channel and
 * waits a minute will see it without reloading the page.
 *
 * Permission: `channel/list` is `ACCESS_ANONYMOUS` server-side
 * (`src/api/api_channel.c:233`), so we don't gate the commands
 * with a `requires` field — every authenticated user sees the
 * channels they have access to (server-side `channel_access()`
 * filters per-user).
 */
import { ref, type Ref } from 'vue'
import { Tv } from 'lucide-vue-next'
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import type { useAccessStore } from '@/stores/access'
import type { useEntityEditor } from '@/composables/useEntityEditor'
import type { Command } from './commandRegistry'

const CACHE_TTL_MS = 60_000

/* `channel/list` entries arrive in the idnode key/val shape:
 * `{key: <uuid>, val: <display-name>}`. The full list is wrapped
 * in `{entries: [...]}` by the API layer. */
interface ChannelListEntry {
  key: string
  val: string
}
interface ChannelListResponse {
  entries?: ChannelListEntry[]
}

export interface ChannelSourceDeps {
  entityEditor: ReturnType<typeof useEntityEditor>
  router: Router
  /* Used to gate the per-channel "Edit channel" secondary action —
   * the idnode editor is admin-only server-side, so offering it to
   * a non-admin in the palette would surface a drawer they can't
   * actually save from. The channel command itself stays visible
   * for everyone (the primary "Open in EPG" works for any
   * authenticated user). */
  access: ReturnType<typeof useAccessStore>
}

const commands = ref<Command[]>([])
let lastFetchAt = 0
let inflight: Promise<void> | null = null

/* Reactive ref the palette consumes — re-render fires every time
 * we swap the array. */
export function getChannelCommands(): Ref<Command[]> {
  return commands
}

/* Called on each palette open. No-ops when a fresh-enough fetch is
 * already in flight or the cache hasn't expired. */
export function ensureChannelsLoaded(deps: ChannelSourceDeps): Promise<void> {
  const now = Date.now()
  if (inflight) return inflight
  if (commands.value.length > 0 && now - lastFetchAt < CACHE_TTL_MS) {
    return Promise.resolve()
  }
  inflight = doFetch(deps)
  return inflight
}

async function doFetch(deps: ChannelSourceDeps): Promise<void> {
  try {
    const resp = await apiCall<ChannelListResponse>('channel/list')
    const entries = resp.entries ?? []
    commands.value = entries.map((e) => buildChannelCommand(e, deps))
    lastFetchAt = Date.now()
  } catch {
    /* Silent fail — leave the previous cache in place so a transient
     * network blip doesn't wipe the palette's channel results. The
     * next call past the TTL window will retry. */
  } finally {
    inflight = null
  }
}

function buildChannelCommand(entry: ChannelListEntry, deps: ChannelSourceDeps): Command {
  const uuid = entry.key
  const name = entry.val
  const cmd: Command = {
    id: `channel:${uuid}`,
    label: name,
    section: 'Channels',
    icon: Tv,
    /* No permission gate on the command — `channel/list` is
     * ACCESS_ANONYMOUS and per-channel visibility is already
     * enforced server-side. The commands the user receives back
     * from the API are the ones they have access to. */
    keywords: ['channel'],
    actionLabel: 'Open in EPG',
    action: () => {
      /* Pass the channel NAME (not uuid) so TableView can drop it
       * straight into its existing channel-name column filter. The
       * column filter is a substring match — using the name keeps
       * the URL human-readable and reuses the same Table chrome
       * the user already knows. Swallow NavigationFailure (e.g.
       * router-guard redirect) so an aborted navigation doesn't
       * surface as an unhandled rejection. */
      deps.router
        .push({ name: 'epg-table', query: { channelName: name } })
        .catch(() => undefined)
    },
  }
  /* Secondary: Watch in external player. Always available — server
   * enforces channel-read access on the ticket-mint endpoint. The
   * .m3u response triggers the OS player handler in a new tab so
   * the user isn't pulled out of the Vue UI. */
  cmd.secondaryAction = {
    label: 'Watch in external player',
    handler: () => openExternalPlayer(uuid, name),
  }
  /* Tertiary: Edit drawer — admin-only. The idnode editor opens for
   * anyone, but the channel-class save endpoints are admin-gated
   * server-side, so showing "Edit channel" to a non-admin gives a
   * drawer they can't actually save from. Hide it when the user
   * can't follow through. */
  if (deps.access.has('admin')) {
    cmd.tertiaryAction = {
      label: 'Edit channel',
      handler: () => deps.entityEditor.open(uuid),
    }
  }
  return cmd
}

/* Build + open the ticket-authenticated playlist URL in a new tab.
 * `/play/ticket/...` is tvheadend's "open in external player"
 * convention (see Classic's `tvheadend.playLink` in
 * `src/webui/static/app/tvheadend.js`); the server mints a
 * short-lived ticket so the resulting stream URL doesn't need
 * separate auth — important for players like VLC that don't
 * carry browser cookies. The `title=` query param is the human-
 * readable channel name; some players read it as the playlist
 * title. */
function openExternalPlayer(uuid: string, name: string): void {
  const url = `/play/ticket/stream/channel/${encodeURIComponent(uuid)}?title=${encodeURIComponent(name)}`
  /* `_blank` so the user keeps the Vue UI open in the original
   * tab. `noopener` denies the new context access to
   * `window.opener` — defensive against any future hostile
   * redirect from the playlist endpoint. */
  globalThis.window.open(url, '_blank', 'noopener')
}

/* Test helper — drop the module-level cache so leaked entries from
 * one test don't bleed into another. Not part of the public surface. */
export function __resetChannelSourceForTests(): void {
  commands.value = []
  lastFetchAt = 0
  inflight = null
}
