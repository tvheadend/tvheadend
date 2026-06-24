<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeEditor — slide-in drawer for editing a single idnode entity.
 *
 * Generic over any idnode class. Inputs:
 *   - uuid:  null (closed) or row uuid (open)
 *   - level: 'basic' / 'advanced' / 'expert' (inherited from grid)
 *   - list:  optional CSV of field IDs. When set, passed to
 *            api/idnode/load so the SERVER trims the property list
 *            to just those fields (idnode_serialize0 + idclass_serialize0
 *            both honor `?list=...`). This mirrors what ExtJS does:
 *            see e.g. `tvheadend.dvr_upcoming` in dvr.js where the
 *            edit dialog passes a curated property list. Without it
 *            the editor renders every property the class declares —
 *            including read-only computed mirrors (channelname,
 *            disp_*, watched, status, …) which clutter the form.
 *
 * Pipeline:
 *   1. Fetch row data + class metadata via api/idnode/load (`meta=1`,
 *      optional `list=...`). The response is `{entries:[{params:[…]}]}`.
 *   2. Bucket fields into sections. When the server declares named
 *      property groups (`meta.groups` on edit, `groups` on the
 *      create class response) those drive the section split,
 *      mirroring the singleton-config form's behaviour. Classes
 *      without groups fall back to the level-based split — Basic /
 *      Advanced / Expert / Read-only Info — same as the old UI
 *      (idnode.js:887-1059); Read-only Info renders collapsed by
 *      default.
 *   3. For each visible field, dispatch to the per-type component
 *      (string / number / bool / time / enum). Field types not yet
 *      handled (langstr, perm, hexa, intsplit) render a placeholder
 *      row so users can see the field exists rather than getting
 *      silent gaps.
 *   4. Track dirty state per-field; on save, post only the fields
 *      whose current value differs from initial.
 *
 * Concurrent-edit handling: matches the legacy ExtJS UI's
 * "last-save-wins" behaviour. The server has no version/ETag/mtime
 * field on idnodes, so neither client can detect a conflict.
 *
 * Class-defined named groups (e.g. profile, caclient, cclient, wizard
 * steps — `idclass_t.ic_groups` in C) are honoured: when the server
 * returns a non-empty `meta.groups` (edit) or `groups` (create) array,
 * the editor renders a fieldset per root group, with sub-groups
 * flat-merged into their parent (same pattern as IdnodeConfigForm).
 * Classes without groups (DVR, channel, …) keep the level-based
 * split so visual behaviour there is unchanged.
 */
import { computed, onBeforeUnmount, onMounted, ref, watch, type Component } from 'vue'
import Drawer from 'primevue/drawer'
import Checkbox from 'primevue/checkbox'
import EntityPickerTable from './EntityPickerTable.vue'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import type { IdnodeNotification } from '@/types/comet'
import type { UiLevel } from '@/types/access'
import type { IdnodeProp, PropertyGroup } from '@/types/idnode'
import type { PickerColumn, PickerRow } from '@/types/picker'
import { levelMatches, propLevel } from '@/types/idnode'

import { useAccessStore } from '@/stores/access'

import { rendererFor, valueFor } from './idnode-fields/rendererDispatch'
import { validateField } from './idnode-fields/validators'
import { applyClassRules, isFieldRequired } from './idnode-fields/validationRules'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useErrorDialog } from '@/composables/useErrorDialog'
import { apiErrorMessage } from '@/utils/apiErrorMessage'
import { buildMultiEditPayload } from '@/api/multiEditIdnode'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = defineProps<{
  /* uuid of the row to edit; null/undefined means closed. */
  uuid: string | null
  /*
   * Create mode trigger. When set (and `uuid` is null), the editor
   * opens an empty form for creating a new entry of this idnode
   * class. Mutually exclusive with `uuid` — pass exactly one. The
   * value is the API base path for the per-class idnode
   * endpoints, e.g. `'dvr/entry'` → metadata fetch from
   * `api/dvr/entry/class`, save POST to `api/dvr/entry/create`.
   * The convention is consistent across every idnode class
   * (verified for dvr, channel, network, passwd, access, …).
   * Mirrors ExtJS's `conf.url` shape in `tvheadend.idnode_create`.
   */
  createBase?: string | null
  /* Effective view level inherited from the calling grid. */
  level: UiLevel
  /* Optional title override; default is "Edit". */
  title?: string
  /*
   * Optional comma-separated allowlist of property IDs. When set,
   * forwarded to api/idnode/load — the server returns only those
   * properties (in both the `params` value list and the `meta.props`
   * static metadata). Mirrors the ExtJS pattern where each editor
   * caller curates its own field list (e.g. dvr.js' `elist`).
   * Server-driven editor-field-list metadata would replace this
   * hardcoded per-caller list — see ExtJS-pattern parity above.
   */
  list?: string
  /*
   * For multi-subclass create endpoints (mpegts/network/create,
   * …): the specific subclass the user picked (e.g.
   * `'dvb_network_dvbt'`). The create-strategy resolver routes
   * metadata to `api/idnode/class?name=<subclass>` and adds
   * `class=<subclass>` to the create POST body. Single-class
   * consumers (DVR Upcoming, Configuration → Users) leave this
   * null and get the default `<createBase>/class` +
   * `<createBase>/create` behaviour. Mutually exclusive with
   * `parentScoped` — if both are set, `parentScoped` wins.
   */
  subclass?: string | null
  /*
   * For "parent-scoped" create flows where the new entry's class
   * is implied by a parent entity's identity. Example: Mux
   * creation belongs to a network — the network's
   * `mn_mux_class()` callback dispatches on the network's type,
   * so the client sends the network UUID instead of an explicit
   * mux class. When set in create mode:
   *   - metadata fetch: GET `<classEndpoint>` with `<params>`
   *     (e.g. `mpegts/network/mux_class?uuid=<network_uuid>`)
   *   - save POST: `<createEndpoint>` with `<params>` merged
   *     into the body alongside `conf` (e.g.
   *     `mpegts/network/mux_create` with `{uuid, conf}`)
   * Both endpoints REPLACE the default `<createBase>/class` +
   * `<createBase>/create` shape. Mutually exclusive with
   * `subclass` (parentScoped wins on the precedence ladder).
   */
  parentScoped?: {
    classEndpoint: string
    createEndpoint: string
    params: Record<string, unknown>
  } | null
  /*
   * Multi-edit trigger. When set with length >= 2, the drawer
   * opens in multi-edit mode: the form is loaded from the FIRST
   * uuid (used as a template) and each editable row gains an
   * apply-checkbox at the leftmost column. Save POSTs the
   * Case-2 shape `idnode/save` accepts at
   * `src/api/api_idnode.c:408-429` — `{node: {uuid: [...], ...}}`
   * — applying only ticked fields to every selected row in one
   * call. Mutually exclusive with `uuid` and `createBase`.
   *
   * Length 0 → drawer treated as closed. Length 1 — `useEditorMode`
   * normalises single-row selections to `uuid` so this prop only
   * carries 2+ in practice; defending the length>=2 branch keeps
   * the editor's mode dispatch unambiguous.
   */
  uuids?: string[] | null
  /*
   * Allowlist of multi-select enum field IDs to render as inline
   * checkboxes instead of the default MultiSelect dropdown. Right
   * for known-small fixed sets (e.g. DVR Autorec / Timer
   * `weekdays`, exactly 7 entries). Anything not on this list
   * renders as a dropdown — including small variable-size lists,
   * because option count alone isn't a reliable signal.
   */
  inlineEnumMultiFields?: ReadonlyArray<string>
  /*
   * Create-mode initial values, overlaid on top of the class
   * defaults after `loadCreate` resolves. Used by "Clone-as-Add"
   * flows that open the create form pre-filled with the source
   * row's values (the v2 codec-profile Clone mirrors ExtJS's
   * `idnode_create(conf.add, true, currentValues)` shape at
   * `static/app/idnode.js:2378-2388`). Has no effect in edit
   * mode — load resolves the per-instance values from the server.
   */
  preselect?: Readonly<Record<string, unknown>> | null
  /*
   * Picker mode. When `pickerRows` carries 2+ entries, a compact
   * single-select table renders at the top of the drawer body and
   * `uuid` is the currently-selected row — the one the form below
   * edits. Switching rows is the parent's job: a row click emits
   * `pick`, the parent updates `uuid`. `pickerColumns` describes the
   * table's columns. Distinct from `uuids` multi-edit — the picker
   * edits one row at a time, never all at once.
   */
  pickerRows?: PickerRow[] | null
  pickerColumns?: PickerColumn[] | null
  /*
   * Pair- (or n-tuple-) field renderers. Each entry describes a
   * set of related field ids that should render together via one
   * combined component instead of N independent per-field rows.
   * The FIRST key of each `keys` array is where the combined
   * component mounts in section order; all other keys are
   * suppressed from the per-field render loop (no row, no
   * apply-checkbox, no error slot — the combined component owns
   * the visual). On change, the combined component emits
   * `update(id, value)` per affected key, wired into the same
   * `onFieldChange(id, value)` path the per-field renderers use,
   * so dirty tracking + diff-save + comet conflict markers all
   * continue to work per-field.
   *
   * Multi-edit mode falls back to per-field rendering — the
   * paired apply-checkbox semantics aren't worth designing for the
   * single class that uses this hook today.
   *
   * The combined component receives both `groupProps` (keyed by
   * field id) and `groupValues` (keyed by field id) plus a
   * `disabled` flag. See StartWindowRangePicker.vue for the
   * concrete prop / emit contract.
   */
  fieldGroups?: ReadonlyArray<{
    keys: ReadonlyArray<string>
    component: Component
  }>
}>()

const emit = defineEmits<{
  close: []
  saved: []
  /*
   * Fired after a successful CREATE round-trip. Carries the new
   * entry's UUID so the parent can flip the editor from create mode
   * (`createBase` set) to edit mode (`uuid` set) — the typical Apply
   * flow on a fresh record. Without this, Apply in create mode would
   * either close the drawer (defeating the point) or leave it in
   * create mode looking at the same form (the user's next click
   * would create a duplicate).
   *
   * Edit-mode Apply doesn't fire `created` (the entry already exists
   * — the parent doesn't need to flip anything).
   */
  created: [uuid: string]
  /*
   * Fired in picker mode when the user selects a different table
   * row (after a discard-confirm if the form was dirty). The parent
   * updates `uuid` to the carried value.
   */
  pick: [uuid: string]
}>()

/* ---- Drawer visibility & lifecycle ---- */

/*
 * PrimeVue Drawer's v-model:visible is two-way: the close X / mask /
 * Esc all toggle it to false. We translate that into a `close` emit
 * for the parent; the parent owns the `uuid` prop that drives whether
 * we mount at all.
 */
const isCreate = computed(
  () => !!props.createBase && (props.uuid === null || props.uuid === undefined)
)

