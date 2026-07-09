<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * EpgEventDrawer — read-only detail drawer for an EPG event.
 *
 * Visual idiom mirrors IdnodeEditor's drawer (used by every other
 * editor across the app — DVR, Configuration, TV Adapters, etc.):
 *
 *   - PrimeVue <Drawer> with the same width (480 px desktop /
 *     full-screen phone), header carries the event title.
 *   - Body uses collapsible <details>/<summary> "group" cards with
 *     the same border, radius, summary chrome.
 *   - Inside each group, fields render as `.ifld` rows with a
 *     label-left / value-right grid (180 px label, 1fr value).
 *
 * Difference from IdnodeEditor: read-only, so no save/cancel
 * footer — just the X close. EPG events aren't idnodes, so no
 * idnode wiring (no per-prop renderer dispatch, no class metadata).
 *
 * Channel identity sits in a small header strip ABOVE the first
 * group rather than as a `.ifld` row, because the channel logo
 * (40 px) wouldn't align cleanly with the 1-line text rows in the
 * grid.
 *
 * Shared by both EPG views:
 *   - TimelineView's <EpgTimeline>: opens on event-block click.
 *   - TableView: opens on row click.
 *
 * Action surface: Record / Stop recording / Delete recording (via
 * `<ActionMenu>`, top of body). Buttons swap based on the event's
 * `dvrState`, mirroring the legacy ExtJS dialog at
 * `static/app/epg.js:327-435`.
 */
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useDvrEditor } from '@/composables/useDvrEditor'
import Drawer from 'primevue/drawer'
import ActionMenu from '@/components/ActionMenu.vue'
import ChannelLogo from '@/components/ChannelLogo.vue'
import KodiText from '@/components/KodiText.vue'
import PlayProfileDialog from '@/components/PlayProfileDialog.vue'
import EpgRelatedDialog from '@/components/EpgRelatedDialog.vue'
import { recordAiring, switchToAiring } from '@/views/dvr/dvrAiringActions'
import type { EpgRelatedMode } from '@/composables/useEpgRelatedFetch'
import { apiCall } from '@/api/client'
import { ApiError } from '@/api/errors'
import { openPlay } from '@/utils/playUrl'
import { useVideoPlayer } from '@/composables/useVideoPlayer'
import { useStreamProfilesStore } from '@/stores/streamProfiles'
import { useAccessStore } from '@/stores/access'
import { useDvrConfigStore } from '@/stores/dvrConfig'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'
import { useI18n } from '@/composables/useI18n'
import { useResizableDrawerWidth } from '@/composables/useResizableDrawerWidth'

const { t } = useI18n()

/* Resize the drawer width via a drag handle on the inside-left
 * edge. Persistent across sessions per the composable shape;
 * double-click resets to 480 px. Shared composable, also used
 * by ChannelManageDrawer. Bounds tuned for an EPG detail view:
 * narrower min than the channel reorganiser (the read-only
 * vertical content remains readable at ~320 px), modest max
 * because there's nothing inside that benefits from being
 * wider than ~900 px (one column of stacked labelled rows). */
const drawerWidth = useResizableDrawerWidth({
  storageKey: 'epg-event-drawer:width',
  defaultPx: 480,
  minPx: 320,
  maxPx: 900,
})

const resizeHandleEl = ref<HTMLElement | null>(null)
let detachResizeHandle: (() => void) | null = null

watch(resizeHandleEl, (el) => {
  if (detachResizeHandle) {
    detachResizeHandle()
    detachResizeHandle = null
  }
  if (el) detachResizeHandle = drawerWidth.attachHandle(el)
})

function onResizeHandleDblclick(): void {
  drawerWidth.reset()
}

import { iconUrl } from './epgGridShared'
import { fmtDate, fmtDateOnly } from '@/utils/formatTime'
import { classifyRating, type RatingDisplay } from './epgClassification'
import { categoriseCredits, type CreditsDisplay } from './epgCredits'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import type { ActionDef } from '@/types/action'

export interface EpgEventDetail {
  eventId: number
  title?: string
  /* Three supplementary-text fields: subtitle, summary, description.
   * EPG sources populate them inconsistently (XMLTV / EIT / OTA
   * grabbers all differ). Drawer displays each in its own labelled
   * row when set — no fallback collapsing here so the labels stay
   * truthful. The timeline + table use `extraText()` for a single
   * line; see ADR 0012. */
  subtitle?: string
  summary?: string
  description?: string
  channelName?: string
  channelNumber?: number
  channelUuid?: string
  channelIcon?: string
  start?: number
  stop?: number
  /* Server-formatted episode string (e.g. `S14E01`, or a localised
   * "Episode 5 / Season 2"). Built in `src/api/api_epg.c:178/181`
   * from `epg_episode_number_format()` — already a display-ready
   * string, so callers render it as-is without further formatting. */
  episodeOnscreen?: string
  genre?: number[]
  hd?: boolean
  widescreen?: boolean
  audiodesc?: boolean
  /*
   * DVR-entry pairing fields, emitted by `api/epg/events/grid` only
   * for users with `ACCESS_RECORDER` AND when a DVR entry exists for
   * this broadcast (`api/api_epg.c:230-243`). Drives the Record /
   * Stop / Delete action buttons:
   *   - `dvrState` starts with `'recording'` ⇒ event is currently
   *     being recorded ⇒ render Stop button.
   *   - `dvrState` starts with `'scheduled'` ⇒ event is scheduled
   *     to record ⇒ render Delete button.
   *   - Neither field present ⇒ no DVR entry ⇒ render Record button.
   * Full taxonomy in `dvr_db.c:704-737` (`scheduled`, `recording`,
   * `recordingError`, `completed`, `completedError`,
   * `completedRerecord`, `completedWarning`).
   */
  dvrState?: string
  dvrUuid?: string
  /*
   * Classification metadata — all server-emitted by `api_epg_entry`
   * (`src/api/api_epg.c:79-250`). Fields are independent and
   * sparsely populated depending on the event source (EIT only,
   * XMLTV only, both). The drawer's Classification group hides any
   * row whose value is missing / empty.
   *
   *   - `category` / `keyword`     : XMLTV string lists.
   *   - `starRating`               : 0–10 numeric, XMLTV / OTA.
   *   - `ageRating`                : EIT parental age 0x00–0xFF.
   *                                  0x01–0x0F = (value + 3) years
   *                                  per ETSI EN 300 468; broadcaster
   *                                  -defined above 0x0F.
   *   - `ratingLabel`              : localised display label
   *                                  (e.g. "PG-13"); only sent when
   *                                  `epggrab_conf.epgdb_processparentallabels`
   *                                  is true server-side.
   *   - `ratingLabelIcon`          : image-cache path for a small
   *                                  rating glyph (PNG, ~24 px).
   *   - `copyright_year`           : 4-digit year (uint16).
   *   - `first_aired`              : epoch seconds; 0 means unknown.
   */
  category?: string[]
  keyword?: string[]
  starRating?: number
  ageRating?: number
  ratingLabel?: string
  ratingLabelIcon?: string
  copyright_year?: number
  first_aired?: number
  /* XMLTV programme image — server emits a usable URL after
   * `imagecache_get_propstr()` (`api/api_epg.c:184-189`); typical
   * shapes are `imagecache/<id>` for cached entries or an
   * absolute http(s) URL passed through. Empty / missing for
   * events without an image. Rendered as a poster at the bottom
   * of the drawer when present. */
  image?: string
  /* XMLTV credits map — `name → role-type`. Role-type values
   * are lowercase strings from the XMLTV grabber:
   * `actor` / `guest` / `presenter` / `director` / `writer`,
   * plus broadcaster-specific extras like `host` / `producer` /
   * `composer`. The drawer's Credits group categorises into
   * Starring / Director / Writer / Crew via `epgCredits.ts`,
   * matching the legacy ExtJS layout
   * (`tvheadend.js:339-377` `getDisplayCredits`). */
  credits?: Record<string, string>
  /* Series-link URI (CRID-equivalent identifier emitted when the
   * broadcaster tags this episode as part of a series). Drives
   * the "Record series" vs "Autorec" label flip on the per-event
   * autorec button — when present, the button creates an AutoRec
   * keyed on the serieslink, capturing all episodes; when absent,
   * the server-side `create_by_series` endpoint falls back to a
   * title-based AutoRec. Server emits via `api_epg.c:110`. */
  serieslinkUri?: string
}

