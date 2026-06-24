<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Configuration → Stream — Codec Profiles.
 *
 * Master-detail (same shape as ConfigStreamProfilesView):
 *   - LEFT: IdnodeGrid over the `codec_profile/list` custom list
 *     endpoint — returns `{ uuid, title, status }` per profile.
 *     `codec_profile` has no `ic_get_title`, so `idnode/load` rows
 *     would carry the uuid in `text`; the list endpoint's `title`
 *     is the display name (same pattern as
 *     EpgGrabberModulesView).
 *   - RIGHT: IdnodeConfigForm bound to the selected profile's UUID
 *     — the per-codec subclass fields auto-render here on edit.
 *
 * Add / Clone — both form-based, mirroring ExtJS's
 * `tvheadend.codec_tab()` (`static/app/codec.js:550`) +
 * `idnode_form_grid`'s Clone shape
 * (`idnode.js:2378-2388` — `idnode_create(conf.add, true,
 * currentFormValues)`):
 *
 *   - Add: pick a codec → open an IdnodeEditor create-mode drawer
 *     scoped on that codec; the user fills name + settings; save
 *     POSTs `codec_profile/create` with `{ class: <codecName>,
 *     conf }` (`src/api/api_codec.c` `api_codec_profile_create`).
 *     The grid refreshes via the `codec_profile` Comet
 *     notification — the new row appears (no auto-select today;
 *     `codec_profile/create` doesn't return the new uuid, pending
 *     a server-side enhancement).
 *   - Clone: load the source's flattened conf, open the same
 *     create-mode drawer scoped on the source's codec, with the
 *     source's values pre-filled (`name` suffixed " (copy)"). The
 *     simple one-click `cloneIdnode` doesn't fit
 *     `codec_profile/create` (name required, no uuid back) and
 *     ExtJS's Clone is itself form-based, so the v2 follows suit.
 *
 * `codec_profile_class` (`src/transcoding/codec/profile_class.c`,
 * `ic_perm_def = ACCESS_ADMIN`). Only reachable when the server is
 * built with transcoding — the route guard and the Stream tab
 * strip both gate on the `libav` capability.
 */
import { computed, ref } from 'vue'
import IdnodeGrid from '@/components/IdnodeGrid.vue'
import IdnodeConfigForm from '@/components/IdnodeConfigForm.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import MasterDetailLayout from '@/components/MasterDetailLayout.vue'
import IdnodePickClassDialog from '@/components/IdnodePickClassDialog.vue'
import ActionMenu from '@/components/ActionMenu.vue'
import { useDeleteAction } from '@/composables/useMasterDetailActions'
import { useI18n } from '@/composables/useI18n'
import { useToastNotify } from '@/composables/useToastNotify'
import { useAccessStore } from '@/stores/access'
import { apiCall } from '@/api/client'
import type { ColumnDef } from '@/types/column'
import type { ActionDef } from '@/types/action'

const { t } = useI18n()
const toast = useToastNotify()
const access = useAccessStore()
const selected = ref<string | null>(null)

const remove = useDeleteAction({
  selected,
  confirmText: t('Do you really want to delete this codec profile?'),
})

/* Codec-picker visibility for the Add flow. */
const pickerVisible = ref(false)

/* In-flight flag for the Clone source-load round-trip. */
const cloneInflight = ref(false)

/* Active create session — drives the IdnodeEditor below. Null when
 * no create is open; an Add picks a codec into it, a Clone loads
 * the source's values + codec into it. */
interface CreateSession {
  codecName: string
  /* Clone pre-fill; null for a blank Add. */
  preselect: Record<string, unknown> | null
}
const creating = ref<CreateSession | null>(null)

function onAddClick(): void {
  pickerVisible.value = true
}

async function onCloneClick(): Promise<void> {
  if (!selected.value || cloneInflight.value) return
  cloneInflight.value = true
  try {
    /* Mirror ExtJS Clone (`idnode.js:2385`) — open the same Add
     * form pre-filled with the source row's current values. */
    const resp = await apiCall<{
      entries?: Array<{ params?: Array<{ id: string; value?: unknown }> }>
    }>('idnode/load', { uuid: selected.value })
    const params = resp.entries?.[0]?.params ?? []
    const conf: Record<string, unknown> = {}
    for (const p of params) conf[p.id] = p.value
    const codecName = typeof conf.codec_name === 'string' ? conf.codec_name : ''
    if (!codecName) {
      toast.error(t('Source codec profile has no codec_name.'), {
        summary: t('Clone failed'),
      })
      return
    }
    delete conf.uuid
    /* Name must be unique — `tvh_codec_profile_create` rejects
     * EEXIST on a duplicate (`src/transcoding/codec/profile.c`).
     * Suffix " (copy)"; the user can adjust before saving. */
    const rawName = conf.name
    conf.name =
      (typeof rawName === 'string' && rawName ? rawName : 'unnamed') + ' (copy)'
    creating.value = { codecName, preselect: conf }
  } catch (e) {
    toast.error(e instanceof Error ? e.message : String(e), {
      summary: t('Clone failed'),
    })
  } finally {
    cloneInflight.value = false
  }
}

