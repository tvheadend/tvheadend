<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import {
  Calendar,
  Video,
  Settings,
  Activity,
  Info,
  House,
  ExternalLink,
  LogIn,
  LogOut,
  PanelLeftClose,
  PanelLeftOpen,
  type LucideIcon,
} from 'lucide-vue-next'
import { useAccessStore } from '@/stores/access'
import { cometClient } from '@/api/comet'
import { useRailPreference } from '@/composables/useRailPreference'
import { useI18n } from '@/composables/useI18n'
import { useIsPhone } from '@/composables/useIsPhone'
import { useStickyBottom } from '@/composables/useStickyBottom'
import { serverUrl } from '@/utils/base'
import RailInfoArea from './RailInfoArea.vue'
import CommandPaletteTrigger from './CommandPaletteTrigger.vue'

const { t } = useI18n()

/*
 * Cross-UI logo reference. Bound as an expression (not a literal
 * `src`) so Vite's Vue plugin doesn't try to bundle the asset —
 * the URL is served at runtime by tvheadend's existing /static
 * handler from the ExtJS bundle (`src/webui/static/img/logo.png`,
 * registered in `webui_init()` in `src/webui/webui.c`). Once the
 * ExtJS UI retires, copy the asset into `static-vue/src/assets/`
 * and replace this with a Vite-imported reference.
 */
const logoUrl = serverUrl('static/img/logo.png')

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
const toggleTitle = computed(() =>
  props.compact ? t('Expand navigation') : t('Collapse navigation'),
)

/*
 * Top-level nav items, grouped into sections. Order mirrors the
 * existing ExtJS rootTabPanel order (src/webui/static/app/tvheadend.js);
 * the section split — everyday TV use vs system administration — is
 * the Vue UI's own (see ADR 0016). Item labels reuse the exact English
 * strings ExtJS uses, so existing translations cover them (brief §6.1,
 * §9.1); the two section headers are new strings.
 *
 * Permission requirement lives on the route's meta.permission (see
 * router/index.ts) — single source of truth shared with the router
 * guard. This blueprint just declares display order, label, and icon.
 */
interface Blueprint {
  /* Internal SPA route — resolved against the router for its path and
   * meta.permission. Mutually exclusive with `href`. */
  routeName?: string
  /* External, non-SPA destination (the legacy ExtJS UI). Rendered as a
   * plain anchor: a full-page navigation OUT of the Vue app, not a
   * router push. No permission gate — the target enforces its own ACL
   * server-side (`/extjs.html` is ACCESS_WEB_INTERFACE, extjs.c:242). */
  href?: string
  label: string
  icon: LucideIcon
}

interface NavSection {
  id: string
  label: string
  items: Blueprint[]
}

/* Shown above the sections without a header — the Home dashboard is
 * the task-oriented front door (ADR 0017), part of neither cluster. */
const NAV_LEADING: Blueprint[] = [
  { routeName: 'dashboard', label: t('Home'), icon: House },
]

const NAV_SECTIONS: NavSection[] = [
  {
    id: 'tv',
    label: t('TV'),
    items: [
      { routeName: 'epg', label: t('Electronic Program Guide'), icon: Calendar },
      { routeName: 'dvr', label: t('Digital Video Recorder'), icon: Video },
    ],
  },
  {
    id: 'server',
    label: t('Server'),
    items: [
      { routeName: 'configuration', label: t('Configuration'), icon: Settings },
      { routeName: 'status', label: t('Status'), icon: Activity },
    ],
  },
]

/* Shown below the sections without a header — About is general
 * information, neither everyday-use nor administration (ADR 0016).
 * Classic UI follows it: a full-page jump to the legacy ExtJS
 * interface, available while both UIs ship side by side. */
const NAV_UNGROUPED: Blueprint[] = [
  { routeName: 'about', label: t('About'), icon: Info },
  { href: serverUrl('extjs.html'), label: t('Classic UI'), icon: ExternalLink },
]

/*
 * Resolve a blueprint entry against the router so we get the actual
 * route path AND its meta.permission in one place.
 */
