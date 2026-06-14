<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Recording → DVR Profiles.
 *
 * Master-detail layout backed by the `dvr_config_class` idnode
 * (`src/dvr/dvr_config.c:912-1547`, ACCESS_OR(ADMIN, RECORDER)).
 * Mirrors the legacy ExtJS page at `static/app/dvr.js:1019-1038`
 * which uses `idnode_form_grid` against `api/dvr/config`.
 *
 * Same shape as ConfigStreamProfilesView, with two differences:
 *
 *   1. Add: no class picker. `dvr_config_class` has no subclasses
 *      (single concrete class), so Add immediately POSTs `dvr/
 *      config/create` with a placeholder name. User sees the new
 *      entry in the grid + renames it in the right pane.
 *   2. Server requires non-empty `name` on create
 *      (`api_dvr.c:53-56`); the placeholder `"! New config"`
 *      matches the C-side `dvr_config_class.name` default
 *      (`dvr_config.c:945`) so admins recognise the not-yet-
 *      renamed status.
 *
 * Endpoint choice: `idnode/load?class=dvrconfig` (NOT
 * `dvr/config/grid`). The grid endpoint goes through
 * `api_idnode_grid` (`src/api/api_idnode.c:119-168`) which
 * serialises rows via `idnode_read0` — flat properties
 * (`{uuid, name, enabled, …}`) with no `text` top-level field,
 * so the grid's "Profile Name" column would render blank for
 * the default config (whose `name` is empty by design — its
 * display title is `"(Default profile)"` from
 * `dvr_config_class_get_title`). The `idnode/load` route
 * (`api_idnode_load_by_class0` at `api_idnode.c:170-204`)
 * uses `idnode_serialize0` which emits the resolved `text`
 * field at top level (`src/idnode.c:1546`), matching what the
 * column reads. Same workaround Stream Profiles uses for the
 * same reason (different cause: `profile/list` had the `{key,
 * val}` quirk; here it's the `text`-vs-flat distinction).
 *
 * Default-config delete protection: the server's
 * `dvr_config_class_delete` callback silently no-ops on the
 * default config (`dvr_config.c:589-594`), so attempting Delete
 * on it surfaces no error and leaves it in place — which matches
 * Classic's UX. Rename of the default config is also blocked
 * server-side; the form's Save will reject those attempts and
 * we surface them as toasts.
 */
import { computed, ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import MasterDetailLayout from '@/components/MasterDetailLayout.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import { apiCall } from '@/api/client'
import { useToastNotify } from '@/composables/useToastNotify'
import {
  useCloneAction,
  useDeleteAction,
} from '@/composables/useMasterDetailActions'
import { useI18n } from '@/composables/useI18n'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'

const { t } = useI18n()
const toast = useToastNotify()

const selected = ref<string | null>(null)
const addInflight = ref(false)

const clone = useCloneAction({
  selected,
  createEndpoint: 'dvr/config/create',
})

/* Server silently no-ops on the default config; non-default
 * deletions take effect immediately. */
const remove = useDeleteAction({
  selected,
  confirmText: t('Do you really want to delete this DVR profile?'),
})

/* Single column: profile name (read off the row's top-level
 * `text` field — that's the title `idnode_serialize0` emits via
 * `ic_get_title`, which for dvr_config returns the config's name
 * or "(Default profile)" for the unnamed default per
 * `dvr_config_class_get_title` in `src/dvr/dvr_config.c`). The
 * right pane (IdnodeConfigForm against the per-profile uuid)
 * loads the full ~40-field set on selection. */
const cols: ColumnDef[] = [
  {
    field: 'text',
    label: t('Profile Name'),
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
]

/* Add — no class picker (single class). POST `dvr/config/create`
 * with the C-side default placeholder name; the server requires
 * a non-empty string. New uuid becomes the master-detail
 * selection so the right-pane form opens immediately for
 * renaming + further configuration. */
async function onAddClick() {
  if (addInflight.value) return
  addInflight.value = true
  try {
    const resp = await apiCall<{ uuid?: string }>('dvr/config/create', {
      conf: JSON.stringify({ name: '! New config' }),
    })
    if (typeof resp.uuid === 'string' && resp.uuid) {
      selected.value = resp.uuid
      toast.success(t('Profile created. Edit name + settings on the right.'))
    } else {
      toast.error(t('Server returned no profile uuid.'), { summary: t('Add failed') })
    }
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Add failed'),
    })
  } finally {
    addInflight.value = false
  }
}

/* IdnodeGrid emits row-click with a `Record<string, unknown>`.
 * Extract the uuid (always a string for idnode rows) and pass
 * to the slot's `select()` to update v-model:selected-uuid. */
function onRowClick(row: Record<string, unknown>, select: (uuid: string | null) => void) {
  const uuid = row.uuid
  select(typeof uuid === 'string' ? uuid : null)
}

/* Toolbar actions via `ActionMenu` (width-aware — collapses into
 * a `…` overflow menu on a narrow window). Gated on the local
 * `selected` ref; single-select master-detail. */
const actions = computed<ActionDef[]>(() => [
  {
    id: 'add',
    label: addInflight.value ? t('Adding…') : t('Add'),
    disabled: addInflight.value,
    onClick: onAddClick,
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
    disabled: !selected.value || remove.inflight.value,
    onClick: remove.run,
  },
])
</script>

<template>
  <div class="dvr-profiles">
    <MasterDetailLayout v-model:selected-uuid="selected" storage-key="config-recording-dvr-profiles">
      <template #master="{ select }">
        <IdnodeGrid
          endpoint="idnode/load"
          help-page="class/dvrconfig"
          entity-class="dvrconfig"
          :columns="cols"
          :extra-params="{ class: 'dvrconfig' }"
          store-key="config-recording-dvr-profiles"
          :default-sort="{ key: 'text', dir: 'ASC' }"
          :virtual-scroller-options="{ itemSize: 36, lazy: false }"
          :count-label="t('profiles')"
          client-side-filter
          selectable="single"
          class="dvr-profiles__grid"
          @row-click="(row) => onRowClick(row, select)"
        >
          <!--
            Action buttons in IdnodeGrid's #toolbarActions slot.
            ActionMenu collapses them into a `…` overflow menu
            when the window is too narrow.
          -->
          <template #toolbarActions>
            <ActionMenu :actions="actions" />
          </template>
        </IdnodeGrid>
      </template>
      <template #detail="{ selectedUuid }">
        <IdnodeConfigForm v-if="selectedUuid" :uuid="selectedUuid" />
        <p v-else class="dvr-profiles__empty">
          {{ t('Select a profile on the left to view and edit its configuration.') }}
        </p>
      </template>
    </MasterDetailLayout>
  </div>
</template>

<style scoped>
.dvr-profiles {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.dvr-profiles__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.dvr-profiles__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
}
</style>