/* Multi-edit is on when `uuids` carries 2+ entries. Length 0
 * treats the drawer as closed (no row selected); length 1 is
 * normalised to single-edit by `useEditorMode.openEditor`, so it
 * shouldn't reach the editor — but the >= 2 guard keeps the
 * mode dispatch clean if it ever does. */
const isMultiEdit = computed(
  () => Array.isArray(props.uuids) && props.uuids.length >= 2,
)

/* Picker mode is on when `pickerRows` carries 2+ entries — a
 * single-select table renders above the form. A 0/1-entry list
 * never reaches here (useEntityEditor.openList opens a 1-entry
 * list as a plain drill-down). */
const isPicker = computed(
  () => Array.isArray(props.pickerRows) && props.pickerRows.length >= 2,
)

/* Membership lookup for the inline-EnumMulti allowlist. Computed
 * once per prop change; per-field check is O(1). Empty by
 * default — every EnumMulti renders as MultiSelect dropdown
 * unless the caller opts the field in. */
const inlineEnumMultiSet = computed(
  () => new Set(props.inlineEnumMultiFields ?? []),
)

/* Per-id lookup for the fieldGroups hook. Returns the owning
 * group entry + whether the field is the first key (the only key
 * at which the combined component mounts) for any field listed
 * in any group; undefined otherwise. Multi-edit mode short-
 * circuits to undefined-for-everything so the per-field apply-
 * checkbox path stays intact. */
type FieldGroupBinding = {
  group: { keys: ReadonlyArray<string>; component: Component }
  isFirst: boolean
}
const fieldGroupBindings = computed<Map<string, FieldGroupBinding>>(() => {
  const map = new Map<string, FieldGroupBinding>()
  if (isMultiEdit.value) return map
  for (const group of props.fieldGroups ?? []) {
    for (let i = 0; i < group.keys.length; i++) {
      map.set(group.keys[i], { group, isFirst: i === 0 })
    }
  }
  return map
})

function groupBindingFor(id: string): FieldGroupBinding | undefined {
  return fieldGroupBindings.value.get(id)
}

function groupPropsFor(group: FieldGroupBinding['group']): Record<string, IdnodeProp> {
  const out: Record<string, IdnodeProp> = {}
  for (const p of fieldProps.value) {
    if (group.keys.includes(p.id)) out[p.id] = p
  }
  return out
}

function groupValuesFor(group: FieldGroupBinding['group']): Record<string, unknown> {
  const out: Record<string, unknown> = {}
  for (const k of group.keys) {
    out[k] = currentValues.value[k]
  }
  return out
}

function isGroupRowDirty(group: FieldGroupBinding['group']): boolean {
  for (const k of group.keys) {
    if (isFieldDirty(k)) return true
  }
  return false
}

const visible = computed({
  get: () =>
    (props.uuid !== null && props.uuid !== undefined) ||
    isCreate.value ||
    isMultiEdit.value,
  set: (v) => {
    if (!v) attemptClose()
  },
})

/* ---- Form state ---- */

/*
 * api/idnode/load wraps its result in `{ entries: [...] }` (see
 * api_idnode.c:324-326), and each entry's property data is under
 * `params` (idnode_serialize0 in idnode.c:1554), with `meta.props`
 * carrying the static class metadata when the request includes
 * `meta=1`. ExtJS handles either shape via `item.props || item.params`
 * — we match that.
 */
interface LoadEntry {
  uuid?: string
  /* Idnode class identifier (e.g. 'dvrentry'). Top-level on the
   * `idnode/load` response — see idnode_serialize0 in idnode.c:1549.
   * Drives the per-class validation rule lookup
   * (`validationRules.ts`). */
  class?: string
  /* Comet notification event the server emits for this entry's
   * class hierarchy — `idclass_get_event` walks `ic_super` for the
   * nearest `ic_event` (idnode.c:1334-1341) and idnode_serialize0
   * puts it on the wire (idnode.c:1551-1552). Often a superclass
   * name: a 'dvb_mux_dvbt' entry notifies under 'mpegts_mux'.
   * Absent when no class in the chain declares `ic_event`. */
  event?: string
  /* Per-instance display title (e.g. "Channel One",
   * "Match of the Day") emitted server-side via
   * `idnode_get_title` at idnode.c:1546. Used by the default
   * drawer title — "Edit Channel One" — when the caller didn't
   * pass an explicit `title` prop. The class caption
   * (`ic_caption` at idnode.c:1547) is intentionally NOT used
   * for the editor title; it's a nav-tree path string and
   * appending it to "Edit " produces nonsense like "Edit
   * Channels / EPG - Channels". */
  text?: string
  params?: IdnodeProp[]
  props?: IdnodeProp[]
  /* When the client passes `meta=1` to api/idnode/load, the server
   * merges `idclass_serialize0(...)` into this field (api_idnode.c:
   * 366-367). The class serialization carries the per-class `groups`
   * array (idnode.c:1521-1522) when the class declared
   * `ic_groups` — drives the section split. */
  meta?: { groups?: PropertyGroup[]; props?: IdnodeProp[] }
}

interface LoadResponse {
  entries?: LoadEntry[]
}

const loading = ref(false)
const saving = ref(false)
const error = ref<string | null>(null)
const fieldProps = ref<IdnodeProp[]>([])
/* Class-declared named groups (e.g. profile.c declares General /
 * Output / Stream filters). Empty array when the class has no
 * `ic_groups` — `displayedSections` falls back to level buckets.
 * Re-populated on every load; cleared on close or error. */
const propertyGroups = ref<PropertyGroup[]>([])
const initialValues = ref<Record<string, unknown>>({})
const currentValues = ref<Record<string, unknown>>({})
/* Idnode class identifier ('dvrentry', 'dvrautorec', etc.) populated
 * on each load. Drives the per-class validation rule lookup. Null
 * while the drawer is closed or before the first response lands. */
const currentClass = ref<string | null>(null)
/* Server-declared comet event for the loaded entry's class
 * hierarchy (`event` on the load/class response). Notifications
 * fire under the superclass `ic_event` — idnode_notify
 * (idnode.c:1899-1917) walks `ic_super` and emits only where
 * `ic_event` is set — so a leaf class like 'dvb_mux_dvbt' notifies
 * as 'mpegts_mux'. Null when the class chain declares no event;
 * the subscription then falls back to the class name. */
const currentEvent = ref<string | null>(null)
/* Per-instance display title ("Channel One", "Match of the Day",
 * …), populated from the server's load response (idnode.c:1546
 * via `idnode_get_title`). Drives the default drawer title —
 * "Edit Channel One" — when the caller didn't pass an explicit
 * `title` prop. Null while the drawer is closed or before the
 * first response lands. */
const currentInstanceText = ref<string | null>(null)
/* Set of field IDs the user has interacted with this session. Errors
 * stay hidden on untouched fields so opening a drawer with empty
 * required fields doesn't immediately yell at the user; they
 * surface on edit-and-blur or on Save/Apply attempt. */
const touched = ref<Set<string>>(new Set())
/* Flips on the first Save/Apply attempt that ran into validation
 * errors. Promotes every error to "visible" so the user gets a
 * single overview of what's wrong rather than discovering one error
 * per click. Resets per drawer open. */
const submitAttempted = ref(false)

/* Multi-edit: per-field apply-checkbox state. Only consulted when
 * `isMultiEdit` is true. Defaults all to false on drawer open;
 * auto-flips to true when the user edits a field's value
 * (`onFieldChange`) AND can be toggled directly via the checkbox
 * input. Save's payload picks only the keys whose value here is
 * true. Resets every time `uuids` changes (drawer reopens with a
 * fresh selection). */
const applyChecked = ref<Record<string, boolean>>({})

const hasApplyChecks = computed(() =>
  Object.values(applyChecked.value).some(Boolean),
)

/* Drawer title — defers to a caller-supplied `title` prop if
 * any, otherwise falls back to the loaded entry's per-instance
 * title ("Channel One", "Match of the Day", …) emitted server-
 * side via `idnode_get_title` at `idnode.c:1546`. Render shapes:
 *   - Explicit prop          → use it verbatim ("Edit Recording")
 *   - Instance text loaded   → "Edit {text}" ("Edit Channel One")
 *   - Pending / empty / new  → "Edit" (briefly, before first
 *                              load response, or for create
 *                              mode where no instance exists)
 *   - Multi-edit             → appends " (N entries)" to
 *                              whichever base was chosen
 * The class caption (`ic_caption` at idnode.c:1547) is
 * intentionally NOT used here — it's a nav-tree path string
 * (e.g. "Channels / EPG - Channels") authored for the Classic
 * left-nav tree, and appending it to "Edit " produces nonsense.
 * Classic UI uses a separately-hardcoded per-grid `titleS`
 * label for the same reason (`static/app/chconf.js`'s
 * `tvheadend.channel_class`). Existing callers passing
 * explicit `title` props (DVR editor → "Edit Recording") still
 * win over the instance-text fallback so their phrasing is
 * preserved. */
const effectiveTitle = computed(() => {
  let base: string
  if (props.title) {
    base = props.title
  } else if (currentInstanceText.value) {
    base = t('Edit {0}', currentInstanceText.value)
  } else {
    base = t('Edit')
  }
  if (isMultiEdit.value && props.uuids) {
    return t('{0} ({1} entries)', base, props.uuids.length)
  }
  return base
})

/* Per-field dirty tracking. `dirtyIds` is the source of truth — both
 * the global `dirty` flag (used to gate Apply + the Cancel-confirm
 * prompt) AND the per-row dirty marker (dot before label) derive
 * from it. Single pass per render rather than re-comparing per
 * field per render. */
const dirtyIds = computed(() => {
  const ids = new Set<string>()
  for (const k of Object.keys(currentValues.value)) {
    /* JSON.stringify is good enough for the primitive values we
     * round-trip (string / number / boolean / null) and the array
     * shapes the multi-select renderers emit. The only "deep"
     * shape would be langstr objects, which the editor doesn't
     * currently render. */
    if (JSON.stringify(currentValues.value[k]) !== JSON.stringify(initialValues.value[k])) {
      ids.add(k)
    }
  }
  return ids
})

const dirty = computed(() => dirtyIds.value.size > 0)

function isFieldDirty(id: string): boolean {
  return dirtyIds.value.has(id)
}

