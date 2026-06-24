// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * autorecSource — dynamic command palette source for autorec
 * (auto-recording) rules.
 *
 * Lazy-fetches `dvr/autorec/grid` on the first palette open and
 * caches for `CACHE_TTL_MS`. Each autorec becomes a Command with
 * three tiered actions (mirrors the recordingSource Option-A
 * pattern):
 *
 *   primary   ↵     — Edit (opens the autorec's idnode editor
 *                     drawer via `useEntityEditor().open(uuid)`)
 *   secondary ⌘↵    — Toggle enabled. One-keystroke pause/resume
 *                     without diving into the editor; flips
 *                     `enabled` via `idnode/save`. The visible
 *                     label adapts per-entry ("Disable" when
 *                     currently on, "Enable" when off).
 *   tertiary  ⇧⌘↵   — Delete the autorec. Confirm-gated (matches
 *                     the recording-delete UX) since the rule —
 *                     and any future recordings it would have
 *                     made — cannot be recovered.
 *
 * Permission: `dvr/autorec/grid` requires the RECORDER access
 * flag, so non-dvr users get an empty list (and therefore no
 * autorec commands at all). The command itself doesn't carry a
 * `requires` field; the empty-list outcome is the gate.
 */
import { ref, type Ref } from 'vue'
import { Repeat } from 'lucide-vue-next'
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import { t } from '@/composables/useI18n'
import type { useAccessStore } from '@/stores/access'
import type { useConfirmDialog } from '@/composables/useConfirmDialog'
import type { useEntityEditor } from '@/composables/useEntityEditor'
import type { useToastNotify } from '@/composables/useToastNotify'
import type { Command } from './commandRegistry'

const CACHE_TTL_MS = 60_000

/* A typical install has tens of autorecs, not thousands; a heavy
 * power user might have a couple hundred. 500 is generous; the
 * palette's fuse search narrows down to a handful per typed query. */
const FETCH_LIMIT = 500

interface AutorecRow {
  uuid: string
  /* Human label the user gave the rule. May be empty — falls
   * back to `title` (the regex/string the rule matches against
   * EPG event titles). */
  name?: string
  title?: string
  enabled?: boolean | number
  channelname?: string
}

interface AutorecGridResponse {
  entries?: AutorecRow[]
}

export interface AutorecSourceDeps {
  entityEditor: ReturnType<typeof useEntityEditor>
  router: Router
  access: ReturnType<typeof useAccessStore>
  toast: ReturnType<typeof useToastNotify>
  /* Used by the Delete tertiary action — autorecs are durable
   * rules and the deletion can't be undone, so a confirm dialog
   * mirrors the recording-delete UX. */
  confirm: ReturnType<typeof useConfirmDialog>
}

const commands = ref<Command[]>([])
let lastFetchAt = 0
let inflight: Promise<void> | null = null

export function getAutorecCommands(): Ref<Command[]> {
  return commands
}

export function ensureAutorecsLoaded(deps: AutorecSourceDeps): Promise<void> {
  const now = Date.now()
  if (inflight) return inflight
  if (commands.value.length > 0 && now - lastFetchAt < CACHE_TTL_MS) {
    return Promise.resolve()
  }
  inflight = doFetch(deps)
  return inflight
}

async function doFetch(deps: AutorecSourceDeps): Promise<void> {
  try {
    const resp = await apiCall<AutorecGridResponse>('dvr/autorec/grid', {
      start: 0,
      limit: FETCH_LIMIT,
      sort: 'name',
      dir: 'ASC',
    })
    commands.value = (resp.entries ?? []).map((e) => buildAutorecCommand(e, deps))
    lastFetchAt = Date.now()
  } catch {
    /* Silent fail — non-dvr users get an empty list naturally
     * (their access is the actual gate), and a transient
     * network blip shouldn't wipe the cache. Next call past
     * the TTL retries. */
  } finally {
    inflight = null
  }
}

