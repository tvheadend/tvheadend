<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import {
  Calendar,
  Video,
  Settings,
  Activity,
  Info,
  HardDrive,
  Clock,
  UserCircle2,
  PanelLeftClose,
  PanelLeftOpen,
  type LucideIcon,
} from 'lucide-vue-next'
import { useAccessStore } from '@/stores/access'
import { useNowCursor } from '@/composables/useNowCursor'
import { useRailPreference } from '@/composables/useRailPreference'

const props = defineProps<{
  open: boolean
  /*
   * Icons-only collapsed mode. Driven by AppShell from the OR of
   * (a) the user's stored manual preference and (b) the auto rule
   * at mid widths (768–1279px) for routes that declare meta.hasL2.
   * Labels move to title-attribute tooltips. Below 768px the
   * existing phone drawer takes over and `compact` is ignored (the
   * @media block in CSS shadows it).
   */
  compact?: boolean
}>()
defineEmits<{ navigate: [] }>()

const router = useRouter()
const access = useAccessStore()
const { toggle: toggleRail } = useRailPreference()

/*
 * The chevron icon and title reflect the visible (effective) compact
 * state. Click always produces a visible change: on routes where the
 * auto rule is firing, the composable's toggle reaches for a transient
 * per-view override (so the user can expand the rail mid-Configuration
 * even though auto wants it compact); on regular routes, it flips the
 * persistent manual preference. See useRailPreference's header for
 * the full state machine.
 */
const toggleIcon = computed(() => (props.compact ? PanelLeftOpen : PanelLeftClose))
const toggleTitle = computed(() => (props.compact ? 'Expand navigation' : 'Collapse navigation'))

/*
 * Top-level nav items mirror the existing ExtJS rootTabPanel order
 * (src/webui/static/app/tvheadend.js). Labels are the exact English
 * strings ExtJS uses, so existing translations cover them when the
 * Vue i18n surface is wired (brief §6.1, §9.1).
 *
 * Permission requirement lives on the route's meta.permission (see
 * router/index.ts) — single source of truth shared with the router
 * guard. This blueprint just declares display order, label, and icon.
 */
interface Blueprint {
  routeName: string
  label: string
  icon: LucideIcon
}

const NAV_BLUEPRINT: Blueprint[] = [
  { routeName: 'epg', label: 'Electronic Program Guide', icon: Calendar },
  { routeName: 'dvr', label: 'Digital Video Recorder', icon: Video },
  { routeName: 'configuration', label: 'Configuration', icon: Settings },
  { routeName: 'status', label: 'Status', icon: Activity },
  { routeName: 'about', label: 'About', icon: Info },
]

/*
 * Resolve each blueprint entry against the router so we get the actual
 * route path AND its meta.permission in one place.
 */
interface ResolvedItem {
  to: string
  label: string
  icon: LucideIcon
  permission?: string
}

const allItems = computed<ResolvedItem[]>(() =>
  NAV_BLUEPRINT.map((bp) => {
    const route = router.resolve({ name: bp.routeName })
    return {
      to: route.path,
      label: bp.label,
      icon: bp.icon,
      permission: route.meta.permission,
    }
  })
)

const visibleItems = computed<ResolvedItem[]>(() =>
  allItems.value.filter(
    (item) => !item.permission || access.has(item.permission as 'admin' | 'dvr')
  )
)

/*
 * Loading state strategy (matches the existing ExtJS UI's behaviour):
 * don't render the rail until access has loaded. The first accessUpdate
 * via Comet typically arrives in <500ms after page load; a 5-second
 * timeout falls through to optimistic rendering for the rare case where
 * Comet is slow (e.g. WebSocket blocked, long-poll fallback's first
 * response can take ~10s).
 *
 * Without the timeout, a user on a network that blocks ws:// would
 * stare at an empty rail for up to 10s. With it, after 5s we render
 * everything (server-side ACL still enforces actual permissions on any
 * API calls, so this is purely a UX safety net).
 */
const accessTimedOut = ref(false)
onMounted(() => {
  setTimeout(() => {
    if (!access.loaded) accessTimedOut.value = true
  }, 5000)
})

const railReady = computed(() => access.loaded || accessTimedOut.value)

/*
 * Phone-only "closed drawer" focus exclusion. The closed rail is slid
 * off-screen via `transform: translateX(-100%)` (purely visual), so its
 * links would otherwise stay in the tab order and screen-reader output.
 * `inert` removes element + descendants from focus and the
 * accessibility tree. Desktop renders the rail always — never inert.
 */
