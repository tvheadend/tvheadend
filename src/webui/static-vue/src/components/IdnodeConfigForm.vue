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
import { computed, onMounted, ref, watch } from 'vue'
import { apiCall } from '@/api/client'
import { useAccessStore } from '@/stores/access'
import { levelMatches, propLevel, type IdnodeProp, type PropertyGroup } from '@/types/idnode'
import type { UiLevel } from '@/types/access'
import { rendererFor, valueFor } from '@/components/idnode-fields/rendererDispatch'
import LevelMenu from '@/components/LevelMenu.vue'

const props = withDefaults(
  defineProps<{
    /* Server endpoint for the initial load. Always passed
     * `{ meta: 1 }` so the response includes group metadata. */
    loadEndpoint: string
    /* Server endpoint for save. The form POSTs
     * `{ node: JSON.stringify(currentValues) }`. */
    saveEndpoint: string
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
  }>(),
  {
    reloadFields: () => [],
    lockLevel: undefined,
  }
)

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
    params?: IdnodeProp[]
    props?: IdnodeProp[]
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

async function load() {
  loading.value = true
  error.value = null
  try {
    const res = await apiCall<LoadResponse>(props.loadEndpoint, { meta: 1 })
    const entry = res.entries?.[0]
    if (!entry) {
      error.value = 'Failed to load configuration'
      return
    }
    fieldProps.value = entry.params ?? entry.props ?? []
    groups.value = entry.meta?.groups ?? []
    const init: Record<string, unknown> = {}
    for (const p of fieldProps.value) {
      init[p.id] = p.value ?? null
    }
    baseline.value = init
    currentValues.value = { ...init }
  } catch (e) {
    error.value = e instanceof Error ? e.message : `Failed to load: ${String(e)}`
  } finally {
    loading.value = false
  }
}

onMounted(load)

/* ---- View-level filter + grouping ---- */

const visibleFields = computed(() =>
  fieldProps.value.filter((p) => {
    if (p.noui || p.phidden) return false
    return levelMatches(propLevel(p), currentLevel.value)
  })
)

/* Build the displayed group list from the server-provided group
 * order, dropping any group whose visible-field bucket is empty.
 * Within a group, fields preserve declaration order from the C-
 * side prop_t array (`Array.filter` is stable; bucketing uses
 * push() so within-group order is preserved too).
 *
 * For pages with NO server groups (e.g., Image Cache emits
 * fields without a group structure), every field falls into a
 * synthetic group 0 — single-section layout, same template path. */
const fieldsByGroup = computed(() => {
  const map = new Map<number, IdnodeProp[]>()
  for (const p of visibleFields.value) {
    const g = p.group ?? 0
    const list = map.get(g) ?? []
    list.push(p)
    map.set(g, list)
  }
  return map
})

const displayedGroups = computed(() => {
  /* Server-defined groups in their declared order. */
  const out = groups.value
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

/* ---- Save / Undo ---- */

async function save() {
  if (!isDirty.value || saving.value) return
  saving.value = true
  error.value = null
  try {
    /* Snapshot the reload-trigger decision against the PRE-save
     * baseline before the api call mutates anything we're tracking. */
    const needsReload = props.reloadFields.some(
      (k) => currentValues.value[k] !== baseline.value[k]
    )

    await apiCall(props.saveEndpoint, {
      node: JSON.stringify(currentValues.value),
    })

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
    error.value = e instanceof Error ? e.message : `Save failed: ${String(e)}`
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
</script>

<template>
  <article class="idnode-config-form">
    <header class="idnode-config-form__toolbar">
      <button
        type="button"
        class="idnode-config-form__btn idnode-config-form__btn--save"
        :disabled="!isDirty || saving || loading"
        @click="save"
      >
        {{ saving ? 'Saving…' : 'Save' }}
      </button>
      <button
        type="button"
        class="idnode-config-form__btn"
        :disabled="!isDirty || saving"
        @click="undo"
      >
        Undo
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
    </header>

    <div v-if="loading" class="idnode-config-form__status">Loading configuration…</div>
    <div
      v-else-if="error"
      class="idnode-config-form__status idnode-config-form__status--error"
    >
      {{ error }}
    </div>
    <form v-else class="idnode-config-form__form" @submit.prevent="save">
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
          <div class="idnode-config-form__group-body">
            <template v-for="p in g.fields" :key="p.id">
              <div
                v-if="rendererFor(p)"
                class="ifld-row"
                :class="{ 'ifld-row--dirty': isFieldDirty(p.id) }"
              >
                <component
                  :is="rendererFor(p)!"
                  :prop="p"
                  :model-value="valueFor(p, currentValues[p.id])"
                  @update:model-value="currentValues[p.id] = $event"
                />
              </div>
              <p v-else class="idnode-config-form__placeholder">
                <strong>{{ p.caption ?? p.id }}:</strong>
                editor support not implemented for this field type
              </p>
            </template>
          </div>
        </details>
        <section v-else class="idnode-config-form__group">
          <div class="idnode-config-form__group-body">
            <template v-for="p in g.fields" :key="p.id">
              <div
                v-if="rendererFor(p)"
                class="ifld-row"
                :class="{ 'ifld-row--dirty': isFieldDirty(p.id) }"
              >
                <component
                  :is="rendererFor(p)!"
                  :prop="p"
                  :model-value="valueFor(p, currentValues[p.id])"
                  @update:model-value="currentValues[p.id] = $event"
                />
              </div>
              <p v-else class="idnode-config-form__placeholder">
                <strong>{{ p.caption ?? p.id }}:</strong>
                editor support not implemented for this field type
              </p>
            </template>
          </div>
        </section>
      </template>
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
  font-size: 13px;
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
  font-size: 12px;
}

/* Single-line field layout — label-left / control-right,
 * mirroring the legacy ExtJS `idnode_simple` form (`labelWidth: 250`
 * in `static/app/idnode.js:2843`). Each `.ifld` becomes a 2-column
 * grid; label cell on the left at a fixed width, control cell on
 * the right taking the remainder. Long labels wrap rather than
 * truncate. `.ifld__row` is the wrapper Time fields use; pushing
 * it into column 2 keeps those inputs aligned. Below 768 px the
 * stacked default takes over so narrow phones don't get cramped. */
.idnode-config-form__group-body :deep(.ifld) {
  display: grid;
  grid-template-columns: 250px 1fr;
  align-items: center;
  gap: var(--tvh-space-3);
}

.idnode-config-form__group-body :deep(.ifld__row) {
  grid-column: 2;
}

@media (max-width: 767px) {
  .idnode-config-form__group-body :deep(.ifld) {
    grid-template-columns: 1fr;
  }
  .idnode-config-form__group-body :deep(.ifld__row) {
    grid-column: auto;
  }
}

.idnode-config-form__placeholder {
  margin: 0;
  font-size: 13px;
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
</style>
