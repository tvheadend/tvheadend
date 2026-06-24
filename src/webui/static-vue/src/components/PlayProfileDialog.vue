<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * PlayProfileDialog — pick a stream profile, then open a live
 * channel in the user's external media player.
 *
 * "Play in external player" normally streams with the server's
 * default profile. This dialog lets the user pick an explicit
 * profile instead — useful for exercising a specific transcoding
 * setup. The choice is appended as `?profile=<name>` to the
 * `/play/ticket/...` URL; the server's playlist generator bakes it
 * into the m3u handed to the external player, so no server change
 * is needed.
 *
 * Profiles come from `api/profile/list` via the streamProfiles
 * store. That endpoint is permission-scoped server-side (it returns
 * only profiles the requesting user may use) and the stream route
 * re-validates the choice — so no client-side access check is
 * needed here.
 *
 * Drawer-scoped: opened only from the EPG event drawer, controlled
 * by the `channelUuid` prop (non-null = open). The last-used
 * profile is remembered in localStorage so repeated plays don't
 * re-pick from scratch.
 */
import { computed, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import { useI18n } from '@/composables/useI18n'
import { useStreamProfilesStore } from '@/stores/streamProfiles'
import { openPlay } from '@/utils/playUrl'

const { t } = useI18n()

const props = defineProps<{
  /* Channel to play. Non-null opens the dialog; null keeps it shut. */
  channelUuid: string | null
}>()

const emit = defineEmits<{ close: [] }>()

const streamProfiles = useStreamProfilesStore()

/* localStorage key for the last-used external-play profile. */
const LAST_PROFILE_KEY = 'tvh:play-profile'

const selected = ref<string>('')

const profiles = computed<string[]>(() => streamProfiles.profileNames)

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

/* On open: make sure the profile list is loaded (the store caches
 * it after the first fetch), then preselect the remembered profile
 * when it is still offered, otherwise the first one. */
watch(
  () => props.channelUuid,
  async (uuid) => {
    if (uuid === null) return
    await streamProfiles.ensure()
    const last = readLastProfile()
    selected.value =
      last && profiles.value.includes(last) ? last : (profiles.value[0] ?? '')
  },
  { immediate: true },
)

const canPlay = computed(() => !!selected.value)

function onPlay(): void {
  const uuid = props.channelUuid
  const profile = selected.value
  if (uuid === null || !profile) return
  writeLastProfile(profile)
  openPlay(`stream/channel/${uuid}?profile=${encodeURIComponent(profile)}`)
  emit('close')
}

function onClose(): void {
  emit('close')
}
</script>

<template>
  <Dialog
    :visible="channelUuid !== null"
    :header="t('Play with stream profile')"
    modal
    :closable="true"
    :draggable="false"
    :style="{ width: '420px' }"
    @update:visible="(v) => { if (!v) onClose() }"
  >
    <div class="play-profile">
      <p v-if="profiles.length === 0" class="play-profile__status">
        {{ t('No stream profiles are available.') }}
      </p>
      <label v-else class="play-profile__field">
        <span class="play-profile__label">{{ t('Stream profile') }}</span>
        <select
          v-model="selected"
          class="play-profile__select"
          :aria-label="t('Stream profile')"
        >
          <option v-for="p in profiles" :key="p" :value="p">{{ p }}</option>
        </select>
      </label>
    </div>
    <template #footer>
      <button type="button" class="play-profile__btn" @click="onClose">
        {{ t('Cancel') }}
      </button>
      <button
        type="button"
        class="play-profile__btn play-profile__btn--primary"
        :disabled="!canPlay"
        @click="onPlay"
      >
        {{ t('Play') }}
      </button>
    </template>
  </Dialog>
</template>

<style scoped>
.play-profile {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) 0;
}

.play-profile__field {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.play-profile__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.play-profile__select {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
}

.play-profile__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.play-profile__status {
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-md);
  margin: 0;
}

.play-profile__btn {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px var(--tvh-space-3);
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.play-profile__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.play-profile__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.play-profile__btn--primary {
  background: var(--tvh-primary);
  color: var(--tvh-on-primary, #fff);
  border-color: var(--tvh-primary);
}

.play-profile__btn--primary:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 90%, black);
}
</style>
