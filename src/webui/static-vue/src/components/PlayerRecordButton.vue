<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * PlayerRecordButton — Record split button in the in-browser
 * player's toolbar (issue #2291).
 *
 * The main part records the programme on air on the player's
 * channel. It looks up the channel's current EPG event
 * (`epg/events/grid` with mode=now) and schedules it through
 * `dvr/entry/create_by_event`, the call the EPG drawer's Record
 * makes, so the recording ends with the programme. When the channel
 * has no event on air there is no end to record to, so it opens
 * RecordLengthDialog instead, which asks for a length and creates a
 * plain DVR entry from now (`dvr/entry/create`). The arrow offers
 * both choices explicitly.
 *
 * While the channel is being recorded the split button gives way to
 * Stop recording. It stops every recording of the channel the user
 * can see (`dvr/entry/stop`, behind a danger confirm like the
 * drawer's Stop), so a recording started elsewhere (EPG, autorec)
 * shows here too. The state comes from the shared dvrEntries store,
 * which Comet keeps fresh.
 *
 * Shown only with DVR access, like the drawer's DVR actions: the
 * DVR endpoints need ACCESS_RECORDER. There is no profile picker, so
 * recordings use the user's default DVR profile: an empty profile
 * lets the server pick it, as the command palette's Record does.
 */
import { computed, ref, watch } from 'vue'
import Button from 'primevue/button'
import SplitButton from 'primevue/splitbutton'
import type { MenuItem } from 'primevue/menuitem'
import { Circle, Square } from 'lucide-vue-next'
import RecordLengthDialog from '@/components/RecordLengthDialog.vue'
import { apiCall } from '@/api/client'
import { ApiError } from '@/api/errors'
import { useAccessStore } from '@/stores/access'
import { useDvrEntriesStore } from '@/stores/dvrEntries'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useToastNotify } from '@/composables/useToastNotify'
import { useI18n } from '@/composables/useI18n'
import { useNowCursor } from '@/composables/useNowCursor'

const props = defineProps<{
  /* The channel in the player, '' while none is selected. */
  channelUuid: string
  /* Its name, the title of a fixed-length recording. */
  channelName?: string
}>()

const { t } = useI18n()
const access = useAccessStore()
const dvrEntries = useDvrEntriesStore()
const confirmDialog = useConfirmDialog()
const toast = useToastNotify()

const shown = computed(() => access.has('dvr') && props.channelUuid !== '')

/* `dvr/entry/grid_upcoming` needs ACCESS_RECORDER, so it is fetched
 * only once the button shows (see RecordingNow.vue). From then on
 * the store refreshes itself on Comet `dvrentry` notifications. */
watch(
  shown,
  (s) => {
    if (s) void dvrEntries.ensure()
  },
  { immediate: true },
)

/* Ticks every 30 s, so Record comes back when a programme ends. */
const { now } = useNowCursor()

/* Recordings of this channel in progress, `recordingError` included:
 * it is still writing to disk. Only within the entry's own start and
 * stop: the server reports it as recording from its warm-up and
 * pre-padding until its post-padding ends, and in those margins the
 * programme on air is another one. */
const recordings = computed(() =>
  dvrEntries.entries.filter(
    (e) =>
      e.channel === props.channelUuid &&
      e.sched_status.startsWith('recording') &&
      e.start <= now.value &&
      e.stop > now.value,
  ),
)

/* Fetch the entries again after a change, so Record and Stop follow.
 * The clock is set first: a fixed-length entry starts this second,
 * which the 30 s tick may not have reached yet. */
async function refreshEntries(): Promise<void> {
  now.value = Math.floor(Date.now() / 1000)
  await dvrEntries.refresh()
}

const busy = ref(false)
/* Set by an action that takes focus away from this control, see the
 * watchers at the end. */
const focusBack = ref(false)
const lengthDialogOpen = ref(false)
/* The length dialog says why it opened when Record found no EPG. */
const lengthDialogNoEpg = ref(false)

function clock(epoch: number | undefined): string {
  if (typeof epoch !== 'number' || epoch <= 0) return ''
  return new Date(epoch * 1000).toLocaleTimeString(undefined, {
    hour: '2-digit',
    minute: '2-digit',
  })
}

