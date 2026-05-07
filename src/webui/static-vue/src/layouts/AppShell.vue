<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch, watchEffect } from 'vue'
import { useRoute } from 'vue-router'
import NavRail from '@/components/NavRail.vue'
import TopBar from '@/components/TopBar.vue'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import ConfirmDialog from 'primevue/confirmdialog'
import Toast from 'primevue/toast'
import { HelpCircle } from 'lucide-vue-next'
import { useRailPreference } from '@/composables/useRailPreference'
import { usePageTitle } from '@/composables/usePageTitle'
import { useDvrEditor } from '@/composables/useDvrEditor'
import { useAccessStore } from '@/stores/access'

/* Mount the document.title manager once, here at the persistent
 * root, so it survives every navigation. See usePageTitle.ts. */
usePageTitle()

/* Singleton DVR-entry editor — overlays whatever view is currently
 * shown. EPG callers (TimelineView / MagazineView / EpgEventDrawer)
 * call `useDvrEditor().open(uuid)` instead of routing to the DVR
 * list page; the drawer slides in over the EPG view, leaving the
 * user's day picker / scroll position / hover state intact when
 * closed. The composable owns URL sync (router.replace, EPG-routes
 * only) and route-change auto-close. */
const { editingUuid, close: closeDvrEditor } = useDvrEditor()
const access = useAccessStore()
/* Union of the field sets the per-view DVR editors use
 * (UpcomingView.vue:164-174 + FinishedView.vue:75-79 + the analogous
 * FailedView list) so this singleton renders meaningfully regardless
 * of whether the EPG-clicked entry is upcoming, finished, or failed.
 * The server returns only the props that exist on the loaded entry,
 * so unused fields just don't render. */
const dvrEditList =
  'enabled,disp_title,disp_extratext,episode_disp,channel,start,start_extra,' +
  'stop,stop_extra,pri,uri,config_name,playcount,owner,creator,comment,' +
  'retention,removal'

const railOpen = ref(false)
const route = useRoute()

function toggleRail() {
  railOpen.value = !railOpen.value
}

function closeRail() {
  railOpen.value = false
}

/*
 * "Full-bleed" layouts manage their own padding so the inner layout
 * (e.g. ConfigurationLayout's L2 sidebar) can sit flush against
 * NavRail's right edge instead of floating in a strip of page-bg
 * gap. Opt in via `meta.fullBleed: true` on the route. Required for
 * layouts that introduce a second-column nav surface; standard
 * page-padded views (EPG / DVR / Status) don't set the flag and
 * keep the comfortable margin.
 */
const fullBleed = computed(() => !!route.meta?.fullBleed)

/*
 * Mid-width L1 collapse. When a route declares `meta.hasL2`, the
 * layout already owns a second-column sidebar — so on screens
 * narrower than 1280px we collapse the L1 NavRail to icons-only
 * (56px) to keep two nav columns + content readable. Above 1280px
 * the rail stays at full width. Below 768px the existing phone
 * drawer takes over and `compact` is moot — NavRail's @media block
 * shadows it.
 *
 * Reactive viewport tracking lives here (rather than inside NavRail)
 * because AppShell already orchestrates layout decisions across the
 * shell — keeping the rule next to fullBleed makes the policy easy
 * to read, and NavRail stays a dumb presentational component that
 * just consumes the boolean.
 */
const COMPACT_BELOW = 1280
const PHONE_BELOW = 768

const viewportWidth = ref(globalThis.window?.innerWidth ?? 1920)
function onResize() {
  viewportWidth.value = globalThis.window.innerWidth
}
onMounted(() => globalThis.window.addEventListener('resize', onResize))
onBeforeUnmount(() => globalThis.window.removeEventListener('resize', onResize))

/*
 * Effective compact-rail rule combines three pieces of state from
 * the rail-preference composable:
 *
 *   manualCollapsed   user's persistent preference (localStorage).
 *   autoActive        derived right here — the auto rule's "wants
 *                     compact right now" signal, written into the
 *                     composable so its toggle function can see it.
 *   autoOverride      transient per-view override; non-null when the
 *                     user clicked the chevron while autoActive was
 *                     true. Reset on every route change below.
 *
 * Rule:
 *   < 768px           expanded (phone hamburger drawer takes over)
 *   override !== null override
 *   otherwise         manual || autoActive
 *
 * Full rationale and the case-by-case walkthrough live in
 * useRailPreference.ts's header.
 */
