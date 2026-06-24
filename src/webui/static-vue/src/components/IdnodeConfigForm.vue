<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * IdnodeConfigForm — shared scaffold for Configuration pages whose
 * body is a single editable idnode (Base, Image Cache, SAT>IP
 * Server, and future Configuration L2/L3 leaves).
 *
 * Owns:
 *   - load (`apiCall(loadEndpoint, { meta: 1 })`)
 *   - dirty detection (shallow primitive comparison vs `baseline`)
 *   - save (`apiCall(saveEndpoint, { node: JSON.stringify(currentValues) })`)
 *   - undo (reset currentValues to baseline)
 *   - error / loading state
 *   - per-page UI-level override via `<LevelMenu>` (defaults to
 *     `access.uilevel`, honours `access.locked` cap; pages can
 *     opt out + pin a fixed level via the `lockLevel` prop)
 *   - server-defined property-group rendering (collapsing empty
 *     groups out of the layout)
 *   - reload-on-save for fields whose value rides Comet
 *     `accessUpdate` only at WS-connect time (caller passes
 *     `reloadFields`)
 *
 * Each per-page consumer (ConfigGeneralBaseView,
 * ConfigGeneralImageCacheView, ConfigGeneralSatipServerView)
 * supplies the endpoints + an optional `#actions` slot for
 * page-specific toolbar buttons (clean / trigger / discover) that
 * sit between Undo and the LevelMenu. The slot receives
 * `{ loading, saving }` so custom actions can disable themselves
 * consistently.
 *
 * Renderer dispatch (`rendererFor` / `valueFor`) is the same
 * helper IdnodeEditor.vue uses — see
 * `idnode-fields/rendererDispatch.ts`.
 */
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { apiCall } from '@/api/client'
import { useAccessStore } from '@/stores/access'
import { levelMatches, propLevel, type IdnodeProp, type PropertyGroup } from '@/types/idnode'
import type { UiLevel } from '@/types/access'
import { rendererFor, valueFor } from '@/components/idnode-fields/rendererDispatch'
import { validateField } from '@/components/idnode-fields/validators'
import {
  applyClassRules,
  isFieldRequired,
} from '@/components/idnode-fields/validationRules'
import LevelMenu from '@/components/LevelMenu.vue'
import { useI18n } from '@/composables/useI18n'
import { useHelp } from '@/composables/useHelp'
import { HelpCircle } from 'lucide-vue-next'

const { t } = useI18n()
const help = useHelp()

/* Help button click — same `toggle` semantics IdnodeGrid uses. */
function onHelpClick(): void {
  if (!props.helpPage) return
  help.toggle(props.helpPage).catch(() => {})
}