function errorText(e: unknown): string {
  return e instanceof ApiError || e instanceof Error ? e.message : String(e)
}

/* i18n: new string */
function recordingText(title: string, stop: number | undefined): string {
  return t('Recording "{0}" until {1}', title, clock(stop))
}

const stopTooltip = computed(() =>
  recordings.value.map((e) => recordingText(e.disp_title, e.stop)).join('\n'),
)

/* `create_by_event` answers `{ uuid: [..] }`, `dvr/entry/create`
 * answers `{ uuid: ".." }`, and both answer `{}` when the server did
 * not create an entry (for example an event no longer in the EPG). */
interface CreateResponse {
  uuid?: string | string[]
}

function assertCreated(resp: CreateResponse): void {
  const uuids = Array.isArray(resp.uuid) ? resp.uuid : [resp.uuid]
  /* i18n: new string */
  if (!uuids.some((u) => typeof u === 'string' && u !== '')) {
    throw new Error(t('No recording was created.'))
  }
}

interface NowEvent {
  eventId: number
  channelUuid?: string
  title?: string
  stop?: number
}

function openLengthDialog(noEpg: boolean): void {
  lengthDialogNoEpg.value = noEpg
  lengthDialogOpen.value = true
}

/* Record the programme on air now, or ask for a length when the
 * channel has no EPG event now. */
async function recordCurrent(): Promise<void> {
  if (busy.value) return
  const channel = props.channelUuid
  focusBack.value = true
  busy.value = true
  try {
    const resp = await apiCall<{ entries?: NowEvent[] }>('epg/events/grid', {
      mode: 'now',
      channel,
      limit: 1,
    })
    /* The channel is checked: for a channel it does not know (deleted
     * meanwhile) the server answers with every channel's events. */
    const ev = resp.entries?.find((e) => e.channelUuid === channel)
    if (!ev) {
      openLengthDialog(true)
      return
    }
    const created = await apiCall<CreateResponse>('dvr/entry/create_by_event', {
      event_id: ev.eventId,
      config_uuid: '',
    })
    assertCreated(created)
    toast.success(recordingText(ev.title || props.channelName || '', ev.stop))
    await refreshEntries()
  } catch (e) {
    toast.error(`${t('Failed to schedule recording')}: ${errorText(e)}`)
  } finally {
    busy.value = false
  }
}

/* "<channel> YYYY-MM-DD HH:MM" in local time — the server's own
 * title for a timer recording is Time-%F_%R. */
function fixedTitle(start: number): string {
  const d = new Date(start * 1000)
  const p2 = (n: number) => String(n).padStart(2, '0')
  const date = `${d.getFullYear()}-${p2(d.getMonth() + 1)}-${p2(d.getDate())}`
  const name = props.channelName || t('Recording')
  return `${name} ${date} ${p2(d.getHours())}:${p2(d.getMinutes())}`
}

/* Record a fixed length from now. */
async function recordFixed(minutes: number): Promise<void> {
  lengthDialogOpen.value = false
  if (busy.value) return
  busy.value = true
  try {
    const start = Math.floor(Date.now() / 1000)
    const stop = start + minutes * 60
    const title = fixedTitle(start)
    const created = await apiCall<CreateResponse>('dvr/entry/create', {
      conf: JSON.stringify({
        enabled: true,
        start,
        stop,
        channel: props.channelUuid,
        disp_title: title,
        config_name: '',
      }),
    })
    assertCreated(created)
    toast.success(recordingText(title, stop))
    await refreshEntries()
  } catch (e) {
    toast.error(`${t('Failed to schedule recording')}: ${errorText(e)}`)
  } finally {
    busy.value = false
  }
}

async function stopRecording(): Promise<void> {
  const entries = recordings.value
  if (busy.value || entries.length === 0) return
  /* i18n: new string. Names the recording: in the player it may be
   * one the user did not start (an autorec, another user's entry). */
  const ok = await confirmDialog.ask(
    t('Stop recording "{0}"?', entries.map((e) => e.disp_title).join('", "')),
    { severity: 'danger' },
  )
  if (!ok) return
  focusBack.value = true
  busy.value = true
  try {
    /* `uuid` is a JSON array, as for every idnode-handler route. */
    await apiCall('dvr/entry/stop', { uuid: JSON.stringify(entries.map((e) => e.uuid)) })
    await refreshEntries()
  } catch (e) {
    toast.error(`${t('Failed to stop recording')}: ${errorText(e)}`)
  } finally {
    busy.value = false
  }
}

