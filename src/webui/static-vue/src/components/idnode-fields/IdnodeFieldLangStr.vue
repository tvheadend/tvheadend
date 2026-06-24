<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldLangStr — multilingual string editor for PT_LANGSTR
 * properties.
 *
 * Wire shape (`src/lang_str.c:251-267` `lang_str_serialize_map`):
 *   { "eng": "Title", "ger": "Titel", "fre": "Titre", ... }
 * — a JSON map keyed by ISO 639-2/B 3-letter language codes (the
 * `code2b` form from `src/lang_codes.c`). Server consumes the same
 * shape on save.
 *
 * Today every PT_LANGSTR field is `PO_RDONLY` and explicitly
 * filtered out of the DVR editor, so this renderer has no live
 * production surface — it ships future-ready for whatever class
 * first declares an editable LANGSTR field.
 *
 * Surface: one row per language (language picker + text input +
 * remove button) plus an "Add language" button that opens a
 * picker scoped to languages NOT yet in the map.
 */
import { computed } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: Record<string, string>
}>()

const emit = defineEmits<{
  'update:modelValue': [value: Record<string, string>]
}>()

const isReadonly = computed(() => props.prop.rdonly === true)

/* Common-subset language picker. The full ISO 639-2 table on the C
 * side is hundreds of entries, most of which never appear in EPG
 * streams; offering the common broadcasting languages keeps the
 * dropdown navigable. The text field allows free-form input via
 * the "Other…" sentinel below if a niche code is needed. */
const COMMON_LANGS: { code: string; name: string }[] = [
  { code: 'eng', name: 'English' },
  { code: 'ger', name: 'German' },
  { code: 'fre', name: 'French' },
  { code: 'spa', name: 'Spanish' },
  { code: 'ita', name: 'Italian' },
  { code: 'dut', name: 'Dutch' },
  { code: 'por', name: 'Portuguese' },
  { code: 'rus', name: 'Russian' },
  { code: 'pol', name: 'Polish' },
  { code: 'cze', name: 'Czech' },
  { code: 'swe', name: 'Swedish' },
  { code: 'nor', name: 'Norwegian' },
  { code: 'dan', name: 'Danish' },
  { code: 'fin', name: 'Finnish' },
  { code: 'gre', name: 'Greek' },
  { code: 'hun', name: 'Hungarian' },
  { code: 'tur', name: 'Turkish' },
  { code: 'ara', name: 'Arabic' },
  { code: 'jpn', name: 'Japanese' },
  { code: 'kor', name: 'Korean' },
  { code: 'chi', name: 'Chinese' },
]

const COMMON_LOOKUP = new Map(COMMON_LANGS.map((l) => [l.code, l.name]))

function langLabel(code: string): string {
  return COMMON_LOOKUP.get(code) ?? code
}

const entries = computed<{ lang: string; text: string }[]>(() => {
  const m = props.modelValue
  if (!m || typeof m !== 'object' || Array.isArray(m)) return []
  return Object.entries(m).map(([lang, text]) => ({ lang, text }))
})

const usedLangs = computed<Set<string>>(() => new Set(entries.value.map((e) => e.lang)))

const availableLangs = computed<{ code: string; name: string }[]>(() =>
  COMMON_LANGS.filter((l) => !usedLangs.value.has(l.code)),
)

/* Pick the first unused common language for a fresh row. Default
 * to `eng` if available; otherwise the first available common
 * code; otherwise empty (shouldn't happen — picker is hidden when
 * no langs remain). */
function pickDefaultLang(): string {
  if (!usedLangs.value.has('eng')) return 'eng'
  return availableLangs.value[0]?.code ?? ''
}

/* Emit a new map with insertion order preserved. Helps the editor
 * compare baseline vs current cleanly via JSON.stringify-driven
 * dirty tracking. */
function emitMap(next: Map<string, string>) {
  const out: Record<string, string> = {}
  for (const [k, v] of next) out[k] = v
  emit('update:modelValue', out)
}

function currentMap(): Map<string, string> {
  return new Map(entries.value.map((e) => [e.lang, e.text]))
}

function onTextChange(lang: string, text: string) {
  if (isReadonly.value) return
  const m = currentMap()
  m.set(lang, text)
  emitMap(m)
}