const props = withDefaults(
  defineProps<{
    /* Server endpoint for the initial load (singleton-config
     * mode). Always passed `{ meta: 1 }` so the response includes
     * group metadata. Mutually exclusive with `uuid` — pass one or
     * the other. Required when `uuid` is not set. */
    loadEndpoint?: string
    /* Server endpoint for save (singleton-config mode). The form
     * POSTs `{ node: JSON.stringify(currentValues) }`. Required
     * when `loadEndpoint` is set. */
    saveEndpoint?: string
    /* When set, switches the form into multi-instance mode: load
     * via `idnode/load?uuid=<uuid>&meta=1`, save via
     * `idnode/save` with `{ node: JSON.stringify({ uuid, ...
     * currentValues }) }`. Watching this prop drives reload-on-
     * selection-change in master-detail layouts. Mutually
     * exclusive with `loadEndpoint` / `saveEndpoint`. */
    uuid?: string | null
    /* Field ids whose change forces `globalThis.location.reload()`
     * after a successful save — for fields whose value rides the
     * Comet `accessUpdate` notification only at WS-connect time
     * (e.g., the global config UI prefs). Default empty: page-
     * specific configs (Image Cache, SAT>IP) don't ride
     * accessUpdate. */
    reloadFields?: readonly string[]
    /* Pin the displayed view level for this page and hide the
     * `<LevelMenu>` chooser entirely. Use when every field on the
     * page is gated at a single server-side `PO_*` level and there
     * is nothing for the user to choose between (e.g. Image Cache
     * — every field `PO_EXPERT`; matches legacy ExtJS
     * `uilevel: 'expert'` at `static/app/config.js:111` which
     * suppresses ExtJS's own level button at `idnode.js:2953`).
     * Combines pin + hide in a single prop so call sites can't hide
     * the chooser without committing to a level (which would leave
     * the form blank for users on a lower default level). */
    lockLevel?: UiLevel
    /* Optional predicate that synthesises a disabled state for
     * individual fields based on the current values of OTHER
     * fields. Called during render with `(field.id,
     * currentValues.value)`. When the predicate returns true, the
     * field renders with `prop.rdonly = true` (the existing read-
     * only path each renderer already honours), so no per-renderer
     * changes are needed.
     *
     * Use for ExtJS-style onchange dependencies — e.g. Timeshift's
     * `max_period` disabled when `unlimited_period` is checked,
     * `max_size` disabled when `unlimited_size || ram_only` (per
     * legacy `static/app/timeshift.js:3-14`). The predicate runs
     * inside the form's reactive scope so toggling the trigger
     * field immediately re-renders the dependent field as
     * disabled.
     *
     * Server-side `rdonly: true` always wins — the predicate only
     * adds extra disabled states; it can't re-enable a field the
     * server has marked read-only. */
    disabledFor?: (id: string, values: Record<string, unknown>) => boolean
    /* Optional override for the Save button's text. Defaults to
     * "Save". Use when a page wants different wording — e.g.
     * Configuration → Debugging where Classic's button reads
     * "Apply configuration (run-time only)" because the tvhlog
     * config doesn't persist across restarts. While saving, the
     * button still shows "Saving…" regardless of this prop so
     * the in-flight signal stays consistent. */
    saveLabel?: string
    /* Optional `title` (PrimeVue `v-tooltip`) on the Save button.
     * Pairs with `saveLabel` for pages that need to clarify the
     * save semantic — e.g. Debugging's "They will be lost when
     * the application next restarts.". Empty / unset → no
     * tooltip. */
    saveTooltip?: string
    /* Optional per-field overrides applied after every load.
     * Keys match field IDs; values replace the loaded `value` in
     * `currentValues` (without rewriting `baseline`, so the
     * affected fields read as DIRTY immediately and Save enables
     * without further user interaction).
     *
     * Use for caller-driven preselects that the server doesn't
     * persist itself — e.g. Service Mapper's `services` field
     * pre-populated from the user's grid selection on the
     * Channels / DVB Services pages. Without this prop the form
     * would always load the server's last-saved selection,
     * forcing the user to re-pick services on every navigation.
     *
     * Reactive: changing `preselect` while the form is mounted
     * applies the new overrides on the NEXT load (or the next
     * watcher tick). Pass null/undefined to opt out. */
    preselect?: Readonly<Record<string, unknown>> | null
    /* When true, bypasses the dirty-state guard: the Save button
     * stays enabled even when `currentValues` matches `baseline`,
     * and `save()` proceeds rather than early-returning. Use for
     * "trigger this process" forms where each click should fire
     * the request — Service Mapper is the canonical consumer
     * (one click = start a mapping job, regardless of whether
     * the form's options changed since last save). Mirrors
     * Classic's `idnode_editor_win({ alwaysDirty: true })` flag
     * at `static/app/idnode.js:1155`.
     *
     * Default false — every existing edit-form / singleton-config
     * consumer (Base / Image cache / SAT>IP / Timeshift /
     * Debugging Configuration / DVR Profile / Stream Profile /
     * CA drawer) is unchanged because they don't pass this
     * prop. */
    alwaysDirty?: boolean
    /* Allowlist of multi-select enum field IDs to render as
     * inline checkboxes instead of the default MultiSelect
     * dropdown. Right for known-small fixed sets (e.g. days of
     * the week). Anything not on this list renders as a
     * dropdown — including small variable-size lists, because
     * option count alone isn't a reliable signal. */
    inlineEnumMultiFields?: ReadonlyArray<string>
    /* When true, suppresses the form's built-in action toolbar
     * (Save / Undo / LevelMenu). Used by wrappers that render
     * those actions in their own chrome — the setup wizard's
     * WizardFooter is the canonical consumer. The wrapper
     * triggers a save via the exposed `save()` method (template
     * ref) and re-mounts the form per-step (so Undo is
     * irrelevant). LevelMenu is also dropped because wizard
     * steps are designed for a single uilevel. Default false —
     * every existing consumer keeps its toolbar. */
    hideToolbar?: boolean
    /* Allowlist of str-typed enum field IDs that should NOT
     * advertise a `(none)` clear-to-null option. The form
     * decorates each matching prop's `mandatory: true` flag
     * inside effectiveProp, which IdnodeFieldEnum then honours.
     * Use for config singletons whose runtime defaults are
     * always set (language_ui, theme_ui, etc.) — Classic doesn't
     * offer a clear affordance for these either.
     *
     * Manual mapping holds until a server-emitted `PO_MANDATORY`
     * prop opt surfaces the same flag on `prop.mandatory`
     * directly — once that lands this list collapses to empty.
     * Default empty: every other consumer is unchanged. */
    mandatoryFields?: ReadonlyArray<string>
    /* Field IDs to suppress from the auto-rendered form body.
     * The field's value is still loaded into `currentValues` and
     * still saved on the next POST — only the rendered input is
     * hidden. Use when a wrapper view renders a custom editor for
     * the field above/below the form (Config → Debugging's
     * subsystem pickers are the canonical consumer: the multiline
     * `debugsubs` / `tracesubs` strings are replaced with two
     * dual-pane include/exclude widgets wired through the form's
     * exposed `currentValues`). Default empty. */
    hideFields?: ReadonlyArray<string>
    /* Markdown page key for the in-app Help button — same shape
     * as IdnodeGrid's `helpPage`. When set, renders a Lucide
     * HelpCircle icon button at the right end of the toolbar
     * (after LevelMenu) that toggles the AppShell-mounted
     * HelpDialog on this page. Unset → no Help button. Suppressed
     * automatically when `hideToolbar` is true (the wizard's
     * footer renders its own help affordance). */
    helpPage?: string
  }>(),
  {
    loadEndpoint: undefined,
    saveEndpoint: undefined,
    uuid: undefined,
    reloadFields: () => [],
    lockLevel: undefined,
    disabledFor: undefined,
    saveLabel: undefined,
    saveTooltip: undefined,
    preselect: undefined,
    alwaysDirty: false,
    inlineEnumMultiFields: () => [],
    hideToolbar: false,
    mandatoryFields: () => [],
    hideFields: () => [],
    helpPage: undefined,
  }
)

const emit = defineEmits<{
  /* Fired after a successful save POST resolves, before the
   * post-save `load()` refresh. Mirrors `IdnodeEditor.vue`'s
   * `saved` emit so parent views can react with toasts /
   * navigation / etc. without subscribing to a comet channel. */
  saved: []
  /* Fired after each successful load with the params array.
   * Wrappers (e.g. WizardStepGeneric) use this to pull out
   * specific server-emitted fields like the wizard step's
   * `icon` / `description` and render them as chrome above
   * the form. The emit fires on initial load and on every
   * post-save refresh. */
  loaded: [params: IdnodeProp[]]
}>()

const access = useAccessStore()

/* ---- View-level override (per-view, session-scoped) ---- */

/* Local view level — defaults to the user's effective
 * `access.uilevel`, but the LevelMenu in the toolbar lets the
 * user widen / narrow it for this page only without touching the
 * global preference. Mirrors the IdnodeEditor drawer's per-session
 * level picker. Honours `access.locked` (config.uilevel_nochange):
 * if the admin pinned the level, the menu's radio is disabled and
 * any active local override gets clamped down to the cap.
 *
 * When `props.lockLevel` is set the page commits to that level —
 * `currentLevel` is initialised to it and a watcher keeps it
 * pinned across access-store mutations. The LevelMenu is `v-if`'d
 * out in the template. */
