<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SmartExpressionEditor — the autorec `expression` field's combined
 * editor: a monospace JSONC textarea (the raw text is the stored
 * representation, so comments survive round-trip by construction), a
 * parsed branch tree with per-branch skip toggles, and a
 * server-side match preview.
 *
 * The textarea is the source of truth. The tree is a projection of a
 * structural parse (utils/autorecExpression.ts) and its skip toggles
 * rewrite the text with minimal span edits, never re-serializing —
 * comments and formatting stay byte-for-byte. Semantic validation is
 * NOT duplicated client-side: the preview endpoint runs the server's
 * real parser and matcher and its error text is shown verbatim
 * (the save path has no error channel, preview is the
 * pre-validation surface).
 *
 * Preview posts the WHOLE form state (via IdnodeEditor's formContext),
 * not just the expression: settings like maxsched feed the disposition
 * annotations, and the same action works for flat rules once this
 * widget's preview block is reused there (the follow-up PR).
 *
 * Used via the IdnodeEditor `fieldGroups` hook, keyed on
 * ['expression'] — see AutorecsView.vue.
 */
import { computed, ref, useId, watch } from 'vue'
import type { IdnodeProp, FieldGroupFormContext } from '@/types/idnode'
import { apiCall } from '@/api/client'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { fmtDate } from '@/utils/formatTime'
import { useI18n } from '@/composables/useI18n'
import {
  parseExpression,
  toggleSkip,
  offsetToLineCol,
  type ExprNode,
} from '@/utils/autorecExpression'

const { t } = useI18n()
const labelId = useId()

const props = defineProps<{
  groupProps: Record<string, IdnodeProp>
  groupValues: Record<string, unknown>
  disabled?: boolean
  formContext?: FieldGroupFormContext
}>()

const emit = defineEmits<{
  update: [id: string, value: unknown]
}>()

const KEY = 'expression'

/* Cap the preview list the server returns; matched/scanned counts
 * stay full-scan regardless (the endpoint flags truncation). */
const PREVIEW_LIMIT = 100

const text = ref(typeof props.groupValues[KEY] === 'string'
  ? (props.groupValues[KEY] as string) : '')

/* External updates (row load, editor reset) replace local text; the
 * guard avoids clobbering in-flight typing when our own emit echoes
 * back through groupValues. Contract with the form layer: the parent
 * must echo the emitted string byte-for-byte — any trim/normalize
 * there would clobber the caret mid-edit. */
watch(() => props.groupValues[KEY], (v) => {
  const s = typeof v === 'string' ? v : ''
  if (s !== text.value) text.value = s
})

function onInput(): void {
  emit('update', KEY, text.value)
}

const parsed = computed(() => parseExpression(text.value))

const parseErrorLine = computed(() => {
  const p = parsed.value
  if (!p.error) return ''
  const { line, col } = offsetToLineCol(text.value, p.errorOffset)
  return t('Expression error at {0}:{1}: {2}')
    .replace('{0}', String(line)).replace('{1}', String(col))
    .replace('{2}', p.error)
})

/* Flat projection of the tree for a simple v-for render — one row
 * per node with its depth. Vue-friendlier than recursion for a
 * strictly presentational tree. */
interface TreeRow {
  node: ExprNode
  depth: number
  label: string
  /* a node inside a skipped subtree renders muted even without its
   * own marker */
  inSkipped: boolean
}

const treeRows = computed<TreeRow[]>(() => {
  const rows: TreeRow[] = []
  const walk = (n: ExprNode, depth: number, inSkipped: boolean) => {
    const label = n.children.length === 0 && n.valueText
      ? `${n.key}: ${n.valueText}` : n.key
    rows.push({ node: n, depth, label, inSkipped })
    for (const ch of n.children) walk(ch, depth + 1, inSkipped || n.skip)
  }
  if (parsed.value.root) walk(parsed.value.root, 0, false)
  return rows
})

function onToggleSkip(row: TreeRow): void {
  if (props.disabled) return
  /* the root has no skip toggle (server validation error); the
   * template hides the control, this is the belt */
  if (row.node === parsed.value.root) return
  text.value = toggleSkip(text.value, row.node, !row.node.skip)
  emit('update', KEY, text.value)
}

/*
 * The master toggle: the root row's switch is the
 * rule-level `enabled` SETTINGS field — visual unity with the branch
 * skip column, storage separation from the expression. This is why
 * the root has no `skip`: the top switch of the tree IS the rule's
 * on/off, and it is never serialized into the JSONC. Written through
 * the same update path as any form field, so dirty tracking and
 * diff-save apply; the ordinary enabled form row shows the same
 * value, kept consistent by formContext reactivity.
 */
const ruleEnabled = computed(() =>
  props.formContext ? props.formContext.values.enabled !== false : true)

function onToggleEnabled(): void {
  if (props.disabled || !props.formContext) return
  emit('update', 'enabled', !ruleEnabled.value)
}

/* --- preview ----------------------------------------------------- */

interface PreviewEntry {
  eventId: number
  channelName?: string
  title?: string
  subtitle?: string
  start: number
  stop: number
  disposition: string
  /* set when the rule would create the entry but the "Duplicate
   * handling" layer would skip it at recording start */
  duplicate?: number
}
interface PreviewResponse {
  error?: string
  entries?: PreviewEntry[]
  matched?: number
  scanned?: number
  truncated?: number
}

const previewing = ref(false)
const preview = ref<PreviewResponse | null>(null)
const previewFailed = ref('')

/* an edited expression invalidates shown results: the match table
 * must never claim to describe text it did not preview */
watch(text, () => {
  preview.value = null
  previewFailed.value = ''
})

async function runPreview(): Promise<void> {
  previewing.value = true
  previewFailed.value = ''
  try {
    /* same conf shape as a create: current form values, empties
     * omitted, plus the uuid when editing an existing rule so the
     * schedules-limit disposition seeds from its live spawns */
    const values = props.formContext?.values ?? { [KEY]: text.value }
    const conf: Record<string, unknown> = {}
    for (const k of Object.keys(values)) {
      const v = values[k]
      if (v === null || v === undefined || v === '') continue
      conf[k] = v
    }
    conf[KEY] = text.value
    if (props.formContext?.uuid) conf.uuid = props.formContext.uuid
    preview.value = await apiCall<PreviewResponse>('dvr/autorec/preview', {
      conf: JSON.stringify(conf),
      limit: PREVIEW_LIMIT,
    })
  } catch (e) {
    preview.value = null
    previewFailed.value = apiErrorMessage(e)
  } finally {
    previewing.value = false
  }
}

const dispositionLabels: Record<string, string> = {
  /* i18n: new strings */
  record: t('will record'),
  scheduled: t('already scheduled'),
  maxsched: t('over schedules limit'),
}

const prop = computed(() => props.groupProps[KEY])
</script>

<template>
  <div
    class="sxe"
    role="group"
    :aria-labelledby="labelId"
  >
    <label
      :id="labelId"
      :for="labelId + '-text'"
      class="ifld__label sxe__label"
    >
      {{ prop?.caption ?? t('Smart expression') }}
    </label>

    <textarea
      :id="labelId + '-text'"
      v-model="text"
      class="sxe__text"
      :disabled="disabled"
      rows="8"
      spellcheck="false"
      :placeholder="t('An expression is required. See Help for the expression language.')"
      @input="onInput"
    />

    <p
      v-if="parseErrorLine"
      class="sxe__error"
    >
      {{ parseErrorLine }}
    </p>
    <p
      v-else-if="parsed.matchesNothing"
      class="sxe__hint"
    >
      {{ t('All branches are skipped — the rule matches nothing until a branch is re-enabled.') }}
    </p>

    <ul
      v-if="treeRows.length > 0"
      class="sxe__tree"
      :class="{ 'sxe__tree--disabled': formContext && !ruleEnabled }"
    >
      <li
        v-for="row in treeRows"
        :key="row.node.start"
        class="sxe__tree-row"
        :class="{ 'sxe__tree-row--skipped': row.node.skip || row.inSkipped }"
        :style="{ paddingLeft: `${row.depth * 1.25}rem` }"
      >
        <label
          v-if="row.node !== parsed.root"
          class="sxe__skip"
        >
          <input
            type="checkbox"
            :checked="!row.node.skip"
            :disabled="disabled || row.inSkipped"
            :aria-label="t('Branch participates in matching')"
            @change="onToggleSkip(row)"
          >
        </label>
        <label
          v-else-if="formContext"
          class="sxe__skip"
          :title="t('The whole rule\'s Enabled setting - branches use skip')"
        >
          <input
            type="checkbox"
            :checked="ruleEnabled"
            :disabled="disabled"
            :aria-label="t('Rule enabled')"
            @change="onToggleEnabled"
          >
        </label>
        <span
          v-else
          class="sxe__skip sxe__skip--root"
        />
        <code class="sxe__tree-label">{{ row.label }}</code>
      </li>
    </ul>

    <div class="sxe__preview-bar">
      <button
        type="button"
        class="sxe__preview-btn"
        :disabled="disabled || previewing"
        @click="runPreview"
      >
        {{ previewing ? t('Previewing…') : t('Preview matches') }}
      </button>
      <span
        v-if="preview && !preview.error"
        class="sxe__counts"
      >
        {{ t('{0} of {1} EPG events match')
          .replace('{0}', String(preview.matched ?? 0))
          .replace('{1}', String(preview.scanned ?? 0)) }}
        <template v-if="preview.truncated">
          — {{ t('first {0} shown').replace('{0}', String(PREVIEW_LIMIT)) }}
        </template>
      </span>
    </div>

    <p
      v-if="previewFailed"
      class="sxe__error"
    >
      {{ previewFailed }}
    </p>
    <p
      v-else-if="preview?.error"
      class="sxe__error"
    >
      {{ preview.error }}
    </p>

    <table
      v-else-if="preview && (preview.entries?.length ?? 0) > 0"
      class="sxe__results"
    >
      <thead>
        <tr>
          <th>{{ t('Start') }}</th>
          <th>{{ t('Channel') }}</th>
          <th>{{ t('Title') }}</th>
          <th>{{ t('Disposition') }}</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="e in preview.entries"
          :key="e.eventId"
        >
          <td>{{ fmtDate(e.start) }}</td>
          <td>{{ e.channelName }}</td>
          <td>
            {{ e.title }}
            <span
              v-if="e.subtitle"
              class="sxe__subtitle"
            >— {{ e.subtitle }}</span>
          </td>
          <td
            v-if="e.duplicate"
            class="sxe__dup"
            :title="t('Will be skipped because it is a rerun (Duplicate handling). It still records if the earlier showing fails; other rules are not affected.')"
          >
            {{ t('duplicate') }}
          </td>
          <td v-else>{{ dispositionLabels[e.disposition] ?? e.disposition }}</td>
        </tr>
      </tbody>
    </table>
    <p
      v-else-if="preview && !preview.error"
      class="sxe__hint"
    >
      {{ t('No matches within the EPG horizon (typically 7–14 days). An empty preview does not prove the rule wrong — the target may not be in the current guide window.') }}
    </p>
  </div>
</template>

<style scoped>
.sxe {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
  width: 100%;
}

.sxe__label {
  font-weight: 600;
}

.sxe__text {
  font-family: monospace;
  font-size: var(--tvh-text-sm);
  width: 100%;
  resize: vertical;
  background: var(--tvh-bg-surface);
  color: inherit;
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: var(--tvh-space-2);
}

.sxe__error {
  color: var(--tvh-error);
  margin: 0;
}

.sxe__hint {
  color: var(--tvh-text-muted);
  margin: 0;
}

.sxe__tree {
  list-style: none;
  margin: 0;
  padding: var(--tvh-space-2);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
}

.sxe__tree-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  line-height: 1.7;
}