function onLangChange(oldLang: string, newLang: string) {
  if (isReadonly.value) return
  if (oldLang === newLang) return
  /* Rebuild the map preserving order — change the key in-place. */
  const m = new Map<string, string>()
  for (const [k, v] of currentMap()) {
    m.set(k === oldLang ? newLang : k, v)
  }
  emitMap(m)
}

function onRemove(lang: string) {
  if (isReadonly.value) return
  const m = currentMap()
  m.delete(lang)
  emitMap(m)
}

function onAdd() {
  if (isReadonly.value) return
  const code = pickDefaultLang()
  if (!code) return
  const m = currentMap()
  m.set(code, '')
  emitMap(m)
}

/* Per-row pickers offer the row's own current code PLUS every code
 * not used elsewhere — so the user can switch the row to a
 * different language but can't pick one that's already in use on
 * another row. */
function pickerOpts(rowLang: string): { code: string; name: string }[] {
  return COMMON_LANGS.filter(
    (l) => l.code === rowLang || !usedLangs.value.has(l.code),
  )
}
</script>

<template>
  <div class="ifld">
    <label class="ifld__label" :for="prop.id">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>
    <div class="ifld-langstr">
      <div
        v-for="entry in entries"
        :key="entry.lang"
        class="ifld-langstr__row"
      >
        <select
          class="ifld-langstr__lang"
          :value="entry.lang"
          :disabled="isReadonly"
          :aria-label="`Language for ${prop.caption ?? prop.id}`"
          @change="onLangChange(entry.lang, ($event.target as HTMLSelectElement).value)"
        >
          <option
            v-for="opt in pickerOpts(entry.lang)"
            :key="opt.code"
            :value="opt.code"
          >
            {{ opt.name }}
          </option>
          <!--
            Render the row's own code as a fallback option even
            if it's not in the common subset (e.g. server emits a
            niche `und` undefined-language entry). Without this,
            the select would silently switch to the first option
            on its own.
          -->
          <option
            v-if="!COMMON_LOOKUP.has(entry.lang)"
            :value="entry.lang"
          >
            {{ langLabel(entry.lang) }}
          </option>
        </select>
        <input
          class="ifld-langstr__text"
          type="text"
          :value="entry.text"
          :disabled="isReadonly"
          :aria-label="`${langLabel(entry.lang)} value`"
          autocomplete="off"
          @input="onTextChange(entry.lang, ($event.target as HTMLInputElement).value)"
        />
        <button
          v-if="!isReadonly"
          type="button"
          class="ifld-langstr__remove"
          :aria-label="`Remove ${langLabel(entry.lang)}`"
          :title="`Remove ${langLabel(entry.lang)}`"
          @click="onRemove(entry.lang)"
        >
          ×
        </button>
      </div>
      <button
        v-if="!isReadonly && availableLangs.length > 0"
        type="button"
        class="ifld-langstr__add"
        @click="onAdd"
      >
        + Add language
      </button>
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
  font-size: var(--tvh-text-md);
  font-weight: 500;
  color: var(--tvh-text);
}

.ifld-langstr {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.ifld-langstr__row {
  display: flex;
  gap: 6px;
  align-items: center;
}

.ifld-langstr__lang {
  flex: 0 0 140px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 8px;
  font: inherit;
  font-size: var(--tvh-text-md);
  min-height: 36px;
}

.ifld-langstr__text {
  flex: 1 1 auto;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  font-size: var(--tvh-text-md);
  min-height: 36px;
}

.ifld-langstr__lang:focus,
.ifld-langstr__text:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.ifld-langstr__lang:disabled,
.ifld-langstr__text:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}

.ifld-langstr__remove {
  flex: 0 0 auto;
  background: transparent;
  color: var(--tvh-text-muted);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 0;
  width: 28px;
  height: 28px;
  font: inherit;
  font-size: var(--tvh-text-xl);
  line-height: 1;
  cursor: pointer;
}

.ifld-langstr__remove:hover {
  border-color: var(--tvh-danger, #c00);
  color: var(--tvh-danger, #c00);
}

.ifld-langstr__add {
  align-self: flex-start;
  background: transparent;
  color: var(--tvh-primary);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 12px;
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
}

.ifld-langstr__add:hover {
  border-color: var(--tvh-primary);
}
</style>
