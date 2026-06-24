<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * ColumnHeaderMenu — kebab-anchored popover for per-column actions
 * (sort / filter / hide / auto-fit width).
 *
 * Replaces PrimeVue's native sort indicator + filter funnel chrome
 * (hidden via `primevue.css`). Rendered inside each column header
 * via `<DataGrid>`'s `#header` slot, sitting at the right edge of
 * the th alongside the active-state indicators (↑/↓ for sort, ▾
 * for active filter).
 *
 * Behaviour:
 *   - Sort A → Z / Sort Z → A: emit `setSort`. Parent grid
 *     translates to PrimeVue's controlled sortField/sortOrder
 *     update.
 *   - Clear sort: emit `setSort` with dir=null.
 *   - Filter…: programmatically click the (visually hidden but
 *     positioned) PrimeVue filter button via DOM lookup —
 *     opens PrimeVue's existing filter popover anchored at that
 *     button's location. Reuses the entire PrimeVue filter UI;
 *     no duplicate input rendering on our side.
 *   - Clear filter: emit `clearFilter`. Parent grid removes the
 *     field from its filter state.
 *   - Hide column: emit `hide`. IdnodeGrid wires this to its
 *     `toggleColumn(field)` method.
 *   - Auto-fit width: emit `autoFit`. IdnodeGrid wires this to
 *     clearing the column's width pref so the def width / 160 px
 *     fallback takes over.
 *
 * Active-state indicators (↑/↓ when sorted, ▾-filled when
 * filtered) sit BEFORE the kebab. They are click-targets too —
 * clicking ↑/↓ flips/clears sort; clicking the filter indicator
 * opens the filter popover.
 *
 * Click-outside dismissal mirrors the SettingsPopover pattern.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { Filter, MoreVertical } from 'lucide-vue-next'
import {
  allocColumnHeaderMenuInstanceId,
  currentlyOpenInstanceId,
} from './columnHeaderMenuState'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

const props = defineProps<{
  field: string
  label: string
  /* Whether the column's DATA model supports the action — sortable
   * is the metadata-driven `col.sortable` from the column def;
   * filterable is `!!col.filterType`. These say "the column CAN
   * be sorted / filtered." */
  sortable?: boolean
  filterable?: boolean
  /* Whether the parent grid is WIRED to handle the kebab's
   * corresponding action emit. If false, the menu item is hidden
   * regardless of the data-level support — clicking would be a
   * no-op since nothing upstream listens. Each consumer (DVR via
   * IdnodeGrid, EPG TableView, etc.) declares which actions it
   * actually wires. Sort flips the controlled sortField/sortOrder
   * via @set-sort; Filter triggers PrimeVue's filter popover via
   * a programmatic click on the (hidden) filter button + an
   * @filter handler in the consumer; Hide column calls
   * IdnodeGrid's toggleColumn; Reset to default width clears the
   * column's persisted width pref. */
  supportsSort?: boolean
  supportsFilter?: boolean
  supportsHide?: boolean
  supportsResetWidth?: boolean
  /* When true, the "Reset to default width" item still renders
   * but is visually disabled and ignores clicks — used to signal
   * the column's width already matches the source default so a
   * reset would be a no-op. Parent grid computes this from its
   * persisted width-pref state. Default false: item stays
   * enabled, matching the pre-flag behaviour. */
  resetWidthDisabled?: boolean
  /* Current state — drives the inline indicators (↑ / ↓ for
   * sort, funnel for filter) and the menu's active markers. */
  sortDir?: 'asc' | 'desc' | null
  filterActive?: boolean
  /* Human-readable description of the active filter (e.g.
   * `Filter: contains "abc"` or `Filter: = 1234`), rendered as
   * the funnel indicator's native `title` so a hover preview
   * tells the user what's currently restricting the view —
   * mirrors the column header's own server-description tooltip.
   * Empty when no filter is active; the parent grid is the
   * authoritative source since it knows both the filter value
   * and the matchMode. */
  filterTitle?: string
  /* Group-by support. `groupable` says this column appears in the
   * grid's `groupableFields` allowlist. `groupActive` says SOME
   * column is the current group; `isGroupedByThis` is the narrow
   * "this column is the active group" case. `supportsGroup` says
   * the parent wires `setGroupField`. When `groupActive` is true
   * the Sort entries disable with a tooltip — sort is locked to
   * the group field while grouping is on. */
  groupable?: boolean
  isGroupedByThis?: boolean
  groupActive?: boolean
  supportsGroup?: boolean
  /* When set, sort entries hide while `groupActive` is true.
   * Used by grids whose backend takes only a single sort key
   * (EPG Table's `epg/events/grid` is single-sort): in grouped
   * mode that one key is consumed by the cluster order, leaving
   * no room for user-driven within-cluster sort. Multi-sort-
   * capable backends (every IdnodeGrid client-side grid)
   * default false — PrimeVue's auto-prepend of the group field
   * as primary lets within-cluster sort work via the secondary
   * key. An upstream multi-sort PR on `epg/events/grid` would
   * retire this flag for EPG Table. */
  sortLockedByGroup?: boolean
}>()

