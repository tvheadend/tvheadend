<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * L2Sidebar — generic vertical-list navigation sidebar for
 * sectioned views that need a second-column nav surface. Same
 * data-shape as PageTabs (`{ to, label, icon }`) so a future
 * refactor can fold both into one component with a `direction:
 * 'horizontal' | 'vertical'` prop.
 *
 * Desktop expanded: vertical list of router-link tiles, NavRail-
 * styled (icon + label, hover background, active-pill highlight).
 * Width fixed at 220 px.
 *
 * Desktop collapsed: 56 px wide, icons only, labels reachable
 * via the browser's native title-attribute tooltip on hover.
 * Mirrors L1 NavRail's collapsed mode for visual parity. State
 * lives in the shared `useL2RailPreference` composable so every
 * L2Sidebar mount (one per L2-using layout) shares one collapse
 * state — flipping it on Configuration also affects future DVR /
 * Status surfaces that adopt this component.
 *
 * Auto-collapse: when the viewport sits in the 768-1279 band
 * (matching NavRail's `COMPACT_BELOW`), both rails collapse
 * together so content has room to breathe at narrow desktop
 * widths. Manual chevron clicks override per-visit and reset on
 * route change, mirroring L1's behaviour.
 *
 * Phone (<768px): collapses to a `<select>` dropdown above the
 * main content (existing behaviour, untouched by the new
 * collapse feature). Hamburger stays L1-only; sidebar IS L2 in
 * select form.
 *
 * Active-tab matching uses the same longest-prefix logic as
 * PageTabs so deep routes (e.g., /configuration/dvb/adapters)
 * keep their L2 entry (DVB Inputs) visually active.
 *
 * Capability / uilevel filtering happens in the caller (e.g.,
 * ConfigurationLayout) — this component just renders what it's
 * given.
 */
import { computed, onBeforeUnmount, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { PanelLeftClose, PanelLeftOpen, type LucideIcon } from 'lucide-vue-next'
import { useL2RailPreference } from '@/composables/useL2RailPreference'
import { useI18n } from '@/composables/useI18n'
import { PHONE_MAX_WIDTH, useIsPhone } from '@/composables/useIsPhone'

/* The composable's `t` shadows the v-for loop variable named `t`
 * lower in the template (`v-for="t in tabs"`), so destructure as
 * `i18n` and use `i18n.t(...)` here to keep the existing loop
 * variable name. */
const i18n = useI18n()

interface SidebarTab {
  to: string
  label: string
  icon?: LucideIcon
}

const props = defineProps<{
  tabs: SidebarTab[]
  /* Accessible label for the nav landmark — defaults to a generic one. */
  ariaLabel?: string
  /*
   * Optional L1 icon — when set, renders to the LEFT of the
   * phone-mode dropdown so the user can tell which top-level
   * section they're in even after the NavRail closes. Mirrors
   * the same prop on PageTabs; only L2 layouts pass this (L3
   * surfaces inherit no icon — only one cue per phone screen).
   * Pure visual; not interactive.
   */
  l1Icon?: LucideIcon
}>()

const route = useRoute()
const router = useRouter()

const activeTo = computed(() => {
  const matches = props.tabs.filter(
    (t) => route.path === t.to || route.path.startsWith(t.to + '/')
  )
  matches.sort((a, b) => b.to.length - a.to.length)
  return matches[0]?.to ?? props.tabs[0]?.to ?? ''
})

/* ---- Responsive breakpoints ---- */

/* Auto-collapse band: same upper bound as NavRail's
 * `COMPACT_BELOW` so both rails kick in together. */
const COMPACT_BELOW = 1280

const isPhone = useIsPhone()

/* ---- Collapse state (shared singleton via composable) ---- */

const { manualCollapsed, autoActive, autoOverride, toggle, clearAutoOverride } =
  useL2RailPreference()

/* Effective collapsed state for this render. Phone path doesn't
 * collapse — the dropdown takes over. Otherwise: per-visit
 * override wins; otherwise manual || auto (mirrors NavRail). */
const collapsed = computed(() => {
  if (isPhone.value) return false
  if (autoOverride.value !== null) return autoOverride.value
  return manualCollapsed.value || autoActive.value
})

/* ---- Resize-driven auto-collapse ---- */

function onResize() {
  const w = globalThis.window.innerWidth
  /* Auto-collapse band is `[PHONE_MAX_WIDTH, COMPACT_BELOW)` —
   * below that, phone takes over; at or above COMPACT_BELOW, the
   * rail expands. The composable consumes this flag and combines
   * with the user's manual preference + transient override. */
  autoActive.value = w >= PHONE_MAX_WIDTH && w < COMPACT_BELOW
}

onMounted(() => {
  globalThis.window.addEventListener('resize', onResize)
  onResize() /* prime initial state */
})
onBeforeUnmount(() => globalThis.window.removeEventListener('resize', onResize))

/* Reset transient override on route change — re-entering an
 * auto-active route should re-fire the auto rule fresh. Mirrors
 * the equivalent reset in `useRailPreference` (L1). */
watch(
  () => route.path,
  () => clearAutoOverride()
)

function onSelectChange(ev: Event) {
  const target = ev.target as HTMLSelectElement
  if (target.value && target.value !== route.path) {
    router.push(target.value)
  }
}
</script>

<template>
  <nav
    class="l2-sidebar"
    :class="{
      'l2-sidebar--phone': isPhone,
      'l2-sidebar--collapsed': collapsed,
    }"
    :aria-label="ariaLabel ?? i18n.t('Section navigation')"
  >
    <template v-if="isPhone">
      <span
        v-if="l1Icon"
        class="l2-sidebar__l1-icon"
        aria-hidden="true"
      >
        <component :is="l1Icon" :size="20" :stroke-width="2" />
      </span>
      <select
        class="l2-sidebar__select"
        :aria-label="ariaLabel ?? i18n.t('Section navigation')"
        :value="activeTo"
        @change="onSelectChange"
      >
        <option v-for="t in tabs" :key="t.to" :value="t.to">
          {{ t.label }}
        </option>
      </select>
    </template>
    <template v-else>
      <ul class="l2-sidebar__list">
        <li v-for="t in tabs" :key="t.to">
          <router-link
            :to="t.to"
            class="l2-sidebar__item"
            :class="{ 'l2-sidebar__item--active': activeTo === t.to }"
            :title="collapsed ? t.label : undefined"
          >
            <component
              :is="t.icon"
              v-if="t.icon"
              :size="18"
              :stroke-width="2"
              class="l2-sidebar__icon"
            />
            <span class="l2-sidebar__label">{{ t.label }}</span>
          </router-link>
        </li>
      </ul>
      <!--
        Manual collapse toggle — sits at the bottom of the sidebar,
        same relative position the L1 NavRail's toggle sits at
        (NavRail's chevron is between its nav list and its
        live-status footer; L2 has no footer so the toggle anchors
        at the very bottom). Row-shaped icon + label that mirrors
        `.l2-sidebar__item` so it visually fits the column. Icon
        + title both reflect the EFFECTIVE collapsed state
        (`collapsed`, which combines manual preference + auto rule
        + per-visit override). Hidden on phone.
      -->
      <button
        type="button"
        class="l2-sidebar__toggle"
        :title="collapsed ? i18n.t('Expand') : i18n.t('Collapse')"
        :aria-label="collapsed ? i18n.t('Expand sidebar') : i18n.t('Collapse sidebar')"
        :aria-expanded="!collapsed"
        @click="toggle"
      >
        <PanelLeftOpen v-if="collapsed" :size="18" :stroke-width="2" />
        <PanelLeftClose v-else :size="18" :stroke-width="2" />
        <span class="l2-sidebar__toggle-label">{{
          collapsed ? i18n.t('Expand') : i18n.t('Collapse')
        }}</span>
      </button>
    </template>
  </nav>
</template>

<style scoped>
.l2-sidebar {
  width: 220px;
  flex-shrink: 0;
  background: var(--tvh-bg-surface);
  border-right: 1px solid var(--tvh-border);
  overflow-y: auto;
  /* Aligns the sidebar's vertical extent with the main content
     within the same flex/grid row. */
  align-self: stretch;
  /* Flex-column: nav list takes the available space, the
     collapse toggle sits at the bottom (mirrors NavRail's
     three-pane shape: nav list — toggle — footer; L2 has no
     footer so the toggle is the bottom-most element). */
  display: flex;
  flex-direction: column;
  /* Smooth width transition when toggling collapsed/expanded —
     mirrors NavRail's transition feel. Phone-mode is gated above
     by `--phone` modifier and never animates the desktop width. */
  transition: width var(--tvh-transition);
}

.l2-sidebar--collapsed {
  width: 56px;
}

.l2-sidebar__list {
  list-style: none;
  margin: 0;
  /* Take all available space above the toggle — pushes the
     toggle to the bottom regardless of how short the nav list
     is. */
  flex: 1 1 auto;
  padding: var(--tvh-space-2) 0;
}

/*
 * Manual collapse toggle button. Mirrors `.l2-sidebar__item`'s
 * row shape (and L1 NavRail's `.nav-rail__toggle`) so it
 * visually fits the same column of clickable rows.
 */
.l2-sidebar__toggle {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 8px var(--tvh-space-3);
  margin: 2px var(--tvh-space-2);
  color: var(--tvh-text-muted);
  background: none;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: var(--tvh-text-lg);
  font-weight: 500;
  text-align: left;
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition);
}