const currentLevel = ref<UiLevel>(props.lockLevel ?? access.uilevel)

watch([() => access.locked, () => access.uilevel], ([locked, cap]) => {
  if (props.lockLevel) return
  if (locked && currentLevel.value !== cap) {
    currentLevel.value = cap
  }
})

watch(
  () => props.lockLevel,
  (lock) => {
    if (lock) currentLevel.value = lock
  }
)

/* ---- Load ---- */

interface LoadResponse {
  entries?: Array<{
    uuid?: string
    class?: string
    params?: IdnodeProp[]
    meta?: { groups?: PropertyGroup[]; props?: IdnodeProp[] }
  }>
}

const fieldProps = ref<IdnodeProp[]>([])
const groups = ref<PropertyGroup[]>([])
const baseline = ref<Record<string, unknown>>({})
const currentValues = ref<Record<string, unknown>>({})

const loading = ref(true)
const saving = ref(false)
const error = ref<string | null>(null)

/* Class id of the loaded entry — keyed lookup into `CLASS_RULES`
 * for required / cross-field / minLength rules. Server emits it on
 * every `idnode/load` response and on singleton-config load endpoints
 * (e.g. `config/load`). Falls back to null when absent — `applyClassRules`
 * treats null as "no class-level rules", same default as IdnodeEditor. */
const currentClass = ref<string | null>(null)

/* Touched fields — error messages stay hidden on untouched fields so
 * opening a fresh form with empty required fields doesn't immediately
 * shout at the user. The Save click promotes every error to visible
 * via `submitAttempted`. Mirrors IdnodeEditor.vue:251-484. */
const touched = ref<Set<string>>(new Set())
const submitAttempted = ref(false)

async function load() {
  /* In uuid mode, an unset uuid (null / undefined / '') means
   * "nothing selected yet" — don't fire a load. Caller normally
   * gates the component with `v-if="selectedUuid"`, but defend
   * against a transient empty-string case during route transitions. */
  if (!props.uuid && !props.loadEndpoint) {
    loading.value = false
    fieldProps.value = []
    groups.value = []
    baseline.value = {}
    currentValues.value = {}
    return
  }
  loading.value = true
  error.value = null
  try {
    /* uuid mode: idnode/load with uuid + meta. Server returns the
     * same `{ entries: [{ params, meta: { groups } }] }` shape as
     * the singleton config-load endpoints. Singleton mode: caller-
     * supplied loadEndpoint with meta=1. Either is OK; the response
     * shape converges. */
    const [endpoint, params] = props.uuid
      ? ['idnode/load', { uuid: props.uuid, meta: 1 }]
      : [props.loadEndpoint as string, { meta: 1 }]
    if (!endpoint) {
      error.value = t('Configuration form misconfigured (no endpoint)')
      return
    }
    const res = await apiCall<LoadResponse>(endpoint, params)
    const entry = res.entries?.[0]
    if (!entry) {
      error.value = t('Failed to load configuration')
      return
    }
    fieldProps.value = entry.params ?? []
    groups.value = entry.meta?.groups ?? []
    currentClass.value = entry.class ?? null
    /* Fresh load resets the touch tracking so previously-touched
     * fields don't carry their visible-error state across a reload
     * of the same form (e.g. after Save). */
    touched.value = new Set()
    submitAttempted.value = false
    const init: Record<string, unknown> = {}
    for (const p of fieldProps.value) {
      init[p.id] = p.value ?? null
    }
    baseline.value = init
    /* Apply caller-supplied preselects to currentValues but NOT
     * to baseline. The mismatch lights up the dirty marker on
     * every overridden field + enables Save immediately, which
     * is the user-visible point of preselect: "the page just
     * pre-filled this for me, I don't have to do anything else
     * before clicking the button". */
    const next: Record<string, unknown> = { ...init }
    if (props.preselect) {
      for (const k of Object.keys(props.preselect)) {
        if (k in next) next[k] = props.preselect[k]
      }
    }
    currentValues.value = next
    /* Notify wrappers — wizard-side WizardStepGeneric pulls
     * `icon` / `description` out of the params here so it can
     * render them as chrome above the form without a second
     * fetch. Other consumers don't listen and are unaffected. */
    emit('loaded', fieldProps.value)
  } catch (e) {
    error.value = e instanceof Error ? e.message : t('Failed to load: {0}', String(e))
  } finally {
    loading.value = false
  }
}

onMounted(load)

/* Re-load when the caller switches the selected uuid (master-
 * detail layouts). Singleton mode never sets uuid so this watcher
 * is a no-op there. */
watch(
  () => props.uuid,
  (uuid, prev) => {
    if (uuid !== prev) load()
  }
)

/* ---- View-level filter + grouping ---- */

const hideFieldsSet = computed(() => new Set(props.hideFields))

const visibleFields = computed(() =>
  fieldProps.value.filter((p) => {
    if (p.noui || p.phidden) return false
    if (hideFieldsSet.value.has(p.id)) return false
    return levelMatches(propLevel(p), currentLevel.value)
  })
)

/* Build the displayed group list from the server-provided group
 * order, dropping any group whose visible-field bucket is empty.
 * Within a group, fields preserve declaration order from the C-
 * side prop_t array (`Array.filter` is stable; bucketing uses
 * push() so within-group order is preserved too).
 *
 * Sub-groups (PropertyGroup entries with `parent` set) get their
 * fields MERGED into the parent group's bucket — they don't
 * render as a separate section. This matches the C-side intent:
 * dvr_config.c declares the Filename/Tagging block as group 4
 * with name "Filename/Tagging Settings", and a sibling group 5
 * with `name: ""` and `parent: 4` carrying the secondary
 * filename fields (omit-title, clean-title, whitespace-in-title,
 * windows-compatible-filenames, tag-files, create-scene-markers).
 * Without merging, those secondary fields render as an unnamed
 * orphan section between Filename/Tagging and EPG/Autorec —
 * users complained that "these belong to the filename/tagging
 * group". Classic ExtJS renders them nested inside the parent's
 * fieldset; flat-merge gets the same end-state with simpler
 * markup.
 *
 * For pages with NO server groups (e.g., Image Cache emits
 * fields without a group structure), every field falls into a
 * synthetic group 0 — single-section layout, same template path. */