/* ---- Comet live-update merge --------------------------------------
 *
 * While the drawer is open, the entry can change server-side (another
 * session edits it, recording engine flips its sched_status, etc.).
 * We listen to the entity-class Comet notification, debounced-refetch
 * via `idnode/load`, and merge per-field:
 *
 *   - Field is CLEAN (current === initial): take the server's new
 *     value into both `currentValues` and `initialValues`. The user
 *     sees the live value automatically; the dirty math stays
 *     consistent.
 *   - Field is DIRTY (user edited it): keep `currentValues` (don't
 *     clobber unsaved work). Add to `serverPendingIds` so the visual
 *     dirty marker can flag the conflict ("server has a new value
 *     too"). Don't update `initialValues` — leaving it on the
 *     pre-comet baseline preserves "Cancel = revert to what I
 *     started with" semantics. Lost-update on the user's eventual
 *     Save is the current trade-off; resolving it cleanly is
 *     pending a server-side ETag / mtime concurrency check.
 *
 * `serverPendingIds` clears on Save (we just pushed our values, the
 * server's pending state is now overwritten or our values), on the
 * uuid changing (a different entry has its own state), and on
 * unmount.
 */
const serverPendingIds = ref<Set<string>>(new Set())

function isFieldServerPending(id: string): boolean {
  return serverPendingIds.value.has(id)
}

/* Per-field smart merge. Called on each refetched entry; mutates
 * `currentValues`, `initialValues`, and `serverPendingIds` in place. */
function mergeFromServer(serverParams: IdnodeProp[]): void {
  const dirtySnapshot = dirtyIds.value
  const nextInitial = { ...initialValues.value }
  const nextCurrent = { ...currentValues.value }
  const nextPending = new Set(serverPendingIds.value)
  for (const p of serverParams) {
    const id = p.id
    const serverValue = p.value ?? null
    if (dirtySnapshot.has(id)) {
      /* User has unsaved changes — preserve them. Flag the
       * server's competing value via the pending set so the UI
       * can render the conflict marker. Don't touch initialValues
       * here: revert (Cancel) takes the user back to the
       * pre-comet baseline. */
      nextPending.add(id)
    } else {
      /* Clean field — apply the live update to BOTH baselines so
       * the field stays clean post-merge. Drop from pending in
       * case a previous merge had flagged it (the user reverted
       * by hand to the server's value, or save just landed). */
      nextInitial[id] = serverValue
      nextCurrent[id] = serverValue
      nextPending.delete(id)
    }
  }
  initialValues.value = nextInitial
  currentValues.value = nextCurrent
  serverPendingIds.value = nextPending
}

/* Debounce window for comet → refetch. The recording engine emits
 * `dvrentry` notifications constantly during an active recording
 * (file-size growth, signal stats, error counters); a naive
 * refetch-per-notification would spam the server. 250 ms trailing
 * collapses bursts to one fetch. */
const COMET_REFETCH_DEBOUNCE_MS = 250
let cometRefetchTimer: ReturnType<typeof setTimeout> | null = null

async function refetchAndMerge(uuid: string): Promise<void> {
  try {
    const res = await apiCall<LoadResponse>('idnode/load', { uuid, meta: 1 })
    const entry = res.entries?.[0]
    if (!entry || !props.uuid || entry.uuid !== props.uuid) return
    const serverParams = (entry.params ?? entry.props ?? []) as IdnodeProp[]
    mergeFromServer(serverParams)
    /* Refresh the per-instance title so a rename via Apply (or by
     * another connected client) updates the drawer header within
     * the comet-refetch window. */
    currentInstanceText.value = entry.text ?? currentInstanceText.value
  } catch {
    /* Silent on transient errors — comet will fire again on the
     * next change and we'll try again. The drawer's existing
     * load-error path handles the initial load; live-merge
     * failures don't merit user-visible noise. */
  }
}

function scheduleRefetchAndMerge(uuid: string): void {
  if (cometRefetchTimer !== null) clearTimeout(cometRefetchTimer)
  cometRefetchTimer = setTimeout(() => {
    cometRefetchTimer = null
    void refetchAndMerge(uuid)
  }, COMET_REFETCH_DEBOUNCE_MS)
}

function onCometNotification(msg: unknown): void {
  const note = msg as IdnodeNotification
  const uuid = props.uuid
  if (!uuid) return
  /* Only react when this entry's uuid is in the change set —
   * other entries' notifications are noise to this drawer. */
  if (!note.change?.includes(uuid)) return
  scheduleRefetchAndMerge(uuid)
}

/* Subscribe to comet notifications keyed on the server-declared
 * event name, falling back to the class name when the class chain
 * declares no `ic_event`. Comet dispatch is exact-match on
 * `notificationClass`, and the server notifies under the superclass
 * event (e.g. 'mpegts_mux' for every mux leaf class) — subscribing
 * to the leaf class name would never fire for those hierarchies.
 * Both arrive via the load response, so this watcher fires once
 * after first load and again whenever they change (rare — only
 * relevant on drawer reuse). Ref-counted via the Unsubscribe
 * handle to avoid stale listeners across flips. */
let cometUnsub: (() => void) | null = null

watch(
  () => currentEvent.value ?? currentClass.value,
  (newEvent) => {
    if (cometUnsub) {
      cometUnsub()
      cometUnsub = null
    }
    if (newEvent) {
      cometUnsub = cometClient.on(newEvent, onCometNotification)
    }
  },
)

/* ---- Validation ----
 *
 * Two layers per render:
 *
 *   1. Per-field type-driven (`validateField`) — covers what
 *      prop_t metadata declares: integer / float ranges, hex format,
 *      intsplit format, enum membership.
 *   2. Per-class hardcoded (`applyClassRules`) — required + cross-
 *      field comparisons + minLength. Sourced from C set-callbacks
 *      and create-handlers; see validationRules.ts.
 *
 * Per-field errors win on the same field (a "must be a number" beats
 * a cross-field "stop must be after start" when stop is also bad).
 *
 * `errors` is the full picture; `visibleError(id)` gates display on
 * the touched + submitAttempted UX rules.
 */

/* Pristine values are exempt from validation in edit / multi-edit
 * mode. The server legitimately stores values the client-side rules
 * reject — e.g. a timerec start/stop getter returns the literal
 * "Any" (dvr_timerec.c:407) while its enum list only carries a
 * translated entry (dvr_timerec.c:426-429) — so flagging an
 * untouched field would lock Save/Apply on an entry the user never
 * broke. A field validates once its value differs from what the
 * server sent. Create mode validates everything: the loaded values
 * are class defaults and required-field rules must fire before the
 * first submit. Multi-edit additionally validates ticked-but-
 * unedited fields — ticking means "write this value to every row",
 * a user action worth checking. */
function shouldValidate(id: string): boolean {
  if (isCreate.value) return true
  if (isMultiEdit.value) return dirtyIds.value.has(id) || !!applyChecked.value[id]
  return dirtyIds.value.has(id)
}

const errors = computed<Map<string, string>>(() => {
  const map = new Map<string, string>()
  /* Per-field type-driven first */
  for (const p of fieldProps.value) {
    if (p.rdonly || p.noui || p.phidden) continue
    if (!shouldValidate(p.id)) continue
    const e = validateField(p, currentValues.value[p.id])
    if (e) map.set(p.id, e)
  }
  /* Class-level rules (required, cross-field, minLength) — only
   * fill IDs that don't already carry a per-field error AND only
   * for fields actually loaded into the editor. Limited edit lists
   * (e.g. DVR Finished's `disp_title,…,comment` — omits start /
   * stop / channel because they're read-only on a finished
   * recording) would otherwise see CLASS_RULES.dvrentry's
   * `required: ['start', 'stop', 'channel']` flag the absent
   * fields as empty, locking Save permanently. */
  const loadedIds = new Set(fieldProps.value.map((p) => p.id))
  for (const [id, msg] of applyClassRules(currentClass.value, currentValues.value)) {
    if (loadedIds.has(id) && shouldValidate(id) && !map.has(id)) map.set(id, msg)
  }
  return map
})

const hasErrors = computed(() => errors.value.size > 0)

function visibleError(id: string): string | null {
  if (!touched.value.has(id) && !submitAttempted.value) return null
  return errors.value.get(id) ?? null
}

function isRequired(id: string): boolean {
  return isFieldRequired(currentClass.value, id)
}

/* Soft-target lengths for known string fields. Pure UX hint — the
 * server enforces no maximum on PT_STR. Counter renders only when
 * length > 80 % of the soft target. Unknown field IDs fall through
 * to the multiline-only counter (see lengthHint below). */
const SOFT_LENGTH_TARGETS: Record<string, number> = {
  name: 200,
  title: 200,
  caption: 200,
  pattern: 200,
  comment: 2000,
  description: 2000,
  notes: 2000,
}

interface LengthHint {
  text: string
  /** True when the user has crossed the soft target — counter goes warning-tinted. */
  warn: boolean
}

function lengthHint(prop: IdnodeProp): LengthHint | null {
  const value = currentValues.value[prop.id]
  if (typeof value !== 'string') return null
  const len = value.length
  if (len === 0) return null
  const soft = SOFT_LENGTH_TARGETS[prop.id]
  if (soft) {
    if (len < Math.floor(soft * 0.8)) return null
    return { text: `${len.toLocaleString()} / ${soft.toLocaleString()}`, warn: len > soft }
  }
  /* No soft target — multiline gets a plain counter when text grows
   * past the "obviously short" threshold; single-line strings stay
   * silent (no clutter on every text input). */
  if (prop.multiline && len > 100) {
    return { text: `${len.toLocaleString()} chars`, warn: false }
  }
  return null
}

function onFieldChange(id: string, value: unknown) {
  currentValues.value[id] = value
  touched.value.add(id)
  /* Auto-check the apply-checkbox in multi-edit mode — typing
   * into a field implies "apply this". User can still uncheck
   * explicitly if they want to revert that field. Goes beyond
   * Classic (which is manual-only) per the design decision. */
  if (isMultiEdit.value) applyChecked.value[id] = true
}

/*
 * Level-bucket fallback (only consumed when the class declares no
 * `ic_groups`). Buckets each property into one of four synthetic
 * sections matching the ExtJS editor (idnode.js:887-1059):
 *
 *   - basic     (df): default, visible at all levels
 *   - advanced  (af): only at level >= advanced
 *   - expert    (ef): only at level == expert
 *   - readonly  (rf): rdonly fields, regardless of category
 *
 * Read-only routing wins over level routing — a rdonly+expert field
 * goes into Read-only Info, not Expert Settings (it's still
 * informational; the user can't edit it). When the class DOES
 * declare groups, this routing is bypassed: rdonly fields then
 * render inline in their declared group as disabled inputs.
 */
type Bucket = 'basic' | 'advanced' | 'expert' | 'readonly'

function bucketOf(p: IdnodeProp): Bucket {
  if (p.rdonly) return 'readonly'
  if (p.expert) return 'expert'
  if (p.advanced) return 'advanced'
  return 'basic'
}