.l2-sidebar__toggle:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.l2-sidebar__toggle:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.l2-sidebar__toggle-label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.l2-sidebar__item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 8px var(--tvh-space-3);
  margin: 2px var(--tvh-space-2);
  color: var(--tvh-text-muted);
  text-decoration: none;
  font-size: var(--tvh-text-lg);
  font-weight: 500;
  border-radius: var(--tvh-radius-sm);
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition);
}

.l2-sidebar__item:hover {
  background: color-mix(
    in srgb,
    var(--tvh-primary) var(--tvh-hover-strength),
    transparent
  );
  color: var(--tvh-text);
}

.l2-sidebar__item:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.l2-sidebar__item--active {
  background: color-mix(in srgb, var(--tvh-primary) 14%, var(--tvh-bg-surface));
  color: var(--tvh-text);
  font-weight: 600;
}

.l2-sidebar__icon {
  flex-shrink: 0;
  color: var(--tvh-text-muted);
}

.l2-sidebar__item--active .l2-sidebar__icon {
  color: var(--tvh-primary);
}

.l2-sidebar__label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* Collapsed-mode tweaks — icons centred, labels hidden. The
 * `:title` attribute on each item drives the hover tooltip
 * (touch-safe; no custom tooltip component required). */
.l2-sidebar--collapsed .l2-sidebar__item,
.l2-sidebar--collapsed .l2-sidebar__toggle {
  justify-content: center;
  margin: 2px var(--tvh-space-1);
  padding: 8px 0;
  gap: 0;
}