const fieldsByGroup = computed(() => {
  /* Resolve a field's effective destination group: walk the
   * `parent` chain on PropertyGroup entries until we hit a root
   * (no `parent` set). Defensive against missing parents — a
   * sub-group whose parent isn't declared falls back to its own
   * number so the field still renders somewhere. */
  const groupByNumber = new Map<number, PropertyGroup>()
  for (const g of groups.value) groupByNumber.set(g.number, g)

  function rootGroup(num: number): number {
    /* Bound the walk to avoid infinite loops on malformed data
     * (cycle / self-parent). 8 hops is far more than any C-side
     * group hierarchy goes. */
    let cur = num
    for (let i = 0; i < 8; i++) {
      const g = groupByNumber.get(cur)
      if (!g || g.parent === undefined || g.parent === cur) return cur
      /* Parent declared but not actually present in the groups
       * list — stop here so the field stays bucketed under its
       * declared group, which `displayedGroups` will surface as
       * a root section. */
      if (!groupByNumber.has(g.parent)) return cur
      cur = g.parent
    }
    return cur
  }

  const map = new Map<number, IdnodeProp[]>()
  for (const p of visibleFields.value) {
    const declared = p.group ?? 0
    const g = rootGroup(declared)
    const list = map.get(g) ?? []
    list.push(p)
    map.set(g, list)
  }
  return map
})

/* Classes with NO server-defined groups (Timeshift, Image cache)
 * get level-bucketed rendering — same as Classic `idnode_simple`
 * at `static/app/idnode.js:1047-1059`. Three synthetic fieldsets
 * ("Basic Settings" / "Advanced Settings" / "Expert Settings"),
 * each holding the visible fields of that level. Special case
 * (line 1047): if ONLY the Basic bucket has fields, render a
 * single unnamed section — no point titling a fieldset when
 * there's only one. Matches Classic's `df.length && !af.length
 * && !rf.length` gate (we don't track read-only as a separate
 * bucket today; read-only fields stay in their natural level
 * bucket and render disabled inline). Negative synthetic group
 * numbers so we don't collide with any real server group number
 * (always >= 0). */
function bucketByLevel(
  fields: IdnodeProp[],
): { group: PropertyGroup; fields: IdnodeProp[] }[] {
  const basic: IdnodeProp[] = []
  const advanced: IdnodeProp[] = []
  const expert: IdnodeProp[] = []
  for (const p of fields) {
    const lvl = propLevel(p)
    if (lvl === 'expert') expert.push(p)
    else if (lvl === 'advanced') advanced.push(p)
    else basic.push(p)
  }
  if (basic.length > 0 && advanced.length === 0 && expert.length === 0) {
    return [{ group: { number: 0, name: '' }, fields: basic }]
  }
  const out: { group: PropertyGroup; fields: IdnodeProp[] }[] = []
  if (basic.length) out.push({ group: { number: -1, name: t('Basic Settings') }, fields: basic })
  if (advanced.length)
    out.push({ group: { number: -2, name: t('Advanced Settings') }, fields: advanced })
  if (expert.length)
    out.push({ group: { number: -3, name: t('Expert Settings') }, fields: expert })
  return out
}

const displayedGroups = computed(() => {
  if (groups.value.length === 0 && visibleFields.value.length > 0) {
    return bucketByLevel(visibleFields.value)
  }

  /* A group is a "root" — and renders as a top-level section —
   * when it has no parent OR its declared parent doesn't exist
   * in the groups list (defensive: malformed metadata shouldn't
   * cause its fields to vanish). Sub-groups whose parent IS
   * present had their fields merged in `fieldsByGroup` so we
   * skip them here. */
  const groupNumbers = new Set(groups.value.map((g) => g.number))
  const out = groups.value
    .filter((g) => g.parent === undefined || !groupNumbers.has(g.parent))
    .map((g) => ({ group: g, fields: fieldsByGroup.value.get(g.number) ?? [] }))
    .filter((g) => g.fields.length > 0)
  /* If the page has fields outside any server group (group 0 with
   * no matching PropertyGroup entry), append a synthetic
   * "ungrouped" section so they still render. Common for simple
   * idnode classes that don't bother declaring groups. */
  const ungrouped = fieldsByGroup.value.get(0)
  const hasGroupZero = groups.value.some((g) => g.number === 0)
  if (ungrouped && ungrouped.length > 0 && !hasGroupZero) {
    out.push({ group: { number: 0, name: '' }, fields: ungrouped })
  }
  return out
})

/* ---- Dirty detection ---- */

/* Shallow comparison: works for the primitives this form deals
 * with (str / int / u32 / u16 / s64 / dbl / bool). PT_LORDER
 * list-shaped fields currently route to the placeholder renderer
 * — the user can't change them, so currentValues[k] stays
 * referentially equal to baseline[k] by definition. */
const isDirty = computed(() => {
  const b = baseline.value
  const c = currentValues.value
  for (const k of Object.keys(b)) {
    if (b[k] !== c[k]) return true
  }
  return false
})

/* Per-field dirty set — drives the small `•` marker on each
 * field's label so the user can spot which fields they've
 * touched without scanning every value. JSON-stringify lets
 * the comparison work for any value shape (matches
 * IdnodeEditor.vue's same-purpose computed). The overall
 * `isDirty` above stays for the Save / Undo gate. */
