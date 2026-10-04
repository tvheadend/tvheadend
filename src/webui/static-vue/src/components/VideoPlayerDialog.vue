<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * VideoPlayerDialog — in-browser live-channel player.
 *
 * A PrimeVue <Dialog> wrapping a native HTML5 <video> element.
 * Mounted once under AppShell (same singleton pattern as
 * HelpDialog); visibility is two-way bound to useVideoPlayer().
 *
 * A profile dropdown lets the user switch stream profile live:
 * changing it re-points the <video> at the new `?profile=` URL and
 * reloads, so the stream stops and restarts on the chosen profile.
 * The dropdown offers every stream profile (see the streamProfiles
 * store); if the chosen one cannot be decoded the element fires a
 * media error and the overlay below surfaces it.
 *
 * An error before the first `progress` event means the stream never
 * got going, and the player cannot tell whether the server did not
 * start it or the browser cannot play it (see `sawProgress`). The
 * overlay then names both and does not flag the profile.
 *
 * Teardown: on close (and implicitly on every profile switch via
 * load()) the <video> is paused, its src cleared and load() called.
 * A live stream holds a server-side subscription open for as long as
 * the HTTP connection lasts; dropping the connection deterministically
 * releases the subscription without waiting for element GC.
 */
import { computed, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import Select from 'primevue/select'
import { TriangleAlert } from 'lucide-vue-next'
import { useI18n } from '@/composables/useI18n'
import { useVideoPlayer } from '@/composables/useVideoPlayer'
import { useStreamProfilesStore } from '@/stores/streamProfiles'
import { useAccessStore } from '@/stores/access'
import { channelStreamUrl } from '@/utils/playUrl'
import { apiCall } from '@/api/client'
import { GRID_LIMIT_ALL } from '@/api/gridConstants'
import type { GridResponse, FilterDef } from '@/types/grid'

const { t } = useI18n()
const player = useVideoPlayer()
const streamProfiles = useStreamProfilesStore()
const access = useAccessStore()

/* localStorage key for the last in-browser profile the user chose. */
const LAST_PROFILE_KEY = 'tvh:browser-play-profile'

/* Enabled channels for the Channel dropdown. Fetched once (the dialog
 * is a mounted singleton, so this survives across opens) and shared by
 * every entry point into the player. */
interface PlayerChannel {
  uuid: string
  name?: string
  /* Integer LCN, or a string for ATSC fractional numbers ("10.1"). */
  number?: number | string
  /* Precomputed "number · name" so the Select's filter matches both. */
  label: string
}
const channels = ref<PlayerChannel[]>([])
let channelsLoaded = false

async function loadChannels(): Promise<void> {
  if (channelsLoaded) return
  try {
    const resp = await apiCall<GridResponse<PlayerChannel>>('channel/grid', {
      start: 0,
      filter: JSON.stringify([
        { field: 'enabled', type: 'boolean', value: true } satisfies FilterDef,
      ]),
      limit: GRID_LIMIT_ALL,
      sort: 'number',
      dir: 'ASC',
    })
    channels.value = (resp.entries ?? []).map((c) => {
      /* number is an int, or an ATSC fractional string ("10.1"); ?? ''
       * guards absent/empty so the label never shows "undefined". */
      const numStr = String(c.number ?? '')
      return {
        ...c,
        label: numStr ? `${numStr} · ${c.name ?? c.uuid}` : (c.name ?? c.uuid),
      }
    })
    channelsLoaded = true
  } catch (e) {
    /* Non-fatal — the dropdown just stays empty; a channel passed in
     * via open() still plays. Logged for visibility. */
    console.error('VideoPlayerDialog: channel load failed:', e)
  }
}

/* The selected channel, bound to the Channel dropdown. Reading/writing
 * goes through the composable's `current` so switching channels here
 * re-points the <video> the same way the profile switch does. */
const selectedChannel = computed<string>({
  get: () => player.current.value?.channelUuid ?? '',
  set: (uuid: string) => {
    const ch = channels.value.find((c) => c.uuid === uuid)
    player.current.value = ch ? { channelUuid: ch.uuid, title: ch.name ?? ch.uuid } : null
  },
})

const hasChannel = computed(() => player.current.value !== null)

const videoEl = ref<HTMLVideoElement | null>(null)
const playbackError = ref(false)
/* Human-readable detail from the <video> MediaError. */
const playbackErrorDetail = ref('')
/* True while a profile or channel switch tears down + reloads the
 * stream, so the UI shows a hint instead of a frozen frame. Both go
 * through the same src-change reload, so one neutral message covers
 * them. Only set once the stream has played at least once — the
 * initial load uses the <video> element's own loading UI. */
const switching = ref(false)
const hasPlayed = ref(false)
/* True when the error came before the first `progress` event. */
const notStarted = ref(false)
/* Whether the current load fired `progress`, reset on every load.
 * The HTML spec fires it while media data is being fetched. An error
 * before it means the stream never got going, for one of two reasons
 * the player cannot tell apart:
 *   - The server did not start the stream. tvheadend sends the status
 *     line and headers only once the stream starts (SMT_START in
 *     http_stream_run, src/webui/webui.c), and a subscription gets
 *     SMT_START only once packets arrive (TSS_PACKETS in
 *     src/subscriptions.c). When it cannot start one (no free tuner,
 *     no access to an encrypted channel, no signal) it closes the
 *     connection with zero bytes, or answers 503 when it could not
 *     even subscribe. Chrome 154 and WebKit (a macOS WKWebView) both
 *     fire no `progress` then and report code 4.
 *   - The browser cannot play the stream with this profile. WebKit
 *     rejects such a stream before any `progress` too (measured with
 *     H.264 in Matroska and with raw MPEG-TS: code 4, empty message),
 *     so in WebKit (Safari's engine) an unplayable profile lands here
 *     and is not flagged.
 * Chrome fires `progress` as soon as a 200 response arrives, even an
 * empty one. A 200 from tvheadend means packets arrived, so a stream
 * Chrome cannot decode fails after `progress` and the profile is
 * flagged as before. Firefox was not measured. */
let sawProgress = false

/* Status → Log (an admin-only route, see router/index.ts) shows
 * whether the server logged why it did not start the stream.
 * Non-admin users get no link to a page they could not open. */
const canOpenLog = computed(() => access.has('admin'))

/* MediaError codes (HTMLMediaElement spec) → short phrases. */
const MEDIA_ERROR_LABELS: Record<number, string> = {
  1: 'playback aborted',
  2: 'network error',
  3: 'decode error',
  4: 'format not supported',
}

/* Two-way bind PrimeVue's `visible` to the composable's `isOpen`. */
const visibleProxy = computed({
  get: () => player.isOpen.value,
  set: (v: boolean) => {
    if (!v) player.close()
  },
})

const headerTitle = computed(() => player.current.value?.title ?? t('Live TV'))

/* Selectable profiles + the active one (a ref shared with the
 * composable so the <select> and the <video> src agree). */
const profiles = computed(() => streamProfiles.playableProfiles)
const selectedProfile = player.profile

/* Profiles that failed on the current channel earlier this session.
 * The same profile may play another channel fine, so the flag is per
 * channel (see the streamProfiles store). */
const failedHere = computed(() =>
  streamProfiles.failedProfiles.get(player.current.value?.channelUuid ?? ''),
)
/* i18n: new string */
const failedProfileText = computed(() => t('Failed to play this channel earlier this session'))
/* i18n: new string */
const notStartedText = computed(() =>
  t(
    'The stream did not start. Either the server could not start it (for example no free tuner, no access to an encrypted channel or no signal), or this browser cannot play it with profile "{0}".',
    selectedProfile.value,
  ),
)
/* i18n: new string */
const logLinkText = computed(() => t('Open Status → Log to see whether the server logged a reason.'))

function profileLabel(name: string): string {
  return profiles.value.find((p) => p.name === name)?.label ?? name
}

const videoSrc = computed(() => {
  const target = player.current.value
  const profile = player.profile.value
  return target && profile ? channelStreamUrl(target.channelUuid, profile) : ''
})

function readLastProfile(): string {
  try {
    return localStorage.getItem(LAST_PROFILE_KEY) ?? ''
  } catch {
    return ''
  }
}
function writeLastProfile(name: string): void {
  try {
    localStorage.setItem(LAST_PROFILE_KEY, name)
  } catch {
    /* Private mode / storage disabled — remembering is best-effort. */
  }
}

/* Pick the initial profile when the dialog opens: the remembered
 * one if still offered, else the first profile in the list. */
async function pickInitialProfile(): Promise<void> {
  await streamProfiles.ensure()
  const list = streamProfiles.playableProfiles
  const last = readLastProfile()
  player.profile.value = list.find((p) => p.name === last)?.name ?? list[0]?.name ?? ''
}

function teardownVideo(): void {
  const el = videoEl.value
  if (el) {
    el.pause()
    el.removeAttribute('src')
    el.load()
  }
}

/* Reset / teardown wired to the open<->close transition. Vue's
 * pre-flush watcher runs this BEFORE the dialog content unmounts, so
 * the <video> element still exists for teardown. `immediate` so the
 * open branch also runs if the component is mounted while already
 * open (teardown is a no-op when there is no element yet). */
watch(
  () => player.isOpen.value,
  (open) => {
    if (open) {
      playbackError.value = false
      playbackErrorDetail.value = ''
      notStarted.value = false
      sawProgress = false
      switching.value = false
      hasPlayed.value = false
      void pickInitialProfile()
      void loadChannels()
      return
    }
    teardownVideo()
  },
  { immediate: true },
)

/* Profile switch (or initial load): the stream URL changed, so
 * explicitly reload the element — a <video> does not re-fetch on a
 * bare src change. `flush: 'post'` so the :src attribute is already
 * updated when this runs. */
watch(
  videoSrc,
  () => {
    const el = videoEl.value
    if (!el || !player.isOpen.value || !videoSrc.value) return
    playbackError.value = false
    playbackErrorDetail.value = ''
    notStarted.value = false
    sawProgress = false
    /* Show the "switching" hint only for a genuine switch, not the
     * first load (the <video> shows its own loading UI). */
    switching.value = hasPlayed.value
    writeLastProfile(player.profile.value)
    el.load()
  },
  { flush: 'post' },
)

/* Capture the real reason from the element's MediaError. */
function onError(): void {
  switching.value = false
  playbackError.value = true
  /* The stream never got going, see `sawProgress`. */
  if (!sawProgress) {
    notStarted.value = true
    return
  }
  const err = videoEl.value?.error
  if (err) {
    const label = MEDIA_ERROR_LABELS[err.code] ?? `error ${err.code}`
    playbackErrorDetail.value = err.message ? `${label} — ${err.message}` : label
    /* A decode (3) or unsupported-format (4) error is the profile's
     * codecs failing on this channel — flag it for the session so
     * the dropdown warns. Aborted (1) and network (2) errors are
     * transient and not the profile's fault, so they are not
     * flagged. */
    if (err.code === 3 || err.code === 4) {
      streamProfiles.markProfileFailed(
        player.profile.value,
        player.current.value?.channelUuid ?? '',
      )
    }
  }
}

/* Media data is arriving, so a later error is about the stream
 * itself. */
function onProgress(): void {
  sawProgress = true
}

/* Stream is up — clear the transient "switching" hint, and drop any
 * earlier-this-session failure flag on this profile and channel: it
 * just played. */
function onPlaying(): void {
  switching.value = false
  hasPlayed.value = true
  streamProfiles.clearProfileFailed(player.profile.value, player.current.value?.channelUuid ?? '')
}
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :draggable="false"
    :dismissable-mask="true"
    :header="headerTitle"
    class="video-player-dialog"
    :style="{ width: '800px', maxWidth: 'calc(100vw - 32px)' }"
    :breakpoints="{ '768px': '100vw' }"
  >
    <!-- Channel + profile switchers. The Channel dropdown is how you
         pick what to watch (and switch live); the profile switcher
         appears only when there is more than one profile. A profile
         that failed to play this channel earlier this session is
         flagged, in the list and on the selected value. The icon's
         wrapper carries the tooltip: PrimeVue measures the target's
         size only for an HTMLElement, not an <svg>. -->
    <div class="video-player-dialog__toolbar">
      <label class="video-player-dialog__field-label" for="video-player-channel">
        {{ t('Channel') }}
      </label>
      <Select
        v-model="selectedChannel"
        input-id="video-player-channel"
        :aria-label="t('Channel')"
        :options="channels"
        option-label="label"
        option-value="uuid"
        filter
        :filter-placeholder="t('Search channels…')"
        :placeholder="t('Select a channel')"
        class="video-player-dialog__channel-select"
      />
      <template v-if="profiles.length > 1">
        <label class="video-player-dialog__field-label" for="video-player-profile">
          {{ t('Stream profile') }}
        </label>
        <Select
          v-model="selectedProfile"
          input-id="video-player-profile"
          :aria-label="t('Stream profile')"
          :options="profiles"
          option-label="label"
          option-value="name"
          class="video-player-dialog__profile-select"
        >
          <template #value="{ value }">
            <span class="video-player-dialog__profile-option">
              <span
                v-if="failedHere?.has(value)"
                v-tooltip.top="failedProfileText"
                class="video-player-dialog__profile-warn"
                role="img"
                :aria-label="failedProfileText"
              >
                <TriangleAlert :size="14" :stroke-width="2" aria-hidden="true" />
              </span>
              <span class="video-player-dialog__profile-name">{{ profileLabel(value) }}</span>
            </span>
          </template>
          <template #option="{ option }">
            <span class="video-player-dialog__profile-option">
              <span
                v-if="failedHere?.has(option.name)"
                v-tooltip.top="failedProfileText"
                class="video-player-dialog__profile-warn"
                role="img"
                :aria-label="failedProfileText"
              >
                <TriangleAlert :size="14" :stroke-width="2" aria-hidden="true" />
              </span>
              <span>{{ option.label }}</span>
            </span>
          </template>
        </Select>
      </template>
    </div>
    <div class="video-player-dialog__body">
      <!-- The <video> stays mounted across errors and switches so
           teardown / reload always target a stable element; the
           error and switching states render as overlays.
           controlslist hides Chrome's Download and Playback speed
           items. A download of a live stream never ends, and a
           speed makes no sense for live TV. -->
      <video
        ref="videoEl"
        class="video-player-dialog__video"
        :src="videoSrc"
        controls
        controlslist="nodownload noplaybackrate"
        autoplay
        playsinline
        @error="onError"
        @progress="onProgress"
        @playing="onPlaying"
      />
      <div v-if="!hasChannel" class="video-player-dialog__overlay">
        <p>{{ t('Select a channel to watch.') }}</p>
      </div>
      <div v-else-if="notStarted" class="video-player-dialog__overlay">
        <!-- No MediaError detail here: before the first progress
             event it was code 4 ("format not supported") in every
             measured case, in Chrome and WebKit alike, so it cannot
             say which cause applies and would point at the profile.
             Following the link navigates away, so it closes the
             player too. -->
        <p>{{ notStartedText }}</p>
        <p v-if="canOpenLog">
          <router-link
            :to="{ name: 'status-log' }"
            class="video-player-dialog__log-link"
            @click="player.close()"
          >
            {{ logLinkText }}
          </router-link>
        </p>
      </div>
      <div v-else-if="playbackError" class="video-player-dialog__overlay">
        <p>
          {{ t('Playback failed. The stream could not be played in the browser using profile "{0}" — try another profile, or use the external player instead.', selectedProfile) }}
        </p>
        <p v-if="playbackErrorDetail" class="video-player-dialog__error-detail">
          {{ playbackErrorDetail }}
        </p>
      </div>
      <div v-else-if="switching" class="video-player-dialog__overlay">
        <p>{{ t('Switching…') }}</p>
      </div>
    </div>
  </Dialog>
</template>

<style scoped>
.video-player-dialog__toolbar {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding-bottom: var(--tvh-space-2);
}

.video-player-dialog__field-label {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}

/* PrimeVue Select for the channel switcher — a touch wider than the
 * profile one since channel names run longer. Same inline-flex caveat
 * as the profile select below (don't force display:block). */
.video-player-dialog__channel-select {
  min-width: 240px;
}

/* PrimeVue Select for the profile switcher. `.p-select` is
 * display: inline-flex — don't force `display: block` on it or a
 * wrapper, or the label collapses and the chevron drops. */
.video-player-dialog__profile-select {
  min-width: 200px;
}

/* A profile in the dropdown or the selected value: optional warning
 * icon + label. */
.video-player-dialog__profile-option {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

/* Marks a profile that failed to play this channel earlier this
 * session. */
.video-player-dialog__profile-warn {
  display: inline-flex;
  flex: none;
  color: var(--tvh-warning);
}

/* The selected profile's name: shrinks and ends with an ellipsis like
 * PrimeVue's default label when the select is narrow. */
.video-player-dialog__profile-name {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.video-player-dialog__body {
  position: relative;
  /* 16:9 frame; the video fills it. On phone the dialog goes
   * full-width (breakpoints above) and the aspect-ratio keeps the
   * video letterboxed rather than stretched. */
  width: 100%;
  aspect-ratio: 16 / 9;
  background: #000;
}

.video-player-dialog__video {
  width: 100%;
  height: 100%;
  /* `contain` so a non-16:9 stream letterboxes inside the frame
   * instead of cropping. */
  object-fit: contain;
  background: #000;
}

/* Error / switching message centred over the video frame. */
.video-player-dialog__overlay {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-4);
  text-align: center;
  color: #fff;
  background: rgba(0, 0, 0, 0.7);
}

.video-player-dialog__overlay p {
  margin: 0;
}

/* The MediaError code/message — smaller, dimmer. */
.video-player-dialog__error-detail {
  font-size: var(--tvh-text-sm);
  opacity: 0.75;
}

/* Link to Status → Log. The overlay is always dark, so it keeps the
 * overlay's white text and marks itself with the underline. */
.video-player-dialog__log-link {
  color: inherit;
  text-decoration: underline;
}
</style>
