/*
 * useEditorMode — shared state + handlers for the Add/Edit/Delete
 * drawer pattern that DVR sub-views (and any future grid+editor
 * pair) repeat.
 *
 * Until this composable existed, every DVR sub-view that owned an
 * IdnodeEditor drawer (Upcoming, Autorecs, Timers — the ones with
 * a "create" path; plus Finished, Failed, Removed in their edit-
 * only variant) re-declared the same six pieces of state +
 * handlers:
 *
 *   - `editingUuid: Ref<string | null>`         which row is being edited
 *   - `creatingBase: Ref<string | null>`        non-null = create mode against this base
 *   - `gridRef: Ref<{ effectiveLevel } | null>` template-ref into IdnodeGrid for level pull-through
 *   - `editorLevel: ComputedRef<UiLevel>`       current level (grid → editor)
 *   - `editorList: ComputedRef<string>`         active field-list (edit vs create)
 *   - `openEditor / openCreate / closeEditor`   the three state-mutators
 *
 * Extracted because the same shape was repeated across UpcomingView,
 * AutorecsView, and TimersView (the create-mode variant; the
 * edit-only variant in Finished/Failed/Removed is smaller).
 *
 * Per-view differences:
 *   - `editList`     — caller-provided reactive (usually a `computed()`
 *                       that's admin-aware; differs per class).
 *   - `createList`   — caller-provided string (the field set used in
 *                       the Add dialog; usually narrower than editList).
 *   - `createBase`   — the idnode-class base path that openCreate()
 *                       writes into `creatingBase` (e.g. `'dvr/entry'`).
 *
 * Edit-only views omit `createBase` and `createList`; openCreate()
 * becomes a no-op for them and editorList just returns editList.
 *
 * Composable returns refs+handlers. Caller destructures so template
 * refs (`<IdnodeGrid ref="gridRef" />`) and emit handlers
 * (`@close="closeEditor"`) bind via the local script-setup scope.
 */