const dirtyIds = computed(() => {
  const ids = new Set<string>()
  const c = currentValues.value
  const b = baseline.value
  for (const k of Object.keys(c)) {
    if (JSON.stringify(c[k]) !== JSON.stringify(b[k])) ids.add(k)
  }
  return ids
})

function isFieldDirty(id: string): boolean {
  return dirtyIds.value.has(id)
}

/* Membership lookup for the inline-EnumMulti allowlist. Computed
 * once per prop change; per-field check is O(1). Empty by
 * default — every EnumMulti renders as MultiSelect dropdown
 * unless the caller opts the field in. */
const inlineEnumMultiSet = computed(
  () => new Set(props.inlineEnumMultiFields),
)

/* Membership lookup for the mandatory-fields allowlist. Same
 * O(1) per-field shape as the EnumMulti set above. Read inside
 * effectiveProp to synthesise prop.mandatory on the matching
 * fields. */
const mandatorySet = computed(
  () => new Set(props.mandatoryFields),
)

/* Per-render prop synthesis for two caller-supplied overlays:
 *
 *   - `disabledFor` predicate — synthesises `rdonly: true` on
 *     fields the predicate flags. Field renderers already wire
 *     `prop.rdonly` to the input's `disabled` attribute (e.g.
 *     `IdnodeFieldBool.vue:47`), so no per-renderer changes are
 *     required. Server-side `rdonly: true` always wins.
 *   - `mandatoryFields` allowlist — synthesises `mandatory: true`
 *     on the matching field ids. IdnodeFieldEnum reads that flag
 *     to suppress the `(none)` clear-to-null option. Manual
 *     allowlist today; future C-side `PO_MANDATORY` flag will
 *     surface this directly via the server's prop serializer.
 *
 * Most fields hit neither overlay and pass through unchanged.
 * Called from the template, so Vue's reactivity tracks
 * `currentValues.value` — toggling a trigger field re-evaluates
 * the disabled predicate and re-renders dependent fields with
 * the synthesised state. */
function effectiveProp(p: IdnodeProp): IdnodeProp {
  const isMandatory = mandatorySet.value.has(p.id)
  const isDisabled =
    !!props.disabledFor &&
    !p.rdonly &&
    props.disabledFor(p.id, currentValues.value)
  if (!isMandatory && !isDisabled) return p
  const out: IdnodeProp = { ...p }
  if (isMandatory) out.mandatory = true
  if (isDisabled) out.rdonly = true
  return out
}

/* ---- Validation ----
 *
 * Two layers per render, same shape IdnodeEditor.vue:462-487 uses:
 *
 *   1. Per-field type-driven (`validateField`) — covers what
 *      prop_t metadata declares: integer / float ranges, hex
 *      format, intsplit format, enum membership.
 *   2. Per-class hardcoded (`applyClassRules`) — required + cross-
 *      field comparisons + minLength. Sourced from C set-callbacks
 *      and create-handlers; see validationRules.ts.
 *
 * Per-field errors win on the same field (a "must be a number" beats
 * a cross-field "stop must be after start" when stop is also bad).
 * `visibleError(id)` gates display on the touched / submitAttempted
 * UX rules so a freshly-loaded form doesn't shout about empty
 * required fields before the user has done anything.
 */
