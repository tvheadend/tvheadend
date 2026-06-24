<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * PageTabs — generic L2 navigation strip for section-level
 * routing (DVR sub-tabs, Configuration L2 / L3 tabs, etc.).
 *
 * Phone (<768 px) collapses the row to a <select> for
 * consistency with the IdnodeGrid sort dropdown — one
 * "select to navigate" idiom rather than two.
 *
 * Active-tab matching uses prefix-of-route, not exact match,
 * so deeper detail routes (e.g. /dvr/upcoming/<uuid>) keep
 * the parent tab visually active.
 *
 * Overflow gradient + auto-scroll mirror IdnodeGrid's
 * table-shell scroll-shadow shape for cross-component
 * consistency.
 */
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch, type Component } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { computeOverflow } from './pageTabsOverflow'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'

const { t } = useI18n()

interface PageTab {
  to: string
  label: string
}

const props = defineProps<{
  tabs: PageTab[]
  /*
   * Optional L1 icon — when set, renders to the LEFT of the
   * phone-mode dropdown so the user can tell which top-level
   * section they're in even after the NavRail closes. Only the
   * topmost in-content nav bar passes this (the L2 layouts):
   * inner L3 PageTabs leaves it undefined and renders nothing.
   * Pure visual cue; not interactive.
   */
  l1Icon?: Component
}>()

const route = useRoute()
const router = useRouter()

const activeTo = computed(() => {
  /*
   * Choose the longest matching prefix so nested routes don't
   * accidentally light up a parent's sibling tab. e.g. for
   * /dvr/upcoming we want to match the `/dvr/upcoming` tab, not
   * `/dvr` if it ever existed as a tab too.
   */
  const matches = props.tabs.filter((t) => route.path === t.to || route.path.startsWith(t.to + '/'))
  matches.sort((a, b) => b.to.length - a.to.length)
  return matches[0]?.to ?? props.tabs[0]?.to ?? ''
})

const isPhone = useIsPhone()

function onSelectChange(ev: Event) {
  const target = ev.target as HTMLSelectElement
  if (target.value && target.value !== route.path) {
    router.push(target.value)
  }
}

/* ---- Overflow indicator + auto-scroll ---- */

/* Template ref on the inner scroller. We read scrollLeft /
 * scrollWidth / clientWidth to drive the fade-in/out modifier
 * classes; same scroll observer pattern as IdnodeGrid. */
const scrollerRef = ref<HTMLDivElement | null>(null)
const hasLeftOverflow = ref(false)
const hasRightOverflow = ref(false)

function updateOverflow() {
  const el = scrollerRef.value
  if (!el) {
    hasLeftOverflow.value = false
    hasRightOverflow.value = false
    return
  }
  const { hasLeft, hasRight } = computeOverflow({
    scrollLeft: el.scrollLeft,
    scrollWidth: el.scrollWidth,
    clientWidth: el.clientWidth,
  })
  hasLeftOverflow.value = hasLeft
  hasRightOverflow.value = hasRight
}

let resizeObs: ResizeObserver | null = null

onMounted(() => {
  /* Initial measurement after layout settles. nextTick flushes
   * the post-mount render so scrollWidth reflects the actual
   * tabs width, not the pre-render zero. */
  void nextTick(updateOverflow)
  if (globalThis.ResizeObserver !== undefined) {
    resizeObs = new ResizeObserver(updateOverflow)
    if (scrollerRef.value) resizeObs.observe(scrollerRef.value)
  }
})

onBeforeUnmount(() => {
  resizeObs?.disconnect()
  resizeObs = null
})

/* Re-measure when the tabs prop changes (e.g. capability gate
 * removes a tab; new layout's overflow state may flip). */
watch(
  () => props.tabs.map((t) => t.to).join('|'),
  () => {
    void nextTick(updateOverflow)
  }
)

/* Active tab → auto-scroll into view. Triggers on initial mount
 * (deep-link lands on a partially-off-screen tab) and on every
 * route change while the section is mounted. `inline: 'nearest'`
 * keeps the row stable when the active tab is already visible
 * — only scrolls if it would actually be off-screen. */
function scrollActiveIntoView() {
  const root = scrollerRef.value
  if (!root) return
  const el = root.querySelector<HTMLElement>('.page-tabs__tab--active')
  if (!el) return
  el.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'nearest' })
}

watch(
  activeTo,
  () => {
    void nextTick(scrollActiveIntoView)
  },
  { immediate: true }
)
</script>

<template>
  <nav
    class="page-tabs"
    :class="{
      'page-tabs--phone': isPhone,
      'page-tabs--has-left-overflow': hasLeftOverflow,
      'page-tabs--has-right-overflow': hasRightOverflow,
    }"
    :aria-label="t('Section navigation')"
  >
    <template v-if="isPhone">
      <span
        v-if="l1Icon"
        class="page-tabs__l1-icon"
        aria-hidden="true"
      >
        <component :is="l1Icon" :size="20" :stroke-width="2" />
      </span>
      <select
        class="page-tabs__select"
        :aria-label="t('Section navigation')"
        :value="activeTo"
        @change="onSelectChange"
      >
        <option v-for="tab in tabs" :key="tab.to" :value="tab.to">
          {{ tab.label }}
        </option>
      </select>
    </template>
    <div
      v-else
      ref="scrollerRef"
      class="page-tabs__scroller"
      @scroll.passive="updateOverflow"
    >
      <router-link
        v-for="tab in tabs"
        :key="tab.to"
        :to="tab.to"
        class="page-tabs__tab"
        :class="{ 'page-tabs__tab--active': activeTo === tab.to }"
      >
        {{ tab.label }}
      </router-link>
    </div>
  </nav>
