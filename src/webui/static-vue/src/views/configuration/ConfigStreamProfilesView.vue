<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Stream — Stream Profiles.
 *
 * Master-detail layout:
 *   - LEFT: IdnodeGrid against `profile/list` — every defined
 *     stream profile (built-in `pass` / `matroska` / `htsp`,
 *     plus any user-created profiles).
 *   - RIGHT: IdnodeConfigForm bound to the selected profile's
 *     UUID — fields auto-render per the `profile_class` base
 *     (name, enabled, default, comment, timeout, priority,
 *     etc.) plus whichever subclass-specific group applies
 *     (Matroska's `webm` / `dvbsub_reorder`, MPEG-TS Pass'
 *     SI-rewrite booleans, Transcode's codec selectors, …).
 *
 * Mirrors the legacy ExtJS `tvheadend.profile_tab()` page at
 * `static/app/profile.js:51` — same `idnode_form_grid` shape.
 *
 * Source-of-truth references:
 *   - C base class: `src/profile.c:284-456`
 *     (`profile_class`, ic_perm_def = ACCESS_ADMIN).
 *   - Subclasses: `src/profile.c:1202+` — htsp / mpegts /
 *     mpegts-spawn / matroska / audio / libav-mpegts /
 *     libav-matroska / libav-mp4 / transcode.
 *   - API endpoints: `src/api/api_profile.c:134-140` —
 *     `profile/list`, `profile/builders`, `profile/create`,
 *     `profile/class`. Standard `idnode/load` / save / delete
 *     for per-profile editing. The `pass` profile is
 *     server-protected from deletion (`src/profile.c:2912`).
 *
 * Toolbar: Add (subclass picker) / Clone / Delete. The Clone
 * action goes through the generic `cloneIdnode` helper
 * (`src/api/cloneIdnode.ts`) so future master-detail grids
 * that want a Clone affordance reuse the same shape.
 *
 * Phone behaviour: list-first drilldown — selecting a row
 * swaps to the form with a `← Back` button (handled by
 * MasterDetailLayout).
 */
import { computed, ref, watch } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import MasterDetailLayout from '@/components/MasterDetailLayout.vue'
import IdnodePickClassDialog from '@/components/IdnodePickClassDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import {
  useAddViaPicker,
  useCloneAction,
  useDeleteAction,
} from '@/composables/useMasterDetailActions'
import { useI18n } from '@/composables/useI18n'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'

const { t } = useI18n()
const selected = ref<string | null>(null)
/* Track the full selected row alongside the uuid so we can detect
 * shielded (undeletable) profiles via the row's `params` array.
 * Cleared when selection clears (deselect / row removal). */
const selectedRow = ref<Record<string, unknown> | null>(null)

watch(selected, (uuid) => {
  if (uuid === null) selectedRow.value = null
})

/* A profile is shielded — and therefore undeletable server-side —
 * when its `name` param carries `rdonly: true`. That flag is set
 * by `profile_class_name_opts` at `src/profile.c:240-247` whenever
 * `pro_shield=1`, which is the SAME condition `profile_class_delete`
 * (src/profile.c:184-190) checks to short-circuit a delete request.
 * Auto-shielded profiles: `pass` (hardcoded), `matroska` + `htsp`
 * (from `data/conf/profiles`), and all `webtv-*` libav transcode
 * profiles when CONFIG_LIBAV=yes (from
 * `data/conf/transcoder/profiles`). User-created profiles are not
 * shielded. */
const selectedShielded = computed(() => {
  const row = selectedRow.value
  if (!row) return false
  const params = row.params
  if (!Array.isArray(params)) return false
  for (const p of params) {
    if (typeof p !== 'object' || p === null) continue
    const param = p as { id?: unknown; rdonly?: unknown }
    if (param.id === 'name') return param.rdonly === true
  }
  return false
})

const add = useAddViaPicker({
  selected,
  createEndpoint: 'profile/create',
  successMessage: t('Profile created. Edit name + settings on the right.'),
  noUuidErrorMessage: t('Server returned no profile uuid.'),
})

const clone = useCloneAction({
  selected,
  createEndpoint: 'profile/create',
})

/* Server-side delete is gated by `pro_shield` (src/profile.c:187):
 * shielded profiles can't be deleted at all. We mirror that in the
 * UI via `selectedShielded` so the Delete button greys out instead
 * of silently failing. */
const remove = useDeleteAction({
  selected,
  confirmText: t('Do you really want to delete this stream profile?'),
})

