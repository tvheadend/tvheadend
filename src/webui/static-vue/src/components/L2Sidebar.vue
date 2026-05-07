<script setup lang="ts">
/*
 * L2Sidebar — generic vertical-list navigation sidebar.
 *
 * Used by sectioned views that need a second-column nav surface
 * (Configuration is the first; future sections with deep hierarchies
 * may follow). Same data-shape as PageTabs (`{ to, label, icon }`)
 * so a future refactor can fold both into one component with a
 * `direction: 'horizontal' | 'vertical'` prop. Kept separate today
 * to keep the diff focused on the new use case.
 *
 * Desktop: vertical list of router-link tiles, NavRail-styled (icon
 * + label, hover background, active-pill highlight). Width fixed at
 * a comfortable text-readable size.
 *
 * Phone (<768px): collapses to a `<select>` dropdown above the main
 * content, mirroring PageTabs' phone idiom (ADR 0008). Hamburger
 * stays L1-only; sidebar IS L2 in select form.
 *
 * Active-tab matching uses the same longest-prefix logic as
 * PageTabs so deep routes (e.g., /configuration/dvb/adapters) keep
 * their L2 entry (DVB Inputs) visually active.
 *
 * Capability / uilevel filtering happens in the caller (e.g.,
 * ConfigurationLayout) — this component just renders what it's
 * given.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import type { LucideIcon } from 'lucide-vue-next'

interface SidebarTab {
  to: string
  label: string
  icon?: LucideIcon
}

const props = defineProps<{
  tabs: SidebarTab[]
  /* Accessible label for the nav landmark — defaults to a generic one. */
  ariaLabel?: string
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

const PHONE_MAX_WIDTH = 768
const isPhone = ref(
  globalThis.window !== undefined &&
    globalThis.window.innerWidth < PHONE_MAX_WIDTH
)
function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < PHONE_MAX_WIDTH
}
onMounted(() => globalThis.window.addEventListener('resize', checkPhone))
onBeforeUnmount(() =>
  globalThis.window.removeEventListener('resize', checkPhone)
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
    :class="{ 'l2-sidebar--phone': isPhone }"
    :aria-label="ariaLabel ?? 'Section navigation'"
  >
    <select
      v-if="isPhone"
      class="l2-sidebar__select"
      :value="activeTo"
      @change="onSelectChange"
    >
      <option v-for="t in tabs" :key="t.to" :value="t.to">
        {{ t.label }}
      </option>
    </select>
    <ul v-else class="l2-sidebar__list">
      <li v-for="t in tabs" :key="t.to">
        <router-link
          :to="t.to"
          class="l2-sidebar__item"
          :class="{ 'l2-sidebar__item--active': activeTo === t.to }"
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
}

.l2-sidebar__list {
  list-style: none;
  margin: 0;
  padding: var(--tvh-space-2) 0;
}

.l2-sidebar__item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 8px var(--tvh-space-3);
  margin: 2px var(--tvh-space-2);
  color: var(--tvh-text-muted);
  text-decoration: none;
  font-size: 14px;
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

/* Phone — replace the vertical list with a select dropdown,
   matching PageTabs' phone behaviour. */
.l2-sidebar--phone {
  width: 100%;
  background: transparent;
  border-right: none;
  padding: 0;
  margin-bottom: var(--tvh-space-3);
}

.l2-sidebar__select {
  width: 100%;
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