</template>

<style scoped>
/*
 * Outer nav holds the bottom border + carries the overflow-fade
 * pseudo-elements; the inner `.page-tabs__scroller` is the
 * actual scrollable element. Splitting them lets the gradients
 * stay pinned to the visible viewport edges (absolute positioned
 * to the non-scrolling outer) while the tabs scroll behind.
 * Mirrors the IdnodeGrid table-shell pattern.
 */
.page-tabs {
  position: relative;
  border-bottom: 1px solid var(--tvh-border);
  /* Reduced from --tvh-space-4 (16 px) — tab → content gap
   * doesn't need that much breathing room when the bottom
   * border already provides a visual separator. */
  margin-bottom: var(--tvh-space-2);
  flex-shrink: 0;
  /* See the inner scroller's overflow-x — keeping it on the
   * inner element means the bottom border on this outer
   * doesn't scroll with the tab content. */
}

.page-tabs__scroller {
  display: flex;
  gap: var(--tvh-space-4);
  align-items: stretch;
  /*
   * Allow the row to scroll horizontally when there are many tabs
   * rather than wrapping; wrapping breaks the underline alignment
   * and makes the row visually jump as new tabs are added.
   */
  overflow-x: auto;
  /*
   * Explicitly suppress overflow-y. Per CSS Overflow Module
   * Level 3, setting overflow-x: auto silently promotes
   * overflow-y from `visible` to `auto` to prevent content
   * clipping. Tabs carry margin-bottom: -1px to overlap the
   * nav's bottom border into one continuous underline — that
   * negative margin pushes the tab's visual bottom 1 px past
   * the scroller's content box, so scrollHeight ends up 1 px
   * larger than clientHeight. The implicit overflow-y: auto
   * sees that 1 px and renders a vertical scrollbar over a
   * single-row horizontal tab strip — which is never useful.
   * Pin overflow-y to hidden so the 1 px clip is invisible
   * and the spurious scrollbar can't appear.
   */
  overflow-y: hidden;
  scrollbar-width: thin;
  /*
   * Pinning shrink to 0 — same rationale as the prior
   * `.page-tabs` had: when a tall sibling is in the same flex
   * column, `flex: 0 1 auto` lets the algorithm collapse the
   * tab row vertically and clip the tabs.
   */
  flex-shrink: 0;
}

.page-tabs__tab {
  display: inline-flex;
  align-items: center;
  /* Reduced from --tvh-space-3 (12 px) on top + bottom — saves
   * 8 px of chrome around the tab row without compressing the
   * underline + text baseline noticeably. */
  padding: var(--tvh-space-2) 0;
  color: var(--tvh-text-muted);
  text-decoration: none;
  font-weight: 500;
  font-size: var(--tvh-text-lg);
  border-bottom: 2px solid transparent;
  /* Pull the tab's bottom border 1 px down so it overlaps the
     outer nav's border, producing a clean continuous underline
     rather than two stacked lines. */
  margin-bottom: -1px;
  white-space: nowrap;
  transition:
    color var(--tvh-transition),
    border-color var(--tvh-transition);
}

.page-tabs__tab:hover {
  color: var(--tvh-text);
}

.page-tabs__tab:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}

.page-tabs__tab--active {
  color: var(--tvh-text);
  font-weight: 600;
  border-bottom-color: var(--tvh-primary);
}

/*
 * Overflow fade gradients — 14 px wide, anchored to the visible
 * left/right edges of the (non-scrolling) outer nav. Toggled via
 * the `--has-{left,right}-overflow` modifier classes driven by
 * the scroll observer in <script>. Same colour and transition
 * the IdnodeGrid table-shell uses (`IdnodeGrid.vue:1326-1355`)
 * so the "more this way" idiom reads identically across tabs
 * and tables.
 *
 * `bottom: 1px` clears the bottom border so the fade doesn't
 * paint over the underline rule.
 */
.page-tabs::before,
.page-tabs::after {
  content: '';
  position: absolute;
  top: 0;
  bottom: 1px;
  width: 14px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 150ms ease-out;
  z-index: 2;
}

.page-tabs::before {
  left: 0;
  background: linear-gradient(90deg, var(--tvh-scroll-fade), transparent);
}

.page-tabs::after {
  right: 0;
  background: linear-gradient(-90deg, var(--tvh-scroll-fade), transparent);
}

.page-tabs--has-left-overflow::before {
  opacity: 1;
}

.page-tabs--has-right-overflow::after {
  opacity: 1;
}

/* Phone — replace the tab row with a select dropdown. The
 * fade gradients are suppressed (visibility: hidden) since the
 * dropdown is the affordance there. */
.page-tabs--phone {
  border-bottom: none;
  margin-bottom: var(--tvh-space-3);
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
}

.page-tabs--phone::before,
.page-tabs--phone::after {
  display: none;
}

/* L1 icon on phone — informational cue showing which top-level
 * section the user is currently in (NavRail collapses to the
 * hamburger on phone so the active section indicator otherwise
 * disappears). Same lucide-vue-next icon NavRail uses for the
 * matching L1 item, sized to align with the dropdown. */
.page-tabs__l1-icon {
  flex-shrink: 0;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: var(--tvh-text);
}

.page-tabs__select {
  flex: 1;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 8px var(--tvh-space-3);
  font: inherit;
  min-height: 40px;
}

.page-tabs__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
