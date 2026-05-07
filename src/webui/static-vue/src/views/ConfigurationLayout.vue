<script setup lang="ts">
/*
 * ConfigurationLayout — the L2 navigation scaffold for the
 * Configuration section. Shape mirrors DvrLayout's: filter the L2
 * tab list against access-store state, render an L2Sidebar (vertical
 * on desktop, select dropdown on phone), delegate to the active
 * child route.
 *
 * Per ADR 0008, four UI-affordance gates apply at the L2 / L3 level
 * mirroring ExtJS exactly:
 *   - `tvadapters` capability gates the entire DVB Inputs L2 entry
 *   - `caclient` capability gates CA Clients (L3 inside Users)
 *   - `timeshift` capability gates Timeshift (L3 inside Recording)
 *   - `uilevel >= advanced` gates the Debugging L2 entry
 *
 * The two L3-level capability gates apply inside their respective L2
 * layouts (UsersLayout, RecordingLayout) once those are built; today
 * only the L2-level ones are wired here.
 *
 * All Configuration endpoints require ACCESS_ADMIN — that's enforced
 * once at the parent route's `meta.permission = 'admin'`. Per-entry
 * gates here are UX/visibility, not security.
 */
import { computed } from 'vue'
import {
  Settings,
  Users,
  RadioTower,
  Tv,
  Activity,
  HardDrive,
  Bug,
  type LucideIcon,
} from 'lucide-vue-next'
import L2Sidebar from '@/components/L2Sidebar.vue'
import { useAccessStore } from '@/stores/access'
import { useCapabilitiesStore } from '@/stores/capabilities'
import type { UiLevel } from '@/types/access'

interface ConfigTab {
  to: string
  label: string
  icon: LucideIcon
  /* Hidden unless `access.uilevel` reaches the named level. */
  requiredLevel?: UiLevel
  /* Hidden unless `capabilities.has(name)` returns true. */
  requiredCapability?: string
}

const ALL_TABS: ConfigTab[] = [
  { to: '/configuration/general', label: 'General', icon: Settings },
  { to: '/configuration/users', label: 'Users', icon: Users },
  {
    to: '/configuration/dvb',
    label: 'DVB Inputs',
    icon: RadioTower,
    requiredCapability: 'tvadapters',
  },
  { to: '/configuration/channel-epg', label: 'Channel / EPG', icon: Tv },
  { to: '/configuration/stream', label: 'Stream', icon: Activity },
  { to: '/configuration/recording', label: 'Recording', icon: HardDrive },
  {
    to: '/configuration/debugging',
    label: 'Debugging',
    icon: Bug,
    requiredLevel: 'advanced',
  },
]

const access = useAccessStore()
const capabilities = useCapabilitiesStore()

/* `requiredLevel: 'advanced'` means "show at advanced OR expert" —
 * not strict equality. Mirrors ExtJS dvr.js:1180 where Debugging
 * shows for both `'advanced'` and `'expert'`. Treat the level as a
 * monotonic enum: basic < advanced < expert. */
const LEVEL_ORDER: Record<UiLevel, number> = {
  basic: 0,
  advanced: 1,
  expert: 2,
}

const tabs = computed(() =>
  ALL_TABS.filter((t) => {
    if (t.requiredCapability && !capabilities.has(t.requiredCapability))
      return false
    if (
      t.requiredLevel &&
      LEVEL_ORDER[access.uilevel] < LEVEL_ORDER[t.requiredLevel]
    )
      return false
    return true
  })
)
</script>

<template>
  <article class="config-layout">
    <L2Sidebar :tabs="tabs" aria-label="Configuration sub-section navigation" />
    <main class="config-layout__main">
      <router-view />
    </main>
  </article>
</template>

<style scoped>
.config-layout {
  display: flex;
  /*
   * `height: 100%` (mirroring DvrLayout's pattern) instead of
   * `flex: 1 1 auto` because the parent `<main>` isn't a flex
   * container — `flex: 1` here would be ignored and the layout
   * would shrink to its content height, leaving the L2 sidebar
   * stopping at the last item instead of extending to the
   * viewport bottom.
   */
  height: 100%;
  min-height: 0;
}

.config-layout__main {
  flex: 1 1 auto;
  min-width: 0;
  display: flex;
  flex-direction: column;
  padding: var(--tvh-space-4);
  overflow: auto;
}

/* Phone — sidebar collapses to a top-of-content select via L2Sidebar's
   own phone branch, so the layout stacks vertically rather than
   side-by-side. */
@media (max-width: 767px) {
  .config-layout {
    flex-direction: column;
  }
  .config-layout__main {
    padding: var(--tvh-space-3);
  }
}
</style>
