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
  /* Current state — drives the inline indicators (↑ / ↓ for
   * sort, funnel for filter) and the menu's active markers. */
  sortDir?: 'asc' | 'desc' | null
  filterActive?: boolean
}>()

const emit = defineEmits<{
  setSort: [field: string, dir: 'asc' | 'desc' | null]
  hide: [field: string]
  /* Reset the column's width back to its declared default (or
   * the fallback if no default was set). User-triggered via the
   * "Reset to default width" menu item. NOT a fit-to-content
   * resize — that's a separate feature, filed in BACKLOG. */
  resetWidth: [field: string]
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
 * once here so the template stays readable. */
const showSortItems = computed(() => !!props.sortable && !!props.supportsSort)
const showFilterItem = computed(() => !!props.filterable && !!props.supportsFilter)
const showHideItem = computed(() => !!props.supportsHide)
const showResetWidthItem = computed(() => !!props.supportsResetWidth)

/* Whether the kebab should render at all. If the parent grid
 * supports nothing, render no chrome — keeps the column header
 * clean on grids that haven't wired the kebab actions (e.g.
 * Status views). */
const hasAnyAction = computed(
  () =>
    showSortItems.value ||
    showFilterItem.value ||
    showHideItem.value ||
    showResetWidthItem.value
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
function clickClearSort() {
  emit('setSort', props.field, null)
  close()
}
function clickIndicatorSort() {
  /* Inline ↑/↓ click flips between asc and desc; second click on
   * an already-active direction clears. Matches PrimeVue's
   * removableSort cycle. */
  if (props.sortDir === 'asc') emit('setSort', props.field, 'desc')
  else if (props.sortDir === 'desc') emit('setSort', props.field, null)
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
      :aria-label="`Sort direction: ascending. Click to change.`"
      @click.stop="clickIndicatorSort"
    >
      ↑
    </button>
    <button
      v-else-if="sortDir === 'desc'"
      type="button"
      class="column-header-menu__indicator column-header-menu__indicator--sort"
      :aria-label="`Sort direction: descending. Click to change.`"
      @click.stop="clickIndicatorSort"
    >
      ↓
    </button>

    <button
      v-if="filterActive"
      type="button"
      class="column-header-menu__indicator column-header-menu__indicator--filter"
      aria-label="Filter is active. Click to edit."
      @click.stop="clickIndicatorFilter"
    >
      <Filter :size="14" :stroke-width="2" />
    </button>

    <button
      ref="trigger"
      type="button"
      class="column-header-menu__trigger"
      :aria-label="`Column actions for ${label}`"
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
        Sort A → Z
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
        Sort Z → A
      </button>
      <button
        v-if="showSortItems && sortDir"
        type="button"
        class="settings-popover__option"
        @click="clickClearSort"
      >
        <span class="settings-popover__radio" aria-hidden="true">✕</span>
        Clear sort
      </button>

      <hr v-if="showSortItems && showFilterItem" class="settings-popover__divider" />

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
        Filter…
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
        Hide column
      </button>
      <button
        v-if="showResetWidthItem"
        type="button"
        class="settings-popover__option"
        @click="clickResetWidth"
      >
        <span class="settings-popover__radio" aria-hidden="true">↔</span>
        Reset to default width
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