const errors = computed<Map<string, string>>(() => {
  const map = new Map<string, string>()
  /* Per-field type-driven first. Skip server-rdonly + UI-hidden
   * props — the user can't fix them anyway. */
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

/* Field-change handler — write through to currentValues and mark
 * the field touched so its error (if any) becomes visible. Replaces
 * the previous `currentValues[p.id] = $event` inline assignment in
 * the template. */
function onFieldChange(id: string, value: unknown) {
  currentValues.value[id] = value
  touched.value.add(id)
}

/* ---- Save / Undo ---- */

async function save() {
  /* `alwaysDirty` bypasses the dirty-state guard for trigger
   * forms (Service Mapper). Saving in-flight still gates so a
   * user can't double-fire while a previous post is pending. */
  if ((!isDirty.value && !props.alwaysDirty) || saving.value) return
  /* Validation gate. Promote every error to visible so the user
   * sees what's wrong even on fields they haven't touched yet,
   * mirroring IdnodeEditor's submit-attempt behaviour. The Save
   * button is also disabled when hasErrors, but keystroke-driven
   * Enter-to-submit can still arrive here. */
  submitAttempted.value = true
  if (hasErrors.value) return
  saving.value = true
  error.value = null
  try {
    /* Snapshot the reload-trigger decision against the PRE-save
     * baseline before the api call mutates anything we're tracking. */
    const needsReload = props.reloadFields.some(
      (k) => currentValues.value[k] !== baseline.value[k]
    )

    /* uuid mode: idnode/save with the uuid baked into the node
     * payload — server's `idnode/save` handler reads `uuid` out of
     * the parsed node and dispatches to the matching idnode. */
    const [endpoint, body] = props.uuid
      ? [
          'idnode/save',
          {
            node: JSON.stringify({ uuid: props.uuid, ...currentValues.value }),
          },
        ]
      : [
          props.saveEndpoint as string,
          { node: JSON.stringify(currentValues.value) },
        ]
    if (!endpoint) {
      error.value = t('Configuration form misconfigured (no save endpoint)')
      return
    }

    await apiCall(endpoint, body)
    /* Fire the saved emit AFTER the apiCall resolves but BEFORE
     * needsReload's full-page reload (which would unmount us
     * before the listener could run). The pre-`load()` ordering
     * matters less — listeners typically don't care whether
     * baseline has refreshed yet. */
    emit('saved')

    if (needsReload) {
      /* Forced reload mirrors ExtJS's postsave behaviour. The
       * reconnect's first accessUpdate carries fresh values for
       * the affected user-pref fields. */
      globalThis.location.reload()
      return
    }

    /* Refresh from server so baseline + currentValues snap to the
     * persisted values (in case the server normalised anything we
     * sent). */
    await load()
  } catch (e) {
    error.value = e instanceof Error ? e.message : t('Save failed: {0}', String(e))
  } finally {
    saving.value = false
  }
}

function undo() {
  if (!isDirty.value) return
  /* Spread the baseline so currentValues becomes a fresh object
   * — triggers reactive updates on every field input. */
  currentValues.value = { ...baseline.value }
}

/* Expose imperative handles for wrappers that render their own
 * action chrome (setup wizard's WizardFooter is the canonical
 * consumer). The wrapper acquires a template ref to this
 * component and:
 *   - calls `save()` when the user clicks its external Save
 *     button (it then waits for the `saved` emit to navigate);
 *   - calls `reload()` to refetch the load endpoint in place —
 *     used by the wizard hello-step's live-preview language
 *     refresh (POST save + refetch with new language → form
 *     captions re-render in the new language without
 *     remounting / navigating);
 *   - reads `saving` to disable its Save button while a POST
 *     is in flight;
 *   - reads `loading` to gate UI on the initial fetch;
 *   - reads `currentValues` at `saved`-emit time to inspect
 *     what was just POSTed (e.g. the wizard's hello-step
 *     wrapper compares pre/post `ui_lang` to detect a
 *     language change). `saved` fires BEFORE the post-save
 *     `load()` refresh, so `currentValues` still reflects the
 *     submitted shape at that moment. */
defineExpose({ save, reload: load, loading, saving, currentValues })

/* ---- Hash-driven field focus ---------------------------------
 *
 * Settings search in the Cmd-K palette pushes routes with
 * `#field=<id>` to deep-link a specific config field; the same
 * URL also works when pasted directly into the address bar. This
 * watcher reads `route.hash`, finds the matching prop in
 * `fieldProps`, auto-promotes the page's display level if the
 * field needs Advanced or Expert (purely local to this mount —
 * the LevelMenu's persisted preference is untouched), then
 * scrolls the row into view and pulses a 2.5 s highlight on it.
 *
 * The level auto-promote is intentional UX: the user explicitly
 * asked for a specific field, so silently bumping their visible
 * level honours their intent without making them hunt for the
 * LevelMenu after navigation. It only ever WIDENS visibility (no
 * demotion); switching to a different page or back-navigating
 * doesn't change the persisted preference because we never
 * called the LevelMenu's pick.
 *
 * `currentLevel` is the local override ref established earlier in
 * setup; assigning to it triggers `displayedGroups` to recompute
 * and the row to appear (or stay) in the DOM before we scroll.
 *
 * `targetedField` drives the `.ifld-row--targeted` class binding
 * in the template; the keyframe pulse on that class is the visual
 * cue. Tracked in script rather than via direct DOM mutation so
 * the row re-renders preserve the highlight across reactive
 * updates (e.g. the watcher firing twice in quick succession on
 * a load-then-hash-change). */
/* `useRoute()` returns undefined outside a router context. Some
 * call sites mount IdnodeConfigForm in isolation (test harnesses,
 * the wizard's hello step) without installing vue-router. Tolerate
 * that — the hash-focus feature simply no-ops when there is no
 * route to read from. */
const route = useRoute() as ReturnType<typeof useRoute> | undefined
const rootEl = ref<HTMLElement | null>(null)
const targetedField = ref<string | null>(null)
let targetedTimer: ReturnType<typeof setTimeout> | null = null

const FIELD_HASH_RE = /^#field=([\w.-]+)$/

function parseFieldHash(hash: string): string | null {
  const m = FIELD_HASH_RE.exec(hash)
  return m && m[1] ? m[1] : null
}

async function focusHashedField(): Promise<void> {
  const targetId = parseFieldHash(route?.hash ?? '')
  if (!targetId) {
    targetedField.value = null
    return
  }
  /* Wait for fieldProps to populate — initial mount fires this
   * watcher before load() resolves. The watcher's source list
   * includes `fieldProps` so it re-fires once props arrive. */
  if (loading.value || fieldProps.value.length === 0) return
  const targetProp = fieldProps.value.find((p) => p.id === targetId)
  if (!targetProp) return

  /* Auto-promote level when needed. Skips the bump when the page
   * is `lockLevel`-pinned (the caller has committed the page to a
   * single level by design — Image Cache pins to expert; bumping
   * past would surface nothing new). */
  if (!props.lockLevel) {
    const needed = propLevel(targetProp)
    if (!levelMatches(needed, currentLevel.value)) {
      currentLevel.value = needed
    }
  }

  /* Wait one tick so the row is in the DOM under the (possibly
   * new) level filter before we measure scroll. */
  await nextTick()

  /* Field id is server-controlled (always a C identifier:
   * letters / digits / underscore), so a raw template-literal
   * selector would be safe — defensive `CSS.escape` covers any
   * future server extension to richer id syntax. Scope the
   * lookup to the form's root so detached-from-document mounts
   * (test-utils default) and same-id collisions with other
   * forms on the page both work. */
  const sel = `#field-${CSS.escape(targetId)}`
  const root = rootEl.value
  const el = (root ?? document).querySelector<HTMLElement>(sel)
  if (!el) return
  el.scrollIntoView({ block: 'center', behavior: 'smooth' })

  targetedField.value = targetId
  if (targetedTimer !== null) clearTimeout(targetedTimer)
  targetedTimer = setTimeout(() => {
    targetedField.value = null
    targetedTimer = null
  }, 2500)
}

watch(
  [() => route?.hash ?? '', fieldProps, loading],
  () => {
    focusHashedField().catch(() => undefined)
  },
  { flush: 'post' },
)

onBeforeUnmount(() => {
  if (targetedTimer !== null) clearTimeout(targetedTimer)
})
</script>

<template>
  <article ref="rootEl" class="idnode-config-form">
    <header v-if="!hideToolbar" class="idnode-config-form__toolbar">
      <button
        v-tooltip.bottom="saveTooltip || ''"
        type="button"
        class="idnode-config-form__btn idnode-config-form__btn--save"
        :disabled="
          (!isDirty && !alwaysDirty)
            || saving
            || loading
            || (isDirty && hasErrors)
        "
        @click="save"
      >
        {{ saving ? t('Saving…') : saveLabel || t('Save') }}
      </button>
      <button
        type="button"
        class="idnode-config-form__btn"
        :disabled="!isDirty || saving"
        @click="undo"
      >
        {{ t('Undo') }}
      </button>
      <!-- Page-specific actions slot. Receives `loading` and
           `saving` so custom buttons can disable themselves
           consistently with Save / Undo. -->
      <slot name="actions" :loading="loading" :saving="saving" />
      <span class="idnode-config-form__spacer" aria-hidden="true" />
      <!-- Hide the chooser when the page pins its level via
           `lockLevel`. See the prop comment for rationale. -->
      <LevelMenu
        v-if="!lockLevel"
        :effective-level="currentLevel"
        :locked="access.locked"
        @set-level="(l) => (currentLevel = l)"
      />
      <!--
        Help button — same shape + behaviour as IdnodeGrid's
        help button. Sits at the very right of the toolbar
        (after LevelMenu) so it's the trailing-edge affordance,
        matching where the Help button sits in the Classic UI's
        idnode dialogs (`idnode.js:2521`). Suppressed when
        `lockLevel` is set OR when the page has no help page
        configured — the LevelMenu's absence shifts the Help
        button into its place visually rather than leaving a
        gap.
      -->
      <button
        v-if="props.helpPage"
        v-tooltip.bottom="t('Help')"
        type="button"
        class="idnode-config-form__help"
        :aria-label="t('Help')"
        :aria-pressed="help.isOpen.value"
        @click="onHelpClick"
      >
        <HelpCircle :size="16" :stroke-width="2" />
      </button>
    </header>

    <div v-if="loading" class="idnode-config-form__status">{{ t('Loading configuration…') }}</div>
    <div
      v-else-if="error"
      class="idnode-config-form__status idnode-config-form__status--error"
    >
      {{ error }}
    </div>
    <form v-else class="idnode-config-form__form" @submit.prevent="save">
      <!-- Optional caller-supplied content rendered above the
           auto-rendered groups but INSIDE the scroll area. Used by
           views that supplement the standard form with custom
           editors threaded through `currentValues` (e.g. the
           Config → Debugging subsystem pickers). Receives no
           slot props — callers acquire `currentValues` via the
           component template ref. -->
      <slot name="beforeBody" />
      <!-- Two render paths so unnamed groups don't surface the
           browser's default details element disclosure text. Named
           groups render as a collapsible details + summary block;
           unnamed groups render as a plain section with no heading
           and no collapse affordance. Mirrors ExtJS for single-
           section idnodes (e.g. Image Cache) that don't declare
           any property groups server-side. -->
      <template v-for="g in displayedGroups" :key="g.group.number">
        <details v-if="g.group.name" class="idnode-config-form__group" open>
          <summary class="idnode-config-form__group-title">{{ g.group.name }}</summary>
          <div class="idnode-config-form__group-body ifld-form">
            <template v-for="p in g.fields" :key="p.id">
              <div
                v-if="rendererFor(p)"
                :id="`field-${p.id}`"
                class="ifld-row"
                :class="{
                  'ifld-row--dirty': isFieldDirty(p.id),
                  'ifld-row--required': isRequired(p.id),
                  'ifld-row--invalid': !!visibleError(p.id),
                  'ifld-row--targeted': targetedField === p.id,
                }"
              >
                <component
                  :is="rendererFor(p)!"
                  :prop="effectiveProp(p)"
                  :model-value="valueFor(p, currentValues[p.id])"
                  :inline="inlineEnumMultiSet.has(p.id)"
                  @update:model-value="onFieldChange(p.id, $event)"
                />
                <p v-if="visibleError(p.id)" class="ifld-row__error" role="alert">
                  {{ visibleError(p.id) }}
                </p>
              </div>
              <p v-else class="idnode-config-form__placeholder">
                <strong>{{ p.caption ?? p.id }}:</strong>
                {{ t('editor support not implemented for this field type') }}
              </p>
            </template>
          </div>
        </details>
        <section v-else class="idnode-config-form__group">
          <div class="idnode-config-form__group-body ifld-form">
            <template v-for="p in g.fields" :key="p.id">
              <div
                v-if="rendererFor(p)"
                :id="`field-${p.id}`"
                class="ifld-row"
                :class="{
                  'ifld-row--dirty': isFieldDirty(p.id),
                  'ifld-row--required': isRequired(p.id),
                  'ifld-row--invalid': !!visibleError(p.id),
                  'ifld-row--targeted': targetedField === p.id,
                }"
              >
                <component
                  :is="rendererFor(p)!"
                  :prop="effectiveProp(p)"
                  :model-value="valueFor(p, currentValues[p.id])"
                  :inline="inlineEnumMultiSet.has(p.id)"
                  @update:model-value="onFieldChange(p.id, $event)"
                />
                <p v-if="visibleError(p.id)" class="ifld-row__error" role="alert">
                  {{ visibleError(p.id) }}
                </p>
              </div>
              <p v-else class="idnode-config-form__placeholder">
                <strong>{{ p.caption ?? p.id }}:</strong>
                {{ t('editor support not implemented for this field type') }}
              </p>
            </template>
          </div>
        </section>
      </template>
      <!-- Optional caller-supplied content rendered BELOW the auto-
           rendered groups, still inside the scroll area. Symmetric
           to `#beforeBody`; use when a custom editor reads more
           naturally as the last group on the page (e.g. the Config
           → Debugging subsystem picker). Same slot-prop contract:
           callers acquire `currentValues` via the component
           template ref. -->
      <slot name="afterBody" />
    </form>
  </article>