.l2-sidebar--collapsed .l2-sidebar__label,
.l2-sidebar--collapsed .l2-sidebar__toggle-label {
  display: none;
}

/* Phone — replace the vertical list with a select dropdown,
   matching PageTabs' phone behaviour. */
.l2-sidebar--phone {
  width: 100%;
  background: transparent;
  border-right: none;
  /* Match AppShell's non-full-bleed <main> padding on phone so
   * Configuration's L2 row (icon + dropdown) sits at the same
   * top + left position as PageTabs on DVR / EPG / Status.
   * Configuration is full-bleed at the AppShell level — the
   * outer <main> has padding: 0 — so the layout has to supply
   * the equivalent itself. AppShell's main uses
   * --tvh-space-2 8px top / --tvh-space-6 24px L+R + bottom; we
   * mirror top + L/R here. .config-layout__main below picks up
   * the same horizontal value so content aligns with the L2
   * row above. */
  margin-top: var(--tvh-space-2);
  padding: 0 var(--tvh-space-6);
  margin-bottom: var(--tvh-space-3);
  display: flex;
  /* Override the base `flex-direction: column` (the sidebar is
   * vertical at desktop sizes) so the L1 icon sits to the LEFT
   * of the dropdown on phone, matching PageTabs' affordance. */
  flex-direction: row;
  align-items: center;
  gap: var(--tvh-space-3);
}

/* L1 icon on phone — informational cue showing which top-level
 * section the user is currently in. Same lucide-vue-next icon
 * NavRail uses for the matching L1 item; flush left of the
 * dropdown. Mirrors PageTabs' .page-tabs__l1-icon. */
.l2-sidebar__l1-icon {
  flex-shrink: 0;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: var(--tvh-text);
}

.l2-sidebar__select {
  flex: 1;
  min-width: 0;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 8px var(--tvh-space-3);
  font: inherit;
  min-height: 40px;
}

.l2-sidebar__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