/*
 * Editor-local view level.
 *
 * Defaults to whatever the calling grid passed via `props.level` —
 * single source of truth for "what was the user looking at when they
 * opened this row". A small picker in the drawer body lets power
 * users bump the visibility scope of THIS edit session without
 * touching the grid's level (= without changing what columns the
 * underlying grid shows). Reset on every new uuid so opening the
 * next row starts from the inherited level again.
 *
 * Honors access.locked: if the server forbids level changes
 * (uilevel_nochange), the picker only offers levels up to
 * access.uilevel and an active override gets clamped down. Mirrors
 * what the GridSettingsMenu does on the grid side.
 */
const access = useAccessStore()

const ALL_LEVELS: UiLevel[] = ['basic', 'advanced', 'expert']

const availableLevels = computed<UiLevel[]>(() => {
  if (!access.locked) return ALL_LEVELS
  const cap = access.uilevel
  const idx = ALL_LEVELS.indexOf(cap)
  /* Defensive: if access.uilevel isn't in our list (shouldn't happen),
   * fall back to all levels rather than emitting an empty option set. */
  return idx >= 0 ? ALL_LEVELS.slice(0, idx + 1) : ALL_LEVELS
})

const currentLevel = ref<UiLevel>(props.level)

watch(
  () => props.uuid,
  (uuid) => {
    /* New row opening — re-inherit. (`null → uuid` is the open path;
     * the `uuid → null` close path is fine to also re-init since the
     * drawer's mounted-but-hidden state shouldn't carry stale state.) */
    if (uuid) currentLevel.value = props.level
  }
)

/* Symmetric reset for multi-edit drawer opens. Same rationale as
 * the `uuid` watch above — per-session state shouldn't leak
 * across drawer-opens with a different selection. */
watch(
  () => props.uuids,
  (uuids) => {
    if (Array.isArray(uuids) && uuids.length >= 2) {
      currentLevel.value = props.level
    }
  },
)

/* Re-sync local level whenever the parent's `props.level`
 * changes. The editor binds it from the grid's
 * `effectiveLevel`, which settles AFTER first paint — without
 * this watcher, create-mode dialogs (which don't touch
 * `uuid`) stayed frozen at the initialisation-time value. */
watch(
  () => props.level,
  (lvl) => {
    currentLevel.value = lvl
  },
)

watch([() => access.locked, () => access.uilevel], () => {
  if (access.locked && !availableLevels.value.includes(currentLevel.value)) {
    currentLevel.value = access.uilevel
  }
})

/* Capitalize for the picker display — matches the GridSettingsMenu's
 * level options. If vue-i18n lands these become translation lookups. */
const LEVEL_LABELS: Record<UiLevel, string> = {
  basic: t('Basic'),
  advanced: t('Advanced'),
  expert: t('Expert'),
}

function levelLabel(l: UiLevel): string {
  return LEVEL_LABELS[l]
}

/*
 * Level-and-visibility-filtered slice of `fieldProps`. The
 * downstream section computeds (group-based or level-bucketed)
 * both start from this list, so the filter rules stay in one
 * place.
 *
 * No mode-specific skipping. ExtJS only skips fields that are
 * `noui` (always) or `rdonly` AND in bulk-edit mode (the
 * `conf.uuids && p.rdonly` continue at idnode.js:900-901). For
 * single-edit and create, rdonly fields render in their natural
 * destination section (Read-only Info bucket in the level-
 * bucket fallback; their declared group in the group-based
 * path); they're informational. PO_NOSAVE fields (e.g.
 * `disp_title`/`disp_extratext` on dvrentry) DO accept writes
 * via the prop's `set` callback even though they aren't
 * persisted directly — skipping them broke creation because the
 * entry would have no title.
 */
const visibleFields = computed(() =>
  fieldProps.value.filter((p) => {
    if (p.noui || p.phidden) return false
    return levelMatches(propLevel(p), currentLevel.value)
  })
)

/*
 * Group-based routing: walk each field's declared `group` up the
 * `parent` chain on PropertyGroup entries until we hit a root (no
 * parent declared or parent not present in the groups list). This
 * mirrors IdnodeConfigForm's `fieldsByGroup`/`displayedGroups`
 * pattern so multi-level groups (e.g. dvr_config's filename/tagging
 * sub-block) flat-merge into their parent's fieldset — single
 * `<details>` per visible root group, not one per sub-group.
 *
 * Bound the parent-walk to 8 hops as a defense against malformed
 * metadata (cycles, self-parent) — every real ic_groups
 * declaration is at most 2 levels deep.
 *
 * Empty when the class declared no `ic_groups` (idnode.c:1376) —
 * `displayedSections` then falls through to the level-bucket
 * layout.
 */
const fieldsByGroup = computed(() => {
  const groupByNumber = new Map<number, PropertyGroup>()
  for (const g of propertyGroups.value) groupByNumber.set(g.number, g)

  function rootGroup(num: number): number {
    let cur = num
    for (let i = 0; i < 8; i++) {
      const g = groupByNumber.get(cur)
      if (!g || g.parent === undefined || g.parent === cur) return cur
      /* Parent declared on the sub-group but not actually present
       * in the groups list — stop walking; the sub-group itself
       * becomes a root in `displayedSections`. */
      if (!groupByNumber.has(g.parent)) return cur
      cur = g.parent
    }
    return cur
  }

  const map = new Map<number, IdnodeProp[]>()
  for (const p of visibleFields.value) {
    const declared = p.group ?? 0
    const root = rootGroup(declared)
    const list = map.get(root) ?? []
    list.push(p)
    map.set(root, list)
  }
  return map
})

/*
 * Single-level classes (every prop is basic, none have advanced /
 * expert flags) get no value out of the level-picker — switching
 * to advanced or expert reveals nothing new. Hide the picker for
 * those classes; they render with the same chrome as the
 * already-locked uilevel case at the header. mpegts_mux_sched is
 * the canonical example: 5 fields, all basic.
 */
const hasNonBasicFields = computed(() =>
  fieldProps.value.some((p) => p.advanced || p.expert)
)

/*
 * Renderer dispatch — `rendererFor(p)` and `valueFor(p, v)` live in
 * the shared `idnode-fields/rendererDispatch.ts` module so the
 * template stays a single `<component :is="...">` line per field.
 * The pair is shared with ConfigGeneralBaseView's inline-form variant
 * so renderer rules stay consistent across drawer + inline forms.
 *
 * Unsupported types (langstr, perm, hexa, intsplit) fall through to
 * a placeholder; `rendererFor` returns null for those and the
 * template's `v-else` renders an "editor support not implemented"
 * line.
 */

/*
 * Level-bucket fallback definitions — used when the class declares
 * no `ic_groups`. Order + open-by-default + extraClass for each of
 * the four synthetic sections. The readonly bucket starts collapsed
 * and adds a class; the basic / advanced / expert buckets share the
 * default open style.
 */
interface BucketDef {
  bucket: Bucket
  title: string
  open: boolean
  extraClass?: string
}

const BUCKETS: readonly BucketDef[] = [
  { bucket: 'basic', title: t('Basic Settings'), open: true },
  { bucket: 'advanced', title: t('Advanced Settings'), open: true },
  { bucket: 'expert', title: t('Expert Settings'), open: true },
  {
    bucket: 'readonly',
    title: t('Read-only Info'),
    open: false,
    extraClass: 'idnode-editor__group--readonly',
  },
]

/*
 * Unified section model the template consumes — one `<details>` (or
 * bare `<div>` when only one section) per entry, in declaration
 * order. Two paths feed this:
 *
 *   - Group-based: when the class declared `ic_groups`. Each root
 *     group becomes one section; sub-groups had their fields
 *     merged into the parent in `fieldsByGroup`.
 *   - Level-bucketed: when no groups declared. Synthesizes Basic /
 *     Advanced / Expert / Read-only Info from the level + rdonly
 *     flags on each prop — matches the pre-groups behaviour and
 *     what the ExtJS editor does (idnode.js:887-1059).
 */
interface Section {
  key: string
  title: string
  open: boolean
  extraClass?: string
  fields: IdnodeProp[]
}

function sectionsByGroups(): Section[] {
  const groupNumbers = new Set(propertyGroups.value.map((g) => g.number))
  const out: Section[] = []
  for (const g of propertyGroups.value) {
    /* A group is a "root" — and renders as a top-level section —
     * when it has no parent OR its declared parent isn't actually
     * present in the groups list. Sub-groups whose parent IS
     * present had their fields merged into the parent in
     * `fieldsByGroup`, so we skip them here. */
    const isRoot = g.parent === undefined || !groupNumbers.has(g.parent)
    if (!isRoot) continue
    const fields = fieldsByGroup.value.get(g.number) ?? []
    if (fields.length === 0) continue
    out.push({ key: `g${g.number}`, title: g.name, open: true, fields })
  }
  /* Ungrouped fields (declared group 0 or fields with no group
   * declaration at all) when the class doesn't declare a group 0:
   * surface them in a trailing unnamed section so they don't
   * silently vanish. Matches IdnodeConfigForm's tail handling. */
  if (!groupNumbers.has(0)) {
    const orphans = fieldsByGroup.value.get(0) ?? []
    if (orphans.length > 0) {
      out.push({ key: 'g0', title: '', open: true, fields: orphans })
    }
  }
  return out
}

function sectionsByBuckets(): Section[] {
  const buckets: Record<Bucket, IdnodeProp[]> = {
    basic: [],
    advanced: [],
    expert: [],
    readonly: [],
  }
  for (const p of visibleFields.value) {
    buckets[bucketOf(p)].push(p)
  }
  const out: Section[] = []
  for (const def of BUCKETS) {
    const fields = buckets[def.bucket]
    if (fields.length === 0) continue
    out.push({
      key: def.bucket,
      title: def.title,
      open: def.open,
      extraClass: def.extraClass,
      fields,
    })
  }
  return out
}

const displayedSections = computed<Section[]>(() =>
  propertyGroups.value.length > 0 ? sectionsByGroups() : sectionsByBuckets(),
)

/*
 * When only ONE section has content (typically the all-basic case
 * for the level fallback, an all-readonly informational drawer, or
 * a single group containing every visible field), the
 * `<details>/<summary>` wrapper is redundant — there's nothing else
 * to compare against, and the collapsible chrome wastes a row at
 * the top of the form. Drop the wrapper and render fields
 * directly. Multi-section classes keep the wrappers so the user
 * can still distinguish sections + collapse Read-only Info.
 */
const singleSection = computed(() => displayedSections.value.length === 1)

/* ---- Loading ---- */

/*
 * Edit-mode load — fetches the row's current values via api/idnode/load.
 * Response shape: { entries: [{uuid, params: [<IdnodeProp>], meta}] }.
 * The `params` array carries each prop's current `value`; `meta=1`
 * adds the static class metadata (caption, type, enum, …) merged into
 * each prop entry.
 */