interface ResolvedItem {
  /* Exactly one of `to` (internal route path) or `href` (external
   * URL) is set, mirroring the Blueprint split. */
  to?: string
  href?: string
  label: string
  icon: LucideIcon
  permission?: string
}

interface ResolvedSection {
  id: string
  /* null for the trailing ungrouped block — rendered without a header
   * and without group semantics. */
  label: string | null
  items: ResolvedItem[]
}

function resolveItem(bp: Blueprint): ResolvedItem {
  /* External link — no router resolution, no permission gate (the
   * target page enforces its own access server-side). */
  if (bp.href) {
    return { href: bp.href, label: bp.label, icon: bp.icon }
  }
  const route = router.resolve({ name: bp.routeName! })
  return {
    to: route.path,
    label: bp.label,
    icon: bp.icon,
    permission: route.meta.permission,
  }
}

function isItemVisible(item: ResolvedItem): boolean {
  return !item.permission || access.has(item.permission as 'admin' | 'dvr')
}

/*
 * Sections with their items resolved and permission-filtered. A
 * section whose items are all permission-hidden is dropped entirely,
 * so its header never appears over nothing (e.g. a non-admin user has
 * no System section). The ungrouped items, if any, are appended as a
 * final header-less group.
 */
const railGroups = computed<ResolvedSection[]>(() => {
  const groups: ResolvedSection[] = []

  /* Home — leading, ungrouped, header-less; sits above the labelled
   * sections (ADR 0017). */
  const leading = NAV_LEADING.map(resolveItem).filter(isItemVisible)
  if (leading.length > 0)
    groups.push({ id: 'home', label: null, items: leading })

  for (const s of NAV_SECTIONS) {
    const items = s.items.map(resolveItem).filter(isItemVisible)
    if (items.length > 0) groups.push({ id: s.id, label: s.label, items })
  }

  const ungrouped = NAV_UNGROUPED.map(resolveItem).filter(isItemVisible)
  if (ungrouped.length > 0)
    groups.push({ id: 'general', label: null, items: ungrouped })

  return groups
})

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
 * Scroll-shadow indicators for the nav list. When the menu is taller
 * than the viewport, `useStickyBottom` tracks whether it is scrolled
 * away from the top / bottom edge; the wrapper's fade gradients (see
 * `.nav-rail__nav-wrap`) then signal "more this way" — the same
 * pattern LogView and the grids use. Works in the phone drawer too,
 * where the rail is a slide-in panel.
 */
const navScrollEl = ref<HTMLElement | null>(null)
/* Tight slop (2 px) so the gradient shows for any real overflow —
 * a short viewport (notably phone landscape) can overflow the rail
 * by only a little, which the composable's generous default slop
 * would otherwise swallow. */
const { isAtTop: navAtTop, isAtBottom: navAtBottom } = useStickyBottom(
  navScrollEl,
  { slopPx: 2 },
)

/*
 * Phone-only "closed drawer" focus exclusion. The closed rail is slid
 * off-screen via `transform: translateX(-100%)` (purely visual), so its
 * links would otherwise stay in the tab order and screen-reader output.
 * `inert` removes element + descendants from focus and the
 * accessibility tree. Desktop renders the rail always — never inert.
 */
const isPhone = useIsPhone()

/* The live-status footer (login / storage / time) is rendered by
 * the shared `RailInfoArea` component — same instance type also
 * lives in TopBar at phone width where the rail's own footer is
 * hidden. Logic, formatting, and Comet wiring live in that
 * component, not here. */