const PHONE_MAX_WIDTH = 768
const isPhone = ref(
  globalThis.window !== undefined && globalThis.window.innerWidth < PHONE_MAX_WIDTH
)
function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < PHONE_MAX_WIDTH
}
onMounted(() => globalThis.window.addEventListener('resize', checkPhone))
onBeforeUnmount(() => globalThis.window.removeEventListener('resize', checkPhone))

/*
 * Disk-free display in the rail footer.
 *
 * `freediskspace` is pushed in every Comet `accessUpdate` (see
 * comet_access_update in src/webui/comet.c — emitted only when the
 * user has DVR access), so we get live updates as recordings land or
 * are cleaned up. Falls back to em-dash before the first update
 * arrives, or for non-DVR users where the field is absent.
 *
 * Binary suffixes (TiB / GiB / MiB / KiB) match the existing ExtJS
 * formatter in src/webui/static/app/tvheadend.js setDiskSpace().
 *
 * Subscriptions and Recording counters are not surfaced here: their
 * underlying APIs are admin-only or require server-side push
 * infrastructure that doesn't exist yet. Those counters could ride
 * the same plumbing as a future `whoami` endpoint.
 */
function formatBytes(b: number): string {
  if (b >= 1024 ** 4) return `${(b / 1024 ** 4).toFixed(1)} TiB`
  if (b >= 1024 ** 3) return `${(b / 1024 ** 3).toFixed(1)} GiB`
  if (b >= 1024 ** 2) return `${(b / 1024 ** 2).toFixed(1)} MiB`
  if (b >= 1024) return `${(b / 1024).toFixed(1)} KiB`
  return `${b} B`
}

/* Compact-mode footer abbreviation — single-letter suffix, no decimal
 * except for TiB where the digit matters (the difference between
 * 1.2T and 1.8T is 600 GiB). Width-budget is the 56px compact rail;
 * `1.2T` and `999G` both fit comfortably. The full text remains
 * available via the `Disk free 32.4 GiB` tooltip. */
function formatBytesAbbr(b: number): string {
  if (b >= 1024 ** 4) return `${(b / 1024 ** 4).toFixed(1)}T`
  if (b >= 1024 ** 3) return `${Math.round(b / 1024 ** 3)}G`
  if (b >= 1024 ** 2) return `${Math.round(b / 1024 ** 2)}M`
  if (b >= 1024) return `${Math.round(b / 1024)}K`
  return `${b}B`
}

/* ---- info_area driven footer items ----
 *
 * `config.info_area` (`src/config.c:2045-2052`) is a CSV of up to
 * three keys from { 'login', 'storage', 'time' } controlling which
 * items appear in the rail's footer and in what order. Default is
 * all three. The setting arrives via the `accessUpdate` Comet
 * payload (`comet.c:204-205`); a fresh value lands either at WS
 * connect or after the General → Save reload triggered by the
 * RELOAD_FIELDS allowlist in ConfigGeneralBaseView. Storage values
 * stay live via the separate `diskspaceUpdate` notification (~30 s
 * server cadence, wired in stores/access.ts). */
const KNOWN_INFO_ITEMS = new Set(['login', 'storage', 'time'])
const DEFAULT_INFO_ITEMS = ['login', 'storage', 'time']

const infoItems = computed<string[]>(() => {
  const raw = access.data?.info_area
  if (!raw) return DEFAULT_INFO_ITEMS
  const parsed = raw
    .split(',')
    .map((s) => s.trim())
    .filter((s) => KNOWN_INFO_ITEMS.has(s))
  return parsed.length > 0 ? parsed : DEFAULT_INFO_ITEMS
})

/* ---- Login ---- */
const username = computed(() => access.data?.username ?? '')
const usernameInitial = computed(() => (username.value[0] ?? '·').toUpperCase())

/* ---- Storage ---- */
function fmtMaybe(v: number | undefined, fmt: (b: number) => string): string {
  return typeof v === 'number' && v >= 0 ? fmt(v) : '—'
}

const storageWide = computed(() => ({
  free: fmtMaybe(access.data?.freediskspace, formatBytesAbbr),
  used: fmtMaybe(access.data?.useddiskspace, formatBytesAbbr),
  total: fmtMaybe(access.data?.totaldiskspace, formatBytesAbbr),
}))

const freePct = computed<string>(() => {
  const f = access.data?.freediskspace
  const t = access.data?.totaldiskspace
  if (typeof f !== 'number' || typeof t !== 'number' || t <= 0) return '—'
  return `${Math.round((f / t) * 100)}%`
})

