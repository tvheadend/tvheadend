<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeFieldPerm — Unix-style file permission editor for PT_PERM
 * properties (DVR Profile's `directory-permissions` /
 * `file-permissions`).
 *
 * Two synced surfaces, both editable:
 *   - 3×3 checkbox matrix for Owner/Group/Other × Read/Write/Execute
 *   - octal text input ("0775") for keyboard / copy-paste
 *
 * Plus a collapsed "Advanced" disclosure carrying the rarely-used
 * special bits (setuid, setgid, sticky) — the leading octal digit.
 *
 * Wire format: server emits a 4-digit octal STRING via `%04o`
 * (`src/prop.c:368-371`); accepts any base on save via
 * `strtol(s, NULL, 0)` (`src/prop.c:243-249`). We always emit a
 * leading-`0` string so base-0 strtol reads it back as octal.
 */
import { computed } from 'vue'
import type { IdnodeProp } from '@/types/idnode'
import { useAccessStore } from '@/stores/access'

const access = useAccessStore()

const props = defineProps<{
  prop: IdnodeProp
  modelValue: string
}>()

const emit = defineEmits<{
  'update:modelValue': [value: string]
}>()

const isReadonly = computed(() => props.prop.rdonly === true)

/* Decompose the octal string into its 12 bits. Tolerant on input —
 * any base via parseInt-with-base-0-equivalent fallback so a server
 * value of "493" (decimal) or "0x1ED" (hex) round-trips correctly,
 * matching the C side's strtol(s, NULL, 0) acceptance. */
function parseOctal(str: string): number {
  if (typeof str !== 'string' || str === '') return 0
  if (/^0[xX]/.test(str)) return Number.parseInt(str, 16) || 0
  /* Plain digit run — interpret as octal whether or not it has a
   * leading 0. Most callers ship "0664"; some test fixtures or
   * pasted CLI values omit the leading zero. */
  if (/^[0-7]+$/.test(str)) return Number.parseInt(str, 8) || 0
  /* Decimal fallback for any other digit-only value (e.g. "493"). */
  if (/^\d+$/.test(str)) return Number.parseInt(str, 10) || 0
  return 0
}

/* Always emit a leading `0` — the server saves via strtol(s, NULL, 0),
 * which reads "2755" as DECIMAL. Plain values stay 4 chars ("0775");
 * special bits make 5 ("02755"). */
function formatOctal(bits: number): string {
  return '0' + (bits & 0o7777).toString(8).padStart(3, '0')
}

const bits = computed<number>(() => parseOctal(props.modelValue))

/* Per-bit accessors — each emits an updated octal string on toggle.
 * Bit positions follow the standard chmod layout:
 *
 *   special  owner   group   other
 *   [s u s]  [r w x] [r w x] [r w x]
 *   bits 11..9, 8..6, 5..3, 2..0
 */
function bit(mask: number): boolean {
  return (bits.value & mask) !== 0
}

function toggle(mask: number, on: boolean) {
  if (isReadonly.value) return
  const next = on ? bits.value | mask : bits.value & ~mask
  emit('update:modelValue', formatOctal(next))
}

const ownerR = computed({ get: () => bit(0o400), set: (v) => toggle(0o400, v) })
const ownerW = computed({ get: () => bit(0o200), set: (v) => toggle(0o200, v) })
const ownerX = computed({ get: () => bit(0o100), set: (v) => toggle(0o100, v) })
const groupR = computed({ get: () => bit(0o040), set: (v) => toggle(0o040, v) })
const groupW = computed({ get: () => bit(0o020), set: (v) => toggle(0o020, v) })
const groupX = computed({ get: () => bit(0o010), set: (v) => toggle(0o010, v) })
const otherR = computed({ get: () => bit(0o004), set: (v) => toggle(0o004, v) })
const otherW = computed({ get: () => bit(0o002), set: (v) => toggle(0o002, v) })
const otherX = computed({ get: () => bit(0o001), set: (v) => toggle(0o001, v) })

const setuid = computed({ get: () => bit(0o4000), set: (v) => toggle(0o4000, v) })
const setgid = computed({ get: () => bit(0o2000), set: (v) => toggle(0o2000, v) })
const sticky = computed({ get: () => bit(0o1000), set: (v) => toggle(0o1000, v) })

const hasSpecialBits = computed(() => (bits.value & 0o7000) !== 0)

/* Octal text input. Accepts 3 or 4 octal digits with optional
 * leading 0; reformats to canonical leading-0 form on commit.
 * Invalid input doesn't propagate — we hold the prior bits until
 * the input becomes valid. */
const octalDisplay = computed<string>(() => formatOctal(bits.value))

function onOctalInput(ev: Event) {
  const raw = (ev.target as HTMLInputElement).value
  if (!/^0?[0-7]{3,4}$/.test(raw)) return
  const parsed = Number.parseInt(raw, 8)
  if (!Number.isFinite(parsed)) return
  emit('update:modelValue', formatOctal(parsed))
}
</script>