interface Props {
  event: EpgEventDetail | null
}

const props = defineProps<Props>()

const emit = defineEmits<{
  close: []
}>()

const access = useAccessStore()
const dvrConfig = useDvrConfigStore()
const contentTypes = useEpgContentTypeStore()
const dvrEditor = useDvrEditor()

/* Profile selection for the Record button. Empty string means
 * "use the server's default DVR config" — the server resolves
 * that in `dvr_config_find_by_list` (`src/dvr/dvr_config.c:103-132`).
 * Reset to '' on every event change so the dropdown doesn't carry
 * a previous selection across drawer opens (matches the legacy
 * ExtJS popup, which builds a fresh combo per dialog). */
const selectedConfigUuid = ref<string>('')

/* Related / alternative showings dialog state. Opens the same
 * EpgRelatedDialog browser DVR Upcoming uses, scoped to this event.
 * `relatedMode` retains the last-picked mode so the dialog always
 * has a valid `mode` prop, even while closed. */
const relatedVisible = ref(false)
const relatedMode = ref<EpgRelatedMode>('related')

watch(
  () => props.event,
  (ev) => {
    selectedConfigUuid.value = ''
    /* A re-targeted drawer closes any open showings dialog. */
    relatedVisible.value = false
    if (!ev) return
    /* Genre code → localised label resolution; idempotent fetch. */
    contentTypes.ensure()
    if (!access.data?.dvr) return
    dvrConfig.ensure().then(() => {
      /* Land on the first entry — the auto-created default config
       * (title "(Default profile)" sorts first via the leading paren).
       * If the fetch failed or returned nothing, keep '' — the server
       * still resolves an empty config_uuid to its default. */
      selectedConfigUuid.value = dvrConfig.entries[0]?.key ?? ''
    })
  },
  { immediate: true }
)

const visible = computed<boolean>({
  get: () => props.event !== null,
  set: (v) => {
    if (!v) emit('close')
  },
})

/* Esc-to-close — wired manually since `:dismissable="false"` on the
 * Drawer disables PrimeVue's built-in Esc handling (along with the
 * outside-click handling we explicitly want gone). Listener attaches
 * at the document level so it works regardless of focus location. */
function onKeyDown(ev: KeyboardEvent) {
  if (ev.key === 'Escape' && visible.value) emit('close')
}
onMounted(() => document.addEventListener('keydown', onKeyDown))
onBeforeUnmount(() => {
  document.removeEventListener('keydown', onKeyDown)
  /* Defensive: drop the drag-handle listeners if the drawer
   * tears down mid-drag so document-level mousemove/touchmove
   * listeners can't leak past the component's lifetime. */
  if (detachResizeHandle) {
    detachResizeHandle()
    detachResizeHandle = null
  }
})

/* Stream profiles for the Play actions. `ensure()` is a cached
 * single fetch; calling it on mount means the profile list is
 * settled by the time the user opens the Play dropdown. */
const streamProfiles = useStreamProfilesStore()
onMounted(async () => {
  await streamProfiles.ensure()
})

/* Channel queued for the "play with profile" dialog — non-null
 * while PlayProfileDialog is open; cleared on close. */
const playProfileChannel = ref<string | null>(null)

/* ---- Record / Stop / Delete actions ----
 *
 * Mirrors ExtJS' EPG detail dialog at `static/app/epg.js:327-435`.
 * One of three buttons is rendered depending on `event.dvrState`:
 *
 *   - Record         ← no DVR entry. POST `dvr/entry/create_by_event`.
 *   - Stop recording ← dvrState starts with 'recording'. POST
 *                      `dvr/entry/stop`. Confirms before firing.
 *   - Delete recording ← dvrState starts with 'scheduled'. POST
 *                      `idnode/delete`. Confirms before firing.
 *
 * Confirmation strings copied verbatim from the legacy UI for
 * translation reuse when vue-i18n lands. Confirmations use the
 * themed PrimeVue `<ConfirmDialog>` (`useConfirmDialog`); errors
 * surface via PrimeVue Toast (`useToastNotify`).
 *
 * After every action the drawer closes — matches ExtJS exactly. The
 * server's `idnode_notify_changed` for `dvrentry` fires; TimelineView
 * subscribes to that class and refetches events, so the underlying
 * grid reflects the new state on the next render.
 *
 * The Record call passes `config_uuid: ''` so the server uses the
 * default DVR profile. Users can switch to a non-default config by
 * editing the row from DVR Upcoming after creation.
 */