const emit = defineEmits<{
  setSort: [field: string, dir: 'asc' | 'desc' | null]
  hide: [field: string]
  /* Reset the column's width back to its declared default (or
   * the fallback if no default was set). User-triggered via the
   * "Reset to default width" menu item. NOT a fit-to-content
   * resize — that's a separate, deferred feature. */
  resetWidth: [field: string]
  /* Group-by toggle. Field name to group on, or null to ungroup.
   * Parent IdnodeGrid persists via the layout blob and forces the
   * sort to the group field; DataGrid receives the field change
   * reactively and re-renders with PrimeVue's row grouping. */
  setGroupField: [field: string | null]
}>()

/* Each instance allocates a unique id at setup time. Open state
 * is DERIVED from the shared `currentlyOpenInstanceId` — opening
 * any other instance flips the shared ref to its id, which makes
 * all other instances' `open` evaluate to false in the same render
 * pass. Net result: only one menu can be open at a time across
 * the page. */
const instanceId = allocColumnHeaderMenuInstanceId()
const open = computed(() => currentlyOpenInstanceId.value === instanceId)

/* Per-item gates: data-level support AND parent-wiring. Computed
 * once here so the template stays readable. Sort entries hide
 * specifically when the grid's backend can't accept a within-
 * cluster sort while grouping is on (`sortLockedByGroup` + a
 * group field active). Multi-sort-capable backends (default)
 * keep sort entries visible during grouping — PrimeVue auto-
 * prepends the group field as primary; the user's chosen sort
 * becomes the within-cluster secondary. */
const showSortItems = computed(
  () =>
    !!props.sortable &&
    !!props.supportsSort &&
    !(props.groupActive && props.sortLockedByGroup),
)
const showFilterItem = computed(() => !!props.filterable && !!props.supportsFilter)
const showHideItem = computed(() => !!props.supportsHide)
const showResetWidthItem = computed(() => !!props.supportsResetWidth)
const showGroupItem = computed(
  () => !!props.groupable && !!props.supportsGroup,
)

/* Whether the kebab should render at all. If the parent grid
 * supports nothing, render no chrome — keeps the column header
 * clean on grids that haven't wired the kebab actions (e.g.
 * Status views). */
const hasAnyAction = computed(
  () =>
    showSortItems.value ||
    showFilterItem.value ||
    showHideItem.value ||
    showResetWidthItem.value ||
    showGroupItem.value
)

const root = ref<HTMLElement | null>(null)
const trigger = ref<HTMLButtonElement | null>(null)
const panel = ref<HTMLElement | null>(null)

/* Panel position is computed from the trigger's bounding rect at
 * open time. The panel itself is teleported to document.body so
 * it isn't clipped by the th's `overflow: hidden` (set by
 * PrimeVue's resizable-table CSS) or by the scroll viewport. */