</template>

<style scoped>
.idnode-config-form {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.idnode-config-form__toolbar {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  flex: 0 0 auto;
}

.idnode-config-form__spacer {
  flex: 1 1 auto;
}

.idnode-config-form__btn {
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

.idnode-config-form__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-config-form__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.idnode-config-form__btn--save {
  background: var(--tvh-primary);
  color: var(--tvh-bg-surface);
  border-color: var(--tvh-primary);
}

.idnode-config-form__btn--save:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 80%, black);
}

.idnode-config-form__btn--save:disabled {
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border-color: var(--tvh-border);
}

.idnode-config-form__status {
  padding: var(--tvh-space-6);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  text-align: center;
  color: var(--tvh-text-muted);
}

.idnode-config-form__status--error {
  color: var(--tvh-text);
  border-color: color-mix(in srgb, var(--tvh-primary) 40%, var(--tvh-border));
}

.idnode-config-form__form {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
  overflow-y: auto;
}

.idnode-config-form__group {
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.idnode-config-form__group-title {
  padding: var(--tvh-space-3) var(--tvh-space-4);
  font-weight: 600;
  cursor: pointer;
  user-select: none;
}

.idnode-config-form__group-body {
  padding: 0 var(--tvh-space-4) var(--tvh-space-4);
  display: flex;
  flex-direction: column;
  /* 8 px inter-row gap (vs the standard 12 px). Inline forms with
   * many short fields read tighter at this spacing — matches
   * IdnodeEditor's drawer body. */
  gap: var(--tvh-space-2);
}

/* When a group has no name (the synthetic ungrouped bucket),
 * skip the empty summary heading by giving the body its own
 * top padding instead. */
.idnode-config-form__group:not(:has(.idnode-config-form__group-title))
  .idnode-config-form__group-body {
  padding-top: var(--tvh-space-4);
}

/* Slightly smaller label than the per-field default (12 px vs
 * 13 px). Most labels then fit on a single line within the
 * fixed-width label column; long captions still wrap rather than
 * ellipsis-truncate. */
.idnode-config-form__group-body :deep(.ifld__label) {
  font-size: var(--tvh-text-sm);
}

/* Field-row layout (label-left / control-right, input width
 * clamped, row min-height, checkbox sizing, phone-mode stack)
 * lives in the shared `src/styles/ifld.css`. Both this component
 * and `<IdnodeEditor>` opt in via the `ifld-form` class on the
 * group-body container so both surfaces render fields
 * identically. */

.idnode-config-form__placeholder {
  margin: 0;
  font-size: var(--tvh-text-md);
  color: var(--tvh-text-muted);
}

/* Per-field dirty marker — small primary-tinted bullet prepended to
 * the field's label so the user can spot which fields they've
 * touched at a glance. Mirrors the same affordance on the drawer
 * editor (see IdnodeEditor.vue's matching rule). `:deep()` is
 * required because `.ifld__label` lives inside the field
 * renderer's own scoped style boundary. */
.ifld-row--dirty :deep(.ifld__label)::before {
  content: '•';
  color: var(--tvh-primary);
  font-weight: bold;
  margin-right: 4px;
}

/* Required-field caption bolded as an always-on hint. Matches the
 * drawer editor's rule at IdnodeEditor.vue:1668. */
.ifld-row--required :deep(.ifld__label) {
  font-weight: 700;
}

/* Invalid field — red border on the renderer's primary input.
 * Multi-select / ordered-list widgets that don't expose
 * `.ifld__input` fall through to the inline error text only. */
.ifld-row--invalid :deep(.ifld__input) {
  border-color: var(--tvh-error);
  outline-color: var(--tvh-error);
}

/* Error message under the input — same metric as the drawer
 * editor's rule (192 px left margin matches the label column
 * width). */
.ifld-row__error {
  margin: 4px 0 0 192px;
  font-size: var(--tvh-text-sm);
  line-height: 1.4;
  color: var(--tvh-error);
}

/* Help button — same 32px icon-button shape IdnodeGrid uses
 * so the trailing-edge affordance reads consistently across
 * grid and form surfaces. aria-pressed lights the button with
 * a primary tint when the help dialog is open. */
.idnode-config-form__help {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
  flex: 0 0 auto;
  transition: background var(--tvh-transition);
}

.idnode-config-form__help:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    var(--tvh-bg-surface)
  );
}