/* Login / logout affordance — exactly one of the two buttons
 * renders, or neither. The `Access.username` field drives the
 * split:
 *   - non-empty username → user is authenticated → show LOGOUT
 *   - empty username + anonymous (not admin) → unauthenticated
 *     user with limited rights → show LOGIN so they have a path
 *     in (otherwise the only workaround is logging in via the
 *     legacy UI to seed the browser's cached credentials)
 *   - empty username + admin (server started with `--noacl`, or
 *     an anonymous-admin ACL) → no auth backend in use; neither
 *     button is meaningful, render nothing
 *   - pre-auth (`loaded` still false) → render nothing until the
 *     first `accessUpdate` arrives, to avoid a flash
 *
 * The two clicks behave differently:
 *   - `/logout` (`src/webui/webui.c:177-218`) is a top-level
 *     navigation: it renders an HTML page with a 401-returning
 *     link that clears the browser's cached HTTP basic/digest
 *     credentials. (Classic's IE-only
 *     `document.execCommand("ClearAuthenticationCache")` branch
 *     is skipped — the brief doesn't target legacy IE.)
 *   - `/login` (`src/webui/webui.c:164-175`) is fetched as a
 *     background request, NOT navigated to. The fetch trips a
 *     401 + `WWW-Authenticate`, which makes the browser pop its
 *     native auth prompt; on credential entry the request
 *     retries and the server's 302 redirect to `/` is followed
 *     and discarded (we only care that auth succeeded). We then
 *     reload the current `/gui` page so the Comet connection
 *     reconnects with the cached Authorization header and the
 *     access store re-hydrates with the real username. This
 *     keeps the user on `/gui`; a top-level navigation to
 *     `/login` would land them on whatever UI the server has
 *     configured at `/` (extjs / classic), and ExtJS would also
 *     auto-launch its setup wizard on a fresh install. The
 *     fetch path also recovers from "stuck anonymous" — when a
 *     prior admin-gated request already triggered a prompt and
 *     the browser cached credentials but the access store still
 *     reads anonymous (Comet's `accessUpdate` is one-shot at
 *     WS-connect time), the fetch succeeds immediately and the
 *     reload picks up the now-valid state.
 */
const showLogout = computed(
  () => typeof access.data?.username === 'string' && access.data.username.length > 0,
)
const showLogin = computed(
  () =>
    access.loaded &&
    !showLogout.value &&
    !access.data?.admin,
)
function onLogoutClick() {
  globalThis.window.location.href = serverUrl('logout')
}
async function onLoginClick() {
  try {
    const res = await fetch(serverUrl('login'), {
      method: 'GET',
      credentials: 'include',
      cache: 'no-store',
    })
    if (!res.ok) return
    /* `/login` succeeded — the browser now has cached HTTP
     * credentials. The existing comet mailbox was created
     * anonymously and the server's `comet_access_update()` only
     * runs on mailbox creation (or when a state-changing
     * endpoint like the wizard explicitly broadcasts) — so we
     * drop the boxid and reconnect, which forces a fresh
     * mailbox + a fresh `accessUpdate` carrying the now-authed
     * identity. No page reload needed. */
    cometClient.reset()
  } catch {
    /* Network error or browser-cancelled prompt — leave the
     * user on the current page; the Login button stays
     * available for another try. */
  }
}
</script>

