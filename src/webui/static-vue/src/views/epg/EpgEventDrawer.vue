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
import KodiText from '@/components/KodiText.vue'
import { apiCall } from '@/api/client'
import { ApiError } from '@/api/errors'
import { useAccessStore } from '@/stores/access'
import { useDvrConfigStore } from '@/stores/dvrConfig'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'
import { fmtTimeRange, iconUrl } from './epgGridShared'
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

watch(
  () => props.event,
  (ev) => {
    selectedConfigUuid.value = ''
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
onBeforeUnmount(() => document.removeEventListener('keydown', onKeyDown))

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
    'Failed to schedule recording'
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
    'Do you really want to gracefully stop/unschedule this recording?',
    'Failed to stop recording'
  )
}

function deleteRecording() {
  if (!props.event?.dvrUuid) return
  runAction(
    'idnode/delete',
    { uuid: JSON.stringify([props.event.dvrUuid]) },
    'Do you really want to remove this recording?',
    'Failed to delete recording'
  )
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

/* True when the action area should render the profile dropdown
 * alongside the Record button — i.e. user has DVR access AND the
 * event has no associated DVR entry yet. Once a DVR entry exists
 * (any state), the action area carries View / Stop / Delete; the
 * profile picker has no meaning there and is skipped, matching
 * the legacy popup. */
const showRecordRow = computed(() => {
  const ev = props.event
  if (!ev || !access.data?.dvr) return false
  return !ev.dvrUuid
})

const actions = computed<ActionDef[]>(() => {
  const ev = props.event
  if (!ev || !access.data?.dvr) return []
  const recording = ev.dvrState?.startsWith('recording') ?? false
  const scheduled = ev.dvrState?.startsWith('scheduled') ?? false
  const out: ActionDef[] = []
  /* "View DVR entry" — non-destructive, leads. Whenever the event
   * has an associated DVR entry, offer a path to inspect/edit it
   * without manually navigating to DVR Upcoming/Finished/Failed. */
  if (ev.dvrUuid) {
    out.push({
      id: 'view',
      label: 'View DVR entry',
      tooltip: "Open this event's DVR entry",
      onClick: viewDvrEntry,
    })
  }
  if (recording) {
    out.push({
      id: 'stop',
      label: inflight.value ? 'Stopping…' : 'Stop recording',
      tooltip: 'Stop recording of this program',
      disabled: inflight.value,
      onClick: stopRecording,
    })
  } else if (scheduled) {
    out.push({
      id: 'delete',
      label: inflight.value ? 'Deleting…' : 'Delete recording',
      tooltip: 'Delete scheduled recording of this program',
      disabled: inflight.value,
      onClick: deleteRecording,
    })
  } else {
    out.push({
      id: 'record',
      label: inflight.value ? 'Scheduling…' : 'Record',
      tooltip: 'Record this program now',
      disabled: inflight.value,
      onClick: recordEvent,
    })
  }
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
  return new Intl.DateTimeFormat(undefined, { dateStyle: 'medium' }).format(new Date(ts * 1000))
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

function fmtDate(epoch: number | undefined): string {
  if (typeof epoch !== 'number') return ''
  return new Date(epoch * 1000).toLocaleDateString(undefined, {
    weekday: 'short',
    day: 'numeric',
    month: 'short',
    year: 'numeric',
  })
}

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
  if (e.hd) out.push('HD')
  if (e.widescreen) out.push('16:9')
  if (e.audiodesc) out.push('Audio described')
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
    :pt="{ root: { class: 'epg-event-drawer__root' } }"
  >
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
            :text="event?.title ?? 'Event'"
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
      <div v-if="showRecordRow && actions.length > 0" class="epg-event-drawer__record-row">
        <select
          v-model="selectedConfigUuid"
          class="epg-event-drawer__profile-select"
          aria-label="DVR profile"
        >
          <option v-for="cfg in dvrConfig.entries" :key="cfg.key" :value="cfg.key">
            {{ cfg.val }}
          </option>
        </select>
        <ActionMenu :actions="actions" class="epg-event-drawer__actions" />
      </div>
      <ActionMenu
        v-else-if="actions.length > 0"
        :actions="actions"
        class="epg-event-drawer__actions"
      />
      <!--
        Channel identity strip — sits above the field groups,
        outside the .ifld grid because the 40 px logo wouldn't align
        cleanly with the 1-line text rows.
      -->
      <div class="epg-event-drawer__channel">
        <img
          v-if="iconUrl(event.channelIcon)"
          :src="iconUrl(event.channelIcon) ?? ''"
          :alt="`${event.channelName ?? ''} icon`"
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
        <summary class="epg-event-drawer__group-title">Event</summary>
        <div class="epg-event-drawer__group-body">
          <!--
            Subtitle is rendered above in the drawer header (next to
            the title in smaller text). Don't repeat it here.
          -->
          <div v-if="event.summary" class="ifld">
            <span class="ifld__label">Summary</span>
            <span class="ifld__value">
              <KodiText :text="event.summary" :enabled="!!access.data?.label_formatting" />
            </span>
          </div>
          <div class="ifld">
            <span class="ifld__label">Date</span>
            <span class="ifld__value">{{ fmtDate(event.start) }}</span>
          </div>
          <div class="ifld">
            <span class="ifld__label">Time</span>
            <span class="ifld__value">{{ fmtTimeRange(event.start, event.stop) }}</span>
          </div>
          <div class="ifld">
            <span class="ifld__label">Duration</span>
            <span class="ifld__value">{{ fmtDuration(event.start, event.stop) }}</span>
          </div>
          <div v-if="flags.length > 0" class="ifld">
            <span class="ifld__label">Flags</span>
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
        <summary class="epg-event-drawer__group-title">Description</summary>
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
        <summary class="epg-event-drawer__group-title">Classification</summary>
        <div class="epg-event-drawer__group-body">
          <!-- Parental rating row(s) — see the `classification`
               computed in script-setup for the smart-dedup
               truth table that drives which of these render. -->
          <div v-if="classification.showIcon" class="ifld">
            <span class="ifld__label">Parental rating</span>
            <span class="ifld__value">
              <img
                :src="iconUrl(event.ratingLabelIcon) ?? event.ratingLabelIcon"
                :alt="event.ratingLabel ?? ''"
                class="epg-event-drawer__rating-icon"
              />
            </span>
          </div>
          <div v-if="classification.showLabel" class="ifld">
            <span class="ifld__label">Parental rating</span>
            <span class="ifld__value">{{ event.ratingLabel }}</span>
          </div>
          <div v-if="classification.showAge" class="ifld">
            <span class="ifld__label">Age rating</span>
            <span class="ifld__value">{{ formatAgeRating(event.ageRating!) }}</span>
          </div>
          <div v-if="event.starRating" class="ifld">
            <span class="ifld__label">Star rating</span>
            <span class="ifld__value">{{ event.starRating }} / 10 ★</span>
          </div>
          <div v-if="genreLabels.length" class="ifld">
            <span class="ifld__label">Genre</span>
            <span class="ifld__value">
              <span v-for="g in genreLabels" :key="g" class="epg-event-drawer__badge">
                {{ g }}
              </span>
            </span>
          </div>
          <div v-if="event.category?.length" class="ifld">
            <span class="ifld__label">Categories</span>
            <span class="ifld__value">
              <span v-for="c in event.category" :key="c" class="epg-event-drawer__badge">
                {{ c }}
              </span>
            </span>
          </div>
          <div v-if="event.keyword?.length" class="ifld">
            <span class="ifld__label">Keywords</span>
            <span class="ifld__value">
              <span v-for="k in event.keyword" :key="k" class="epg-event-drawer__badge">
                {{ k }}
              </span>
            </span>
          </div>
          <div v-if="firstAiredLabel" class="ifld">
            <span class="ifld__label">First aired</span>
            <span class="ifld__value">{{ firstAiredLabel }}</span>
          </div>
          <div v-if="event.copyright_year" class="ifld">
            <span class="ifld__label">Copyright</span>
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
        <summary class="epg-event-drawer__group-title">Credits</summary>
        <div class="epg-event-drawer__group-body">
          <div v-if="credits.starring.length > 0" class="ifld">
            <span class="ifld__label">Starring</span>
            <span class="ifld__value">{{ credits.starring.join(', ') }}</span>
          </div>
          <div v-if="credits.director.length > 0" class="ifld">
            <span class="ifld__label">Director</span>
            <span class="ifld__value">{{ credits.director.join(', ') }}</span>
          </div>
          <div v-if="credits.writer.length > 0" class="ifld">
            <span class="ifld__label">Writer</span>
            <span class="ifld__value">{{ credits.writer.join(', ') }}</span>
          </div>
          <div v-if="credits.crew.length > 0" class="ifld">
            <span class="ifld__label">Crew</span>
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
        v-if="event.image"
        class="epg-event-drawer__group epg-event-drawer__group--readonly"
        open
      >
        <summary class="epg-event-drawer__group-title">Image</summary>
        <div class="epg-event-drawer__group-body">
          <img
            :src="iconUrl(event.image) ?? event.image"
            :alt="event.title ?? 'Programme image'"
            class="epg-event-drawer__poster"
            loading="lazy"
          />
        </div>
      </details>
    </div>
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
  font-size: 18px;
  font-weight: 600;
  color: var(--tvh-text);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.epg-event-drawer__subtitle {
  font-size: 13px;
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

/* Record-action row: profile dropdown left of the Record button.
 * Shown only when no DVR entry exists yet — Stop / Delete states
 * render the bare ActionMenu, no dropdown. */
.epg-event-drawer__record-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex-wrap: wrap;
}

.epg-event-drawer__profile-select {
  flex: 0 0 auto;
  min-width: 200px;
  height: 32px;
  padding: 0 var(--tvh-space-2);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  font: inherit;
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
  font-size: 14px;
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
  font-size: 13px;
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
  font-size: 13px;
  color: var(--tvh-text-muted);
}

.epg-event-drawer__group-body .ifld__value {
  font-size: 13px;
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
  font-size: 11px;
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
  font-size: 13px;
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
}

/*
 * Mirror IdnodeEditor's drawer-shell setup: header sits at the top
 * with its own padding + bottom border; `.p-drawer-content` becomes
 * a flex column so the body wrapper can claim the remaining height
 * and scroll inside (see `.epg-event-drawer__body` in the scoped
 * block above for the body half of the contract).
 *
 * Earlier draft put padding directly on `.p-drawer-content` —
 * besides indenting the header (PrimeVue places the header inside
 * `.p-drawer-content` as the first child), it also referenced an
 * undefined `--tvh-space-5` token whose 0-fallback wiped horizontal
 * breathing room entirely. Both fixed by moving padding to the
 * inner body wrapper, matching the editor.
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