/* i18n: new strings */
const menuItems = computed<MenuItem[]>(() => [
  { label: t('Record current programme'), command: () => void recordCurrent() },
  { label: t('Record fixed length…'), command: () => openLengthDialog(false) },
])

/* The arrow has only an icon, so it needs a name. */
/* i18n: new string */
const menuButtonProps = computed(() => ({ 'aria-label': t('Recording options') }))

const rootEl = ref<HTMLElement | null>(null)

/* PrimeVue's menu closes on Escape but lets the key bubble on to the
 * document, where the player Dialog's listener would close the player
 * too, so stop it at the menu's root (the menu is teleported to
 * <body>). The menu also hands focus back to the split button's
 * container, which cannot take it, so put it on the arrow. */
const splitPt = {
  pcMenu: {
    root: {
      onKeydown: (ev: KeyboardEvent) => {
        if (ev.key !== 'Escape') return
        ev.stopPropagation()
        rootEl.value?.querySelector<HTMLElement>('.p-splitbutton-dropdown')?.focus()
      },
    },
  },
}

/* After an action focus goes back to Record or Stop, whichever shows.
 * The browser would drop it to the page: the busy button is disabled,
 * Record and Stop replace each other, and the length dialog opens from
 * a menu item that is gone when it closes. */
const showStop = computed(() => recordings.value.length > 0)

/* Before Record and Stop swap: keep focus if the old one had it. */
watch(showStop, () => {
  if (rootEl.value?.contains(document.activeElement)) focusBack.value = true
})

watch(lengthDialogOpen, (open) => {
  if (!open) focusBack.value = true
})

watch(
  [focusBack, busy, lengthDialogOpen, showStop],
  () => {
    if (!focusBack.value || busy.value || lengthDialogOpen.value) return
    focusBack.value = false
    /* Not when the user has moved on to something else meanwhile.
     * A toast does not count: its close button has autofocus, so the
     * browser puts focus that fell to the page there, and it is gone
     * again with the toast a few seconds later. */
    const active = document.activeElement
    const lost =
      !active ||
      active === document.body ||
      rootEl.value?.contains(active) ||
      active.closest('.record-length-dialog, .p-toast') !== null
    if (!lost) return
    rootEl.value?.querySelector<HTMLElement>('.player-record__stop, .p-splitbutton-button')?.focus()
  },
  { flush: 'post' },
)
</script>

<template>
  <div v-if="shown" ref="rootEl" class="player-record">
    <Button
      v-if="showStop"
      v-tooltip.bottom="stopTooltip"
      severity="danger"
      outlined
      :disabled="busy"
      class="player-record__stop"
      @click="stopRecording"
    >
      <Square :size="12" :stroke-width="2" fill="currentColor" aria-hidden="true" />
      {{ busy ? t('Stopping…') : t('Stop recording') }}
    </Button>
    <SplitButton
      v-else
      :label="busy ? t('Scheduling…') : t('Record')"
      :model="menuItems"
      :disabled="busy"
      severity="secondary"
      outlined
      :menu-button-props="menuButtonProps"
      :pt="splitPt"
      @click="recordCurrent"
    >
      <template #icon>
        <Circle
          :size="12"
          :stroke-width="2"
          fill="currentColor"
          class="player-record__dot"
          aria-hidden="true"
        />
      </template>
    </SplitButton>
    <RecordLengthDialog
      :visible="lengthDialogOpen"
      :no-epg="lengthDialogNoEpg"
      @close="lengthDialogOpen = false"
      @record="recordFixed"
    />
  </div>
</template>

<style scoped>
/* Pushed to the end of the player toolbar, which wraps it onto its
 * own line when the row is full. */
.player-record {
  display: inline-flex;
  flex: none;
  margin-inline-start: auto;
}

/* The record glyph is red, as in the EPG's recording marks. */
.player-record__dot {
  color: var(--tvh-error);
}
</style>
