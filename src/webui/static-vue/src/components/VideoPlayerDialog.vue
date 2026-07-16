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
import { channelStreamUrl } from '@/utils/playUrl'
import { apiCall } from '@/api/client'
import type { GridResponse, FilterDef } from '@/types/grid'

const { t } = useI18n()
const player = useVideoPlayer()
const streamProfiles = useStreamProfilesStore()

/* localStorage key for the last in-browser profile the user chose. */
const LAST_PROFILE_KEY = 'tvh:browser-play-profile'

/* Enabled channels for the Channel dropdown. Fetched once (the dialog
 * is a mounted singleton, so this survives across opens) and shared by
 * every entry point into the player. */
interface PlayerChannel {
  uuid: string
  name?: string
  number?: number
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
      limit: 999_999_999,
      sort: 'number',
      dir: 'ASC',
    })
    channels.value = (resp.entries ?? []).map((c) => ({
      ...c,
      label:
        typeof c.number === 'number' ? `${c.number} · ${c.name ?? c.uuid}` : (c.name ?? c.uuid),
    }))
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
  const err = videoEl.value?.error
  if (err) {
    const label = MEDIA_ERROR_LABELS[err.code] ?? `error ${err.code}`
    playbackErrorDetail.value = err.message ? `${label} — ${err.message}` : label
    /* A decode (3) or unsupported-format (4) error is the profile's
     * own codecs failing — flag it for the session so the dropdown
     * warns. Aborted (1) and network (2) errors are transient and
     * not the profile's fault, so they are not flagged. */
    if (err.code === 3 || err.code === 4) {
      streamProfiles.markProfileFailed(player.profile.value)
    }
  }
}

/* Stream is up — clear the transient "switching" hint, and drop any
 * earlier-this-session failure flag on this profile: it just played. */
function onPlaying(): void {
  switching.value = false
  hasPlayed.value = true
  streamProfiles.clearProfileFailed(player.profile.value)
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
         that failed to play earlier this session is flagged. -->
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
          <template #option="{ option }">
            <span class="video-player-dialog__profile-option">
              <TriangleAlert
                v-if="streamProfiles.failedProfiles.has(option.name)"
                :size="14"
                :stroke-width="2"
                class="video-player-dialog__profile-warn"
                :aria-label="t('Failed to play earlier this session')"
              />
              <span>{{ option.label }}</span>
            </span>
          </template>
        </Select>
      </template>
    </div>
    <div class="video-player-dialog__body">
      <!-- The <video> stays mounted across errors and switches so
           teardown / reload always target a stable element; the
           error and switching states render as overlays. -->
      <video
        ref="videoEl"
        class="video-player-dialog__video"
        :src="videoSrc"
        controls
        autoplay
        playsinline
        @error="onError"
        @playing="onPlaying"
      />
      <div v-if="!hasChannel" class="video-player-dialog__overlay">
        <p>{{ t('Select a channel to watch.') }}</p>
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

/* A profile option in the dropdown: optional warning icon + label. */
.video-player-dialog__profile-option {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

/* Marks a profile that failed to play earlier this session. */
.video-player-dialog__profile-warn {
  flex: none;
  color: var(--tvh-warning);
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
</style>
