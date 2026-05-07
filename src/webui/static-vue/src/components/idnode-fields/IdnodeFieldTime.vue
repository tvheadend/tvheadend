<script setup lang="ts">
/*
 * IdnodeFieldTime — time-typed property input.
 *
 * The server's PT_TIME values are unix-epoch seconds. Two modifier
 * variants:
 *   - default              datetime-local input (date + time, no TZ)
 *   - prop.duration        duration in seconds, rendered as a
 *                          minutes-input for human-friendly editing
 *                          (matches what users do in the existing
 *                          ExtJS dvr_extra start/stop_extra fields)
 *   - prop.date            date only (no time component)
 *
 * datetime-local handles its own native picker on every modern
 * browser; iOS Safari and Chrome both render an OS-native date/time
 * spinner which is the right UX for phone.
 */
import { computed } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  /* Always seconds since epoch when prop is a datetime; minutes when
   * `prop.duration` is set (match the server's expectation that
   * duration values come back in seconds — we convert minutes ↔
   * seconds at the boundary). */
  modelValue: number | null
}>()

const emit = defineEmits<{
  'update:modelValue': [value: number | null]
}>()

const isReadonly = props.prop.rdonly === true
const isDuration = props.prop.duration === true
const isDateOnly = props.prop.date === true && !isDuration

/*
 * datetime-local needs a string in "YYYY-MM-DDTHH:mm" format and
 * has no timezone. We convert from epoch seconds via local-time
 * Date methods so the user sees their local clock — same convention
 * as ExtJS's TwinDateTimeField uses.
 */
function epochToLocalInput(s: number, dateOnly: boolean): string {
  const d = new Date(s * 1000)
  const yyyy = d.getFullYear().toString().padStart(4, '0')
  const mm = (d.getMonth() + 1).toString().padStart(2, '0')
  const dd = d.getDate().toString().padStart(2, '0')
  if (dateOnly) return `${yyyy}-${mm}-${dd}`
  const hh = d.getHours().toString().padStart(2, '0')
  const mi = d.getMinutes().toString().padStart(2, '0')
  return `${yyyy}-${mm}-${dd}T${hh}:${mi}`
}

function localInputToEpoch(s: string): number | null {
  if (!s) return null
  /*
   * Two ECMA-262 spec quirks at play here:
   *   - `YYYY-MM-DDTHH:mm`  (no Z) → parsed as LOCAL time. Correct.
   *   - `YYYY-MM-DD` alone   → parsed as UTC midnight. NOT what we want
   *     for date-only fields: a user in UTC-8 entering 2026-04-27
   *     would see the value round-trip as 2026-04-26 because UTC
   *     midnight is the previous local day.
   *
   * For the date-only branch we therefore split the string and feed
   * the integer y/m/d to the multi-arg Date constructor, which always
   * uses local time and avoids the UTC interpretation entirely.
   */
  let d: Date
  if (isDateOnly) {
    const parts = s.split('-').map(Number)
    if (parts.length !== 3 || parts.some((n) => !Number.isFinite(n))) return null
    d = new Date(parts[0], parts[1] - 1, parts[2])
  } else {
    d = new Date(s)
  }
  if (Number.isNaN(d.getTime())) return null
  return Math.floor(d.getTime() / 1000)
}

const inputValue = computed(() => {
  if (props.modelValue === null) return ''
  if (isDuration) return Math.round(props.modelValue / 60).toString()
  return epochToLocalInput(props.modelValue, isDateOnly)
})

function onInput(ev: Event) {
  const raw = (ev.target as HTMLInputElement).value
  if (raw === '') {
    emit('update:modelValue', null)
    return
  }
  if (isDuration) {
    const m = Number(raw)
    emit('update:modelValue', Number.isFinite(m) ? Math.round(m * 60) : null)
    return
  }
  emit('update:modelValue', localInputToEpoch(raw))
}
</script>

<template>
  <div class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <div class="ifld__row">
      <input
        :id="prop.id"
        class="ifld__input"
        :type="isDuration ? 'number' : isDateOnly ? 'date' : 'datetime-local'"
        :value="inputValue"
        :disabled="isReadonly"
        :min="isDuration ? 0 : undefined"
        @input="onInput"
      />
      <span v-if="isDuration" class="ifld__suffix">minutes</span>
    </div>
  </div>
</template>

<style scoped>
.ifld {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.ifld__label {
  font-size: 13px;
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld__row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.ifld__input {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  font-size: 13px;
  min-height: 36px;
  flex: 1 1 auto;
  min-width: 0;
}

.ifld__input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.ifld__input:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}

.ifld__suffix {
  font-size: 13px;
  color: var(--tvh-text-muted);
  flex: 0 0 auto;
}
</style>
