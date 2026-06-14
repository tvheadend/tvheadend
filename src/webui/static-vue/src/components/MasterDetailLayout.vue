<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * MasterDetailLayout — generic two-pane layout shell.
 *
 * Left pane (#master slot) is a list / grid; right pane
 * (#detail slot) is a per-selection detail view (typically a
 * config form bound to the selected uuid). The selected uuid
 * is owned by the caller via v-model and re-passed into both
 * slots so each pane can react.
 *
 * Layout behaviour:
 *   - Desktop (≥ PHONE_MAX_WIDTH): a draggable splitter sits
 *     between master and detail. Initial split is
 *     `defaultDetailFraction` (30% by default — detail pane
 *     small enough to leave the grid generous, large enough
 *     to be useful as a form). Users drag the gutter to
 *     re-allocate; min sizes prevent either pane from being
 *     squashed. When `storageKey` is set, the split position
 *     is persisted to localStorage per consumer.
 *   - Phone (< PHONE_MAX_WIDTH): drilldown — when no
 *     selection, only the master pane is visible; once the
 *     user picks a row, only the detail pane is visible
 *     with a `← Back` affordance to return. No splitter on
 *     phone.
 *
 * URL sync is the caller's job — pair with `useEditorMode`
 * or a similar route-bound state composable so refresh /
 * deep-link work.
 *
 * First consumer: `EpgGrabberModulesView.vue` (Configuration
 * → Channel / EPG → EPG Grabber Modules). Mirrors the legacy
 * `idnode_form_grid` layout.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import Splitter from 'primevue/splitter'
import SplitterPanel from 'primevue/splitterpanel'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = withDefaults(
  defineProps<{
    /* Selected row's uuid. Caller binds via v-model. */
    selectedUuid: string | null
    /* Width threshold (px) below which the layout collapses
     * to drilldown mode. Defaults to DataGrid's 768 so phone
     * card-list and this layout flip in lockstep. */
    phoneBreakpoint?: number
    /* Initial detail-pane size as a percentage of the
     * container width (0-100). The master pane takes the
     * remainder. Once the user drags the splitter, that
     * choice is persisted under `storageKey` and this default
     * is ignored on subsequent loads. */
    defaultDetailFraction?: number
    /* Minimum master-pane percentage — keeps the grid
     * readable even when the user drags the gutter far left. */
    minMasterFraction?: number
    /* Minimum detail-pane percentage — keeps form controls
     * usable even when the user drags far right. */
    minDetailFraction?: number
    /* When set, persists the user's chosen split position to
     * localStorage under this key (with a `:split` suffix so
     * the key composes cleanly with other per-view storage).
     * Omit for one-off / test mounts that don't need
     * persistence. */
    storageKey?: string
  }>(),
  {
    phoneBreakpoint: 768,
    defaultDetailFraction: 30,
    minMasterFraction: 30,
    minDetailFraction: 20,
    storageKey: undefined,
  }
)

const emit = defineEmits<{
  'update:selectedUuid': [uuid: string | null]
}>()

/* ---- Responsive mode (mirror DataGrid's resize-listener
 * pattern). ---- */

const isPhone = ref(
  globalThis.window !== undefined &&
    globalThis.window.innerWidth < props.phoneBreakpoint
)

function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < props.phoneBreakpoint
}

onMounted(() => {
  globalThis.window.addEventListener('resize', checkPhone)
})

onBeforeUnmount(() => {
  globalThis.window.removeEventListener('resize', checkPhone)
})

/* ---- Slot helpers ---- */

function select(uuid: string | null) {
  emit('update:selectedUuid', uuid)
}

function close() {
  emit('update:selectedUuid', null)
}

/* On phone, the master pane shows when nothing is selected,
 * the detail pane shows when something IS selected. On
 * desktop, both panes are always visible via the splitter. */
const showMaster = computed(() => isPhone.value && props.selectedUuid === null)
const showDetail = computed(() => isPhone.value && props.selectedUuid !== null)

/* PrimeVue's Splitter stateKey when persistence is enabled.
 * `:split` suffix keeps the key composable with any future
 * per-consumer storage (e.g. column widths, sort prefs).
 * Returns undefined (not null) so the prop typing on
 * <Splitter> stays happy. */
const splitterStateKey = computed<string | undefined>(() =>
  props.storageKey ? `${props.storageKey}:split` : undefined,
)

