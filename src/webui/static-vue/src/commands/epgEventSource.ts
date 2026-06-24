// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * epgEventSource — dynamic command palette source for EPG events.
 *
 * Type a show title; results come from `epg/events/grid?title=<q>`
 * with a 300 ms debounce and a 3-character minimum (matches the
 * existing `useEpgTitleSearch` thresholds — keeps the server load
 * bounded, avoids per-keystroke fan-out for short queries that
 * would match too much).
 *
 * Each event becomes a Command:
 *
 *   id          : `epg-event:<eventId>`
 *   label       : event title
 *   description : "<Channel> · <Day> <HH:MM>" (or whichever fields
 *                 the server returned)
 *   action      : navigate to /epg/table?title=<title>, which the
 *                 TableView's existing URL-→-column-filter handler
 *                 consumes. User lands on a filtered table view of
 *                 every airing of that title; clicking a row opens
 *                 the event drawer in the normal Table-view flow.
 *
 * No secondary action in v1 — opening the event drawer directly
 * from outside an EPG view would need a new
 * `useEpgEventDrawer`-style composable; deferred.
 *
 * Permission: `epg/events/grid` is `ACCESS_ANONYMOUS` server-side
 * (api_epg.c), so no `requires` gate on the command. Per-event
 * visibility is enforced server-side via channel access.
 */
import { ref, type Ref } from 'vue'
import { Calendar } from 'lucide-vue-next'
import type { Router } from 'vue-router'
import { apiCall } from '@/api/client'
import { createDebounce } from '@/utils/debounce'
import { t } from '@/composables/useI18n'
import type { useAccessStore } from '@/stores/access'
import type { useToastNotify } from '@/composables/useToastNotify'
import type { Command } from './commandRegistry'

/* Debounce window for live-typing — matches `useEpgTitleSearch`. */
const SEARCH_DEBOUNCE_MS = 300

/* Below the minimum we skip the fetch entirely; "a" or "ab" would
 * match thousands of events and is almost never the user's intent. */
const MIN_QUERY_LENGTH = 3

/* Cap the result list. EPG can return thousands of matches for a
 * common word ("News") — even capped, the palette would otherwise
 * be dominated by one section. Tight cap at 5 keeps the EPG group
 * compact so Channels / Actions / Navigation stay visible above
 * the fold; users who need the full set hit Enter on any result
 * and land in `/epg/table?title=…`, which renders every airing. */
const RESULT_LIMIT = 5

/* Reactive backing store for the palette's allCommands computed.
 * Cleared synchronously when the query falls below the minimum or
 * when the module is reset; replaced atomically when each fetch
 * resolves. */
const commands = ref<Command[]>([])

/* Debounce + cancellation. `debouncedFire` lets a fast typer's
 * intermediate queries collapse into one fetch. `activeToken` is
 * bumped on every new query so out-of-order responses (the server
 * returned slowly while a newer query already fired) get dropped
 * instead of overwriting fresher results. */
const debouncedFire = createDebounce((q: string, deps: EpgEventSourceDeps) => {
  void fire(q, deps)
}, SEARCH_DEBOUNCE_MS)
let activeToken = 0

export interface EpgEventSourceDeps {
  router: Router
  /* Used to gate the per-event "Record" secondary action — the
   * dvr/entry/create_by_event server endpoint requires the
   * RECORDER access flag (= the 'dvr' permission client-side).
   * The command itself stays visible for everyone (the primary
   * "Open in EPG" works for any authenticated user). */
  access: ReturnType<typeof useAccessStore>
  /* Used to toast Record success/failure. */
  toast: ReturnType<typeof useToastNotify>
}

/* Server response shape — only the fields the palette renders.
 * Other columns (description, channelUuid, dvrState, etc.) are
 * returned but ignored. */
interface EpgEventEntry {
  eventId: number
  title?: string
  channelName?: string
  start?: number
}

interface EpgEventResponse {
  entries?: EpgEventEntry[]
}

/* Reactive ref the palette consumes — re-render fires every time
 * we swap the array. */
export function getEpgEventCommands(): Ref<Command[]> {
  return commands
}

/*
 * Drive a new query. Called from the palette whenever
 * `palette.query` changes. Debounce + min-length gating prevent
 * per-keystroke fanout; cancellation tokens prevent stale results
 * from overwriting fresh ones.
 */
export function updateEpgQuery(query: string, deps: EpgEventSourceDeps): void {
  debouncedFire.cancel()
  const q = query.trim()
  if (q.length < MIN_QUERY_LENGTH) {
    /* Below the gate: bump token so any in-flight fetch's response
     * is dropped on arrival, then clear visible state. */
    activeToken += 1
    commands.value = []
    return
  }
  debouncedFire(q, deps)
}