const storageTooltip = computed(() => {
  const free = fmtMaybe(access.data?.freediskspace, formatBytes)
  const used = fmtMaybe(access.data?.useddiskspace, formatBytes)
  const total = fmtMaybe(access.data?.totaldiskspace, formatBytes)
  return `Storage — Free: ${free} · Used: ${used} · Total: ${total}`
})

/* ---- Time ----
 *
 * Reuses `useNowCursor` (already aligned to wall-clock :00 / :30
 * ticks). HH:MM display only visibly changes at :00 of each minute
 * — :30 ticks are silent, which is fine. */
const { now } = useNowCursor()

const dayTimeWide = computed(() => {
  const d = new Date(now.value * 1000)
  return {
    day: new Intl.DateTimeFormat(undefined, { weekday: 'short' }).format(d),
    time: new Intl.DateTimeFormat(undefined, {
      hour: '2-digit',
      minute: '2-digit',
      hour12: false,
    }).format(d),
    tooltip: new Intl.DateTimeFormat(undefined, {
      dateStyle: 'full',
      timeStyle: 'short',
    }).format(d),
  }
})
</script>

<template>
  <aside
    class="nav-rail"
    :class="{ 'nav-rail--open': open, 'nav-rail--compact': compact }"
    :inert="!open && isPhone"
    aria-label="Main navigation"
  >
    <nav v-if="railReady" class="nav-rail__nav">
      <RouterLink
        v-for="item in visibleItems"
        :key="item.to"
        :to="item.to"
        :title="compact ? item.label : undefined"
        class="nav-item"
        active-class="nav-item--active"
        @click="$emit('navigate')"
      >
        <component :is="item.icon" :size="18" :stroke-width="2" />
        <span class="nav-item__label">{{ item.label }}</span>
      </RouterLink>
    </nav>
    <div v-else class="nav-rail__loading" aria-busy="true" />
    <!--
      Manual collapse toggle — sits between the nav list and the
      live-status footer so the disk-free row stays at the bottom of
      the rail. Hidden on phone (the rail is a slide-in drawer there;
      compact mode doesn't apply). The icon and title both reflect
      the EFFECTIVE compact state (props.compact, which combines
      manual preference and auto-rule); click flips the user's stored
      preference. On auto-active routes the visible state won't
      change until the user navigates away — see useRailPreference's
      header for the full semantics.
    -->
    <button
      v-if="railReady && !isPhone"
      type="button"
      class="nav-rail__toggle"
      :title="toggleTitle"
      :aria-label="toggleTitle"
      @click="toggleRail"
    >
      <component :is="toggleIcon" :size="18" :stroke-width="2" />
      <span class="nav-rail__toggle-label">{{ toggleTitle }}</span>
    </button>
    <!--
      Live-status footer — driven by `config.info_area` (CSV of up to
      three keys from login / storage / time, server default
      "login,storage,time"). Items render in the configured order;
      each has a wide form (full-width row) and a compact form
      (stacked icon + short value). Storage values come live via
      Comet `diskspaceUpdate` (~30 s cadence); time ticks via
      `useNowCursor`; login is server-pushed at WS connect.
    -->
    <div class="nav-rail__footer">
      <template v-for="item in infoItems" :key="item">
        <!-- LOGIN -->
        <template v-if="item === 'login'">
          <div v-if="!compact" class="nav-rail__footer-row" :title="username">
            <UserCircle2 :size="14" :stroke-width="2" />
            <span class="nav-rail__footer-text">
              Logged in as <strong>{{ username || '—' }}</strong>
            </span>
          </div>
          <div v-else class="nav-rail__footer-stack" :title="username || ''">
            <UserCircle2 :size="16" :stroke-width="2" />
            <span class="muted">{{ usernameInitial }}</span>
          </div>
        </template>

        <!-- STORAGE -->
        <template v-else-if="item === 'storage'">
          <div v-if="!compact" class="nav-rail__footer-row" :title="storageTooltip">
            <HardDrive :size="14" :stroke-width="2" />
            <span class="nav-rail__footer-text">
              Free <strong>{{ storageWide.free }}</strong> Used
              <strong>{{ storageWide.used }}</strong> Total
              <strong>{{ storageWide.total }}</strong>
            </span>
          </div>
          <div v-else class="nav-rail__footer-stack" :title="storageTooltip">
            <HardDrive :size="16" :stroke-width="2" />
            <span class="muted">{{ freePct }}</span>
          </div>
        </template>

        <!-- TIME -->
        <template v-else-if="item === 'time'">
          <div v-if="!compact" class="nav-rail__footer-row" :title="dayTimeWide.tooltip">
            <Clock :size="14" :stroke-width="2" />
            <span class="nav-rail__footer-text">
              {{ dayTimeWide.day }} <strong>{{ dayTimeWide.time }}</strong>
            </span>
          </div>
          <div v-else class="nav-rail__footer-stack" :title="dayTimeWide.tooltip">
            <Clock :size="16" :stroke-width="2" />
            <span class="muted">{{ dayTimeWide.time }}</span>
          </div>
        </template>
      </template>
    </div>
  </aside>
</template>

<style scoped>
.nav-rail {
  width: var(--tvh-rail-width);
  flex-shrink: 0;
  background: var(--tvh-bg-surface);
  border-right: 1px solid var(--tvh-border);
  display: flex;
  flex-direction: column;
  overflow-y: auto;
}

.nav-rail__nav {
  flex: 1;
  padding: var(--tvh-space-2) 0;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.nav-rail__loading {
  flex: 1;
  /* Empty placeholder during the brief pre-access window. Could be
     populated with shimmer skeletons in a follow-up if the wait
     becomes noticeable. */
}

.nav-item {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  height: var(--tvh-nav-item-height);
  padding: 0 var(--tvh-space-4);
  margin: 0 var(--tvh-space-2);
  color: var(--tvh-text);
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition);
  text-decoration: none;
}

.nav-item:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.nav-item--active {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-active-strength), transparent);
  font-weight: 500;
}