async function loadRow(uuid: string) {
  loading.value = true
  error.value = null
  try {
    const params: Record<string, unknown> = { uuid, meta: 1 }
    if (props.list) params.list = props.list
    const res = await apiCall<LoadResponse>('idnode/load', params)
    const entry = res.entries?.[0]
    if (!entry) {
      error.value = t('Entry not found')
      fieldProps.value = []
      return
    }
    fieldProps.value = entry.params ?? entry.props ?? []
    propertyGroups.value = entry.meta?.groups ?? []
    currentClass.value = entry.class ?? null
    currentEvent.value = entry.event ?? null
    currentInstanceText.value = entry.text ?? null
    const init: Record<string, unknown> = {}
    for (const p of fieldProps.value) {
      init[p.id] = p.value ?? null
    }
    initialValues.value = init
    /* Spread to break reference sharing — currentValues is mutable
     * per-field via v-model. */
    currentValues.value = { ...init }
  } catch (e) {
    error.value = e instanceof Error ? e.message : `Failed to load: ${String(e)}`
    fieldProps.value = []
    propertyGroups.value = []
    currentClass.value = null
    currentEvent.value = null
    currentInstanceText.value = null
  } finally {
    loading.value = false
  }
}

/*
 * Create-mode load — fetches the class metadata + per-field defaults
 * via the per-class endpoint `<base>/class` (e.g. `dvr/entry/class`).
 * Mirrors what ExtJS does in `tvheadend.idnode_create.dodirect`
 * (idnode.js:1456-1481). Response shape is `{ caption, class, props: […] }`
 * with each prop's `default` (or `value` for fields whose type has
 * a useful default) populated. Field order matches the `list` query
 * if supplied — same behavior as edit mode.
 */
interface ClassResponse {
  caption?: string
  class?: string
  /* Comet notification event for the class hierarchy — see the
   * LoadEntry.event comment. Emitted by idclass_serialize0
   * (idnode.c:1517-1518). */
  event?: string
  props?: IdnodeProp[]
  /* Top-level for the class-fetch endpoint (api/idnode/class, the
   * create-mode path), produced by `idclass_serialize0` directly —
   * carries class-declared named groups when `ic_groups` is set
   * (idnode.c:1507-1528). Unlike edit-mode, no `meta` wrapper. */
  groups?: PropertyGroup[]
}

/* Three create-flow shapes resolve into one normalised strategy:
 *
 *   - Single-class (DVR Upcoming, Configuration → Users):
 *       metadata: GET <createBase>/class
 *       create:   POST <createBase>/create  with {conf}
 *
 *   - Multi-subclass (Networks: pick DVB-T / IPTV / …):
 *       metadata: GET idnode/class?name=<subclass>
 *       create:   POST <createBase>/create  with {class, conf}
 *
 *   - Parent-scoped (Muxes: pick a network entity):
 *       metadata: GET <classEndpoint>     with <params>
 *       create:   POST <createEndpoint>   with <params> + {conf}
 *
 * The resolver picks the parent-scoped path when `parentScoped`
 * is set, falls through to multi-subclass when `subclass` is set,
 * defaults to single-class otherwise. Both `loadCreate` and
 * `saveCreate` consume the same strategy so the wire format stays
 * consistent across modes. */
interface CreateStrategy {
  classEndpoint: string
  classParams: Record<string, unknown>
  createEndpoint: string
  createParams: Record<string, unknown>
}

function resolveCreateStrategy(): CreateStrategy | null {
  const base = props.createBase
  if (!base) return null
  if (props.parentScoped) {
    return {
      classEndpoint: props.parentScoped.classEndpoint,
      classParams: { ...props.parentScoped.params },
      createEndpoint: props.parentScoped.createEndpoint,
      createParams: { ...props.parentScoped.params },
    }
  }
  if (props.subclass) {
    return {
      classEndpoint: 'idnode/class',
      classParams: { name: props.subclass },
      createEndpoint: `${base}/create`,
      createParams: { class: props.subclass },
    }
  }
  return {
    classEndpoint: `${base}/class`,
    classParams: {},
    createEndpoint: `${base}/create`,
    createParams: {},
  }
}

async function loadCreate() {
  loading.value = true
  error.value = null
  try {
    const strategy = resolveCreateStrategy()
    if (!strategy) {
      loading.value = false
      return
    }
    const params: Record<string, unknown> = { ...strategy.classParams }
    if (props.list) params.list = props.list
    const res = await apiCall<ClassResponse>(strategy.classEndpoint, params)
    fieldProps.value = res.props ?? []
    propertyGroups.value = res.groups ?? []
    currentClass.value = res.class ?? null
    currentEvent.value = res.event ?? null
    /* Create mode has no instance yet → no per-instance title.
     * Callers that need a specific create-mode title pass the
     * `title` prop directly. */
    currentInstanceText.value = null
    const init: Record<string, unknown> = {}
    for (const p of fieldProps.value) {
      /* `default` wins over `value` for create — `value` on a class
       * fetch is sometimes the type's zero-value (0, '', false), but
       * `default` is what the C-side declared as the user-facing
       * default. Fall through to value, then null. */
      let initial: unknown = p.default ?? p.value ?? null
      /*
       * Time defaults of 0 are the C-side "unset" sentinel
       * (`pl->def.tm = 0` in the property declaration). Epoch 1970
       * is never a meaningful default for a real time field — for
       * DVR start/stop it would create a phantom entry in the past
       * that's invisible on the Upcoming tab, and in
       * `dvr_entry_create` the start/stop guard at dvr_db.c:1046-1049
       * silently rejects creation when the values are missing OR
       * when they're a long-past epoch. Treating 0 as null keeps
       * IdnodeFieldTime rendering empty inputs (its inputValue
       * computed returns '' for null), which forces the user to
       * pick a real time. If they don't, our saveCreate filter
       * skips null values from the conf payload, so the server
       * returns a proper required-field error instead of a silent
       * 200-with-no-entry.
       */
      if (p.type === 'time' && initial === 0) initial = null
      init[p.id] = initial
    }
    /* Overlay caller-supplied preselect values (Clone-as-Add
     * pre-fill). They become the initial baseline so saving sends
     * them as-is unless the user edits a field. */
    if (props.preselect) {
      for (const [k, v] of Object.entries(props.preselect)) {
        init[k] = v
      }
    }
    initialValues.value = init
    currentValues.value = { ...init }
  } catch (e) {
    error.value = e instanceof Error ? e.message : `Failed to load: ${String(e)}`
    fieldProps.value = []
    propertyGroups.value = []
    currentClass.value = null
    currentEvent.value = null
    currentInstanceText.value = null
  } finally {
    loading.value = false
  }
}

/*
 * Drive load on either uuid (edit) or createBase (create) flipping in.
 * The two are mutually exclusive — exactly one is set when the editor
 * is open. Watch BOTH so reopening with a different mode triggers a
 * fresh load.
 */
watch(
  () => [props.uuid, props.createBase, props.uuids] as const,
  ([uuid, createBase, uuids]) => {
    /* Reset per-session validation UX on every drawer open / mode
     * flip. Touched state belongs to the current edit; submit
     * attempts likewise. Errors recompute from the new fieldProps
     * once the load resolves. Multi-edit's per-field apply-
     * checkbox state is also session-scoped and resets here.
     * Server-pending state is also session-scoped — a different
     * uuid has its own conflict set. */
    touched.value = new Set()
    submitAttempted.value = false
    applyChecked.value = {}
    serverPendingIds.value = new Set()
    if (cometRefetchTimer !== null) {
      clearTimeout(cometRefetchTimer)
      cometRefetchTimer = null
    }
    if (uuid) {
      loadRow(uuid)
    } else if (Array.isArray(uuids) && uuids.length >= 2) {
      /* Multi-edit: load the FIRST uuid as a template. Form
       * state reflects row 0's values; the user picks fields
       * to apply to all selected rows via the per-field
       * apply-checkbox in the template. Matches Classic at
       * `static/app/idnode.js:1324-1361`. */
      loadRow(uuids[0])
    } else if (createBase) {
      loadCreate()
    } else {
      fieldProps.value = []
      propertyGroups.value = []
      initialValues.value = {}
      currentValues.value = {}
      currentClass.value = null
      currentEvent.value = null
      currentInstanceText.value = null
      error.value = null
    }
  },
  { immediate: true }
)

/* ---- Save / Apply / Cancel ----
 *
 * Two save-paths sharing one `save({ keepOpen })` entry point:
 *
 *   - Save  (`keepOpen=false`): post diff/conf, emit `saved`, emit
 *     `close`. The historical default — drawer disappears after a
 *     successful round-trip.
 *
 *   - Apply (`keepOpen=true`): post diff/conf, refresh the dirty
 *     baseline so the form reads "clean" again, leave the drawer
 *     open. In create mode, additionally emit `created(uuid)` so
 *     the parent can flip the editor to edit mode against the
 *     freshly-minted entry — the next Apply click then becomes a
 *     diff-save, not another create.
 *
 * Save preserves the legacy "close even if not dirty" behaviour —
 * it doubles as a "close" for users who treat it as "confirm and
 * leave". Apply requires a dirty form (no point in applying
 * nothing).
 */

interface CreateResponse {
  uuid?: string
}

async function save({ keepOpen = false }: { keepOpen?: boolean } = {}) {
  if (isMultiEdit.value) {
    await saveMultiEdit({ keepOpen })
    return
  }
  if (isCreate.value) {
    await saveCreate({ keepOpen })
    return
  }
  if (!props.uuid) return
  if (!dirty.value) {
    /* No changes — Save just closes; Apply is disabled in this
     * state (button is gated on `dirty.value`), so `keepOpen` here
     * implies a programmatic call we treat as a no-op. Match
     * ExtJS behavior on Save (its Save also skips the server
     * round-trip when the form isn't dirty). */
    if (!keepOpen) emit('close')
    return
  }
  if (hasErrors.value) {
    /* Promote every field-error to visible (untouched required
     * fields included) so the user gets one overview rather than
     * having to discover one error per Save click. Don't submit. */
    submitAttempted.value = true
    return
  }
  saving.value = true
  error.value = null
  try {
    /* Build the diff payload — only fields whose current value
     * differs from initial. Omitting unchanged fields keeps the
     * payload small and avoids triggering server-side change
     * notifications for properties the user didn't touch. */
    const node: Record<string, unknown> = { uuid: props.uuid }
    for (const k of Object.keys(currentValues.value)) {
      if (JSON.stringify(currentValues.value[k]) !== JSON.stringify(initialValues.value[k])) {
        node[k] = currentValues.value[k]
      }
    }
    await apiCall('idnode/save', { node: JSON.stringify(node) })
    emit('saved')
    /* We just pushed our values; whatever the server had pending
     * is now overwritten by what we sent. Clear the conflict
     * set — markers are no longer relevant. */
    serverPendingIds.value = new Set()
    if (keepOpen) {
      /* Apply path — refresh the dirty baseline to whatever the
       * user just persisted. The form keeps the same values
       * visually; `dirty` flips back to false on next compute. The
       * Comet `change` notification refreshes the grid behind the
       * drawer in parallel. */
      initialValues.value = { ...currentValues.value }
    } else {
      emit('close')
    }
  } catch (e) {
    /* Surface server-side validation errors via the global error
     * dialog rather than mutating `error.value` — `error` is
     * load-error state that, when truthy, replaces the form with
     * an inline-text block and disables Save (see template at
     * `idnode-editor__body`). Setting it on save failure would
     * brick the drawer until the user closed + reopened it. */
    errorDialog.show({
      title: t('Save failed'),
      message: apiErrorMessage(e),
    })
  } finally {
    saving.value = false
  }
}