const inflight = ref(false)
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

async function runAction(
  endpoint: string,
  params: Record<string, unknown>,
  confirmText: string | null,
  failPrefix: string
): Promise<void> {
  if (confirmText) {
    /* Stop / Delete are destructive — render the accept button red.
     * Record (the only non-destructive action) passes confirmText=null
     * so this branch is skipped. */
    const ok = await confirmDialog.ask(confirmText, { severity: 'danger' })
    if (!ok) return
  }
  inflight.value = true
  try {
    await apiCall(endpoint, params)
  } catch (e) {
    const msg = e instanceof ApiError || e instanceof Error ? e.message : String(e)
    inflight.value = false
    toast.error(`${failPrefix}: ${msg}`)
    return
  }
  inflight.value = false
  emit('close')
}

function recordEvent() {
  if (!props.event) return
  /* Empty `config_uuid` means "use the default DVR config" server-
   * side — see `api_dvr.c:207` `api_dvr_entry_create_by_event`. */
  runAction(
    'dvr/entry/create_by_event',
    { event_id: props.event.eventId, config_uuid: selectedConfigUuid.value },
    null,
    t('Failed to schedule recording'),
  )
}

function stopRecording() {
  if (!props.event?.dvrUuid) return
  /* `uuid` must be a JSON-encoded array (`'["..."]'`); the server's
   * idnode-handler routes parse it that way. Bare-string form returns
   * 404. Same convention as `useBulkAction.ts:85`. */
  runAction(
    'dvr/entry/stop',
    { uuid: JSON.stringify([props.event.dvrUuid]) },
    t('Do you really want to gracefully stop/unschedule this recording?'),
    t('Failed to stop recording'),
  )
}

function deleteRecording() {
  if (!props.event?.dvrUuid) return
  runAction(
    'idnode/delete',
    { uuid: JSON.stringify([props.event.dvrUuid]) },
    t('Do you really want to remove this recording?'),
    t('Failed to delete recording'),
  )
}

/* Per-event AutoRec creation. Mirrors Classic's `recordSeries`
 * in `static/app/epg.js:546-548` — single handler regardless of
 * the button's "Record series" / "Autorec" label, POSTs to
 * `api/dvr/autorec/create_by_series` with the event id +
 * selected DVR profile. Server-side the rule is keyed on the
 * event's serieslink when available, falls back to title. The
 * drawer closes on success; toast confirms (departure from
 * Classic which is silent — matches our app's existing
 * Record / Stop / Delete feedback convention). */
async function recordSeries() {
  if (!props.event) return
  inflight.value = true
  try {
    await apiCall('dvr/autorec/create_by_series', {
      event_id: props.event.eventId,
      config_uuid: selectedConfigUuid.value,
    })
  } catch (e) {
    const msg = e instanceof ApiError || e instanceof Error ? e.message : String(e)
    inflight.value = false
    toast.error(`${t('Failed to create AutoRec rule')}: ${msg}`)
    return
  }
  inflight.value = false
  toast.success(t('AutoRec rule created'))
  emit('close')
}

/* "View DVR entry" action — opens the entry's edit drawer in
 * place over the EPG view (no route change). The EPG event drawer
 * closes itself first since the DVR editor's modal backdrop would
 * obscure it. The singleton editor handles all DVR states (upcoming,
 * recording, finished, failed) — server flags non-applicable fields
 * read-only per the loaded entry's class. */
function viewDvrEntry() {
  if (!props.event?.dvrUuid) return
  emit('close')
  dvrEditor.open(props.event.dvrUuid)
}

/* ---- Related / alternative showings ----
 *
 * Opens EpgRelatedDialog — the same broadcast browser DVR Upcoming
 * uses — for the current event. Per-row Record schedules the picked
 * airing; per-row Switch additionally cancels this event's own
 * scheduled recording, and is offered only when the event has a DVR
 * entry (the dialog's `show-switch` gate). The browse endpoints are
 * anonymous-access, so the menu entry itself needs no recorder gate.
 */
function openRelated(mode: EpgRelatedMode): void {
  relatedMode.value = mode
  relatedVisible.value = true
}

async function onRelatedRecord(airing: EpgEventDetail): Promise<void> {
  try {
    await recordAiring(airing.eventId)
    toast.success(t('Recording scheduled'))
  } catch (e) {
    const msg = e instanceof ApiError || e instanceof Error ? e.message : String(e)
    toast.error(`${t('Failed to schedule recording')}: ${msg}`)
  }
}

async function onRelatedSwitch(airing: EpgEventDetail): Promise<void> {
  const original = props.event?.dvrUuid
  if (!original) return
  const ok = await confirmDialog.ask(
    t('Replace this recording with the selected airing?'),
    { severity: 'danger' },
  )
  if (!ok) return
  try {
    const outcome = await switchToAiring(airing.eventId, original)
    if (outcome === 'cancel-failed') {
      toast.error(
        t('Recorded the new airing, but could not cancel the original — both are now scheduled.'),
      )
      return
    }
    toast.success(t('Switched to the selected airing'))
    relatedVisible.value = false
    emit('close')
  } catch (e) {
    const msg = e instanceof ApiError || e instanceof Error ? e.message : String(e)
    toast.error(`${t('Failed to schedule recording')}: ${msg}`)
  }
}

/* Tooltip for the "Play in browser" item. The item is disabled only
 * in the degenerate case of a server offering no stream profile at
 * all; otherwise the player attempts playback and reports its own
 * error if the chosen profile cannot be decoded. */
function browserPlayTooltip(): string {
  if (streamProfiles.canPlayInBrowser) {
    return t('Watch the live channel here in the browser')
  }
  return t('No stream profiles are available.')
}