const panelStyle = ref<Record<string, string>>({})

const PANEL_WIDTH_PX = 200
const PANEL_GAP_PX = 4
const VIEWPORT_MARGIN_PX = 4

function computePanelPosition() {
  if (!trigger.value) return
  const rect = trigger.value.getBoundingClientRect()
  const top = rect.bottom + PANEL_GAP_PX

  /* Default: right-align panel right edge to kebab right edge.
   * If that would push the panel past the viewport's left edge,
   * flip to left-align (panel left edge = kebab left edge). The
   * panel stays attached to the kebab in either direction.
   *
   * Final clamp: if even the flipped position would overflow the
   * right edge (rare — viewport narrower than PANEL_WIDTH_PX +
   * kebab x), pin to the right edge with a margin. Last-resort
   * defence on tiny viewports. */
  let left = rect.right - PANEL_WIDTH_PX
  if (left < VIEWPORT_MARGIN_PX) {
    left = rect.left
    const maxLeft = window.innerWidth - PANEL_WIDTH_PX - VIEWPORT_MARGIN_PX
    if (left > maxLeft) left = maxLeft
  }

  panelStyle.value = {
    position: 'fixed',
    top: `${top}px`,
    left: `${left}px`,
    minWidth: `${PANEL_WIDTH_PX}px`,
  }
}

function toggle() {
  if (open.value) {
    currentlyOpenInstanceId.value = null
  } else {
    computePanelPosition()
    currentlyOpenInstanceId.value = instanceId
  }
}

function close() {
  if (open.value) currentlyOpenInstanceId.value = null
}

/* Find the PrimeVue filter button inside the same th and trigger
 * its toggleMenu via a synthetic click. The button is hidden via
 * CSS (visibility / opacity), but it's still in layout (position
 * absolute) so its bounding rect drives PrimeVue's overlay
 * positioning sensibly. */
function openPrimeFilter() {
  if (!root.value) return
  const th = root.value.closest('th')
  const filterBtn = th?.querySelector(
    '.p-datatable-column-filter-button'
  ) as HTMLButtonElement | null
  filterBtn?.click()
}

/* Click-outside dismissal. The panel is teleported to body so
 * `root.contains(target)` alone wouldn't match clicks inside the
 * panel — also check the panel ref. Without that the panel would
 * close before its own item @click handlers fire. */
function onDocClick(ev: MouseEvent) {
  if (!open.value) return
  const t = ev.target as Node
  if (root.value?.contains(t)) return
  if (panel.value?.contains(t)) return
  close()
}

/* Close on scroll — the panel uses `position: fixed` against the
 * viewport, so when the user scrolls the table the kebab moves
 * but the panel doesn't follow. Re-anchoring on scroll is one
 * option; closing is simpler and matches PrimeVue's own popover
 * behaviour. Capture phase so we catch scroll on any ancestor
 * scroll container, not just the window. */
function onScroll() {
  close()
}

/* Close on viewport resize for the same reason — the kebab's
 * position relative to the viewport changes; rather than recompute,
 * close. Modal-style dismissal. */
function onResize() {
  close()
}

/* Escape closes the menu and returns focus to the trigger.
 * Standard menu-popover keyboard contract. */
function onKeydown(ev: KeyboardEvent) {
  if (ev.key === 'Escape' && open.value) {
    close()
    trigger.value?.focus()
  }
}

onMounted(() => {
  document.addEventListener('click', onDocClick)
  window.addEventListener('scroll', onScroll, true)
  window.addEventListener('resize', onResize)
  document.addEventListener('keydown', onKeydown)
})

onBeforeUnmount(() => {
  document.removeEventListener('click', onDocClick)
  window.removeEventListener('scroll', onScroll, true)
  window.removeEventListener('resize', onResize)
  document.removeEventListener('keydown', onKeydown)
})