<template>
  <div class="ifld">
    <label class="ifld__label" :for="prop.id + '-octal'">
      <span v-tooltip.right="(access.quicktips && prop.description) || ''">
        {{ prop.caption ?? prop.id }}
      </span>
    </label>

    <div class="ifld-perm">
      <!--
        Permission matrix as a CSS grid rather than an HTML <table> —
        the cells are interactive form controls, not tabular data, so
        the table semantics tripped accessibility checkers. Each
        checkbox carries an `aria-label` that fully names its
        Owner/Group/Other × R/W/X position; the visible row + column
        headings are decoration aimed at sighted users and marked
        `aria-hidden` to avoid screen-reader double-up. The visible
        column-header tooltips (Read/Write/Execute meanings) ride on
        spans rather than table headers.
      -->
      <div class="ifld-perm__matrix" aria-hidden="false">
        <span class="ifld-perm__matrix-spacer" aria-hidden="true"></span>
        <span
          v-tooltip.top="'Read — list directory contents / read file bytes'"
          class="ifld-perm__matrix-colhead"
          aria-hidden="true"
        >R</span>
        <span
          v-tooltip.top="'Write — create/delete files / modify file'"
          class="ifld-perm__matrix-colhead"
          aria-hidden="true"
        >W</span>
        <span
          v-tooltip.top="'Execute — traverse directory / run as program'"
          class="ifld-perm__matrix-colhead"
          aria-hidden="true"
        >X</span>

        <span class="ifld-perm__matrix-rowhead" aria-hidden="true">Owner</span>
        <input
          v-model="ownerR"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Owner read"
        />
        <input
          v-model="ownerW"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Owner write"
        />
        <input
          v-model="ownerX"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Owner execute"
        />

        <span class="ifld-perm__matrix-rowhead" aria-hidden="true">Group</span>
        <input
          v-model="groupR"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Group read"
        />
        <input
          v-model="groupW"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Group write"
        />
        <input
          v-model="groupX"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Group execute"
        />

        <span class="ifld-perm__matrix-rowhead" aria-hidden="true">Other</span>
        <input
          v-model="otherR"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Other read"
        />
        <input
          v-model="otherW"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Other write"
        />
        <input
          v-model="otherX"
          type="checkbox"
          :disabled="isReadonly"
          aria-label="Other execute"
        />
      </div>

      <div class="ifld-perm__octal">
        <label :for="prop.id + '-octal'" class="ifld-perm__octal-label">Octal</label>
        <input
          :id="prop.id + '-octal'"
          class="ifld-perm__octal-input"
          type="text"
          :value="octalDisplay"
          :disabled="isReadonly"
          maxlength="5"
          inputmode="numeric"
          autocomplete="off"
          spellcheck="false"
          @input="onOctalInput"
        />
      </div>
    </div>

    <!--
      Special bits (setuid / setgid / sticky). Default-collapsed —
      rarely touched outside of niche cases like setgid on a
      shared-recordings directory. Auto-opens when the current value
      already has any of these bits set so the user sees them.
    -->
    <details class="ifld-perm__special" :open="hasSpecialBits">
      <summary class="ifld-perm__special-title">Advanced (special bits)</summary>
      <div class="ifld-perm__special-row">
        <label v-tooltip.right="'Run with file owner\'s privileges (rare on directories)'">
          <input v-model="setuid" type="checkbox" :disabled="isReadonly" />
          setuid
        </label>
        <label
          v-tooltip.right="
            'New files inherit directory\'s group — useful for shared recording dirs'
          "
        >
          <input v-model="setgid" type="checkbox" :disabled="isReadonly" />
          setgid
        </label>
        <label v-tooltip.right="'Only owner can delete their files in this directory'">
          <input v-model="sticky" type="checkbox" :disabled="isReadonly" />
          sticky
        </label>
      </div>
    </details>
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

.ifld-perm {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  align-items: flex-start;
}

.ifld-perm__matrix {
  display: grid;
  grid-template-columns: auto auto auto auto;
  align-items: center;
  font-size: var(--tvh-text-md);
  column-gap: 8px;
  row-gap: 2px;
}

.ifld-perm__matrix-rowhead {
  text-align: right;
  font-weight: 500;
  padding-right: 4px;
}

.ifld-perm__matrix-colhead {
  font-weight: 500;
  color: var(--tvh-text-muted);
  text-align: center;
  cursor: help;
}

.ifld-perm__matrix input[type='checkbox'] {
  margin: 0 auto;
  cursor: pointer;
}

.ifld-perm__matrix input[type='checkbox']:disabled {
  cursor: not-allowed;
}

.ifld-perm__octal {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.ifld-perm__octal-label {
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}

.ifld-perm__octal-input {
  width: 64px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  font-size: var(--tvh-text-md);
  font-variant-numeric: tabular-nums;
  text-align: center;
}

.ifld-perm__octal-input:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.ifld-perm__octal-input:disabled {
  opacity: 0.7;
  cursor: not-allowed;
}

.ifld-perm__special {
  font-size: var(--tvh-text-md);
}

.ifld-perm__special-title {
  cursor: pointer;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
  user-select: none;
}

.ifld-perm__special-row {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  margin-top: 6px;
  padding-left: 12px;
}

.ifld-perm__special-row label {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  cursor: pointer;
}

.ifld-perm__special-row input[type='checkbox'] {
  margin: 0;
}
</style>