/* Play action factory — leads when applicable.
 *
 * Live event: a Play dropdown with three choices —
 *   - "Play in browser": opens the in-browser modal player
 *     (`useVideoPlayer`). Offers every stream profile; the player
 *     itself reports a clear error if the chosen profile cannot be
 *     decoded in this browser.
 *   - "Play in external player": the `/play/ticket/...` route
 *     returns an m3u so the OS hands off to the user's external
 *     media player (server-default stream profile).
 *   - "Play in external player with profile…": opens
 *     PlayProfileDialog to pick an explicit stream profile, then
 *     hands off to the external player as above.
 *
 * Completed recording: a single "Play" action via the external
 * player. In-browser playback isn't offered for recordings —
 * `/dvrfile/<uuid>` serves the file raw with no transcode-on-
 * playback, so a recording only plays in a browser if its on-
 * disk format already happens to be browser-native.
 *
 * Future / past-without-recording: no Play (nothing to stream).
 *
 * The server enforces ACCESS_STREAMING on the underlying
 * /stream/ + /dvrfile/ routes — no client-side access check
 * needed. Extracted from the actions computed so the surrounding
 * function stays below the cognitive-complexity cap. */
function buildPlayAction(ev: EpgEventDetail): ActionDef | null {
  const now = Math.floor(Date.now() / 1000)
  const isLive =
    typeof ev.start === 'number' &&
    typeof ev.stop === 'number' &&
    ev.start <= now &&
    ev.stop > now
  if (isLive && typeof ev.channelUuid === 'string') {
    const channelUuid = ev.channelUuid
    const title = ev.title || ev.channelName || ''
    return {
      id: 'play',
      label: t('Play'),
      tooltip: t('Play the live channel'),
      children: [
        {
          id: 'play-browser',
          label: t('Play in browser'),
          tooltip: browserPlayTooltip(),
          disabled: !streamProfiles.canPlayInBrowser,
          onClick: () => {
            useVideoPlayer().open({ channelUuid, title })
          },
        },
        {
          id: 'play-external',
          label: t('Play in external player'),
          tooltip: t('Open the live channel in your external media player'),
          onClick: () => {
            openPlay(`stream/channel/${channelUuid}`)
          },
        },
        {
          id: 'play-external-profile',
          label: t('Play in external player with profile…'),
          tooltip: t('Choose a stream profile, then open in your external media player'),
          disabled: streamProfiles.profileNames.length === 0,
          onClick: () => {
            playProfileChannel.value = channelUuid
          },
        },
      ],
    }
  }
  if (ev.dvrUuid && ev.dvrState?.startsWith('completed')) {
    const dvrUuid = ev.dvrUuid
    return {
      id: 'play',
      label: t('Play'),
      tooltip: t('Open the recording in your external media player'),
      onClick: () => {
        openPlay(`dvrfile/${dvrUuid}`)
      },
    }
  }
  return null
}

const actions = computed<ActionDef[]>(() => {
  const ev = props.event
  if (!ev) return []
  const out: ActionDef[] = []
  const play = buildPlayAction(ev)
  if (play) out.push(play)
  /* Related / alternative showings — browse-only (anonymous-access
   * endpoints), so offered to every user regardless of DVR access. */
  out.push({
    id: 'showings',
    label: t('Other showings'),
    tooltip: t('Find related broadcasts and alternative showings of this programme'),
    children: [
      {
        id: 'epg-related',
        label: t('Related broadcasts'),
        tooltip: t('Show EPG events related to this programme'),
        onClick: () => openRelated('related'),
      },
      {
        id: 'epg-alternatives',
        label: t('Alternative showings'),
        tooltip: t('Show alternative airings of this programme'),
        onClick: () => openRelated('alternative'),
      },
    ],
  })
  /* DVR-specific actions need recorder access. */
  if (!access.data?.dvr) return out
  const recording = ev.dvrState?.startsWith('recording') ?? false
  const scheduled = ev.dvrState?.startsWith('scheduled') ?? false
  /* "View DVR entry" — non-destructive, leads the DVR group.
   * Whenever the event has an associated DVR entry, offer a path
   * to inspect/edit it without manually navigating to DVR
   * Upcoming/Finished/Failed. */
  if (ev.dvrUuid) {
    out.push({
      id: 'view',
      label: t('View DVR entry'),
      tooltip: t("Open this event's DVR entry"),
      onClick: viewDvrEntry,
    })
  }
  if (recording) {
    out.push({
      id: 'stop',
      label: inflight.value ? t('Stopping…') : t('Stop recording'),
      tooltip: t('Stop recording of this program'),
      disabled: inflight.value,
      onClick: stopRecording,
    })
  } else if (scheduled) {
    out.push({
      id: 'delete',
      label: inflight.value ? t('Deleting…') : t('Delete recording'),
      tooltip: t('Delete scheduled recording of this program'),
      disabled: inflight.value,
      onClick: deleteRecording,
    })
  } else {
    /* Record carries the DVR-profile picker as a leading control so
     * the configure-then-act pairing stays visually unambiguous —
     * matches the legacy ExtJS popup's profile-then-button layout.
     * The picker rides on the action itself so ActionMenu's
     * width-aware overflow keeps the picker glued to the button:
     * if Record overflows into the `…` menu, the picker goes with
     * it. Picker is skipped on dvrUuid-bearing events (View / Stop
     * / Delete branches above) where it would have no effect. */
    const profileOptions = dvrConfig.entries.map((cfg) => ({
      value: String(cfg.key),
      label: cfg.val,
    }))
    out.push({
      id: 'record',
      label: inflight.value ? t('Scheduling…') : t('Record'),
      tooltip: t('Record this program now'),
      disabled: inflight.value,
      onClick: recordEvent,
      leadingControl:
        profileOptions.length > 0
          ? {
              type: 'select',
              value: selectedConfigUuid.value,
              options: profileOptions,
              ariaLabel: t('DVR profile'),
              onChange: (v: string) => {
                selectedConfigUuid.value = v
              },
            }
          : undefined,
    })
  }
  /* AutoRec creation — always available (one of the two
   * labels). When the event has a serieslink (CRID-equivalent),
   * the button says "Record series" and the rule captures all
   * episodes server-side via the serieslink. Without it, label
   * says "Autorec" and the server falls back to a title-based
   * rule. Same handler / same endpoint either way; the label
   * flip is purely user-facing semantics. Mirrors Classic at
   * `static/app/epg.js:430-435`. */
  out.push({
    id: 'autorec',
    label: ev.serieslinkUri ? t('Record series') : t('Autorec'),
    tooltip: t(
      'Create an automatic recording rule to record all future programs that match the current query.',
    ),
    disabled: inflight.value,
    onClick: recordSeries,
  })
  return out
})