import { computed, ref, watch, type ComputedRef, type Ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import type { UiLevel } from '@/types/access'
import type { BaseRow } from '@/types/grid'

export interface EditorModeOptions {
  /**
   * Idnode-class base path. When set, `openCreate()` switches the
   * editor to create-mode against this base (e.g. `'dvr/entry'`).
   * Omit for edit-only views; `openCreate()` then no-ops.
   */
  createBase?: string
  /**
   * Reactive field-list for the Edit drawer. Typically a `computed()`
   * that's admin-aware; differs per idnode class. Required.
   */
  editList: Ref<string> | ComputedRef<string>
  /**
   * Field-list for the Create drawer. Usually narrower than editList
   * (no `enabled`, no admin-only fields, etc.). Required when
   * `createBase` is set; otherwise unused.
   */
  createList?: string
  /**
   * When true, sync the editor's open-state with an `editUuid` query
   * parameter on the current route. URL → editor: opening a route
   * with `?editUuid=<uuid>` opens the Edit drawer for that UUID
   * (used by the EPG drawer to deep-link into a DVR entry; also
   * makes editor URLs shareable / refresh-stable). Editor → URL:
   * the param is added when the drawer opens and dropped when it
   * closes, so the URL stays a faithful reflection of editor state.
   * Caller must be inside a route component (composable invokes
   * `useRoute()` / `useRouter()`).
   */
  urlSync?: boolean
}

export interface EditorModeHandle {
  /** Non-null while the Edit drawer is open. UUID of the row being edited. */
  editingUuid: Ref<string | null>
  /** Non-null while the Create drawer is open. Mirrors options.createBase. */
  creatingBase: Ref<string | null>
  /**
   * Non-null while a multi-subclass create drawer is open. Drives the
   * IdnodeEditor's per-subclass `idnode/class?name=<x>` metadata fetch
   * AND the `class=<x>` body param the create POST needs for classes
   * whose create endpoint dispatches on a class string
   * (mpegts/network/create). Null for single-class create flows (DVR
   * Upcoming, Configuration → Users) and for edit-only views — both
   * paths leave it untouched.
   */
  creatingSubclass: Ref<string | null>
  /**
   * Non-null while a parent-scoped create drawer is open. Drives the
   * IdnodeEditor's `parentScoped` prop directly: the bundle carries
   * the per-flow class-fetch endpoint, create endpoint, and params
   * (typically a parent UUID). Used for create endpoints whose mux /
   * child class is implied by a parent entity rather than picked
   * directly — e.g. Mux creation: the Mux subclass is implicit in
   * the chosen Network's type, so the client sends the network UUID
   * to `mpegts/network/mux_class` + `mpegts/network/mux_create`.
   * Null for the other modes; mutually exclusive with
   * `creatingSubclass`.
   */
  creatingParentScope: Ref<{
    classEndpoint: string
    createEndpoint: string
    params: Record<string, unknown>
  } | null>
  /**
   * Template ref to bind on the IdnodeGrid: `<IdnodeGrid ref="gridRef" />`.
   * The grid's exposed `effectiveLevel` flows to the editor's `level`
   * prop via `editorLevel` below — single source of truth so the
   * editor renders the same view-level the grid is filtered to.
   */
  gridRef: Ref<{ effectiveLevel: UiLevel } | null>
  editorLevel: ComputedRef<UiLevel>
  /**
   * Active field-list — switches between createList (in create mode)
   * and editList (otherwise). When createList is missing, falls back
   * to editList in both modes (defensive — edit-only views never enter
   * create mode anyway).
   */
  editorList: ComputedRef<string>
  /** Open Edit drawer for the single selected row. No-op for 0/many rows or row missing uuid. */
  openEditor: (selected: BaseRow[]) => void
  /**
   * Open Create drawer. No-op when createBase wasn't provided. The
   * optional `subclass` argument is for multi-subclass create
   * endpoints (Networks) where the create POST dispatches on a
   * class string the user picked first via IdnodePickClassDialog.
   * Single-class consumers ignore the argument.
   */
  openCreate: (subclass?: string) => void
  /**
   * Open Create drawer in parent-scoped mode (Muxes: pick a network
   * → create a mux against that network). The bundle is forwarded
   * verbatim to IdnodeEditor's `parentScoped` prop, which uses the
   * `classEndpoint` for metadata fetch and the `createEndpoint` for
   * save POST, with `params` merged into both. No-op when
   * createBase wasn't provided.
   */
  openCreateForParent: (scope: {
    classEndpoint: string
    createEndpoint: string
    params: Record<string, unknown>
  }) => void
  /** Close the drawer regardless of which mode (Edit or Create) was open. */
  closeEditor: () => void
  /**
   * Flip the open drawer from create mode to edit mode against a
   * freshly-created entry's UUID. Wired to IdnodeEditor's `created`
   * emit, which fires after a successful create round-trip when the
   * Apply button is used. Edit-only views never trigger this path
   * (no `createBase` ⇒ no create round-trip ⇒ editor never emits
   * `created`); leaving the method exposed regardless keeps view
   * wiring uniform across create-capable and edit-only consumers.
   */
  flipToEdit: (uuid: string) => void
}

export function useEditorMode(opts: EditorModeOptions): EditorModeHandle {
  const editingUuid = ref<string | null>(null)
  const creatingBase = ref<string | null>(null)
  const creatingSubclass = ref<string | null>(null)
  const creatingParentScope = ref<{
    classEndpoint: string
    createEndpoint: string
    params: Record<string, unknown>
  } | null>(null)
  const gridRef = ref<{ effectiveLevel: UiLevel } | null>(null)

  const editorLevel = computed<UiLevel>(() => gridRef.value?.effectiveLevel ?? 'basic')

  const editorList = computed(() => {
    if (creatingBase.value && opts.createList) return opts.createList
    return opts.editList.value
  })

  function openEditor(selected: BaseRow[]) {
    if (selected.length !== 1) return
    const uuid = selected[0].uuid
    if (typeof uuid === 'string') editingUuid.value = uuid
  }

  function openCreate(subclass?: string) {
    if (!opts.createBase) return
    creatingBase.value = opts.createBase
    creatingSubclass.value = subclass ?? null
    creatingParentScope.value = null
  }

  function openCreateForParent(scope: {
    classEndpoint: string
    createEndpoint: string
    params: Record<string, unknown>
  }) {
    if (!opts.createBase) return
    creatingBase.value = opts.createBase
    creatingSubclass.value = null
    creatingParentScope.value = scope
  }

  function closeEditor() {
    editingUuid.value = null
    creatingBase.value = null
    creatingSubclass.value = null
    creatingParentScope.value = null
  }

  function flipToEdit(uuid: string) {
    creatingBase.value = null
    creatingSubclass.value = null
    creatingParentScope.value = null
    editingUuid.value = uuid
  }

  if (opts.urlSync) {
    const route = useRoute()
    const router = useRouter()

    /* URL → editor: open when the query param appears or changes.
     * The equality guard short-circuits the inverse update from the
     * editor → URL watcher below, so the two sides don't ping-pong. */
    watch(
      () => route.query.editUuid,
      (uuid) => {
        if (typeof uuid !== 'string' || !uuid) return
        if (editingUuid.value === uuid) return
        editingUuid.value = uuid
      },
      { immediate: true }
    )

    /* Editor → URL: keep the param in step both when the editor
     * opens (any path — EPG drawer, row click, etc.) and when it
     * closes. Uses router.replace so back-button history isn't
     * polluted with editor open/close steps. */
    watch(editingUuid, (uuid) => {
      const current = route.query.editUuid
      if (uuid === null) {
        if (!current) return
        const rest = { ...route.query }
        delete rest.editUuid
        router.replace({ query: rest })
        return
      }
      if (current === uuid) return
      router.replace({ query: { ...route.query, editUuid: uuid } })
    })
  }

  return {
    editingUuid,
    creatingBase,
    creatingSubclass,
    creatingParentScope,
    gridRef,
    editorLevel,
    editorList,
    openEditor,
    openCreate,
    openCreateForParent,
    closeEditor,
    flipToEdit,
  }
}