function clickSortAsc() {
  emit('setSort', props.field, 'asc')
  close()
}
function clickSortDesc() {
  emit('setSort', props.field, 'desc')
  close()
}
function clickIndicatorSort() {
  /* Inline ↑/↓ click cycles asc → desc → asc; the grid is
   * never left in an unsorted state, matching the Classic
   * ExtJS UI. The `setSort` emit signature still accepts
   * `null` for type symmetry but no UI path inside this menu
   * emits it. */
  if (props.sortDir === 'asc') emit('setSort', props.field, 'desc')
  else emit('setSort', props.field, 'asc')
}
function clickFilter() {
  openPrimeFilter()
  close()
}
function clickIndicatorFilter() {
  openPrimeFilter()
}
function clickHide() {
  emit('hide', props.field)
  close()
}
function clickResetWidth() {
  emit('resetWidth', props.field)
  close()
}

function clickGroupByThis() {
  emit('setGroupField', props.field)
  close()
}

function clickUngroup() {
  emit('setGroupField', null)
  close()
}

defineExpose({
  close,
})
</script>

<template>
  <span v-if="hasAnyAction" ref="root" class="column-header-menu">
    <button
      v-if="sortDir === 'asc'"
      type="button"
      class="column-header-menu__indicator column-header-menu__indicator--sort"
      :aria-label="t('Sort direction: ascending. Click to change.')"
      @click.stop="clickIndicatorSort"
    >
      ↑
    </button>
    <button
      v-else-if="sortDir === 'desc'"
      type="button"
      class="column-header-menu__indicator column-header-menu__indicator--sort"
      :aria-label="t('Sort direction: descending. Click to change.')"
      @click.stop="clickIndicatorSort"
    >
      ↓
    </button>

    <button
      v-if="filterActive"
      type="button"
      class="column-header-menu__indicator column-header-menu__indicator--filter"
      :title="filterTitle || undefined"
      :aria-label="t('Filter is active. Click to edit.')"
      @click.stop="clickIndicatorFilter"
    >
      <Filter :size="14" :stroke-width="2" />
    </button>

    <button
      ref="trigger"
      type="button"
      class="column-header-menu__trigger"
      :aria-label="t('Column actions for {0}', label)"
      aria-haspopup="menu"
      :aria-expanded="open"
      @click.stop="toggle"
    >
      <MoreVertical :size="14" :stroke-width="2" />
    </button>

    <Teleport to="body">
    <div
      v-if="open"
      ref="panel"
      class="column-header-menu__panel"
      role="menu"
      :style="panelStyle"
    >
      <button
        v-if="showSortItems"
        type="button"
        class="settings-popover__option"
        :class="{ 'settings-popover__option--active': sortDir === 'asc' }"
        @click="clickSortAsc"
      >
        <span class="settings-popover__radio" aria-hidden="true">{{
          sortDir === 'asc' ? '✓' : '↑'
        }}</span>
        {{ t('Sort A → Z') }}
      </button>
      <button
        v-if="showSortItems"
        type="button"
        class="settings-popover__option"
        :class="{ 'settings-popover__option--active': sortDir === 'desc' }"
        @click="clickSortDesc"
      >
        <span class="settings-popover__radio" aria-hidden="true">{{
          sortDir === 'desc' ? '✓' : '↓'
        }}</span>
        {{ t('Sort Z → A') }}
      </button>
      <hr
        v-if="showSortItems && (showGroupItem || showFilterItem)"
        class="settings-popover__divider"
      />

      <!-- Group by this column / Ungroup. The label flips based on
           whether this column is currently the active group. Hidden
           entirely on columns that aren't in `groupableFields`. -->
      <button
        v-if="showGroupItem"
        type="button"
        class="settings-popover__option"
        :class="{ 'settings-popover__option--active': isGroupedByThis }"
        @click="isGroupedByThis ? clickUngroup() : clickGroupByThis()"
      >
        <span class="settings-popover__radio" aria-hidden="true">⛬</span>
        {{ isGroupedByThis ? t('Ungroup') : t('Group by this column') }}
      </button>
      <hr v-if="showGroupItem && showFilterItem" class="settings-popover__divider" />

      <button
        v-if="showFilterItem"
        type="button"
        class="settings-popover__option"
        :class="{ 'settings-popover__option--active': filterActive }"
        @click="clickFilter"
      >
        <span class="column-header-menu__menu-icon" aria-hidden="true">
          <span v-if="filterActive">✓</span>
          <Filter v-else :size="14" :stroke-width="2" />
        </span>
        {{ t('Filter…') }}
      </button>

      <hr
        v-if="(showSortItems || showFilterItem) && (showHideItem || showResetWidthItem)"
        class="settings-popover__divider"
      />

      <button
        v-if="showHideItem"
        type="button"
        class="settings-popover__option"
        @click="clickHide"
      >
        <span class="settings-popover__radio" aria-hidden="true">⊘</span>
        {{ t('Hide column') }}
      </button>
      <button
        v-if="showResetWidthItem"
        type="button"
        class="settings-popover__option"
        :class="{ 'settings-popover__option--disabled': resetWidthDisabled }"
        :disabled="resetWidthDisabled"
        @click="clickResetWidth"
      >
        <span class="settings-popover__radio" aria-hidden="true">↔</span>
        {{ t('Reset to default width') }}
      </button>
    </div>
    </Teleport>
  </span>