function onCodecPicked(codecName: string): void {
  pickerVisible.value = false
  creating.value = { codecName, preselect: null }
}

function onCreateClose(): void {
  creating.value = null
}

function onCreateSaved(): void {
  /* The server doesn't return the new uuid (pending a server-side
   * enhancement), so we can't auto-select the new row — it appears
   * in the grid via the `codec_profile` Comet notification. */
  creating.value = null
  toast.success(t('Codec profile saved.'))
}

const actions = computed<ActionDef[]>(() => [
  {
    id: 'add',
    label: t('Add'),
    onClick: onAddClick,
  },
  {
    id: 'clone',
    label: cloneInflight.value ? t('Cloning…') : t('Clone'),
    disabled: !selected.value || cloneInflight.value,
    onClick: onCloneClick,
  },
  {
    id: 'delete',
    label: remove.inflight.value ? t('Deleting…') : t('Delete'),
    disabled: !selected.value || remove.inflight.value,
    onClick: remove.run,
  },
])

const cols: ColumnDef[] = [
  {
    field: 'title',
    label: t('Name'),
    sortable: true,
    filterType: 'string',
    width: 280,
    minVisible: 'phone',
    phoneRole: 'primary',
  },
]

function onRowClick(row: Record<string, unknown>, select: (uuid: string | null) => void) {
  const uuid = row.uuid
  select(typeof uuid === 'string' ? uuid : null)
}

/* IdnodeEditor's parent-scoped create strategy — fetches the form
 * metadata from `codec_profile/class` and POSTs the new profile to
 * `codec_profile/create` with `class: <codecName>` (codec_profile
 * keys on the codec name, not the idnode class). */
const parentScoped = computed(() =>
  creating.value
    ? {
        classEndpoint: 'codec_profile/class',
        createEndpoint: 'codec_profile/create',
        params: { class: creating.value.codecName },
      }
    : null,
)

const createTitle = computed(() => {
  if (!creating.value) return undefined
  return creating.value.preselect
    ? t('Clone Codec Profile')
    : t('Add Codec Profile')
})
</script>

<template>
  <div class="codec-profiles">
    <MasterDetailLayout
      v-model:selected-uuid="selected"
      storage-key="config-codec-profiles"
      :default-detail-fraction="35"
    >
      <template #master="{ select }">
        <IdnodeGrid
          endpoint="codec_profile/list"
          help-page="class/codec_profile"
          entity-class="codec_profile"
          :columns="cols"
          store-key="config-stream-codec-profiles"
          :default-sort="{ key: 'title', dir: 'ASC' }"
          :virtual-scroller-options="{ itemSize: 36, lazy: false }"
          :count-label="t('codec profiles')"
          client-side-filter
          selectable="single"
          class="codec-profiles__grid"
          @row-click="(row) => onRowClick(row, select)"
        >
          <template #toolbarActions>
            <ActionMenu :actions="actions" />
          </template>
        </IdnodeGrid>
      </template>
      <template #detail="{ selectedUuid }">
        <IdnodeConfigForm v-if="selectedUuid" :uuid="selectedUuid" />
        <p v-else class="codec-profiles__empty">
          {{ t('Select a codec profile on the left to view and edit its configuration.') }}
        </p>
      </template>
    </MasterDetailLayout>

    <IdnodePickClassDialog
      :visible="pickerVisible"
      builders-endpoint="codec/list"
      :title="t('Add Codec Profile')"
      :label="t('Codec')"
      @pick="onCodecPicked"
      @close="pickerVisible = false"
    />

    <!--
      Codec create form (Add or Clone). `createBase` flips
      IdnodeEditor into create mode; `parentScoped` overrides the
      default strategy so the create POST sends
      `class: <codecName>` to `codec_profile/create`. `preselect`
      carries the Clone pre-fill (null for blank Add).
    -->
    <IdnodeEditor
      :uuid="null"
      :level="access.uilevel"
      :create-base="creating ? 'codec_profile' : null"
      :parent-scoped="parentScoped"
      :preselect="creating?.preselect ?? null"
      :title="createTitle"
      @close="onCreateClose"
      @saved="onCreateSaved"
    />
  </div>
</template>

<style scoped>
.codec-profiles {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-3);
  min-height: 0;
}

.codec-profiles__grid {
  flex: 1 1 auto;
  min-height: 0;
}

.codec-profiles__empty {
  margin: 0;
  padding: var(--tvh-space-6);
  text-align: center;
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-lg);
}
</style>