/* Toolbar actions rendered through `ActionMenu` (same width-aware
 * component every other admin grid uses) so they collapse into a
 * `…` overflow menu when the window is too narrow rather than
 * clipping. Gating is on this view's local `selected` ref —
 * single-select master-detail. */
const actions = computed<ActionDef[]>(() => [
  {
    id: 'add',
    label: t('Add'),
    onClick: add.onAddClick,
  },
  {
    id: 'clone',
    label: clone.inflight.value ? t('Cloning…') : t('Clone'),
    disabled: !selected.value || clone.inflight.value,
    onClick: clone.run,
  },
  {
    id: 'delete',
    label: remove.inflight.value ? t('Deleting…') : t('Delete'),
    disabled: !selected.value || remove.inflight.value || selectedShielded.value,
    tooltip: selectedShielded.value
      ? t('Built-in profiles cannot be deleted')
      : undefined,
    onClick: remove.run,
  },
])

/* Single column: name (read off the row's top-level `text`
 * field — that's the title `idnode_serialize0` emits via
 * `ic_get_title`, which for profiles returns
 * `profile_get_name` per `src/profile.c:280-282`). The right
 * pane (IdnodeConfigForm against the per-profile uuid) loads
 * the full set on selection.
 *
 * Avoids `profile/list` because that endpoint
 * (a) emits `{ key: uuid, val: name }` — non-standard shape
 * incompatible with our grid's `uuid`-keyed plumbing;
 * (b) filters by `profile_verify(pro, sflags)` which drops
 * disabled profiles + profiles without PACKET/MPEGTS
 * subscription support, hiding profiles an admin should still
 * be able to manage from this page (`src/profile.c:514`). The
 * generic `idnode/load?class=profile` route returns ALL
 * profiles of the class, filtered only by per-instance
 * `idnode_perm` (admin always passes for ACCESS_ADMIN
 * classes). */
const cols: ColumnDef[] = [
  {
    field: 'text',
    label: t('Name'),
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
]

/* IdnodeGrid emits row-click with a `Record<string, unknown>`.
 * Extract the uuid (always a string for idnode rows) and pass
 * to the slot's `select()` to update v-model:selected-uuid. */
function onRowClick(row: Record<string, unknown>, select: (uuid: string | null) => void) {
  const uuid = row.uuid
  if (typeof uuid === 'string') {
    selectedRow.value = row
    select(uuid)
  } else {
    selectedRow.value = null
    select(null)
  }
}
</script>

<template>
  <div class="stream-profiles">
    <!-- Splitter starts at 35% on the detail side — transcode
         profile forms are dense; users can drag wider via the
         splitter gutter and the position is persisted. -->
    <MasterDetailLayout
      v-model:selected-uuid="selected"
      storage-key="config-stream-profiles"
      :default-detail-fraction="35"
    >
      <template #master="{ select }">
        <IdnodeGrid
          endpoint="idnode/load"
          help-page="class/profile"
          entity-class="profile"
          :columns="cols"
          :extra-params="{ class: 'profile' }"
          store-key="config-stream-profiles"
          :default-sort="{ key: 'text', dir: 'ASC' }"
          :virtual-scroller-options="{ itemSize: 36, lazy: false }"
          :count-label="t('profiles')"
          client-side-filter
          selectable="single"
          class="stream-profiles__grid"
          @row-click="(row) => onRowClick(row, select)"
        >
          <!--
            Action buttons land in IdnodeGrid's #toolbarActions
            slot so they sit in the same row as the search input
            + GridSettings cog + Help button — same shape as
            every other admin grid. ActionMenu collapses them into
            a `…` overflow menu when the window is too narrow. The
            actions gate on this view's local `selected` ref
            (single-select master-detail), so the slot's
            `selection` scope is ignored.
          -->
          <template #toolbarActions>
            <ActionMenu :actions="actions" />
          </template>
        </IdnodeGrid>
      </template>
      <template #detail="{ selectedUuid }">
        <IdnodeConfigForm v-if="selectedUuid" :uuid="selectedUuid" />
        <p v-else class="stream-profiles__empty">
          {{ t('Select a profile on the left to view and edit its configuration.') }}
        </p>
      </template>
    </MasterDetailLayout>
    <IdnodePickClassDialog
      :visible="add.pickerVisible.value"
      builders-endpoint="profile/builders"
      :title="t('Add Stream Profile')"
      :label="t('Profile type')"
      @pick="add.onClassPicked"
      @close="add.pickerVisible.value = false"
    />
  </div>
</template>

<style scoped>
.stream-profiles {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.stream-profiles__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.stream-profiles__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
}
</style>
