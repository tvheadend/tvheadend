<script setup lang="ts">
/*
 * IdnodePickEntityDialog — modal that asks "which existing
 * entity?" before opening an IdnodeEditor for a parent-scoped
 * create flow.
 *
 * Sibling to IdnodePickClassDialog. The two cover the two distinct
 * ways the legacy ExtJS UI surfaces "pick something before create":
 *
 *   - IdnodePickClassDialog → user picks a CLASS
 *     (Networks: pick DVB-T / IPTV / etc. → server creates the
 *      network with that class).
 *
 *   - IdnodePickEntityDialog → user picks an existing ENTITY
 *     (Muxes: pick a network → server creates a mux against that
 *      network, mux subclass implicit in network type).
 *
 * The two stay separate because their wire shapes differ: a class
 * picker fetches `/builders` and emits `pick(classKey)`; an entity
 * picker fetches a `/grid` endpoint and emits `pick(uuid, label)`.
 *
 * Lifecycle:
 *   - parent passes `:visible` true → component fetches the entity
 *     list from `gridEndpoint`, renders a `<select>` over the rows
 *     (label from `displayField`, value = uuid), plus Continue /
 *     Cancel.
 *   - Continue → emits `pick(uuid, label)`. Parent then opens its
 *     IdnodeEditor in parent-scoped mode against that uuid.
 *   - Cancel / X / Esc / outside-click → emits `close`.
 */
import { computed, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import { apiCall } from '@/api/client'

interface EntityRow {
  uuid?: string
  [key: string]: unknown
}

interface GridResponse {
  entries?: EntityRow[]
}

const props = defineProps<{
  /* Toggles the dialog open/closed; parent owns visibility. */
  visible: boolean
  /* Grid endpoint that lists the entities, e.g.
   * `'mpegts/network/grid'`. Component is class-agnostic; the
   * endpoint determines what entities appear. */
  gridEndpoint: string
  /* Field on each row to use as the displayed label
   * (e.g. `'networkname'` for networks). Falls back to `uuid` when
   * the row doesn't carry the field. */
  displayField: string
  /* Title at the top of the dialog (e.g. "Add Mux"). */
  title: string
  /* Label for the picker field (e.g. "Network"). */
  label: string
}>()

const emit = defineEmits<{
  pick: [uuid: string, label: string]
  close: []
}>()

const loading = ref(false)
const error = ref<string | null>(null)
const entries = ref<EntityRow[]>([])
const selected = ref<string>('')

/* Limit chosen high enough to cover any realistic entity list
 * (networks, channel tags, etc. — typically < 100). If a future
 * caller has thousands of entities, the picker should switch to a
 * filterable / typeahead variant; not in scope today. */
const FETCH_LIMIT = 5000

function rowLabel(row: EntityRow): string {
  const v = row[props.displayField]
  if (typeof v === 'string' && v.length > 0) return v
  return row.uuid ?? ''
}

async function loadEntities() {
  loading.value = true
  error.value = null
  try {
    const res = await apiCall<GridResponse>(props.gridEndpoint, {
      start: 0,
      limit: FETCH_LIMIT,
    })
    const list = (res.entries ?? []).filter(
      (r): r is EntityRow & { uuid: string } => typeof r.uuid === 'string' && !!r.uuid
    )
    /* Sort by display label for findability — the server's row
     * order is class-internal (typically TAILQ insertion order) and
     * not particularly useful as a picker default. */
    list.sort((a, b) => rowLabel(a).localeCompare(rowLabel(b)))
    entries.value = list
    selected.value = list[0]?.uuid ?? ''
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
    entries.value = []
    selected.value = ''
  } finally {
    loading.value = false
  }
}

/* `immediate: true` so an initial-mount-with-visible=true also
 * triggers the load — production callers always start visible
 * false and toggle, but tests sometimes mount with true. The
 * visible-false branch is a no-op. */
watch(
  () => props.visible,
  (open) => {
    if (open) loadEntities()
  },
  { immediate: true }
)

const canContinue = computed(() => !loading.value && !!selected.value)

function onContinue() {
  if (!canContinue.value) return
  const row = entries.value.find((r) => r.uuid === selected.value)
  if (!row) return
  emit('pick', selected.value, rowLabel(row))
}

function onClose() {
  emit('close')
}
</script>

<template>
  <Dialog
    :visible="visible"
    :header="title"
    modal
    :closable="true"
    :draggable="false"
    :style="{ width: '420px' }"
    @update:visible="(v) => { if (!v) onClose() }"
  >
    <div class="idnode-pick-entity">
      <p v-if="error" class="idnode-pick-entity__error" role="alert">{{ error }}</p>
      <p v-else-if="loading" class="idnode-pick-entity__status">Loading entries…</p>
      <p v-else-if="entries.length === 0" class="idnode-pick-entity__status">
        No entries available.
      </p>
      <label v-else class="idnode-pick-entity__field">
        <span class="idnode-pick-entity__label">{{ label }}</span>
        <select
          v-model="selected"
          class="idnode-pick-entity__select"
          :aria-label="label"
        >
          <option v-for="row in entries" :key="row.uuid" :value="row.uuid">
            {{ rowLabel(row) }}
          </option>
        </select>
      </label>
    </div>
    <template #footer>
      <button type="button" class="idnode-pick-entity__btn" @click="onClose">Cancel</button>
      <button
        type="button"
        class="idnode-pick-entity__btn idnode-pick-entity__btn--primary"
        :disabled="!canContinue"
        @click="onContinue"
      >
        Continue
      </button>
    </template>
  </Dialog>
</template>

<style scoped>
.idnode-pick-entity {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) 0;
}

.idnode-pick-entity__field {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.idnode-pick-entity__label {
  font-size: 13px;
  font-weight: 500;
  color: var(--tvh-text);
}

.idnode-pick-entity__select {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
}

.idnode-pick-entity__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-pick-entity__status {
  color: var(--tvh-text-muted);
  font-size: 13px;
  margin: 0;
}

.idnode-pick-entity__error {
  color: var(--tvh-danger, #b00020);
  font-size: 13px;
  margin: 0;
}

.idnode-pick-entity__btn {
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

.idnode-pick-entity__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-pick-entity__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.idnode-pick-entity__btn--primary {
  background: var(--tvh-primary);
  color: var(--tvh-on-primary, #fff);
  border-color: var(--tvh-primary);
}

.idnode-pick-entity__btn--primary:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 90%, black);
}
</style>