<template>
  <aside
    class="nav-rail"
    :class="{ 'nav-rail--open': open, 'nav-rail--compact': compact }"
    :inert="!open && isPhone"
    :aria-label="t('Main navigation')"
  >
    <!--
      Brand row — replaces the standalone TopBar on desktop / tablet.
      Holds the logo + "Tvheadend" wordmark; in compact mode the
      wordmark hides and the logo centres in the 56 px column. Same
      48 px height as the removed TopBar so the rail's nav items
      start at the same y-coordinate as before. Logo path mirrors
      the cross-UI reference from TopBar (served by the existing
      `/static/...` handler from the ExtJS bundle; copy into
      `static-vue/src/assets/` once ExtJS retires).
    -->
    <div class="nav-rail__brand">
      <img :src="logoUrl" alt="" class="nav-rail__brand-logo" />
      <span class="nav-rail__brand-title">Tvheadend</span>
    </div>
    <!--
      Command-palette trigger row. Visible affordance for
      pointer / touch users who can't (or don't want to) press
      Cmd-K. Pill in expanded mode shows "Search… ⌘K"; compact
      mode collapses it to a magnifier icon button that fits the
      56 px column. Hidden on phone — the TopBar carries the
      icon variant there instead, alongside the hamburger.
    -->
    <div v-if="!isPhone" class="nav-rail__search">
      <CommandPaletteTrigger :variant="compact ? 'icon' : 'pill'" />
    </div>
    <!--
      Scroll-shadow wrapper: when the nav list is taller than the
      viewport, fade gradients at the top / bottom edges signal there
      is more to scroll to (the same pattern LogView and the grids
      use). The wrapper is the positioning context; the inner <nav>
      is the scroll region.
    -->
    <div
      v-if="railReady"
      class="nav-rail__nav-wrap"
      :class="{
        'nav-rail__nav-wrap--has-top': !navAtTop,
        'nav-rail__nav-wrap--has-bottom': !navAtBottom,
      }"
    >
      <nav ref="navScrollEl" class="nav-rail__nav">
        <!--
          Each section is a labelled cluster of nav links (ADR 0016).
          The trailing ungrouped block (About) reuses the same wrapper
          with `label: null` — no header, no group role — so the
          compact-mode divider styling applies to it uniformly.
        -->
        <div
          v-for="section in railGroups"
          :key="section.id"
          class="nav-rail__section"
          :role="section.label ? 'group' : undefined"
          :aria-label="section.label || undefined"
        >
          <p
            v-if="section.label"
            class="nav-rail__section-header"
            aria-hidden="true"
          >{{ section.label }}</p>
          <template v-for="item in section.items" :key="item.to ?? item.href">
            <!--
              External destination (Classic UI) — a plain anchor so the
              browser performs a full-page navigation OUT of the SPA. A
              RouterLink would try to match `/extjs.html` against the Vue
              routes and 404 inside the app instead of leaving it.
            -->
            <a
              v-if="item.href"
              :href="item.href"
              :title="compact ? item.label : undefined"
              class="nav-item"
            >
              <component :is="item.icon" :size="18" :stroke-width="2" />
              <span class="nav-item__label">{{ item.label }}</span>
            </a>
            <RouterLink
              v-else
              :to="item.to!"
              :title="compact ? item.label : undefined"
              class="nav-item"
              active-class="nav-item--active"
              @click="$emit('navigate')"
            >
              <component :is="item.icon" :size="18" :stroke-width="2" />
              <span class="nav-item__label">{{ item.label }}</span>
            </RouterLink>
          </template>
        </div>
      </nav>
    </div>
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
      Live-status footer — `RailInfoArea` carries the actual
      rendering and Comet wiring; this wrapper just provides the
      footer's column layout + top border. Hidden at phone width
      because the same content moves to the TopBar's right slot
      there (always in compact form), keeping the slide-in
      drawer focused on navigation only.
    -->
    <div class="nav-rail__footer">
      <RailInfoArea :compact="compact" />
    </div>
    <!--
      Login / logout — a SIBLING of the info-area footer, not
      nested inside it. The info area is semantically read-only
      status and can be admin-hidden via `config.info_area`;
      this row is an action that must stay reachable independent
      of that setting. Exactly one of the two buttons renders, or
      neither — see `showLogin` / `showLogout` for the gates.
      Visible on phone too (the rail drawer's bottom) because the
      info-area block moves to TopBar on phone, leaving the
      drawer otherwise action-less.
    -->
    <div v-if="showLogout" class="nav-rail__logout-row">
      <button
        type="button"
        class="nav-rail__logout"
        :title="compact ? t('Logout') : undefined"
        :aria-label="t('Logout')"
        @click="onLogoutClick"
      >
        <LogOut :size="18" :stroke-width="2" />
        <span class="nav-rail__logout-label">{{ t('Logout') }}</span>
      </button>
    </div>
    <div v-else-if="showLogin" class="nav-rail__logout-row">
      <button
        type="button"
        class="nav-rail__logout nav-rail__login"
        :title="compact ? t('Login') : undefined"
        :aria-label="t('Login')"
        @click="onLoginClick"
      >
        <LogIn :size="18" :stroke-width="2" />
        <span class="nav-rail__logout-label">{{ t('Login') }}</span>
      </button>
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
  /* The rail itself never scrolls — the nav list (`.nav-rail__nav`)
   * is the scroll region, so the brand row and the info-area footer
   * stay pinned when the viewport is too short for every item. */
  overflow: hidden;
}