</template>

<style scoped>
.column-header-menu {
  position: relative;
  display: inline-flex;
  align-items: center;
  gap: 4px;
  margin-inline-start: auto;
  /* Push to end of the flex header content (PrimeVue's title is
   * order:0; we want our chrome at the right edge). */
  order: 99;
}

.column-header-menu__indicator,
.column-header-menu__trigger {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 16px;
  background: transparent;
  border: 0;
  color: var(--tvh-text-muted);
  cursor: pointer;
  font-size: 0.875rem;
  line-height: 1;
}

.column-header-menu__indicator {
  width: 16px;
  padding: 0;
}

/* Wider hit target on the trigger so a click slightly off the
 * 14 px MoreVertical icon still registers — easier to aim at,
 * especially on dense column-header rows. The icon stays
 * 14 px and centres via the existing flex alignment; the extra
 * width is invisible padding. */
.column-header-menu__trigger {
  width: 24px;
  padding: 0 4px;
}

.column-header-menu__indicator--sort {
  color: var(--tvh-primary);
  font-weight: bold;
}

.column-header-menu__indicator--filter {
  color: var(--tvh-primary);
}

.column-header-menu__indicator:hover,
.column-header-menu__trigger:hover {
  color: var(--tvh-text);
}

.column-header-menu__trigger:focus-visible,
.column-header-menu__indicator:focus-visible {
  outline: 1px solid var(--tvh-primary);
  outline-offset: 2px;
}

/* Wrapper for the small icon shown to the left of each menu
 * item label (e.g. the funnel icon next to "Filter…"). Mirrors
 * `.settings-popover__radio`'s 1em-wide slot but uses inline-flex
 * so an SVG (lucide Filter) centres correctly inside the slot —
 * `text-align: center` only works for inline text, not for an
 * inline SVG element. */
.column-header-menu__menu-icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 auto;
  width: 1em;
  color: var(--tvh-primary);
}

.column-header-menu__panel {
  /* Position is set inline by `computePanelPosition()` —
   * `position: fixed` + `top` / `left` computed from the
   * trigger's bounding rect. This block holds visual styling
   * only (width / padding / chrome). */
  min-width: 200px;
  padding: var(--tvh-space-2);
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.18);
  z-index: 20;
  /* Stop the menu inheriting the parent th's nowrap so labels
   * inside menu items wrap if needed (none currently do, but
   * future "Reset width" / longer items would). */
  white-space: normal;
}
</style>