/* ---- Classification metadata ----
 *
 * Per-event classification block — genre / category / keyword /
 * parental rating / star rating / first-aired / copyright year. All
 * fields are optional and the group as a whole is hidden when none
 * are populated, so events with no metadata don't grow the drawer. */

const genreLabels = computed<string[]>(() => {
  const codes = props.event?.genre ?? []
  return codes.map((c) => contentTypes.labels.get(c) ?? `0x${c.toString(16)}`)
})

const firstAiredLabel = computed<string | null>(() => {
  const ts = props.event?.first_aired
  if (typeof ts !== 'number' || ts <= 0) return null
  return fmtDateOnly(ts)
})

const hasClassification = computed<boolean>(() => {
  const ev = props.event
  if (!ev) return false
  return Boolean(
    ev.ratingLabel ||
    ev.ratingLabelIcon ||
    ev.ageRating ||
    ev.starRating ||
    (ev.genre && ev.genre.length > 0) ||
    (ev.category && ev.category.length > 0) ||
    (ev.keyword && ev.keyword.length > 0) ||
    (typeof ev.first_aired === 'number' && ev.first_aired > 0) ||
    ev.copyright_year
  )
})

/* The server stores `ageRating` as the already-decoded minimum
 * recommended age in years (0–21 range, sanitised at the EIT
 * decoder in `src/epggrab/module/eit.c:480` which applies the
 * EIT `value + 3` transform server-side, and at the XMLTV
 * lookup in `src/epggrab/module/xmltv.c:541`). The client
 * renders verbatim — applying the +3 again would double-count.
 */
function formatAgeRating(code: number): string {
  return `${code} years`
}

/* Smart deduplication of the parental-rating fields (icon /
 * text label / numeric age) — see ./epgClassification.ts for
 * the full truth table and per-rule reasoning. */
const classification = computed<RatingDisplay>(() => {
  const ev = props.event
  if (!ev) return { showIcon: false, showLabel: false, showAge: false }
  return classifyRating(ev)
})

/* Cast / crew categorised into Starring / Director / Writer /
 * Crew per the legacy ExtJS layout. Null when no credits — the
 * Credits drawer group hides entirely in that case. */
const credits = computed<CreditsDisplay | null>(() =>
  categoriseCredits(props.event?.credits)
)
const hasCredits = computed<boolean>(() => credits.value !== null)

/* Programme image load-error gate. The XMLTV / EIT supplied URL
 * may 404, hot-link-block, or otherwise fail; rendering the
 * Image section anyway leaves a broken-icon + alt-text in place
 * which reads worse than no section at all. Reset on every event
 * change so a fresh URL gets a fresh load attempt. */
const imageFailed = ref(false)
watch(
  () => props.event?.eventId,
  () => {
    imageFailed.value = false
  },
)

function fmtDuration(start: number | undefined, stop: number | undefined): string {
  if (typeof start !== 'number' || typeof stop !== 'number') return ''
  const seconds = stop - start
  if (seconds <= 0) return ''
  const h = Math.floor(seconds / 3600)
  const m = Math.floor((seconds % 3600) / 60)
  if (h === 0) return `${m} min`
  if (m === 0) return `${h} h`
  return `${h} h ${m} min`
}

const channelLine = computed(() => {
  const e = props.event
  if (!e) return ''
  if (!e.channelName) return ''
  return typeof e.channelNumber === 'number'
    ? `${e.channelName} · ${e.channelNumber}`
    : e.channelName
})

const flags = computed(() => {
  const e = props.event
  if (!e) return [] as string[]
  const out: string[] = []
  if (e.hd) out.push(t('HD'))
  if (e.widescreen) out.push(t('16:9'))
  if (e.audiodesc) out.push(t('Audio described'))
  return out
})
</script>