/* Initial master percentage = 100 minus the detail
 * fraction. PrimeVue's SplitterPanel only honours the `size`
 * prop on the FIRST mount; subsequent loads read from
 * stateKey storage when set. */
const initialMasterSize = computed(() => 100 - props.defaultDetailFraction)

/* Double-click-to-reset on the splitter gutter — IDE convention
 * (VS Code, JetBrains). PrimeVue Splitter has no built-in reset;
 * the trick is to clear the persisted entry and force a fresh
 * mount by bumping a reactive `:key`. The remounted Splitter
 * finds no saved state and falls back to each SplitterPanel's
 * `:size` prop, which IS the defaults. */
const splitterMountKey = ref(0)

function resetSplitter(): void {
  if (splitterStateKey.value) {
    try {
      globalThis.localStorage.removeItem(splitterStateKey.value)
    } catch {
      /* localStorage unavailable — silent fail. */
    }
  }
  splitterMountKey.value++
}
</script>

<template>
  <div class="master-detail" :class="{ 'master-detail--phone': isPhone }">
    <!-- Phone: drilldown, one pane at a time, no splitter. -->
    <template v-if="isPhone">
      <div
        v-if="showMaster"
        class="master-detail__master master-detail__master--phone"
      >
        <slot
          name="master"
          :selected-uuid="selectedUuid"
          :select="select"
          :is-phone="isPhone"
        />
      </div>
      <div
        v-if="showDetail"
        class="master-detail__detail master-detail__detail--phone"
      >
        <button
          type="button"
          class="master-detail__back"
          :aria-label="t('Back to list')"
          @click="close"
        >
          ← {{ t('Back') }}
        </button>
        <slot
          name="detail"
          :selected-uuid="selectedUuid"
          :close="close"
          :is-phone="isPhone"
        />
      </div>
    </template>

    <!-- Desktop: resizable two-pane splitter. Gutter is a
         draggable handle; PrimeVue persists the position to
         localStorage when storageKey is set. -->
    <Splitter
      v-else
      :key="splitterMountKey"
      class="master-detail__splitter"
      :state-key="splitterStateKey"
      state-storage="local"
      :gutter-size="6"
      :pt="{
        gutter: {
          onDblclick: resetSplitter,
          title: t('Drag to resize · double-click to reset'),
        },
      }"
    >
      <SplitterPanel :size="initialMasterSize" :min-size="minMasterFraction">
        <div class="master-detail__master">
          <slot
            name="master"
            :selected-uuid="selectedUuid"
            :select="select"
            :is-phone="isPhone"
          />
        </div>
      </SplitterPanel>
      <SplitterPanel
        :size="defaultDetailFraction"
        :min-size="minDetailFraction"
      >
        <div class="master-detail__detail">
          <slot
            name="detail"
            :selected-uuid="selectedUuid"
            :close="close"
            :is-phone="isPhone"
          />
        </div>
      </SplitterPanel>
    </Splitter>
  </div>
</template>

<style scoped>
.master-detail {
  display: flex;
  flex: 1 1 auto;
  min-height: 0;
}

.master-detail--phone {
  /* Phone: only one pane visible at a time, and that pane
   * fills the viewport width. */
  gap: 0;
}

/* Splitter fills the available space on desktop; PrimeVue's
 * own styling owns the gutter chrome. Layered class so
 * consumers can target the splitter root (e.g. for unusual
 * height constraints) without leaking into the panel
 * children. */
.master-detail__splitter {
  flex: 1 1 auto;
  min-width: 0;
  min-height: 0;
  border: 0;
  background: transparent;
}

.master-detail__master {
  flex: 1 1 auto;
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  height: 100%;
}

.master-detail__master--phone {
  width: 100%;
}

.master-detail__detail {
  display: flex;
  flex-direction: column;
  min-height: 0;
  height: 100%;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  padding: var(--tvh-space-3);
  overflow-y: auto;
}

.master-detail__detail--phone {
  flex: 1 1 auto;
  width: 100%;
  border: none;
  border-radius: 0;
  padding: var(--tvh-space-2);
}

.master-detail__back {
  align-self: flex-start;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 6px var(--tvh-space-3);
  margin-bottom: var(--tvh-space-2);
  font: inherit;
  font-size: var(--tvh-text-md);
  cursor: pointer;
}

.master-detail__back:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}
</style>