.idnode-config-form__help:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-config-form__help[aria-pressed='true'] {
  background: color-mix(in srgb, var(--tvh-primary) 14%, transparent);
  color: var(--tvh-primary);
  border-color: var(--tvh-primary);
}

/*
 * Hash-driven field-focus highlight pulse — see the
 * `focusHashedField` watcher in the script section. Fires when
 * the route hash is `#field=<id>` and the row's id matches.
 * The class auto-clears after 2.5 s so the highlight is a brief
 * cue, not a persistent decoration.
 *
 * Uses `outline` (paints OUTSIDE the row's border, above all
 * descendants) rather than `box-shadow: inset` because the
 * field renderers' inputs occupy the entire right grid column
 * with opaque white backgrounds, which paint over an inset
 * shadow's right strip and clip the highlight rectangle. An
 * outline can't be obscured by descendants.
 *
 * Animates `outline-color` (transparent → primary → transparent)
 * rather than `outline` shorthand — only colour is reliably
 * animatable across browsers; transitioning offset/style would
 * flash. The 2 px offset keeps the outline a hair outside the
 * row so the row's inner inputs' own :focus outlines (if the
 * user clicks into a field after navigation) don't visually
 * collide.
 *
 * Rounded outlines (the outline following border-radius) work
 * on Chrome 94+ / Firefox 88+ / Safari 16.4+. Older Safari
 * shows a sharp rectangle — still visible, just less polished.
 */
@keyframes ifld-row-targeted-pulse {
  0%,
  100% {
    outline-color: transparent;
  }
  15% {
    outline-color: var(--tvh-primary);
  }
}

.ifld-row--targeted {
  outline: 2px solid transparent;
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
  animation: ifld-row-targeted-pulse 2.4s ease-out;
}
</style>