<template>
  <!--
    Non-modal side drawer. The modal prop is off so clicks on the
    underlying grid (a table row, an event block on Timeline or
    Magazine) reach their own handlers and re-target the selected
    event for a clean content swap. Without this, PrimeVue's
    default backdrop mask would intercept the click and close the
    drawer instead of letting the new event load.

    The dismissable prop is also off so PrimeVue's document-level
    outside-click handler doesn't race the row-click and close the
    drawer mid-update. Close paths kept: the header's built-in
    close X (which still emits the visibility-update event
    naturally), Esc (wired explicitly below since dismissable also
    governed Esc), and the action buttons close on success.

    Trade-off: no click-outside-to-close. Acceptable on a side
    drawer where the X is always visible. IdnodeEditor (the DVR
    and Configuration editor drawer) keeps its modal default —
    that use case wants focused edit-one-record-at-a-time.
  -->
  <Drawer
    v-model:visible="visible"
    position="right"
    :modal="false"
    :dismissable="false"
    class="epg-event-drawer"
    :style="drawerWidth.widthStyle.value"
    :pt="{ root: { class: 'epg-event-drawer__root' } }"
  >
    <!--
      Resize handle on the inside-left edge of the drawer.
      Mouse drag and touch drag both supported via the shared
      composable; double-click resets the width to the default
      480 px. Tooltip on hover explains both gestures because
      the EPG drawer is read-only — no View options popover to
      surface a discoverable Reset action elsewhere.
    -->
    <div
      ref="resizeHandleEl"
      class="epg-event-drawer__resize-handle"
      role="separator"
      aria-orientation="vertical"
      :title="t('Drag to resize · double-click to reset')"
      @dblclick="onResizeHandleDblclick"
    />
    <!--
      Custom header — title on the main line, subtitle (when set)
      stacks directly below in smaller muted text. Most EPG sources
      treat subtitle as an episode-name-style secondary line, so
      rendering it adjacent to the title (rather than as a labelled
      body row) reads as the canonical event identity. Single-line
      clamp with ellipsis so a long subtitle doesn't push the close
      X off-screen; the event-block tooltip (gated by `ui_quicktips`)
      and the body Description still surface the full text.
    -->
    <template #header>
      <div class="epg-event-drawer__header">
        <span class="epg-event-drawer__title">
          <KodiText
            :text="event?.title ?? t('Event')"
            :enabled="!!access.data?.label_formatting"
          />
        </span>
        <span v-if="event?.subtitle" class="epg-event-drawer__subtitle">
          <KodiText :text="event.subtitle" :enabled="!!access.data?.label_formatting" />
        </span>
      </div>
    </template>
    <div v-if="event" class="epg-event-drawer__body">
      <!--
        Action row — Record / Stop / Delete, depending on the event's
        dvrState. Hidden when the user lacks recorder access OR when
        no actionable button applies (rare; would need ev.dvrState to
        be in a 'completed*' state where neither record nor cancel
        applies). `<ActionMenu>` is the same component DVR/Status
        toolbars use, so the look matches the rest of the app.
      -->
      <!--
        Single ActionMenu hosts every drawer action. The Record
        action carries the DVR-profile picker as a `leadingControl`
        so the picker + Record button render as one inline pair
        — and if the toolbar narrows enough to push Record into the
        `…` overflow, the picker travels with it (the pair is
        measured as one logical entry).
      -->
      <ActionMenu
        v-if="actions.length > 0"
        :actions="actions"
        class="epg-event-drawer__actions"
      />
      <!--
        Channel identity strip — sits above the field groups,
        outside the .ifld grid because the 40 px logo wouldn't align
        cleanly with the 1-line text rows.
      -->
      <div class="epg-event-drawer__channel">
        <ChannelLogo
          :src="iconUrl(event.channelIcon)"
          :name="event.channelName ?? ''"
          class="epg-event-drawer__channel-icon"
        />
        <div class="epg-event-drawer__channel-name">
          {{ channelLine }}
        </div>
      </div>

      <!--
        Event details group — collapsible <details>/<summary>
        matching the IdnodeEditor pattern. Open by default.
      -->
      <details class="epg-event-drawer__group" open>
        <summary class="epg-event-drawer__group-title">{{ t('Event') }}</summary>
        <div class="epg-event-drawer__group-body">
          <!--
            Subtitle is rendered above in the drawer header (next to
            the title in smaller text). Don't repeat it here.
          -->
          <div v-if="event.summary" class="ifld">
            <span class="ifld__label">{{ t('Summary') }}</span>
            <span class="ifld__value">
              <KodiText :text="event.summary" :enabled="!!access.data?.label_formatting" />
            </span>
          </div>
          <div class="ifld">
            <span class="ifld__label">{{ t('Start Time') }}</span>
            <span class="ifld__value">{{ fmtDate(event.start) }}</span>
          </div>
          <div class="ifld">
            <span class="ifld__label">{{ t('End Time') }}</span>
            <span class="ifld__value">{{ fmtDate(event.stop) }}</span>
          </div>
          <div class="ifld">
            <span class="ifld__label">{{ t('Duration') }}</span>
            <span class="ifld__value">{{ fmtDuration(event.start, event.stop) }}</span>
          </div>
          <div v-if="flags.length > 0" class="ifld">
            <span class="ifld__label">{{ t('Flags') }}</span>
            <span class="ifld__value">
              <span v-for="f in flags" :key="f" class="epg-event-drawer__badge">{{ f }}</span>
            </span>
          </div>
        </div>
      </details>

      <!--
        Description group — separate <details> so the user can
        collapse a long synopsis if they want. Free-form text
        rather than .ifld grid because descriptions are often
        multi-line and don't fit a 1-line label-value layout.
      -->
      <details
        v-if="event.description"
        class="epg-event-drawer__group epg-event-drawer__group--readonly"
        open
      >
        <summary class="epg-event-drawer__group-title">{{ t('Description') }}</summary>
        <div class="epg-event-drawer__group-body">
          <p class="epg-event-drawer__description">
            <KodiText :text="event.description" :enabled="!!access.data?.label_formatting" />
          </p>
        </div>
      </details>

      <!--
        Classification — parental rating, genre, categories, keywords,
        star rating, first aired, copyright year. Hidden in its
        entirety when no field is populated so events with no
        metadata don't grow the drawer.
      -->
      <details v-if="hasClassification" class="epg-event-drawer__group" open>
        <summary class="epg-event-drawer__group-title">{{ t('Classification') }}</summary>
        <div class="epg-event-drawer__group-body">
          <!-- Parental rating row(s) — see the `classification`
               computed in script-setup for the smart-dedup
               truth table that drives which of these render. -->
          <div v-if="classification.showIcon" class="ifld">
            <span class="ifld__label">{{ t('Parental rating') }}</span>
            <span class="ifld__value">
              <img
                :src="iconUrl(event.ratingLabelIcon) ?? event.ratingLabelIcon"
                :alt="event.ratingLabel ?? ''"
                class="epg-event-drawer__rating-icon"
              />
            </span>
          </div>
          <div v-if="classification.showLabel" class="ifld">
            <span class="ifld__label">{{ t('Parental rating') }}</span>
            <span class="ifld__value">{{ event.ratingLabel }}</span>
          </div>
          <div v-if="classification.showAge" class="ifld">
            <span class="ifld__label">{{ t('Age rating') }}</span>
            <span class="ifld__value">{{ formatAgeRating(event.ageRating!) }}</span>
          </div>
          <div v-if="event.starRating" class="ifld">
            <span class="ifld__label">{{ t('Star rating') }}</span>
            <span class="ifld__value">{{ event.starRating }} / 10 ★</span>
          </div>
          <div v-if="genreLabels.length" class="ifld">
            <span class="ifld__label">{{ t('Content type') }}</span>
            <span class="ifld__value">
              <span v-for="g in genreLabels" :key="g" class="epg-event-drawer__badge">
                {{ g }}
              </span>
            </span>
          </div>
          <div v-if="event.category?.length" class="ifld">
            <span class="ifld__label">{{ t('Categories') }}</span>
            <span class="ifld__value">
              <span v-for="c in event.category" :key="c" class="epg-event-drawer__badge">
                {{ c }}
              </span>
            </span>
          </div>
          <div v-if="event.keyword?.length" class="ifld">
            <span class="ifld__label">{{ t('Keywords') }}</span>
            <span class="ifld__value">
              <span v-for="k in event.keyword" :key="k" class="epg-event-drawer__badge">
                {{ k }}
              </span>
            </span>
          </div>
          <div v-if="firstAiredLabel" class="ifld">
            <span class="ifld__label">{{ t('First Aired') }}</span>
            <span class="ifld__value">{{ firstAiredLabel }}</span>
          </div>
          <div v-if="event.copyright_year" class="ifld">
            <span class="ifld__label">{{ t('Copyright') }}</span>
            <span class="ifld__value">{{ event.copyright_year }}</span>
          </div>
        </div>
      </details>

      <!--
        Credits — XMLTV cast / crew. Categorised into Starring /
        Director / Writer / Crew via `epgCredits.ts`, matching
        the legacy ExtJS layout (`tvheadend.js:339-377`). The
        whole group hides when there are no credits or every
        category is empty.
      -->
      <details v-if="hasCredits && credits" class="epg-event-drawer__group" open>
        <summary class="epg-event-drawer__group-title">{{ t('Credits') }}</summary>
        <div class="epg-event-drawer__group-body">
          <div v-if="credits.starring.length > 0" class="ifld">
            <span class="ifld__label">{{ t('Starring') }}</span>
            <span class="ifld__value">{{ credits.starring.join(', ') }}</span>
          </div>
          <div v-if="credits.director.length > 0" class="ifld">
            <span class="ifld__label">{{ t('Director') }}</span>
            <span class="ifld__value">{{ credits.director.join(', ') }}</span>
          </div>
          <div v-if="credits.writer.length > 0" class="ifld">
            <span class="ifld__label">{{ t('Writer') }}</span>
            <span class="ifld__value">{{ credits.writer.join(', ') }}</span>
          </div>
          <div v-if="credits.crew.length > 0" class="ifld">
            <span class="ifld__label">{{ t('Crew') }}</span>
            <span class="ifld__value">{{ credits.crew.join(', ') }}</span>
          </div>
        </div>
      </details>

      <!--
        Programme image — XMLTV-supplied poster / still. Pinned
        at the bottom of the drawer below the textual sections.
        Lazy-loaded so opening the drawer doesn't block on the
        imagecache fetch; max-height keeps the drawer scrollable
        when the source image is tall.
      -->
      <details
        v-if="event.image && !imageFailed"
        class="epg-event-drawer__group epg-event-drawer__group--readonly"
        open
      >
        <summary class="epg-event-drawer__group-title">{{ t('Image') }}</summary>
        <div class="epg-event-drawer__group-body">
          <img
            :src="iconUrl(event.image) ?? event.image"
            :alt="event.title ?? t('Programme poster')"
            class="epg-event-drawer__poster"
            loading="lazy"
            @error="imageFailed = true"
          />
        </div>
      </details>
    </div>
    <PlayProfileDialog
      :channel-uuid="playProfileChannel"
      @close="playProfileChannel = null"
    />
    <!--
      Related / alternative showings browser. `event-selected` (dialog
      row double-click) is intentionally not bound — the user is
      already in this event's drawer, and the related endpoints return
      a thinner event shape than the drawer renders. Record / Switch
      are the actionable affordances here.
    -->
    <EpgRelatedDialog
      v-if="event"
      v-model:visible="relatedVisible"
      :event-id="event.eventId"
      :mode="relatedMode"
      :show-switch="!!event.dvrUuid"
      @record="onRelatedRecord"
      @switch="onRelatedSwitch"
    />
  </Drawer>
