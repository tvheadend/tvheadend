/*
 * useDvrEditor — singleton "open DVR entry editor" handle.
 *
 * Backs the AppShell-mounted `<IdnodeEditor>` instance that
 * overlays whatever view is currently shown. Today's only callers
 * are the EPG views (Timeline / Magazine / EpgEventDrawer) — they
 * used to `router.push({ name: 'dvr-upcoming', query: { editUuid }})`,
 * which forced a route change and lost the user's EPG context
 * (day picker, scroll position, hovered programme). Calling
 * `useDvrEditor().open(uuid)` opens the editor in place; closing
 * the drawer leaves the user where they started.
 *
 * The DVR list views (UpcomingView, FinishedView, FailedView,
 * RemovedView, AutorecsView, TimersView) keep their own per-view
 * `useEditorMode` instances — those carry richer state (createBase,
 * editList from the grid's effective columns, level-by-selection,
 * `@created` → `flipToEdit`) that doesn't fit a singleton's
 * "open existing uuid" shape. Migration of those views to this
 * singleton is captured as a BACKLOG row; would only pay off if a
 * cross-view summoning need (notification → editor, etc.) actually
 * lands.
 *
 * Module-level state — not Pinia. One nullable string + two
 * functions is too small for a store. The composable's `useDvrEditor`
 * export is just a typed handle over the module-level refs, mirroring
 * the pattern used by `useNowCursor` and friends.
 *
 * URL sync (router.replace, no history pollution) is gated to EPG
 * routes only. On `/vue/dvr/upcoming?editUuid=…`, the existing
 * `useEditorMode({ urlSync: true })` in UpcomingView already owns
 * the `editUuid` query param; if this composable also reacted to it
 * there, two editors would race to open. The gate keeps the two
 * mechanisms cleanly separated by route.
 *
 * Route-change auto-close — when the user navigates away (back
 * button is the realistic case; modal backdrop blocks normal nav
 * clicks), the editor closes. Avoids the modal "following" the
 * user to a different route with stale state.
 */
import { ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'

/* Module-level singleton state. The two watchers below are
 * registered exactly once when this module is first imported — by
 * AppShell.vue's `useDvrEditor()` call — and live for the app's
 * lifetime. Subsequent `useDvrEditor()` calls (from EPG callers)
 * just hand back the same handle. */
const editingUuid = ref<string | null>(null)

/* Vue-router composables (`useRoute`, `useRouter`) require a
 * component context, so the watchers are wired lazily on first
 * `useDvrEditor()` call rather than at module top-level. The
 * `wired` flag prevents re-wiring when the composable is called
 * from multiple components (EPG callers + AppShell mount). */
let wired = false

function isEpgRoute(path: string): boolean {
  /* Path-based gate. The EPG route tree mounts at `/epg/*`
   * (`router/index.ts:182-191`); anything outside that subtree
   * uses its own editor mechanism (DVR views via useEditorMode,
   * configuration via IdnodeConfigForm, etc.) and shouldn't have
   * its `editUuid` query param consumed by this composable. */
  return path === '/epg' || path.startsWith('/epg/')
}

function wireWatchers() {
  if (wired) return
  wired = true
  const route = useRoute()
  const router = useRouter()

  /* URL → editor: open when the query param appears or changes
   * AND we're on an EPG route. Equality guard short-circuits the
   * inverse update from the editor → URL watcher below. Mirrors
   * the pattern from `useEditorMode.ts:152-160`. */
  watch(
    () => [route.query.editUuid, route.path] as const,
    ([uuid, path]) => {
      if (!isEpgRoute(path)) return
      if (typeof uuid !== 'string' || !uuid) return
      if (editingUuid.value === uuid) return
      editingUuid.value = uuid
    },
    { immediate: true }
  )

  /* Editor → URL: keep the param in step on open and close, but
   * only when we're on an EPG route. On non-EPG routes the local
   * useEditorMode owns `editUuid`; we don't touch it. router.replace
   * (not push) so back-button history isn't polluted. */
  watch(editingUuid, (uuid) => {
    if (!isEpgRoute(route.path)) return
    const current = route.query.editUuid
    if (uuid === null) {
      if (!current) return
      const rest = { ...route.query }
      delete rest.editUuid
      router.replace({ query: rest }).catch(() => {})
      return
    }
    if (current === uuid) return
    router.replace({ query: { ...route.query, editUuid: uuid } }).catch(() => {})
  })

  /* Route-change auto-close. When the user navigates away (back
   * button, NavRail click that somehow bypasses the modal backdrop,
   * etc.), drop the editor. The drawer is a modal overlay — having
   * it survive a route change would be confusing UX (open editor on
   * EPG, navigate to Status, drawer is still there over Status). */
  watch(
    () => route.path,
    (newPath, oldPath) => {
      if (newPath === oldPath) return
      if (editingUuid.value !== null) editingUuid.value = null
    }
  )
}

export function useDvrEditor() {
  wireWatchers()

  function open(uuid: string) {
    editingUuid.value = uuid
  }

  function close() {
    editingUuid.value = null
  }

  return { editingUuid, open, close }
}
