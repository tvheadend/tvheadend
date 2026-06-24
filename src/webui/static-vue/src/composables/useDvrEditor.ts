// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useDvrEditor — singleton "open DVR entry editor" handle.
 *
 * Backs the AppShell-mounted `<IdnodeEditor>` instance that
 * overlays whatever view is currently shown, so opening a DVR
 * entry from the EPG keeps the EPG context (day picker, scroll
 * position) intact. The DVR list views keep their own per-view
 * `useEditorMode` instances — those carry richer state
 * (createBase, editList from the grid's effective columns,
 * level-by-selection, `@created` → `flipToEdit`) that doesn't
 * fit a singleton's "open existing uuid" shape.
 *
 * Module-level state — not Pinia. One nullable string + two
 * functions is too small for a store. The composable's `useDvrEditor`
 * export is just a typed handle over the module-level refs, mirroring
 * the pattern used by `useNowCursor` and friends.
 *
 * URL sync (router.replace, no history pollution) is gated to EPG
 * routes only. On `/gui/dvr/upcoming?editUuid=…`, the existing
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
import { effectScope, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'

/* Module-level singleton state. The watchers below are registered
 * exactly once on the first `useDvrEditor()` call — typically
 * AppShell.vue's — and live for the app's lifetime. Subsequent
 * `useDvrEditor()` calls (from EPG callers) just hand back the
 * same handle. */
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
  /* useRoute()/useRouter() need the calling component's injection
   * context, but the objects they return are app-level and outlive
   * that component — safe to capture here and use from a detached
   * scope. */
  const route = useRoute()
  const router = useRouter()

  /* Detached effectScope: the first caller mounts inside AppShell,
   * which App.vue v-ifs away on wizard routes. Watchers created in
   * that component's own scope would be disposed with it while
   * `wired` stayed true, leaving the remounted shell with dead
   * watchers (no URL sync, no auto-close). The detached scope is
   * never disposed, so they genuinely live for the app's
   * lifetime. */
  effectScope(true).run(() => {
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
  })
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