</template>

<style scoped>
/*
 * Body / group / .ifld styling mirrors IdnodeEditor (see
 * src/components/IdnodeEditor.vue). Class names live in this
 * file's scope so we don't reach into IdnodeEditor's internal
 * conventions, but the values track the same tokens for visual
 * parity. If a third drawer-like surface lands, lift these into
 * a shared `drawer.css` partial.
 */

/*
 * Body wrapper. Padding + flex layout + overflow scroll all mirror
 * IdnodeEditor's `.idnode-editor__body` so both drawers behave the
 * same: header stays pinned at the top, body content scrolls
 * independently when it exceeds the viewport, padding is the same
 * 8 px on all four sides.
 *
 * Body padding 8 px (`--tvh-space-2`). The inner cards
 * (`.epg-event-drawer__group`) carry their own 12 px padding, so
 * total inset from drawer edge to field text is ~21 px — comfortable
 * without wasting horizontal space.
 *
 * `flex: 1 1 auto` + `min-height: 0` are the standard "let me grow,
 * let me shrink, let me have a min-height of 0 so flex-overflow works"
 * triple needed for `overflow-y: auto` to actually scroll inside a
 * flex-column parent — without min-height: 0 the body would expand
 * to fit content, no scroll trigger ever.
 *
 * Gap is 12 px (`--tvh-space-3`) rather than 16 px (`--tvh-space-4`)
 * because the EPG drawer's 4-5 sections (action menu / channel strip
 * / event group / description group) read tighter than the editor's
 * variable-length field stack; 12 px keeps them visually grouped
 * without excessive whitespace.
 */
.epg-event-drawer__body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2);
  flex: 1 1 auto;
  min-height: 0;
  overflow-y: auto;
}

/*
 * Header slot — title on top, subtitle (when set) directly beneath in
 * smaller muted text. Stacked vertically with a tight 2 px gap so the
 * pair reads as a single unit. `flex: 1` claims the space PrimeVue's
 * `.p-drawer-header` flex row gives us before its built-in close-X
 * button. `min-width: 0` is the standard shrink-allowing trick for
 * grid/flex children whose ellipsis-clamped descendants would
 * otherwise demand their natural width.
 */
.epg-event-drawer__header {
  display: flex;
  flex-direction: column;
  gap: 2px;
  flex: 1;
  min-width: 0;
}

