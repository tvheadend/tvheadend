// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * recordingSource — dynamic command palette source for finished
 * recordings (DVR entries with state=completed).
 *
 * Lazy-fetches `dvr/entry/grid_finished` on the first palette
 * open and caches for `CACHE_TTL_MS`. Each finished recording
 * becomes a Command with three tiered actions (matches the channel
 * source's Option-A pattern):
 *
 *   primary   ↵     — Show details (opens the DVR entry's idnode
 *                     editor drawer via `useEntityEditor().open(uuid)`).
 *                     Same drawer the DVR Finished view shows when
 *                     the user double-clicks a row.
 *   secondary ⌘↵    — Play in external player. Opens
 *                     `/play/ticket/dvrfile/<uuid>` in a new tab;
 *                     server returns a ticket-authenticated .m3u
 *                     pointing at the recorded file. Browser-based
 *                     playback isn't shipped yet, so external is
 *                     the only "watch" path today.
 *   tertiary  ⇧⌘↵   — Delete the recording (server-side
 *                     `dvr/entry/remove`). Gated on dvr permission.
 *                     No confirmation dialog in v1 — the modifier-
 *                     stack is the safeguard.
 *
 * Permission: `dvr/entry/grid_finished` requires the RECORDER
 * access flag, so non-dvr users get an empty list (and therefore
 * no recording commands at all). The command itself doesn't carry
 * a `requires` field; the empty-list outcome is the gate.
 */
import { ref, type Ref } from 'vue'
import { Video } from 'lucide-vue-next'
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import { serverUrl } from '@/utils/base'
import { t } from '@/composables/useI18n'
import type { useAccessStore } from '@/stores/access'
import type { useConfirmDialog } from '@/composables/useConfirmDialog'
import type { useEntityEditor } from '@/composables/useEntityEditor'
import type { useToastNotify } from '@/composables/useToastNotify'
import type { Command } from './commandRegistry'

const CACHE_TTL_MS = 60_000

/* Modest page cap — finished recordings can pile up indefinitely;
 * the palette only needs the most recent ones surfaced by typed
 * query. 500 is generous (a heavy DVR user has a few months of
 * weekly shows here); the ranker filters down to displayable count. */
const FETCH_LIMIT = 500

interface DvrEntryRow {
  uuid: string
  disp_title?: string
  disp_extratext?: string
  channelname?: string
  start_real?: number
}

interface DvrEntryGridResponse {
  entries?: DvrEntryRow[]
}

export interface RecordingSourceDeps {
  entityEditor: ReturnType<typeof useEntityEditor>
  router: Router
  access: ReturnType<typeof useAccessStore>
  toast: ReturnType<typeof useToastNotify>
  /* Used by the Delete tertiary action to ask "are you sure?"
   * before posting dvr/entry/remove. Recordings can't be
   * recovered, so the modifier-stack (⇧⌘↵) alone isn't a
   * sufficient safeguard — we mirror what the DVR view's Delete
   * button does. */
  confirm: ReturnType<typeof useConfirmDialog>
}

const commands = ref<Command[]>([])
let lastFetchAt = 0
let inflight: Promise<void> | null = null

export function getRecordingCommands(): Ref<Command[]> {
  return commands
}

export function ensureRecordingsLoaded(deps: RecordingSourceDeps): Promise<void> {
  const now = Date.now()
  if (inflight) return inflight
  if (commands.value.length > 0 && now - lastFetchAt < CACHE_TTL_MS) {
    return Promise.resolve()
  }
  inflight = doFetch(deps)
  return inflight
}

async function doFetch(deps: RecordingSourceDeps): Promise<void> {
  try {
    const resp = await apiCall<DvrEntryGridResponse>('dvr/entry/grid_finished', {
      start: 0,
      limit: FETCH_LIMIT,
      sort: 'start_real',
      dir: 'DESC',
    })
    commands.value = (resp.entries ?? []).map((e) => buildRecordingCommand(e, deps))
    lastFetchAt = Date.now()
  } catch {
    /* Silent fail — non-dvr users get an empty list (their
     * access is the actual gate), and a transient network blip
     * shouldn't wipe the cache. Next call past the TTL retries. */
  } finally {
    inflight = null
  }
}

function buildRecordingCommand(
  entry: DvrEntryRow,
  deps: RecordingSourceDeps,
): Command {
  const uuid = entry.uuid
  const title = entry.disp_title ?? '(Untitled)'
  const cmd: Command = {
    id: `recording:${uuid}`,
    label: titleWithExtras(title, entry.disp_extratext),
    description: formatEntryContext(entry.channelname, entry.start_real),
    section: 'Recordings',
    icon: Video,
    keywords: ['recording', 'recorded', 'dvr'],
    actionLabel: 'Show details',
    action: () => deps.entityEditor.open(uuid),
    secondaryAction: {
      label: 'Play in external player',
      handler: () => openExternalPlayer(uuid, title),
    },
  }
  /* Tertiary: Delete. Gated on dvr permission. The server enforces
   * the same check, but hiding the action keeps the affordance
   * honest. The handler asks for confirmation first — recordings
   * can't be undone, so the modifier-stack alone isn't enough of
   * a safeguard; we match the DVR view's Delete-button UX. */
  if (deps.access.has('dvr')) {
    cmd.tertiaryAction = {
      label: 'Delete recording',
      handler: () => deleteRecording(uuid, title, deps.toast, deps.confirm),
    }
  }
  return cmd
}

function titleWithExtras(title: string, extra?: string): string {
  if (!extra) return title
  return `${title} — ${extra}`
}

function formatEntryContext(channelName?: string, startEpoch?: number): string {
  const parts: string[] = []
  if (channelName) parts.push(channelName)
  if (typeof startEpoch === 'number' && Number.isFinite(startEpoch)) {
    parts.push(
      new Date(startEpoch * 1000).toLocaleDateString(undefined, {
        year: 'numeric',
        month: 'short',
        day: 'numeric',
      }),
    )
  }
  return parts.join(' · ')
}

function openExternalPlayer(uuid: string, title: string): void {
  const url = serverUrl(`play/ticket/dvrfile/${encodeURIComponent(uuid)}?title=${encodeURIComponent(title)}`)
  globalThis.window.open(url, '_blank', 'noopener')
}

async function deleteRecording(
  uuid: string,
  title: string,
  toast: ReturnType<typeof useToastNotify>,
  confirm: ReturnType<typeof useConfirmDialog>,
): Promise<void> {
  /* Confirm first — recordings can't be recovered, so a stray
   * ⇧⌘↵ shouldn't immediately wipe the file. `severity: 'danger'`
   * renders the accept button red, matching the rest of the UI's
   * destructive-confirm convention. */
  const ok = await confirm.ask(t('Delete "{0}"? This cannot be undone.', title), {
    header: t('Delete recording'),
    acceptLabel: t('Delete'),
    rejectLabel: t('Cancel'),
    severity: 'danger',
  })
  if (!ok) return
  try {
    await apiCall('dvr/entry/remove', { uuid: JSON.stringify([uuid]) })
    toast.success(t('Deleted: {0}', title))
    /* Reset the cache so the next palette open re-fetches —
     * otherwise the deleted recording would still surface for
     * up to TTL seconds. */
    __resetRecordingSourceForTests()
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not delete recording'),
    })
  }
}

/* Test helper — drop the module-level cache so leaked entries from
 * one test don't bleed into another. Also called by deleteRecording
 * to invalidate after a successful delete. */
export function __resetRecordingSourceForTests(): void {
  commands.value = []
  lastFetchAt = 0
  inflight = null
}