/*
 * Create-mode save — POSTs the full filled-in field set to
 * `<createBase>/create` as `conf=<JSON>`. ExtJS does the same
 * (idnode.js:1500-1517). Empty / null fields are omitted; the
 * server falls back to its own defaults when a key is absent.
 *
 * Unlike edit-mode save, "no changes" still submits — the user
 * may have chosen to accept all defaults, and we must create the
 * entry. (If the server rejects an empty payload, the error
 * surfaces inline like any other validation error.)
 *
 * Apply variant (`keepOpen=true`) reads the new entry's UUID from
 * the response (api_idnode_create.c:724-730 always sets it) and
 * emits `created(uuid)`; the parent flips the editor to edit
 * mode and the watch on `props.uuid` reloads the canonical
 * server-side row — including any defaulted/computed fields the
 * server filled in. Brief loading flicker on the flip is
 * acceptable.
 */
async function saveCreate({ keepOpen = false }: { keepOpen?: boolean } = {}) {
  const strategy = resolveCreateStrategy()
  if (!strategy) return
  if (hasErrors.value) {
    submitAttempted.value = true
    return
  }
  saving.value = true
  error.value = null
  try {
    const conf: Record<string, unknown> = {}
    for (const k of Object.keys(currentValues.value)) {
      const v = currentValues.value[k]
      if (v === null || v === undefined || v === '') continue
      conf[k] = v
    }
    /* The strategy's createParams carry whatever the per-flow
     * create endpoint needs alongside `conf`:
     *   - single-class:    {} (just conf)
     *   - multi-subclass:  {class: <subclass>}
     *   - parent-scoped:   {uuid: <parent uuid>}
     * Server-side handlers ignore unrecognised params, so the
     * wire format stays correct across all three. */
    const createPayload: Record<string, unknown> = {
      ...strategy.createParams,
      conf: JSON.stringify(conf),
    }
    const res = await apiCall<CreateResponse>(strategy.createEndpoint, createPayload)
    emit('saved')
    if (keepOpen && res.uuid) {
      emit('created', res.uuid)
      /* The parent's flipToEdit will set `props.uuid` to the new
       * UUID; the existing watch on `[uuid, createBase]` then fires
       * `loadRow(newUuid)`, which sets `initialValues` from the
       * server's canonical state. No baseline refresh needed here. */
    } else {
      emit('close')
    }
  } catch (e) {
    /* See note on the edit-path catch above — `error.value` is
     * reserved for load failures; using it here would replace
     * the form with inline text and bury the user's typed
     * values. Pop the modal instead. */
    errorDialog.show({
      title: t('Could not create'),
      message: apiErrorMessage(e),
    })
  } finally {
    saving.value = false
  }
}

/*
 * Multi-edit save — Case-2 shape `idnode/save` accepts at
 * `src/api/api_idnode.c:408-429`. One POST applies the SAME
 * field set (only ticked checkboxes' values) to every selected
 * uuid. Server iterates internally; partial-success (e.g. one
 * row perm-denied) returns 0 — Vue surfaces a generic success
 * either way (matches Classic).
 *
 * Apply variant (`keepOpen=true`) reloads the template from row
 * 0 after the save lands and resets every apply-checkbox so the
 * user can do another round of changes against the freshly-
 * saved values. Save (`keepOpen=false`) closes the drawer.
 */
async function saveMultiEdit({ keepOpen = false }: { keepOpen?: boolean } = {}) {
  if (!props.uuids || props.uuids.length < 2) return
  if (!hasApplyChecks.value) {
    /* Save with no ticked fields is a no-op. Disabled-button
     * gating in the template normally prevents this; the guard
     * here defends against programmatic save() calls (Apply,
     * keyboard Enter, etc.). */
    if (!keepOpen) emit('close')
    return
  }
  if (hasErrors.value) {
    submitAttempted.value = true
    return
  }
  saving.value = true
  error.value = null
  try {
    const payload = buildMultiEditPayload(
      props.uuids,
      currentValues.value,
      applyChecked.value,
    )
    await apiCall('idnode/save', { node: JSON.stringify(payload) })
    emit('saved')
    if (keepOpen) {
      /* Reload template from row 0 + reset apply-checkboxes so
       * the user can do another round. The freshly-saved values
       * are now row 0's canonical state — initialValues in the
       * reload reflect them. */
      applyChecked.value = {}
      await loadRow(props.uuids[0])
    } else {
      emit('close')
    }
  } catch (e) {
    /* Same rationale as the single-edit save catch — keep the
     * form visible so the user can retry or amend their bulk
     * change. */
    errorDialog.show({
      title: t('Bulk save failed'),
      message: apiErrorMessage(e),
    })
  } finally {
    saving.value = false
  }
}

function apply() {
  void save({ keepOpen: true })
}

const confirmDialog = useConfirmDialog()
const errorDialog = useErrorDialog()

async function attemptClose() {
  /* Confirm-on-close when EITHER the field values are dirty
   * (single-edit semantics) OR the user has ticked apply-
   * checkboxes in multi-edit mode but hasn't saved yet. The
   * second case matters because users can tick a checkbox
   * without changing the value (mass-copy from row 0 — a real
   * use case); closing without confirmation would lose that
   * intent silently. */
  const hasUnsaved =
    dirty.value || (isMultiEdit.value && hasApplyChecks.value)
  if (hasUnsaved && !saving.value) {
    const ok = await confirmDialog.ask(t('Discard unsaved changes?'), {
      severity: 'danger',
      acceptLabel: t('Discard'),
      rejectLabel: t('Keep editing'),
    })
    if (!ok) return
  }
  emit('close')
}

/* Picker mode — the user clicked a different row in the top table.
 * Switching rows reloads the form (the `uuid` watcher), which would
 * silently drop unsaved edits, so reuse the same discard confirm
 * `attemptClose()` uses. On accept (or a clean form) the `pick`
 * emit lets the parent move `uuid` to the new row. */
async function attemptPick(uuid: string) {
  if (uuid === props.uuid) return
  if (dirty.value && !saving.value) {
    const ok = await confirmDialog.ask(t('Discard unsaved changes?'), {
      severity: 'danger',
      acceptLabel: t('Discard'),
      rejectLabel: t('Keep editing'),
    })
    if (!ok) return
  }
  emit('pick', uuid)
}

/* ---- Keyboard: Esc closes. This listener owns Esc exclusively —
 *      the Drawer's built-in closeOnEscape is disabled in the
 *      template, because both firing per press would run
 *      attemptClose twice (double 'close' emit on a clean form;
 *      a doubled discard-confirm on a dirty one). ---- */
function onKeyDown(ev: KeyboardEvent) {
  if (ev.key === 'Escape' && visible.value) {
    attemptClose()
  }
}
onMounted(() => document.addEventListener('keydown', onKeyDown))
onBeforeUnmount(() => {
  document.removeEventListener('keydown', onKeyDown)
  if (cometUnsub) {
    cometUnsub()
    cometUnsub = null
  }
  if (cometRefetchTimer !== null) {
    clearTimeout(cometRefetchTimer)
    cometRefetchTimer = null
  }
})
</script>