.epg-event-drawer__title {
  font-size: var(--tvh-text-2xl);
  font-weight: 600;
  color: var(--tvh-text);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.epg-event-drawer__subtitle {
  font-size: var(--tvh-text-md);
  font-weight: 400;
  color: var(--tvh-text-muted);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/*
 * `<ActionMenu>` carries `flex: 1 1 auto` on its own root (so it
 * fills horizontal space in a toolbar — its primary use). Inside
 * this drawer the parent is a flex *column*, so the same rule made
 * the action row balloon vertically until it consumed all unused
 * height — buttons stayed 32 px tall but the bar around them grew.
 * Constrain to content height in this context. The `!important` is
 * the explicit signal that we're intentionally overriding the
 * component's default; without it specificity ties between the two
 * scoped rules and source order decides the winner.
 */
.epg-event-drawer__actions {
  flex: 0 0 auto !important;
}

/* Channel identity strip. */
.epg-event-drawer__channel {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-primary) 6%, transparent);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.epg-event-drawer__channel-icon {
  width: 40px;
  height: 40px;
  object-fit: contain;
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
  flex: 0 0 auto;
}

.epg-event-drawer__channel-name {
  font-weight: 600;
  color: var(--tvh-text);
  font-size: var(--tvh-text-lg);
}

/* Group card — matches IdnodeEditor's group shape. */
.epg-event-drawer__group {
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  background: var(--tvh-bg-page);
}

.epg-event-drawer__group--readonly {
  background: color-mix(in srgb, var(--tvh-text-muted) 6%, transparent);
}

.epg-event-drawer__group-title {
  font-size: var(--tvh-text-md);
  font-weight: 600;
  color: var(--tvh-text);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  cursor: pointer;
  user-select: none;
  list-style: revert; /* native disclosure triangle */
}

.epg-event-drawer__group-title:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.epg-event-drawer__group-title:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
  border-radius: var(--tvh-radius-md);
}

.epg-event-drawer__group[open] > .epg-event-drawer__group-title {
  border-bottom: 1px solid var(--tvh-border);
}

.epg-event-drawer__group-body {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-3);
}

/*
 * Single-line label-left / value-right `.ifld` row layout —
 * matches IdnodeEditor's drawer convention (180 px label column +
 * 1 fr value, gap = --tvh-space-3, phone fallback to stacked at
 * 767 px). Defined here as a regular selector (not :deep) because
 * the .ifld elements live in this component's scope, not nested
 * field components like in the editor.
 */
.epg-event-drawer__group-body .ifld {
  display: grid;
  grid-template-columns: 180px 1fr;
  align-items: center;
  gap: var(--tvh-space-3);
}

.epg-event-drawer__group-body .ifld__label {
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
}

.epg-event-drawer__group-body .ifld__value {
  font-size: var(--tvh-text-md);
  color: var(--tvh-text);
  min-width: 0;
}

/*
 * Programme poster — sized to fit within the drawer body without
 * pushing the rest of the content off-screen. `max-height` is
 * the load-bearing constraint: tall posters (e.g. 9:16 mobile-
 * sourced art) get clamped instead of scrolling the user past
 * every other group.
 */
.epg-event-drawer__poster {
  display: block;
  max-width: 100%;
  max-height: 280px;
  height: auto;
  margin: 0 auto;
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-page);
}

/* Drag handle for resizing the drawer width. Pinned to the
 * inside-left edge via `:deep()` since the handle is rendered
 * inside PrimeVue's Drawer root (teleported to <body>). Mouse +
 * touch drag both handled by the composable. Double-click =
 * reset to default (480 px); tooltip on hover explains both
 * gestures because this drawer is read-only and has no other
 * Reset affordance to discover the reset path through. Hidden
 * on phone — the drawer fills the viewport width there, so
 * there's no resize axis to expose. */
:deep(.epg-event-drawer__root) {
  position: relative;
}

.epg-event-drawer__resize-handle {
  position: absolute;
  inset-block: 0;
  inset-inline-start: 0;
  width: 6px;
  cursor: col-resize;
  z-index: 10;
  background: transparent;
  transition: background 120ms ease;
  touch-action: none;
}

.epg-event-drawer__resize-handle:hover {
  background: color-mix(in srgb, var(--tvh-primary) 30%, transparent);
}

@media (max-width: 767px) {
  .epg-event-drawer__resize-handle {
    display: none;
  }
}

@media (max-width: 767px) {
  .epg-event-drawer__group-body .ifld {
    grid-template-columns: 1fr;
  }
}

/* Flag badge — small inline chip inside the Flags row's value cell.
 * Reused by the Classification group for genre / category / keyword
 * pills (visually identical to the existing flag chips). */
.epg-event-drawer__badge {
  display: inline-block;
  padding: 1px 6px;
  margin-right: 4px;
  background: color-mix(in srgb, var(--tvh-primary) 12%, var(--tvh-bg-surface));
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text);
}

/* Parental-rating icon — small inline glyph from the server's
 * imagecache (e.g. an MPAA / FSK / VCHIP badge), rendered before
 * the localised label. */
.epg-event-drawer__rating-icon {
  height: 24px;
  width: auto;
  vertical-align: middle;
  margin-right: var(--tvh-space-2);
}

/* Description body — free-form text inside the Description group. */
.epg-event-drawer__description {
  margin: 0;
  white-space: pre-wrap;
  line-height: 1.5;
  color: var(--tvh-text);
  font-size: var(--tvh-text-md);
}
</style>

<style>
/*
 * Unscoped — same pattern as IdnodeEditor's drawer width override.
 * PrimeVue's Drawer root is teleported to <body>; scoped styles
 * don't reach it.
 */
.epg-event-drawer__root.p-drawer {
  width: 480px;
  max-width: 100%;
  /* Pin the teleported drawer surface to the exact theme tokens.
   * Aura's dark palette now activates under Access (main.ts
   * darkModeSelector), but pinning here keeps the drawer surface
   * identical to the app's --tvh-bg-surface rather than Aura's own
   * dark shade. Mirrors IdnodeEditor's `.idnode-editor__root.p-drawer`. */
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
}

/*
 * Mirror IdnodeEditor's drawer-shell setup: header sits at the top
 * with its own padding + bottom border; `.p-drawer-content` becomes
 * a flex column so the body wrapper can claim the remaining height
 * and scroll inside (see `.epg-event-drawer__body` in the scoped
 * block above for the body half of the contract).
 */
.epg-event-drawer__root .p-drawer-header {
  padding: var(--tvh-space-3) var(--tvh-space-4);
  border-bottom: 1px solid var(--tvh-border);
}

.epg-event-drawer__root .p-drawer-content {
  /*
   * `padding: 0` overrides PrimeVue Aura's default
   *   padding: 0 {overlay.modal.padding} {overlay.modal.padding} {overlay.modal.padding}
   * (`0 20px 20px 20px`). Same rationale as IdnodeEditor — that
   * 20 px PrimeVue layer otherwise masks the inner
   * `.epg-event-drawer__body` padding; zeroing it makes the body's
   * 8 px the sole drawer-edge → content inset.
   */
  padding: 0;
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}

@media (max-width: 767px) {
  .epg-event-drawer__root.p-drawer {
    width: 100%;
  }
}
</style>