function buildAutorecCommand(entry: AutorecRow, deps: AutorecSourceDeps): Command {
  const uuid = entry.uuid
  /* Label preference: user-given name first (when set), title
   * fallback (the raw matcher), then a generic placeholder so
   * the row is at least visible even for a misconfigured rule. */
  const label = entry.name?.trim() || entry.title?.trim() || '(Unnamed autorec)'
  /* `enabled` may arrive as boolean or 0/1 over the wire — both
   * coerce truthfully. */
  const isEnabled = !!entry.enabled
  const description = formatContext(entry, isEnabled)
  const cmd: Command = {
    id: `autorec:${uuid}`,
    label,
    description,
    section: 'Autorecs',
    icon: Repeat,
    keywords: ['autorec', 'series', 'rule', 'auto'],
    actionLabel: 'Edit',
    action: () => deps.entityEditor.open(uuid),
  }
  /* The autorec commands themselves are only listed because the
   * server returned them (dvr permission is the gate). The
   * Toggle + Delete secondary/tertiary need the same permission;
   * since the command is here at all, we add them unconditionally
   * — but check `access.has('dvr')` defensively to keep the
   * source self-consistent if someone changes the gating later. */
  if (deps.access.has('dvr')) {
    cmd.secondaryAction = {
      label: isEnabled ? 'Disable' : 'Enable',
      handler: () => toggleEnabled(uuid, label, isEnabled, deps.toast),
    }
    cmd.tertiaryAction = {
      label: 'Delete autorec',
      handler: () => deleteAutorec(uuid, label, deps.toast, deps.confirm),
    }
  }
  return cmd
}

function formatContext(entry: AutorecRow, isEnabled: boolean): string {
  const parts: string[] = []
  /* When the user labelled the rule themselves, the `title`
   * matcher is the most useful description ("matches: Doc
   * Martin"). When name === title (Classic's fallback shape),
   * skip the duplicate. */
  if (entry.title && entry.title !== entry.name) {
    parts.push(entry.title)
  }
  if (entry.channelname) parts.push(entry.channelname)
  /* Explicit "disabled" badge in the description — easier to
   * spot in a long list than checking the secondary-action chip. */
  if (!isEnabled) parts.push(t('disabled'))
  return parts.join(' · ')
}

async function toggleEnabled(
  uuid: string,
  label: string,
  currentlyEnabled: boolean,
  toast: ReturnType<typeof useToastNotify>,
): Promise<void> {
  const next = currentlyEnabled ? 0 : 1
  try {
    /* `idnode/save` accepts a single-object form via the `node`
     * param (JSON-encoded). Only the changed field needs to be
     * present — the server merges into the existing entry. */
    await apiCall('idnode/save', {
      node: JSON.stringify({ uuid, enabled: next }),
    })
    toast.success(
      next === 1 ? t('Enabled: {0}', label) : t('Disabled: {0}', label),
    )
    /* Reset the cache so the next palette open re-fetches with
     * the new enabled state visible on the row (and the chip
     * label adapted). */
    __resetAutorecSourceForTests()
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: currentlyEnabled
        ? t('Could not disable autorec')
        : t('Could not enable autorec'),
    })
  }
}

async function deleteAutorec(
  uuid: string,
  label: string,
  toast: ReturnType<typeof useToastNotify>,
  confirm: ReturnType<typeof useConfirmDialog>,
): Promise<void> {
  const ok = await confirm.ask(
    t('Delete autorec "{0}"? Future recordings from this rule will not be scheduled.', label),
    {
      header: t('Delete autorec'),
      acceptLabel: t('Delete'),
      rejectLabel: t('Cancel'),
      severity: 'danger',
    },
  )
  if (!ok) return
  try {
    await apiCall('idnode/delete', { uuid: JSON.stringify([uuid]) })
    toast.success(t('Deleted: {0}', label))
    __resetAutorecSourceForTests()
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not delete autorec'),
    })
  }
}

/* Test helper — drop module-level cache so leaked entries from
 * one test don't bleed into another. Also called by the toggle
 * and delete handlers to invalidate after a successful mutation. */
export function __resetAutorecSourceForTests(): void {
  commands.value = []
  lastFetchAt = 0
  inflight = null
}