<template>
  <!--
    Escape handling: the Drawer's built-in close-on-escape is turned
    off because the component's own document keydown listener owns
    the dirty-check confirm; leaving both on would close/confirm
    twice per press.
  -->
  <Drawer
    v-model:visible="visible"
    position="right"
    class="idnode-editor"
    :close-on-escape="false"
    :pt="{ root: { class: 'idnode-editor__root' } }"
  >
    <!--
      Custom header: title left, view-level picker right (just before
      PrimeVue's own close-X). Combining them into one row keeps the
      editor compact — no separate full-width level row. The picker
      defaults to the level the calling grid was at; bumping it here
      only affects this edit session (re-inherits when opening the
      next row). Capped at access.uilevel when the user's level is
      admin-locked. Hidden when only one level is available — the
      header then renders just the title.
    -->
    <template #header>
      <div class="idnode-editor__header">
        <span class="idnode-editor__title">{{ effectiveTitle }}</span>
        <select
          v-if="availableLevels.length > 1 && hasNonBasicFields"
          id="idnode-editor-level"
          v-model="currentLevel"
          class="idnode-editor__level-select"
          :aria-label="t('View level')"
        >
          <option v-for="l in availableLevels" :key="l" :value="l">
            {{ levelLabel(l) }}
          </option>
        </select>
      </div>
    </template>
    <div class="idnode-editor__body">
      <p v-if="loading" class="idnode-editor__status">{{ t('Loading…') }}</p>
      <p v-else-if="error" class="idnode-editor__error">{{ error }}</p>
      <!--
        Multi-edit subtitle banner — onboards admins to the
        apply-checkbox affordance so they don't miss the per-
        field opt-in. Renders only in multi-edit mode; absent in
        single-edit and create.
      -->
      <template v-if="!loading && !error">
        <!--
          Picker mode — a single-select table of the passed entities
          above the form; the form below edits the selected row.
          Switching rows is the parent's job (the `pick` emit).
        -->
        <EntityPickerTable
          v-if="isPicker"
          class="idnode-editor__picker"
          :rows="pickerRows ?? []"
          :columns="pickerColumns ?? []"
          :selected="uuid"
          @select="attemptPick"
        />
        <!--
          Multi-edit subtitle banner — onboards admins to the
          apply-checkbox affordance so they don't miss the per-
          field opt-in. Renders only in multi-edit mode; absent
          in single-edit and create.
        -->
        <p v-if="isMultiEdit" class="idnode-editor__multi-banner">
          Editing {{ uuids?.length ?? 0 }} entries. Tick fields on the
          left to apply changes to all selected rows.
        </p>
        <form class="idnode-editor__form" @submit.prevent="save()">
        <!--
          Grouped fieldsets, mirroring the ExtJS editor (idnode.js).
          Native <details>/<summary> for collapsibility — accessible
          (keyboard + screen-reader) without any custom toggle code.
          Read-only Info opens collapsed; the others open by default.

          Inner per-prop rendering is delegated to <component :is>
          dispatch via `rendererFor(p)` — see the script section's
          renderer-dispatch comment for the rationale and the
          previously-duplicated cascade this replaces.
        -->
        <!--
          Section wrapper: `<details>/<summary>` when multiple
          sections have content (Basic + Advanced + Read-only
          Info; or two+ named groups for classes with declared
          `ic_groups`), plain `<div>` when only one section
          exists (the summary header would have nothing to
          compare against and just adds a wasted row).
          `<component :is>` flips the element while keeping the
          same inner field-rendering block — `<summary>` is
          `v-if`'d out in single-section mode where it would be
          invalid inside a `<div>`, and also when the section
          has no title (an unnamed tail-group fallback).
        -->
        <template v-for="s in displayedSections" :key="s.key">
          <component
            :is="singleSection ? 'div' : 'details'"
            class="idnode-editor__group"
            :class="s.extraClass"
            :open="singleSection ? undefined : s.open"
          >
            <summary
              v-if="!singleSection && s.title"
              class="idnode-editor__group-title"
            >
              {{ s.title }}
            </summary>
            <div class="idnode-editor__group-body ifld-form">
              <template v-for="p in s.fields" :key="p.id">
                <!--
                  fieldGroups hook: when this field is the FIRST key
                  of a declared group, render the combined component
                  once and skip the standard per-field row. Non-first
                  group keys are suppressed entirely.
                -->
                <div
                  v-if="groupBindingFor(p.id)?.isFirst"
                  class="ifld-row"
                  :class="{
                    'ifld-row--dirty': isGroupRowDirty(groupBindingFor(p.id)!.group),
                  }"
                >
                  <component
                    :is="groupBindingFor(p.id)!.group.component"
                    :group-props="groupPropsFor(groupBindingFor(p.id)!.group)"
                    :group-values="groupValuesFor(groupBindingFor(p.id)!.group)"
                    :disabled="false"
                    @update="(id: string, value: unknown) => onFieldChange(id, value)"
                  />
                </div>
                <template v-else-if="groupBindingFor(p.id)">
                  <!-- non-first group key — fully owned by the first-key render above -->
                </template>
                <!--
                  Per-field row wrapper. Carries the dirty-state class
                  hook so the dot-before-label can light up without
                  touching each `IdnodeFieldXxx` internals. CSS
                  injection point is the `.ifld-row--dirty` rule
                  below. Read-only fields can never be dirty (their
                  renderers ignore input) but the wrapper is uniform
                  — `isFieldDirty(p.id)` just stays false for them.
                -->
                <div
                  v-else
                  class="ifld-row"
                  :class="{
                    'ifld-row--dirty': isFieldDirty(p.id),
                    'ifld-row--server-pending': isFieldServerPending(p.id),
                    'ifld-row--required': isRequired(p.id),
                    'ifld-row--invalid': !!visibleError(p.id),
                    'ifld-row--multi': isMultiEdit,
                  }"
                >
                  <!--
                    Multi-edit apply-checkbox — leftmost column of
                    the row. Read-only fields can't bulk-apply
                    (the value is server-managed) so the cell
                    stays empty for them, preserving the column
                    alignment with editable rows above/below.
                    Auto-checks when the user edits the matching
                    field's value (see `onFieldChange`).
                  -->
                  <Checkbox
                    v-if="isMultiEdit && !p.rdonly"
                    class="ifld-row__apply-check"
                    :model-value="!!applyChecked[p.id]"
                    binary
                    :aria-label="t('Apply {0} to all selected entries', p.caption ?? p.id)"
                    @update:model-value="(v: boolean) => (applyChecked[p.id] = v)"
                  />
                  <span
                    v-else-if="isMultiEdit"
                    class="ifld-row__apply-spacer"
                    aria-hidden="true"
                  />
                  <component
                    :is="rendererFor(p)!"
                    v-if="rendererFor(p)"
                    :prop="p"
                    :model-value="valueFor(p, currentValues[p.id])"
                    :inline="inlineEnumMultiSet.has(p.id)"
                    @update:model-value="onFieldChange(p.id, $event)"
                  />
                  <p v-else class="idnode-editor__placeholder">
                    <strong>{{ p.caption ?? p.id }}:</strong>
                    editor support not implemented for this field type
                  </p>
                  <!--
                    Error message under the input. Visible when the
                    field has been touched OR the user has attempted
                    Save / Apply. The wrapper's `--invalid` class
                    gates the red border styling above; this `<p>`
                    carries the human-readable reason.
                  -->
                  <p v-if="visibleError(p.id)" class="ifld-row__error" role="alert">
                    {{ visibleError(p.id) }}
                  </p>
                  <!--
                    Soft length counter — pure UX hint. See
                    `lengthHint()` in the script for shape; renders
                    only on multiline fields with > 100 chars or on
                    soft-target single-line fields when length
                    crosses 80 % of the target.
                  -->
                  <p
                    v-if="lengthHint(p)"
                    class="ifld-row__counter"
                    :class="{ 'ifld-row__counter--warn': lengthHint(p)?.warn }"
                  >
                    {{ lengthHint(p)?.text }}
                  </p>
                </div>
              </template>
            </div>
          </component>
        </template>
      </form>
      </template>
    </div>
    <template #footer>
      <div class="idnode-editor__footer">
        <button
          type="button"
          class="idnode-editor__btn idnode-editor__btn--cancel"
          :disabled="saving"
          @click="attemptClose"
        >
          {{ t('Cancel') }}
        </button>
        <!--
          Apply: save and stay open. Disabled until the form is
          dirty — applying nothing has no useful effect, and Save
          already covers "I'm done, close" for the no-changes case.
          Also disabled when the form has validation errors — no
          point round-tripping data the client knows is invalid.
          In create mode, the first Apply triggers `created(uuid)`
          and the parent flips us to edit mode; subsequent Applies
          are diff-saves like any other edit-mode Apply.
        -->
        <button
          type="button"
          class="idnode-editor__btn idnode-editor__btn--apply"
          :disabled="
            loading ||
            saving ||
            !!error ||
            hasErrors ||
            (isMultiEdit ? !hasApplyChecks : !dirty)
          "
          @click="apply"
        >
          {{ t('Apply') }}
        </button>
        <!--
          Save gating:
            - loading / saving / error: never submit while busy or
              broken.
            - (dirty || isCreate) && hasErrors: never submit invalid
              values. In CREATE mode this gates from the start — a
              fresh create whose required fields are still empty is
              invalid before anything is touched. In EDIT mode it
              only gates once dirty: a non-dirty form with stale
              invalid server state can still be dismissed (via
              Cancel) — the user didn't change it, so client
              validation has nothing to flag; fixing that data is a
              separate visit. Classes with no required / cross-field
              rules keep create-with-defaults — hasErrors stays
              false for them, so this clause never fires.
            - (!isCreate && !dirty): in EDIT mode, no-op submits are
              wasteful — disable until the user changes something.
        -->
        <button
          type="button"
          class="idnode-editor__btn idnode-editor__btn--save"
          :disabled="
            loading ||
            saving ||
            !!error ||
            ((dirty || isCreate) && hasErrors) ||
            (isMultiEdit
              ? !hasApplyChecks || hasErrors
              : !isCreate && !dirty)
          "
          @click="save()"
        >
          {{ saving ? t('Saving…') : t('Save') }}
        </button>
      </div>
    </template>
  </Drawer>
</template>

<style scoped>
/*
 * Body padding 8 px (`--tvh-space-2`). Group cards
 * (`.idnode-editor__group`) carry their own 12 px inner padding, so
 * the total inset from drawer edge to field text is ~21 px —
 * comfortable without wasting horizontal space. Same value used in
 * the EPG event drawer for cross-drawer parity.
 */
.idnode-editor__body {
  padding: var(--tvh-space-2);
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-4);
  min-height: 0;
  flex: 1 1 auto;
  overflow-y: auto;
}

.idnode-editor__status,
.idnode-editor__error {
  color: var(--tvh-text-muted);
  margin: 0;
  font-size: var(--tvh-text-md);
}

.idnode-editor__error {
  color: var(--tvh-error);
}

.idnode-editor__form {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
}

/*
 * Header slot — title on the left, view-level picker pushed to the
 * right (just before PrimeVue's close X). `flex: 1` makes our slot
 * content fill the space PrimeVue gave us inside `.p-drawer-header`,
 * which is itself a flex row with the close button at the trailing
 * end; `margin-right: auto` on the title is what shoves the picker
 * over.
 */
.idnode-editor__header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  flex: 1;
  min-width: 0;
}

.idnode-editor__title {
  margin-right: auto;
  font-size: var(--tvh-text-2xl);
  font-weight: 600;
  color: var(--tvh-text);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.idnode-editor__level-select {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 4px 8px;
  font: inherit;
  font-size: var(--tvh-text-md);
  min-height: 28px;
}

.idnode-editor__level-select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

/*
 * Fieldset group. <details> + <summary> gives us native collapsibility
 * for free — keyboard-toggle on Enter/Space, screen-reader-aware
 * (announces "expanded"/"collapsed"), and no JS state to manage. The
 * default disclosure-triangle marker is fine; we just style the
 * surrounding chrome so the section reads as a card.
 */
.idnode-editor__group {
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  background: var(--tvh-bg-page);
}

.idnode-editor__group-title {
  font-size: var(--tvh-text-md);
  font-weight: 600;
  color: var(--tvh-text);
  padding: var(--tvh-space-2) var(--tvh-space-3);
  cursor: pointer;
  user-select: none;
  list-style: revert; /* keep the disclosure triangle */
}

.idnode-editor__group-title:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-editor__group-title:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
  border-radius: var(--tvh-radius-md);
}

.idnode-editor__group[open] > .idnode-editor__group-title {
  border-bottom: 1px solid var(--tvh-border);
}

.idnode-editor__group-body {
  display: flex;
  flex-direction: column;
  /*
   * Inter-row gap of 8 px (`--tvh-space-2`). Tighter than the
   * standard `--tvh-space-3` (12 px) because drawer forms tend
   * toward many short fields where 12 px reads as wasteful. Still
   * reads as visually grouped without feeling crammed.
   */
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-3);
}

/*
 * Drawer-scoped label font-size — 12 px (vs the per-field default
 * of 13 px). At 12 px most labels fit on a single line within the
 * 180 px label column without wrapping; long captions (e.g.
 * "Number of tuners to export for HDHomeRun Server Emulation")
 * wrap rather than ellipsis-truncate so the full meaning stays
 * readable without a hover.
 */
.idnode-editor__group-body :deep(.ifld__label) {
  font-size: var(--tvh-text-sm);
}

/* Field-row layout (label-left / control-right, input width
 * clamped, row min-height, checkbox sizing, phone-mode stack)
 * lives in the shared `src/styles/ifld.css`. Both this drawer
 * editor and `<IdnodeConfigForm>` opt in via the `ifld-form`
 * class on the group-body container so both surfaces render
 * fields identically. */

