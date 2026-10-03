<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * RecordLengthDialog — asks how long to record a live channel.
 *
 * Opened by the player's Record split button (PlayerRecordButton),
 * either through "Record fixed length…" or by Record itself when the
 * channel has no EPG event on air, so there is no programme end to
 * record to. It offers a few preset lengths and a custom number of
 * minutes, shows when the recording will end, and emits the length.
 * The caller creates the DVR entry.
 *
 * The last length is remembered in localStorage, like the player's
 * stream profile.
 *
 * Escape closes only this dialog. PrimeVue's Dialog listens for
 * Escape on the document, so without the capture listener below the
 * player dialog underneath would close as well.
 */
import { computed, onBeforeUnmount, ref, useId, watch } from 'vue'
import Dialog from 'primevue/dialog'
import { useI18n } from '@/composables/useI18n'
import { useNowCursor } from '@/composables/useNowCursor'

const props = defineProps<{
  visible: boolean
  /* True when opened because the channel has no EPG event now. */
  noEpg?: boolean
}>()

const emit = defineEmits<{
  close: []
  record: [minutes: number]
}>()

const { t } = useI18n()

/* Ids for aria-labelledby: the presets group and the custom input
 * are named by the visible "Duration" label (and the unit). */
const labelId = useId()
const unitId = useId()

/* Preset lengths in minutes. */
const PRESETS = [15, 30, 60, 90, 120]
const DEFAULT_MINUTES = 60
/* Upper bound for a custom length: one day. */
const MAX_MINUTES = 1440
/* localStorage key for the last length the user recorded with. */
const LAST_MINUTES_KEY = 'tvh:player-record-minutes'

const minutes = ref<number | null>(DEFAULT_MINUTES)

function isValid(m: number | null): m is number {
  return m !== null && Number.isInteger(m) && m >= 1 && m <= MAX_MINUTES
}

const canRecord = computed(() => isValid(minutes.value))

function readLastMinutes(): number {
  try {
    const n = Number(localStorage.getItem(LAST_MINUTES_KEY))
    return isValid(n) ? n : DEFAULT_MINUTES
  } catch {
    return DEFAULT_MINUTES
  }
}

function writeLastMinutes(m: number): void {
  try {
    localStorage.setItem(LAST_MINUTES_KEY, String(m))
  } catch {
    /* Private mode / storage disabled — remembering is best-effort. */
  }
}

/* Clock for the end-time hint. It ticks every 30 s and is set again
 * on open, so the hint stays right while the dialog is open. */
const { now } = useNowCursor()

/* i18n: new string */
const endText = computed(() => {
  if (!isValid(minutes.value)) return ''
  const end = new Date((now.value + minutes.value * 60) * 1000)
  return t('Ends at {0}', end.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' }))
})

function onInput(ev: Event): void {
  const raw = (ev.target as HTMLInputElement).value
  minutes.value = raw === '' ? null : Number(raw)
}

function onRecord(): void {
  if (!isValid(minutes.value)) return
  writeLastMinutes(minutes.value)
  emit('record', minutes.value)
}

/* Escape closes this dialog only: take the key in the capture phase
 * on the window, before it reaches the document listeners of this
 * dialog and of the player dialog underneath. */
function onEscape(ev: KeyboardEvent): void {
  if (ev.key !== 'Escape') return
  ev.stopPropagation()
  ev.preventDefault()
  emit('close')
}

function detachEscape(): void {
  window.removeEventListener('keydown', onEscape, true)
}

watch(
  () => props.visible,
  (open) => {
    if (open) {
      minutes.value = readLastMinutes()
      now.value = Math.floor(Date.now() / 1000)
      window.addEventListener('keydown', onEscape, true)
    } else {
      detachEscape()
    }
  },
  { immediate: true },
)

onBeforeUnmount(detachEscape)
</script>

<template>
  <!-- i18n: "Record fixed length" is a new string. -->
  <Dialog
    :visible="visible"
    :header="t('Record fixed length')"
    class="record-length-dialog"
    modal
    :draggable="false"
    :close-on-escape="false"
    :style="{ width: '420px', maxWidth: 'calc(100vw - 32px)' }"
    @update:visible="(v) => { if (!v) emit('close') }"
  >
    <div class="record-length">
      <!-- i18n: new string -->
      <p v-if="noEpg" class="record-length__note">
        {{ t('There is no programme information for this channel right now. Choose how long to record.') }}
      </p>
      <div class="record-length__field">
        <span :id="labelId" class="record-length__label">{{ t('Duration') }}</span>
        <div class="record-length__presets" role="group" :aria-labelledby="labelId">
          <button
            v-for="p in PRESETS"
            :key="p"
            type="button"
            class="record-length__preset"
            :class="{ 'record-length__preset--active': minutes === p }"
            :aria-pressed="minutes === p"
            @click="minutes = p"
          >
            {{ p }} {{ t('min') }}
          </button>
        </div>
        <div class="record-length__custom">
          <input
            class="record-length__input"
            type="number"
            min="1"
            :max="MAX_MINUTES"
            step="1"
            inputmode="numeric"
            :aria-labelledby="`${labelId} ${unitId}`"
            :value="minutes ?? ''"
            @input="onInput"
            @keydown.enter.prevent="onRecord"
          />
          <span :id="unitId" class="record-length__unit">{{ t('min') }}</span>
          <span class="record-length__end" aria-live="polite">{{ endText }}</span>
        </div>
      </div>
    </div>
    <template #footer>
      <button type="button" class="record-length__btn" @click="emit('close')">
        {{ t('Cancel') }}
      </button>
      <button
        type="button"
        class="record-length__btn record-length__btn--primary"
        :disabled="!canRecord"
        autofocus
        @click="onRecord"
      >
        {{ t('Record') }}
      </button>
    </template>
  </Dialog>
</template>

<style scoped>
.record-length {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) 0;
}

.record-length__note {
  margin: 0;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-md);
}

.record-length__field {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
}

.record-length__label {
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.record-length__presets {
  display: flex;
  flex-wrap: wrap;
  gap: var(--tvh-space-2);
}

/* Preset length — same chrome as the toolbar action buttons
 * (ActionMenu), filled with the primary colour when selected. */
.record-length__preset {
  height: 32px;
  padding: 4px var(--tvh-space-3);
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: var(--tvh-text-md);
  white-space: nowrap;
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.record-length__preset:hover:not(.record-length__preset--active) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.record-length__preset--active {
  background: var(--tvh-primary);
  color: var(--tvh-on-primary, #fff);
  border-color: var(--tvh-primary);
}

.record-length__custom {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.record-length__input {
  width: 6rem;
  min-height: 36px;
  padding: 6px 10px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: var(--tvh-text-md);
}

.record-length__unit {
  color: var(--tvh-text);
  font-size: var(--tvh-text-md);
}

.record-length__end {
  margin-left: auto;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
}

.record-length__preset:focus-visible,
.record-length__input:focus,
.record-length__btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/* Footer buttons — the same look as PlayProfileDialog's. */
.record-length__btn {
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

.record-length__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.record-length__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.record-length__btn--primary {
  background: var(--tvh-primary);
  color: var(--tvh-on-primary, #fff);
  border-color: var(--tvh-primary);
}

.record-length__btn--primary:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 90%, black);
}
</style>
