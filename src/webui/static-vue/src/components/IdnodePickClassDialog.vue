<script setup lang="ts">
/*
 * IdnodePickClassDialog — modal that asks "which class?" before
 * opening an IdnodeEditor for a class with multiple subclasses.
 *
 * Mirrors the legacy ExtJS pattern where idnode_grid's `add.select`
 * surfaces a builders dropdown before the create dialog opens
 * (`static/app/mpegts.js:67-83` for Networks; the same shape will
 * apply to Muxes + any other class with a `<base>/builders`
 * endpoint).
 *
 * Lifecycle:
 *   - parent passes `:visible` true → component fetches the
 *     builders list (cached per session per endpoint), renders a
 *     labelled `<select>` + Continue / Cancel.
 *   - Continue → emits `pick(classKey)` with the chosen class.
 *     Parent then opens its IdnodeEditor against that class.
 *   - Cancel / X / Esc / outside-click → emits `close`.
 *
 * Each builder entry from the server (`api_mpegts_network_builders`
 * → `idclass_serialize0` at `idnode.c:1507`) carries `class`,
 * `caption`, `props` and friends. We only need `class` + `caption`
 * for the picker UI; the full metadata round-trip happens later
 * via the editor's `idnode/class?name=<class>` fetch.
 */
import { computed, ref, watch } from 'vue'
import Dialog from 'primevue/dialog'
import { apiCall } from '@/api/client'

interface BuilderEntry {
  class: string
  caption?: string
}

interface BuildersResponse {
  entries?: BuilderEntry[]
}

const props = defineProps<{
  /* Toggles the dialog open/closed. Two-way via the close emit so
   * the parent owns the visibility state. */
  visible: boolean
  /* API endpoint to fetch the builders list from
   * (e.g. `'mpegts/network/builders'`). The component is class-
   * agnostic; the endpoint determines what subclasses appear. */
  buildersEndpoint: string
  /* Title shown at the top of the dialog (e.g. "Add Network"). */
  title: string
  /* Label for the picker field (e.g. "Network type"). */
  label: string
}>()

const emit = defineEmits<{
  pick: [classKey: string]
  close: []
}>()

const loading = ref(false)
const error = ref<string | null>(null)
const entries = ref<BuilderEntry[]>([])
const selected = ref<string>('')

/* Builders list is small (typically < 20 entries) and stable per
 * server build; cache one fetch per visible-true transition.
 * Re-fetch on every open so a server config change (newly
 * compiled-in subclass, capability flip) is picked up without a
 * page reload. */
async function loadBuilders() {
  loading.value = true
  error.value = null
  try {
    const res = await apiCall<BuildersResponse>(props.buildersEndpoint)
    const list = (res.entries ?? []).filter((e) => !!e.class)
    /* Sort by caption so the dropdown reads alphabetically; the
     * server's order is registration-order (DVB-T, DVB-C, DVB-S,
     * ATSC, ISDB, DTMB, DAB, IPTV, IPTV-auto) which is fine but
     * caption-sorted is more findable. */
    list.sort((a, b) => (a.caption ?? a.class).localeCompare(b.caption ?? b.class))
    entries.value = list
    /* Pre-select the first option so Continue is immediately
     * clickable; the user can change before continuing. */
    selected.value = list[0]?.class ?? ''
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e)
    entries.value = []
    selected.value = ''
  } finally {
    loading.value = false
  }
}

/* `immediate: true` so an initial-mount-with-visible=true also
 * triggers the load (production callers always start with visible
 * false and toggle, but tests sometimes mount with true). The
 * gate inside the callback skips the load when visible is false,
 * so the immediate fire on a closed dialog is a no-op. */
watch(
  () => props.visible,
  (open) => {
    if (open) loadBuilders()
  },
  { immediate: true }
)

const canContinue = computed(() => !loading.value && !!selected.value)

function onContinue() {
  if (!canContinue.value) return
  emit('pick', selected.value)
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
    <div class="idnode-pick-class">
      <p v-if="error" class="idnode-pick-class__error" role="alert">{{ error }}</p>
      <p v-else-if="loading" class="idnode-pick-class__status">Loading types…</p>
      <p v-else-if="entries.length === 0" class="idnode-pick-class__status">
        No types available.
      </p>
      <label v-else class="idnode-pick-class__field">
        <span class="idnode-pick-class__label">{{ label }}</span>
        <select
          v-model="selected"
          class="idnode-pick-class__select"
          :aria-label="label"
        >
          <option v-for="opt in entries" :key="opt.class" :value="opt.class">
            {{ opt.caption ?? opt.class }}
          </option>
        </select>
      </label>
    </div>
    <template #footer>
      <button type="button" class="idnode-pick-class__btn" @click="onClose">Cancel</button>
      <button
        type="button"
        class="idnode-pick-class__btn idnode-pick-class__btn--primary"
        :disabled="!canContinue"
        @click="onContinue"
      >
        Continue
      </button>
    </template>
  </Dialog>
</template>

<style scoped>
.idnode-pick-class {
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) 0;
}

.idnode-pick-class__field {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.idnode-pick-class__label {
  font-size: 13px;
  font-weight: 500;
  color: var(--tvh-text);
}

.idnode-pick-class__select {
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px 10px;
  font: inherit;
  min-height: 36px;
}

.idnode-pick-class__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.idnode-pick-class__status {
  color: var(--tvh-text-muted);
  font-size: 13px;
  margin: 0;
}

.idnode-pick-class__error {
  color: var(--tvh-danger, #b00020);
  font-size: 13px;
  margin: 0;
}

.idnode-pick-class__btn {
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

.idnode-pick-class__btn:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.idnode-pick-class__btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.idnode-pick-class__btn--primary {
  background: var(--tvh-primary);
  color: var(--tvh-on-primary, #fff);
  border-color: var(--tvh-primary);
}

.idnode-pick-class__btn--primary:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) 90%, black);
}
</style>