/*
 * Per-field dirty indicator — VS Code preferences pattern.
 *
 * `.ifld-row--dirty` flips on when `isFieldDirty(p.id)` returns true
 * (field's current value differs from its loaded baseline). A `•`
 * glyph in primary colour is prepended to `.ifld__label` via a
 * `::before` pseudo-element on the deep target — anchors the dirty
 * state to the specific label so the user sees exactly which field
 * changed. Inline content (rather than absolutely-positioned) so it
 * doesn't fight the existing grid layout or label-wrap behaviour.
 */
.ifld-row--dirty :deep(.ifld__label)::before {
  content: '•';
  color: var(--tvh-primary);
  font-weight: bold;
  margin-right: 4px;
}

/*
 * Conflict marker — same `•` glyph (zero layout impact), warning
 * colour + bloom-pulse so the user notices a server-side change
 * to a field they're currently editing. The pulse combines three
 * eye-catchers, all space-neutral:
 *
 *   1. Brighter orange (#ff7a1a) than the muted theme warning,
 *      with stronger opacity range (1 → 0.15) for high contrast.
 *   2. `transform: scale(1.4)` at the peak — the dot visibly
 *      "blooms" larger then shrinks back. Transforms don't
 *      reflow, so adjacent label text doesn't shift.
 *   3. `text-shadow` halo at the peak — radiates outward from
 *      the glyph. Text-shadow lives outside the box model, so
 *      no layout impact.
 *
 * Faster cycle (1.5 s) than the typical 2 s "calm" pulse — the
 * dot is shouting "hey, the server has a value for this field
 * too". `inline-block` on the pseudo-element so `transform:
 * scale` has a proper containing block.
 *
 * Triggered when both `--dirty` AND `--server-pending` are set.
 * `--server-pending` alone (clean field with a server update
 * already applied via `mergeFromServer`) does NOT show the
 * marker — the live merge handled it silently.
 */
.ifld-row--dirty.ifld-row--server-pending :deep(.ifld__label)::before {
  color: #ff7a1a;
  display: inline-block;
  animation: ifld-row-pending-pulse 1.5s ease-in-out infinite;
}

@keyframes ifld-row-pending-pulse {
  0%,
  100% {
    opacity: 1;
    transform: scale(1);
    text-shadow: none;
  }
  50% {
    opacity: 0.15;
    transform: scale(1.4);
    text-shadow:
      0 0 4px #ff7a1a,
      0 0 8px rgba(255, 122, 26, 0.6);
  }
}

/* Respect users who have requested reduced motion — keep the
 * colour shift but drop all animation (no opacity fade, no
 * scale, no halo). The colour alone is enough to encode the
 * conflict for non-motion users. */
@media (prefers-reduced-motion: reduce) {
  .ifld-row--dirty.ifld-row--server-pending :deep(.ifld__label)::before {
    animation: none;
  }
}

/*
 * Required-field caption is bolded as an always-on hint. Combines
 * with the dirty-dot from the rule above (a dirty required field
 * shows both signals; they don't compete). Subtle enough not to
 * shout on every form open; chosen over the asterisk pattern to
 * avoid colliding with the dirty-dot's punctuation.
 */
.ifld-row--required :deep(.ifld__label) {
  font-weight: 700;
}

/*
 * Invalid field — red border on the actual input element. Reaches
 * through scoped CSS via :deep(); applies to the standard
 * `.ifld__input` class every renderer carries on its primary
 * input. Multi-select / ordered-list widgets that don't expose
 * `.ifld__input` fall through to the inline error text only — the
 * red `<p>` below the row carries the signal there.
 */
.ifld-row--invalid :deep(.ifld__input) {
  border-color: var(--tvh-error);
  outline-color: var(--tvh-error);
}

/*
 * Multi-edit row layout — leftmost 24 px column for the apply-
 * checkbox, the rest of the row for the renderer. Mirrors
 * Classic's `[checkbox] [label] [input]` shape (the label lives
 * inside the renderer, so the visible chrome is the checkbox
 * sitting just left of the label). Error / counter messages
 * span both columns so they don't get squeezed by the narrow
 * left column.
 *
 * `align-items: center` keeps the checkbox aligned with the
 * field's label + input line: the checkbox and the renderer
 * share grid row 1, so the checkbox centres against the renderer
 * — while the error / counter rows sit in their own grid rows
 * below and so can't drag that centre. (The earlier
 * `align-items: start` + a fixed `margin-top` nudge only
 * approximated it for one field type at one text scale.)
 */
.ifld-row--multi {
  display: grid;
  grid-template-columns: 24px 1fr;
  column-gap: var(--tvh-space-2);
  align-items: center;
}

.ifld-row--multi > .ifld-row__error,
.ifld-row--multi > .ifld-row__counter {
  grid-column: 1 / -1;
}

/* Read-only fields can't bulk-apply — an empty spacer holds the
 * checkbox column so editable + read-only rows stay aligned. */
.ifld-row__apply-spacer {
  display: inline-block;
  width: 16px;
  height: 16px;
}

/*
 * Multi-edit subtitle banner — onboarding hint that sits below
 * the drawer toolbar and above the form. Muted background +
 * small font so it explains the affordance without dominating
 * the form. Single-edit and create modes don't render this.
 */
.idnode-editor__multi-banner {
  margin: 0;
  padding: var(--tvh-space-2) var(--tvh-space-3);
  background: color-mix(in srgb, var(--tvh-primary) 8%, transparent);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-sm);
  line-height: 1.5;
}

/* Picker-mode table — sits above the form (the body's flex `gap`
 * spaces it off). It is itself a scroll container, so as a flex
 * child its auto min-height resolves to 0 and the body's
 * flex-shrink would otherwise collapse it to nothing; pin its
 * size so it always shows. */
.idnode-editor__picker {
  flex-shrink: 0;
}

/*
 * Error message line. Sits in the control column on desktop so it
 * reads as "below the input" rather than "below the row entirely".
 * The 192 px left-margin == 180 px label column + 12 px gap from
 * the editor's `:deep(.ifld)` grid rule. Phone reverts to flush-
 * left because the label-above-control layout makes the input
 * itself flush-left. Same shape for the soft length counter below.
 */
.ifld-row__error,
.ifld-row__counter {
  margin: 4px 0 0 192px;
  font-size: var(--tvh-text-sm);
  line-height: 1.4;
}

.ifld-row__error {
  color: var(--tvh-error);
}

.ifld-row__counter {
  color: var(--tvh-text-muted);
  font-variant-numeric: tabular-nums;
}

.ifld-row__counter--warn {
  color: var(--tvh-warning, var(--tvh-error));
}

@media (max-width: 767px) {
  .ifld-row__error,
  .ifld-row__counter {
    margin-left: 0;
  }
}

/*
 * Read-only group: visually muted to signal "informational, not
 * editable". The fields inside are already rendered disabled by their
 * respective field components honoring `prop.rdonly`.
 */
.idnode-editor__group--readonly {
  background: color-mix(in srgb, var(--tvh-text-muted) 6%, transparent);
}

/*
 * Placeholder row for unsupported types. Muted, non-interactive,
 * indented to align with field labels. Mirrors the field components'
 * label/value vertical rhythm without pretending to be an input.
 */
.idnode-editor__placeholder {
  margin: 0;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
  padding: 6px 0;
}

/*
 * Footer wrapper. PrimeVue's `<Drawer>` mounts our `#footer` slot
 * content inside `.p-drawer-footer`, which Aura applies
 * `padding: {overlay.modal.padding}` (20 px) to by default — same
 * layer that masked the body padding before the
 * `.p-drawer-content { padding: 0 }` override above. We do the same
 * trick here (footer override below) and let this inner wrapper own
 * the inset at 8 px, matching the body for cross-edge parity. The
 * top border keeps the footer visually distinct from the scrolling
 * body content above.
 */
.idnode-editor__footer {
  display: flex;
  justify-content: flex-end;
  gap: var(--tvh-space-2);
  padding: var(--tvh-space-2);
  border-top: 1px solid var(--tvh-border);
}

.idnode-editor__btn {
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 16px;
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
  min-height: 36px;
  transition: background var(--tvh-transition);
}

.idnode-editor__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-editor__btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-editor__btn:disabled {
  opacity: 0.55;
  cursor: not-allowed;
}

.idnode-editor__btn--save {
  background: var(--tvh-primary);
  color: #fff;
  border-color: var(--tvh-primary);
}

.idnode-editor__btn--save:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 88%, black);
}

/* When disabled, drop the primary tint and read like a regular
 * neutral button — same shape as IdnodeConfigForm's
 * `__btn--save:disabled` rule. The base `:disabled` opacity 0.55
 * still applies on top, so the button is visibly inactive AND
 * doesn't beg for attention while there's nothing to save. */
.idnode-editor__btn--save:disabled {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border-color: var(--tvh-border);
}
</style>

<style>
/*
 * Unscoped — PrimeVue Drawer's root element lives outside our
 * component's scope-id, so we target it with a data-pt class hook.
 * 480 px on desktop (per brief §6.6); full-screen on phone.
 */
.idnode-editor__root.p-drawer {
  width: 480px;
  max-width: 100vw;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
}

.idnode-editor__root .p-drawer-header {
  padding: var(--tvh-space-3) var(--tvh-space-4);
  border-bottom: 1px solid var(--tvh-border);
}

.idnode-editor__root .p-drawer-content {
  /*
   * `padding: 0` overrides PrimeVue Aura's default
   *   padding: 0 {overlay.modal.padding} {overlay.modal.padding} {overlay.modal.padding}
   * which resolves to `0 20px 20px 20px`. Without this override, the
   * 20 px on right/bottom/left would dominate any inner padding and
   * push the drawer body 20 px away from each edge regardless of
   * what `.idnode-editor__body` declares. With it zeroed, the inner
   * 8 px body padding becomes the sole drawer-edge → content inset.
   * Header / footer are siblings of `.p-drawer-content` under
   * `.p-drawer`, so this override doesn't affect them.
   */
  padding: 0;
  display: flex;
  flex-direction: column;
  flex: 1 1 auto;
  min-height: 0;
}

/*
 * Same fix as `.p-drawer-content`: zero out PrimeVue Aura's default
 * `padding: {overlay.modal.padding}` (20 px) so our inner
 * `.idnode-editor__footer` is the sole inset. Keeps drawer-edge to
 * Save/Cancel button consistent at 8 px (matching the body).
 */
.idnode-editor__root .p-drawer-footer {
  padding: 0;
}

@media (max-width: 767px) {
  /*
   * `width: 100%` (not `100vw`) intentionally — for a position:fixed
   * drawer, 100% resolves against the initial containing block (the
   * visual viewport) which on iOS Safari is a few pixels narrower
   * than 100vw (the layout viewport). 100vw produced a horizontal
   * overflow in iOS Safari testing.
   */
  .idnode-editor__root.p-drawer {
    width: 100%;
  }
}
</style>
