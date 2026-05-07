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
 *   2. Bucket fields into Basic / Advanced / Expert / Read-only Info
 *      sections — same split the old UI uses (idnode.js:887-1059).
 *      Read-only Info renders collapsed by default.
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
 * Group buckets are level-based (Basic / Advanced / Expert / Read-
 * only Info). The server's `groups` array for class-defined named
 * fieldsets is not consumed here; the level-based split is sufficient
 * for the classes this editor currently renders (DVR has no groups).
 */
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import Drawer from 'primevue/drawer'
import { apiCall } from '@/api/client'
import type { UiLevel } from '@/types/access'
import type { IdnodeProp } from '@/types/idnode'
import { levelMatches, propLevel } from '@/types/idnode'

import { useAccessStore } from '@/stores/access'

import { rendererFor, valueFor } from './idnode-fields/rendererDispatch'
import { validateField } from './idnode-fields/validators'
import { applyClassRules, isFieldRequired } from './idnode-fields/validationRules'
import { useConfirmDialog } from '@/composables/useConfirmDialog'

const props = defineProps<{
  /* uuid of the row to edit; null/undefined means closed. */
  uuid: string | null
  /*
   * Create mode trigger. When set (and `uuid` is null), the editor
   * opens an empty form for creating a new entry of this idnode
   * class. Mutually exclusive with `uuid` — pass exactly one. The
   * value is the API base path used by tvheadend's per-class
   * idnode endpoints, e.g. `'dvr/entry'` → metadata fetch from
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

const visible = computed({
  get: () => (props.uuid !== null && props.uuid !== undefined) || isCreate.value,
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
  params?: IdnodeProp[]
  props?: IdnodeProp[]
  meta?: unknown
}

interface LoadResponse {
  entries?: LoadEntry[]
}

const loading = ref(false)
const saving = ref(false)
const error = ref<string | null>(null)
const fieldProps = ref<IdnodeProp[]>([])
const initialValues = ref<Record<string, unknown>>({})
const currentValues = ref<Record<string, unknown>>({})
/* Idnode class identifier ('dvrentry', 'dvrautorec', etc.) populated
 * on each load. Drives the per-class validation rule lookup. Null
 * while the drawer is closed or before the first response lands. */
const currentClass = ref<string | null>(null)
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
const errors = computed<Map<string, string>>(() => {
  const map = new Map<string, string>()
  /* Per-field type-driven first */
  for (const p of fieldProps.value) {
    if (p.rdonly || p.noui || p.phidden) continue
    const e = validateField(p, currentValues.value[p.id])
    if (e) map.set(p.id, e)
  }
  /* Class-level rules (required, cross-field, minLength) — only
   * fill IDs that don't already carry a per-field error. */
  for (const [id, msg] of applyClassRules(currentClass.value, currentValues.value)) {
    if (!map.has(id)) map.set(id, msg)
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
}

/*
 * Bucket each property into one of four sections matching the ExtJS
 * editor (idnode.js:887-1059):
 *
 *   - basic     (df): default, visible at all levels
 *   - advanced  (af): only at level >= advanced
 *   - expert    (ef): only at level == expert
 *   - readonly  (rf): rdonly fields, regardless of category
 *
 * Read-only routing wins over level routing — a rdonly+expert field
 * goes into Read-only Info, not Expert Settings (it's still
 * informational; the user can't edit it). Skip rules: `noui` and
 * `phidden` always skip; `levelMatches` filters by current view level
 * before bucketing.
 *
 * Explicit type for the bucket map keeps the template tidy and lets
 * TS narrow accesses through `groupedFields.value.basic` etc.
 */
type Bucket = 'basic' | 'advanced' | 'expert' | 'readonly'

function bucketOf(p: IdnodeProp): Bucket {
  if (p.rdonly) return 'readonly'
  if (p.expert) return 'expert'
  if (p.advanced) return 'advanced'
  return 'basic'
}

/*
 * Editor-local view level (Option B from the design discussion).
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

watch([() => access.locked, () => access.uilevel], () => {
  if (access.locked && !availableLevels.value.includes(currentLevel.value)) {
    currentLevel.value = access.uilevel
  }
})

/* Capitalize for the picker display — matches the GridSettingsMenu's
 * level options. If vue-i18n lands these become translation lookups. */
const LEVEL_LABELS: Record<UiLevel, string> = {
  basic: 'Basic',
  advanced: 'Advanced',
  expert: 'Expert',
}

function levelLabel(l: UiLevel): string {
  return LEVEL_LABELS[l]
}

const groupedFields = computed(() => {
  const groups: Record<Bucket, IdnodeProp[]> = {
    basic: [],
    advanced: [],
    expert: [],
    readonly: [],
  }
  for (const p of fieldProps.value) {
    if (p.noui || p.phidden) continue
    if (!levelMatches(propLevel(p), currentLevel.value)) continue
    /*
     * No mode-specific skipping. ExtJS only skips fields that are
     * `noui` (always) or `rdonly` AND in bulk-edit mode (the
     * `conf.uuids && p.rdonly` continue at idnode.js:900-901). For
     * single-edit and create, rdonly fields render in the
     * Read-only Info section; they're informational. PO_NOSAVE
     * fields (e.g. `disp_title`/`disp_extratext` on dvrentry) DO
     * accept writes via the prop's `set` callback even though they
     * aren't persisted directly — skipping them broke creation
     * because the entry would have no title.
     */
    groups[bucketOf(p)].push(p)
  }
  return groups
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
 * When only ONE bucket has content (typically the all-basic case
 * above, or an all-readonly informational drawer), the
 * `<details>/<summary>` group wrapper is redundant — there's
 * nothing else to compare against, and the collapsible chrome
 * wastes a row at the top of the form. Drop the wrapper and
 * render fields directly. Multi-bucket classes (basic + advanced
 * / + readonly / etc.) keep the existing wrapper so the user can
 * still distinguish sections + collapse Read-only Info.
 */
const singleBucket = computed(
  () => Object.values(groupedFields.value).filter((arr) => arr.length > 0).length === 1
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
 * Bucket display order + presentation. Drives a single tuple-loop in
 * the template that replaces the four near-identical `<details>`
 * blocks. Only the readonly bucket starts collapsed and adds a class;
 * the basic / advanced / expert buckets share the default open style.
 */
interface BucketDef {
  bucket: Bucket
  title: string
  open: boolean
  extraClass?: string
}

const BUCKETS: readonly BucketDef[] = [
  { bucket: 'basic', title: 'Basic Settings', open: true },
  { bucket: 'advanced', title: 'Advanced Settings', open: true },
  { bucket: 'expert', title: 'Expert Settings', open: true },
  {
    bucket: 'readonly',
    title: 'Read-only Info',
    open: false,
    extraClass: 'idnode-editor__group--readonly',
  },
]

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
      error.value = 'Entry not found'
      fieldProps.value = []
      return
    }
    fieldProps.value = entry.params ?? entry.props ?? []
    currentClass.value = entry.class ?? null
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
    currentClass.value = null
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
  props?: IdnodeProp[]
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
    currentClass.value = res.class ?? null
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
    initialValues.value = init
    currentValues.value = { ...init }
  } catch (e) {
    error.value = e instanceof Error ? e.message : `Failed to load: ${String(e)}`
    fieldProps.value = []
    currentClass.value = null
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
  () => [props.uuid, props.createBase] as const,
  ([uuid, createBase]) => {
    /* Reset per-session validation UX on every drawer open / mode
     * flip. Touched state belongs to the current edit; submit
     * attempts likewise. Errors recompute from the new fieldProps
     * once the load resolves. */
    touched.value = new Set()
    submitAttempted.value = false
    if (uuid) {
      loadRow(uuid)
    } else if (createBase) {
      loadCreate()
    } else {
      fieldProps.value = []
      initialValues.value = {}
      currentValues.value = {}
      currentClass.value = null
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
    error.value = e instanceof Error ? e.message : `Failed to save: ${String(e)}`
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
    error.value = e instanceof Error ? e.message : `Failed to create: ${String(e)}`
  } finally {
    saving.value = false
  }
}

function apply() {
  void save({ keepOpen: true })
}

const confirmDialog = useConfirmDialog()

async function attemptClose() {
  if (dirty.value && !saving.value) {
    const ok = await confirmDialog.ask('Discard unsaved changes?', {
      severity: 'danger',
      acceptLabel: 'Discard',
      rejectLabel: 'Keep editing',
    })
    if (!ok) return
  }
  emit('close')
}

/* ---- Keyboard: Esc closes (PrimeVue handles, but we wrap for
 *      symmetry with the dirty-confirm) ---- */
function onKeyDown(ev: KeyboardEvent) {
  if (ev.key === 'Escape' && visible.value) {
    attemptClose()
  }
}
onMounted(() => document.addEventListener('keydown', onKeyDown))
onBeforeUnmount(() => document.removeEventListener('keydown', onKeyDown))
</script>

<template>
  <Drawer
    v-model:visible="visible"
    position="right"
    class="idnode-editor"
    :pt="{ root: { class: 'idnode-editor__root' } }"
  >
    <!--
      Custom header: title left, view-level picker right (just before
      PrimeVue's own close-X). Combining them into one row reclaims a
      ~40 px strip the standalone level row used to occupy. The picker
      defaults to the level the calling grid was at; bumping it here
      only affects this edit session (re-inherits when opening the
      next row). Capped at access.uilevel when the user's level is
      admin-locked. Hidden when only one level is available — header
      then renders just the title, identical to before.
    -->
    <template #header>
      <div class="idnode-editor__header">
        <span class="idnode-editor__title">{{ title ?? 'Edit' }}</span>
        <select
          v-if="availableLevels.length > 1 && hasNonBasicFields"
          id="idnode-editor-level"
          v-model="currentLevel"
          class="idnode-editor__level-select"
          aria-label="View level"
        >
          <option v-for="l in availableLevels" :key="l" :value="l">
            {{ levelLabel(l) }}
          </option>
        </select>
      </div>
    </template>
    <div class="idnode-editor__body">
      <p v-if="loading" class="idnode-editor__status">Loading…</p>
      <p v-else-if="error" class="idnode-editor__error">{{ error }}</p>
      <form v-else class="idnode-editor__form" @submit.prevent="save()">
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
          Bucket wrapper: `<details>/<summary>` when multiple
          buckets have content (Basic + Advanced + Read-only Info,
          etc.), plain `<div>` when only one bucket exists (the
          summary header would have nothing to compare against
          and just adds a wasted row). `<component :is>` flips the
          element while keeping the same inner field-rendering
          block — `<summary>` is `v-if`'d out in single-bucket
          mode where it would be invalid inside a `<div>`.
        -->
        <template v-for="b in BUCKETS" :key="b.bucket">
          <component
            :is="singleBucket ? 'div' : 'details'"
            v-if="groupedFields[b.bucket].length > 0"
            class="idnode-editor__group"
            :class="b.extraClass"
            :open="singleBucket ? undefined : b.open"
          >
            <summary v-if="!singleBucket" class="idnode-editor__group-title">
              {{ b.title }}
            </summary>
            <div class="idnode-editor__group-body">
              <template v-for="p in groupedFields[b.bucket]" :key="p.id">
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
                  class="ifld-row"
                  :class="{
                    'ifld-row--dirty': isFieldDirty(p.id),
                    'ifld-row--required': isRequired(p.id),
                    'ifld-row--invalid': !!visibleError(p.id),
                  }"
                >
                  <component
                    :is="rendererFor(p)!"
                    v-if="rendererFor(p)"
                    :prop="p"
                    :model-value="valueFor(p, currentValues[p.id])"
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
    </div>
    <template #footer>
      <div class="idnode-editor__footer">
        <button
          type="button"
          class="idnode-editor__btn idnode-editor__btn--cancel"
          :disabled="saving"
          @click="attemptClose"
        >
          Cancel
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
          :disabled="loading || saving || !!error || !dirty || hasErrors"
          @click="apply"
        >
          Apply
        </button>
        <!--
          Save also disables on validation errors — but only when
          the form is dirty. A non-dirty form with stale invalid
          server state can still close (the user didn't change it,
          so there's nothing for client validation to flag); fixing
          that data is a separate visit.
        -->
        <!--
          Save gating:
            - loading / saving / error: never submit while busy or
              broken.
            - (dirty && hasErrors): never submit invalid values.
            - (!isCreate && !dirty): in EDIT mode, no-op submits
              are wasteful — disable until the user actually
              changes something. In CREATE mode, leave Save
              enabled so users can create-with-defaults (the
              create endpoint accepts an empty conf and the
              server fills its declared defaults).
        -->
        <button
          type="button"
          class="idnode-editor__btn idnode-editor__btn--save"
          :disabled="
            loading || saving || !!error || (dirty && hasErrors) || (!isCreate && !dirty)
          "
          @click="save()"
        >
          {{ saving ? 'Saving…' : 'Save' }}
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
  font-size: 13px;
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
  font-size: 18px;
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
  font-size: 13px;
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
  font-size: 13px;
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
  font-size: 12px;
}

/*
 * Single-line field layout — same convention as ConfigGeneralBaseView,
 * lifted from ExtJS' `idnode_simple` (`labelWidth: 250` in
 * static/app/idnode.js:2843, applied to every editor form). Each
 * field's `.ifld` becomes a 2-column grid; label cell on the left at
 * 180px, control cell on the right taking the remainder. All
 * controls land at the same x-coordinate inside the drawer.
 *
 * Label column is narrower than the General page's 250px because
 * the drawer is only 480px wide (700+px on the General page) — 180px
 * + 12px gap leaves ~256px for the control, which works for text
 * inputs / selects / time pickers / number inputs at typical
 * content lengths. Long labels wrap rather than truncate (default
 * word-break behaviour in a fixed-width grid cell). Below 600px
 * viewport the layout reverts to the field components' default
 * stacked layout so phones don't get cramped.
 *
 * `.ifld__row` is the wrapper Time fields use around their date /
 * datetime / duration inputs; pushing it into column 2 puts those
 * inputs alongside the rest of the form's controls. Bool used to
 * use `.ifld__row` too (single child holding checkbox + inline
 * caption); that shape was retired so the caption now sits in
 * column 1 like every other field — see IdnodeFieldBool's header
 * comment for the rationale.
 *
 * Applies to every IdnodeEditor consumer (DVR Upcoming/Finished/
 * Failed/Removed, Autorecs/Timers, TV Adapters tree leaves) in one
 * place.
 */
.idnode-editor__group-body :deep(.ifld) {
  display: grid;
  grid-template-columns: 180px 1fr;
  align-items: center;
  gap: var(--tvh-space-3);
}

.idnode-editor__group-body :deep(.ifld__row) {
  grid-column: 2;
}

/*
 * Clamp control width to the grid cell. Text/number inputs already
 * respect this because their default width is a fixed character
 * count (~20ch ≈ 150-200px), which slots inside the 1fr track. Native
 * `<select>` elements are different: their intrinsic width is
 * `max-content` of the option list, so a long option (channel names,
 * timezones, the 144-entry HH:MM picklist) makes the select want to
 * be wider than the cell. CSS Grid then grows the `1fr` track past
 * its allocation to satisfy that min-content, pushing the overall
 * layout out. `min-width: 0` on the grid item lets the track stay at
 * 1fr; `width: 100%` makes the control fill exactly the cell.
 * Combined effect: every control type sits at the same right edge
 * regardless of its content.
 */
.idnode-editor__group-body :deep(.ifld__input) {
  width: 100%;
  min-width: 0;
}

/*
 * Phone fallback to stacked layout. 767px matches the rest of the
 * codebase's phone breakpoint (NavRail's hamburger, PageTabs select,
 * the unscoped Drawer-width rule below that pins the drawer to 100%
 * width on phone). At 100%-width drawer on a typical phone (360–
 * 414px), 180px label + 12px gap would leave the control with only
 * ~150–220px which is too cramped — better to give the control the
 * full row width with the label above.
 */
@media (max-width: 767px) {
  .idnode-editor__group-body :deep(.ifld) {
    grid-template-columns: 1fr;
  }
  .idnode-editor__group-body :deep(.ifld__row) {
    grid-column: auto;
  }
}

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
 *
 * Considered + dropped during eyeball review: a 3 px left-border
 * stripe on the row gutter (Salesforce / Jira pattern). Felt
 * redundant on a 5-15-field drawer where the user just typed in
 * the change seconds ago — the dot alone carries enough signal.
 * Reconsider if forms grow (Configuration leaves with 30+ fields)
 * or when the future per-field-undo affordance lands and wants the
 * row gutter for its anchor.
 */
.ifld-row--dirty :deep(.ifld__label)::before {
  content: '•';
  color: var(--tvh-primary);
  font-weight: bold;
  margin-right: 4px;
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
  font-size: 12px;
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
  font-size: 13px;
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
  font-size: 13px;
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