.nav-rail__brand {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  /* 48 px — matches the removed TopBar so the rail's nav items
   * begin at the same y-coordinate as before, preserving keyboard
   * focus muscle memory at the top-left corner. */
  height: var(--tvh-topbar-height);
  padding: 0 var(--tvh-space-4);
  border-bottom: 1px solid var(--tvh-border);
  flex-shrink: 0;
}

.nav-rail__brand-logo {
  height: 28px;
  width: auto;
}

.nav-rail__brand-title {
  font-weight: 600;
  font-size: var(--tvh-text-xl); /* @snap-from: 15px */
  letter-spacing: -0.01em;
  color: var(--tvh-text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/*
 * Command-palette trigger row — sits between the brand row and
 * the scrolling nav list. Padding matches the brand row's
 * horizontal padding so the pill aligns visually with the logo.
 * Compact mode (56px rail) drops to icon padding-zero so the
 * icon centres.
 */
.nav-rail__search {
  flex-shrink: 0;
  padding: var(--tvh-space-2) var(--tvh-space-4);
}

.nav-rail--compact .nav-rail__search {
  padding: var(--tvh-space-2);
  display: flex;
  justify-content: center;
}

/*
 * Scroll-shadow wrapper — the flex child that owns the rail's
 * vertical space. `position: relative` + `overflow: hidden` make it
 * the positioning context for the edge fade gradients; the inner
 * <nav> is the actual scroll region. `min-height: 0` lets it shrink
 * below its content height so the nav scrolls instead of pushing the
 * toggle + info footer off a short viewport.
 */
.nav-rail__nav-wrap {
  position: relative;
  flex: 1;
  min-height: 0;
  overflow: hidden;
}

/*
 * Edge scroll-shadow gradients — pseudo-elements over the top /
 * bottom of the scroll region, shown when there is content past the
 * viewport in that direction (modifier classes bound to
 * useStickyBottom's isAtTop / isAtBottom). Uses the shared
 * `--tvh-scroll-fade` tint — the same scroll-shadow token the grids
 * and EPG views use — so the cue stays clearly visible against the
 * rail surface.
 */
.nav-rail__nav-wrap::before,
.nav-rail__nav-wrap::after {
  content: '';
  position: absolute;
  left: 0;
  right: 0;
  height: 24px;
  pointer-events: none;
  opacity: 0;
  transition: opacity 150ms ease-out;
  z-index: 1;
}

.nav-rail__nav-wrap::before {
  top: 0;
  background: linear-gradient(to bottom, var(--tvh-scroll-fade) 0%, transparent 100%);
}

.nav-rail__nav-wrap::after {
  bottom: 0;
  background: linear-gradient(to top, var(--tvh-scroll-fade) 0%, transparent 100%);
}

.nav-rail__nav-wrap--has-top::before {
  opacity: 1;
}

.nav-rail__nav-wrap--has-bottom::after {
  opacity: 1;
}

.nav-rail__nav {
  height: 100%;
  overflow-y: auto;
  padding: var(--tvh-space-2) 0;
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-2);
}

/*
 * A nav section — a labelled cluster of nav items (ADR 0016). The
 * trailing ungrouped block (About) reuses this class without a header,
 * so the compact-mode divider rule applies to it too.
 */
.nav-rail__section {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.nav-rail__section-header {
  margin: 0;
  /* Left-aligned with the nav items' icon column: their margin
   * (space-2) + padding (space-4). */
  padding: var(--tvh-space-1) calc(var(--tvh-space-2) + var(--tvh-space-4));
  font-size: var(--tvh-text-xs);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
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
  /* Pinned below the scrolling nav list (see `.nav-rail__nav`). */
  flex-shrink: 0;
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

/*
 * Logout row — sits below the info-area footer, separated by
 * its own top border so it reads as a distinct action region
 * (not part of the read-only status block above). Same button
 * chrome shape as `.nav-rail__toggle`: icon + label, hover
 * tint via primary-mix. Hidden via v-if when no username — see
 * `showLogout` computed.
 */
.nav-rail__logout-row {
  flex-shrink: 0;
  border-top: 1px solid var(--tvh-border);
  padding: var(--tvh-space-2) var(--tvh-space-2);
}

.nav-rail__logout {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  width: 100%;
  height: var(--tvh-nav-item-height);
  padding: 0 var(--tvh-space-4);
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

.nav-rail__logout:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
  color: var(--tvh-text);
}

.nav-rail__logout:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.nav-rail__logout-label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.nav-rail__footer {
  flex-shrink: 0;
  border-top: 1px solid var(--tvh-border);
  /* Vertical padding matches the compact-mode footer's
   * `var(--tvh-space-2)` so the total footer block is the same
   * height in expanded and collapsed modes. The compact rule
   * below only overrides horizontal padding (centring the
   * icon+value stacks in the 56 px column); the canonical
   * vertical sizing lives here. */
  padding: var(--tvh-space-2) var(--tvh-space-4);
  display: flex;
  flex-direction: column;
  gap: var(--tvh-space-1);
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

  /* The brand row and live-status footer move to the TopBar at
   * phone width. Inside the slide-in drawer the focus stays on
   * navigation only — no duplicated logo / wordmark / info chips
   * competing with the page-bar versions for the user's eye. */
  .nav-rail__brand,
  .nav-rail__footer {
    display: none;
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
  /* Smooth width animation when collapsing / expanding. Mirrors
   * L2Sidebar's transition. Scoped to ≥768 px so the phone-mode
   * hamburger transform (above) stays the only animation
   * touching the rail at narrow widths. */
  .nav-rail {
    transition: width var(--tvh-transition);
  }

  .nav-rail--compact {
    width: 56px;
  }

  .nav-rail--compact .nav-rail__brand {
    padding: 0;
    justify-content: center;
    gap: 0;
  }

  .nav-rail--compact .nav-rail__brand-title {
    display: none;
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

  /*
   * Compact mode: no room for section header text in the 56 px
   * column — hide it, and let a hairline rule above each section
   * (bar the first, which the brand-row border already separates)
   * carry the grouping instead (ADR 0016).
   */
  .nav-rail--compact .nav-rail__section-header {
    display: none;
  }

  .nav-rail--compact .nav-rail__section:not(:first-child) {
    border-top: 1px solid var(--tvh-border);
    padding-top: var(--tvh-space-2);
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
   * Compact footer: only the horizontal padding changes — the
   * 56 px column needs the icon+value stacks centered without
   * the expanded mode's 16 px side gutter. Vertical padding +
   * per-row min-height live on the base `.nav-rail__footer` /
   * `.nav-rail__footer-row` rules so the footer's total height
   * is the same in both modes; the chevron toggle's y-axis
   * position stays stable across collapse / expand.
   */
  .nav-rail--compact .nav-rail__footer {
    padding-left: 0;
    padding-right: 0;
  }

  /* Compact logout — same icon-only treatment the nav-items and
   * toggle button get in compact mode. Label collapses, button
   * centres the icon in the 56 px column, title-attribute
   * tooltip on hover (already declared on the button when
   * `compact` is true). */
  .nav-rail--compact .nav-rail__logout-row {
    padding-left: 0;
    padding-right: 0;
  }

  .nav-rail--compact .nav-rail__logout {
    padding: 0;
    margin: 0 var(--tvh-space-1);
    justify-content: center;
    gap: 0;
  }

  .nav-rail--compact .nav-rail__logout-label {
    display: none;
  }
}

.nav-rail__footer-stack {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
  font-size: var(--tvh-text-xs);
  font-variant-numeric: tabular-nums;
  /* Same min-height as `.nav-rail__footer-row` so collapsed
   * stacks and expanded rows occupy the same vertical real
   * estate. */
  min-height: 32px;
}
</style>