.sxe__tree-row--skipped .sxe__tree-label {
  color: var(--tvh-text-muted);
  text-decoration: line-through;
}

/* rule-level disable dims the whole tree (no strikethrough — the
 * definition is intact, the rule just is not running) */
.sxe__tree--disabled .sxe__tree-label {
  color: var(--tvh-text-muted);
}

.sxe__skip {
  display: inline-flex;
  width: 1.25rem;
  justify-content: center;
}

.sxe__tree-label {
  font-size: var(--tvh-text-sm);
}

.sxe__preview-bar {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
}

.sxe__preview-btn {
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  background: var(--tvh-bg-surface);
  color: inherit;
  padding: var(--tvh-space-1) var(--tvh-space-3);
  cursor: pointer;
}

.sxe__preview-btn:disabled {
  opacity: 0.6;
  cursor: default;
}

.sxe__counts {
  color: var(--tvh-text-muted);
}

.sxe__results {
  border-collapse: collapse;
  font-size: var(--tvh-text-sm);
  width: 100%;
}

.sxe__results th,
.sxe__results td {
  text-align: left;
  padding: var(--tvh-space-1) var(--tvh-space-2);
  border-bottom: 1px solid var(--tvh-border);
}

.sxe__subtitle {
  color: var(--tvh-text-muted);
}

/* the ExtJS duplicate convention (x-epg-duplicate): line-through */
.sxe__dup {
  color: var(--tvh-text-muted);
  text-decoration: line-through;
}
</style>