async function fire(q: string, deps: EpgEventSourceDeps): Promise<void> {
  activeToken += 1
  const myToken = activeToken
  try {
    const resp = await apiCall<EpgEventResponse>('epg/events/grid', {
      title: q,
      limit: RESULT_LIMIT,
      sort: 'start',
      dir: 'ASC',
    })
    if (myToken !== activeToken) return
    commands.value = (resp.entries ?? []).map((e) => buildEventCommand(e, deps))
  } catch {
    /* Swallow — typed-query searches that fail (network blip)
     * should just show no results rather than throwing a toast.
     * The user can keep typing and the next keystroke fires a
     * fresh fetch. Don't wipe an in-flight token check though —
     * only the freshest fetch may clear results. */
    if (myToken === activeToken) commands.value = []
  }
}

function buildEventCommand(entry: EpgEventEntry, deps: EpgEventSourceDeps): Command {
  const title = entry.title ?? '(Untitled)'
  const cmd: Command = {
    id: `epg-event:${entry.eventId}`,
    label: title,
    description: formatEventContext(entry.channelName, entry.start),
    section: 'EPG',
    icon: Calendar,
    keywords: ['show', 'epg', 'program'],
    actionLabel: 'Open in EPG',
    action: () => {
      /* Navigate with `?title=<title>` so TableView's existing
       * URL-→-perColumn watcher applies the title column filter.
       * Same pattern as channelSource using `?channelName=`. The
       * user lands on a filtered Table view; clicking the row
       * opens the event drawer via the normal Table flow. .catch
       * swallows NavigationFailure (router-guard redirect etc.). */
      deps.router
        .push({ name: 'epg-table', query: { title } })
        .catch(() => undefined)
    },
  }
  /* Secondary: Record this event. Gated on `dvr` permission —
   * the server endpoint requires the RECORDER access flag, so a
   * user without it would get a 403; hiding the action is the
   * honest UX. The handler fires the same endpoint Classic's
   * EPG event drawer uses, with just `event_id` — `config_uuid`
   * is omitted so the server picks the user's default DVR config.
   * Tertiary: Record series — schedules an autorec by the event's
   * series link (or title fallback), so every airing of the show
   * is recorded automatically. Same permission gate; same shape
   * as the single-event record, just a different endpoint. */
  if (deps.access.has('dvr')) {
    cmd.secondaryAction = {
      label: 'Record',
      handler: () => recordEvent(entry.eventId, title, deps.toast),
    }
    cmd.tertiaryAction = {
      label: 'Record series',
      handler: () => recordSeries(entry.eventId, title, deps.toast),
    }
  }
  return cmd
}

async function recordEvent(
  eventId: number,
  title: string,
  toast: ReturnType<typeof useToastNotify>,
): Promise<void> {
  try {
    /* Both `event_id` AND `config_uuid` are required by the
     * server (`api_dvr_entry_create_from_single` in
     * src/api/api_dvr.c — returns EINVAL/400 if either is
     * missing). Passing `config_uuid: ''` lets the server fall
     * back to the user's default DVR config — same as Classic's
     * EPG record dialog when no specific config is selected
     * (`tvheadend.epgrec.js` calls `getValue()` on the unset
     * combo, which returns the empty string). */
    await apiCall('dvr/entry/create_by_event', {
      event_id: eventId,
      config_uuid: '',
    })
    toast.success(t('Recording scheduled: {0}', title))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not schedule recording'),
    })
  }
}

async function recordSeries(
  eventId: number,
  title: string,
  toast: ReturnType<typeof useToastNotify>,
): Promise<void> {
  try {
    /* Mirrors recordEvent — the server's
     * api_dvr_autorec_create_by_series takes the same
     * event_id + config_uuid shape (via the shared
     * api_dvr_entry_create_from_single param helper). The
     * server resolves the event's series link (or falls back
     * to a by-title autorec when no series link is broadcast)
     * and creates one autorec entry; subsequent airings of
     * the show are recorded automatically. */
    await apiCall('dvr/autorec/create_by_series', {
      event_id: eventId,
      config_uuid: '',
    })
    toast.success(t('Series recording set: {0}', title))
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Could not set series recording'),
    })
  }
}

function formatEventContext(channelName?: string, startEpoch?: number): string {
  const parts: string[] = []
  if (channelName) parts.push(channelName)
  if (typeof startEpoch === 'number' && Number.isFinite(startEpoch)) {
    /* Compact human-readable time — "Tue 20:00" style. Local
     * timezone; no year (palette only shows imminent events). */
    parts.push(
      new Date(startEpoch * 1000).toLocaleString(undefined, {
        weekday: 'short',
        hour: '2-digit',
        minute: '2-digit',
      }),
    )
  }
  return parts.join(' · ')
}

/* Test helper — drop the module-level state so leaked entries from
 * one test don't bleed into another. Not part of the public surface. */
export function __resetEpgEventSourceForTests(): void {
  commands.value = []
  activeToken += 1
  debouncedFire.cancel()
}