const { manualCollapsed, autoActive, autoOverride, clearAutoOverride } = useRailPreference()

watchEffect(() => {
  autoActive.value =
    !!route.meta?.hasL2 && viewportWidth.value >= PHONE_BELOW && viewportWidth.value < COMPACT_BELOW
})

/* Reset the per-view override on every route change so re-entering
 * an auto-active route always re-fires the auto rule fresh —
 * matches the original Commit-13 spec ("for configuration we keep
 * to auto collapse"). Includes Configuration sub-tab navigation
 * (e.g., /configuration/general → /configuration/users); each is a
 * fresh visit. */
watch(() => route.path, clearAutoOverride)

const compactRail = computed(() => {
  if (viewportWidth.value < PHONE_BELOW) return false
  if (autoOverride.value !== null) return autoOverride.value
  return manualCollapsed.value || autoActive.value
})
</script>

<template>
  <a class="skip-link" href="#main-content">Skip to main content</a>
  <div class="app-shell">
    <TopBar @toggle-rail="toggleRail" />
    <div class="app-shell__body">
      <NavRail :open="railOpen" :compact="compactRail" @navigate="closeRail" />
      <main
        id="main-content"
        class="app-shell__main"
        :class="{ 'app-shell__main--full-bleed': fullBleed }"
      >
        <RouterView />
      </main>
      <div v-if="railOpen" class="app-shell__scrim" aria-hidden="true" @click="closeRail" />
    </div>

    <!--
      Singleton-instance overlays. Both PrimeVue services are
      registered in main.ts; their visual instances mount once here
      so any consumer that calls the confirm or toast composable
      shares the same DOM root. Default Aura styling picks up the
      tvh CSS-variable tokens via styles/primevue.css.

      The icon slot on ConfirmDialog injects a Lucide HelpCircle —
      a neutral question-mark icon that identifies the dialog as a
      confirmation prompt without claiming a severity. The
      destructive signal lives on the Yes button's danger severity
      (red fill) via the per-call ask-options flag. Using Lucide
      for the icon keeps the app on a single icon package —
      primeicons (the conventional PrimeVue path) would add a font
      file and a second visual style.
    -->
    <ConfirmDialog>
      <template #icon>
        <HelpCircle :size="22" :stroke-width="2" />
      </template>
    </ConfirmDialog>
    <Toast position="top-right" />
    <!-- Singleton DVR-entry editor. PrimeVue's Drawer auto-teleports
         to <body>, so the mount point's location in this template
         only matters logically; the actual DOM renders at body level
         and overlays everything regardless of the wrapper z-index. -->
    <IdnodeEditor
      :uuid="editingUuid"
      :level="access.uilevel"
      :list="dvrEditList"
      title="Edit Recording"
      @close="closeDvrEditor"
    />
  </div>
</template>

<style scoped>
.app-shell {
  display: flex;
  flex-direction: column;
  height: 100vh;
  overflow: hidden;
  background: var(--tvh-bg-page);
}

.app-shell__body {
  display: flex;
  flex: 1;
  min-height: 0;
  position: relative;
}

.app-shell__main {
  flex: 1;
  overflow: auto;
  padding: var(--tvh-space-6);
}

/*
 * Full-bleed mode for layouts with their own internal nav columns
 * (Configuration's L2 sidebar). The layout sits flush against
 * NavRail's right edge — its own internal panes manage padding so
 * content stays comfortable.
 */
.app-shell__main--full-bleed {
  padding: 0;
}

/* Mobile scrim — clicking outside the open rail closes it */
.app-shell__scrim {
  position: absolute;
  inset: 0;
  background: rgba(0, 0, 0, 0.4);
  z-index: 9;
}

@media (min-width: 768px) {
  .app-shell__scrim {
    display: none;
  }
}
</style>