.nav-item__label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/*
 * Manual collapse toggle button. Mirrors .nav-item's overall shape so
 * it visually fits the same column of clickable rows; the only
 * differences are it has no `to` (just a click handler) and uses
 * background: none so the absence of a route highlight doesn't make
 * it look like a permanently inactive nav entry.
 */
.nav-rail__toggle {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  height: var(--tvh-nav-item-height);
  padding: 0 var(--tvh-space-4);
  margin: 0 var(--tvh-space-2);
  color: var(--tvh-text-muted);
  background: none;
  border: 0;
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    color var(--tvh-transition);
}

.nav-rail__toggle:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.nav-rail__toggle-label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.nav-rail__footer {
  border-top: 1px solid var(--tvh-border);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  font-size: 13px;
  color: var(--tvh-text-muted);
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
}

.nav-rail__footer-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  /* Single-line clip — long usernames or stretched storage values
   * shouldn't push the layout. The tooltip carries the full text. */
  overflow: hidden;
}

.nav-rail__footer-text {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-variant-numeric: tabular-nums;
}

.nav-rail__footer-row strong {
  color: var(--tvh-text);
  font-weight: 600;
}

@media (max-width: 767px) {
  .nav-rail {
    position: absolute;
    top: 0;
    left: 0;
    bottom: 0;
    z-index: 10;
    transform: translateX(-100%);
    transition: transform var(--tvh-transition);
    box-shadow: 4px 0 12px rgba(0, 0, 0, 0.1);
  }

  .nav-rail--open {
    transform: translateX(0);
  }
}

/*
 * Compact (icons-only) mode — applied at mid widths (768–1279px) when
 * the active route has its own L2 sidebar, so the L1 + L2 + content
 * trio fits without crowding. Labels become title-attribute tooltips
 * on the link, the disk-free footer is hidden (re-add as an icon +
 * tooltip in a follow-up if missed). Below 768px the phone block
 * above takes over and this class is effectively ignored.
 */
@media (min-width: 768px) {
  .nav-rail--compact {
    width: 56px;
  }

  .nav-rail--compact .nav-item {
    padding: 0;
    margin: 0 var(--tvh-space-1);
    justify-content: center;
    gap: 0;
  }

  .nav-rail--compact .nav-item__label {
    display: none;
  }

  .nav-rail--compact .nav-rail__toggle {
    padding: 0;
    margin: 0 var(--tvh-space-1);
    justify-content: center;
    gap: 0;
  }

  .nav-rail--compact .nav-rail__toggle-label {
    display: none;
  }

  /*
   * Compact footer: tighten horizontal padding so the icon+value
   * pair centers in the 56px column, and reduce vertical padding
   * so the stacked content (icon over value) lands in roughly the
   * same total block height as the expanded "Disk free 32.4 GiB"
   * row. Keeping height stable preserves the chevron toggle's
   * y-axis position when the user collapses/expands manually —
   * the requirement that motivated showing disk-free in compact
   * mode at all.
   */
  .nav-rail--compact .nav-rail__footer {
    padding: var(--tvh-space-2) 0;
  }
}

.nav-rail__footer-stack {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  font-size: 11px;
  font-variant-numeric: tabular-nums;
}
</style>
