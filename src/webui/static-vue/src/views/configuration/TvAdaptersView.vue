<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * TvAdaptersView — PoC for the tree-as-content pattern (ADR 0008).
 *
 * The data here is genuinely tree-shaped: physical DVB adapters are
 * tvh_hardware_t instances, each adapter has a set of frontends and
 * (optionally) CA slots as children — modelled in C as the idclass
 * `ic_get_childs` callback (linuxdvb_adapter.c:89-106). The ExtJS UI
 * surfaces this via `api/hardware/tree`, which we consume here.
 *
 * Loading is lazy per node: the initial fetch returns the top-level
 * adapter list; expanding an adapter triggers a follow-up fetch with
 * `?uuid=<adapter-uuid>` to retrieve its children. PrimeVue's Tree
 * component fires `@node-expand`; we handle the fetch and mutate the
 * node's `children` array reactively.
 *
 * Click a leaf → opens the existing IdnodeEditor drawer. The leaf's
 * idnode class varies (linuxdvb_adapter / linuxdvb_frontend / etc.);
 * IdnodeEditor's `list` prop is omitted so the server returns all
 * fields for that class. Tighter per-class lists could be hardcoded
 * later (matching dvr.js's elist pattern), but for the PoC the full
 * field set is genuinely useful — admins inspecting hardware want to
 * see signal levels, modulation params, error counts, etc.
 *
 * Tree expand state is persisted to `localStorage` under
 * `tvh-config:dvb:adapters:expand` and restored on the next visit:
 * the restore replays the lazy child fetches for each still-present
 * node, reusing the comet-refresh walk.
 *
 * Nodes whose class has the read-only `active` property (adapters,
 * frontends, CA slots, satconf entries) get a green / red dot next
 * to the label, as in the classic tree (tvadapters.js). The value is
 * read on each fetch, so the tree reloads after a save in the editor
 * drawer as well as on the `hardware` notification.
 */
import { onMounted, onBeforeUnmount, ref, computed, watch } from 'vue'
import Tree from 'primevue/tree'
import type { TreeExpandedKeys, TreePassThroughOptions, TreeSelectionKeys } from 'primevue/tree'
import type { TreeNode } from 'primevue/treenode'
import { Circle } from 'lucide-vue-next'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import { useI18n } from '@/composables/useI18n'
import { readStoredJson, writeStoredJson } from '@/utils/storage'

const { t } = useI18n()

/* The hardware/tree endpoint returns this shape (extracted from
 * api_input.c + ExtJS's idnode_tree consumption). Each node has a
 * uuid (the idnode UUID), a text label, an optional `children` array
 * (omitted when not yet fetched), and idnode metadata we mostly
 * ignore here — only `uuid`, the human label, the structural
 * `leaf` flag and the `active` entry of `params` are used. */
interface HardwareNode {
  uuid: string
  text?: string
  /* Some payload variants put the human label in `name`; tolerate both. */
  name?: string
  /* Structural leaf hint from the server's `idnode_is_leaf()` —
   * emitted as u32 by `api_idnode_tree` (api_idnode.c:549, :565).
   * Truthy when the node's idclass has no `ic_get_childs` callback
   * in its inheritance chain, i.e. this KIND of node can never
   * have children regardless of runtime state. Lets the client
   * suppress the expand caret without a probing lazy load. */
  leaf?: number | boolean
  /* When children have been loaded, this is an array. Absent when not
   * yet fetched. Empty array means "fetched, no children" (terminal
   * leaf — render without expand caret). */
  children?: HardwareNode[]
  /* Every property of the node's class with its value
   * (idnode_serialize0, idnode.c:1542). */
  params?: Array<{ id?: unknown; value?: unknown }>
  /* Free-form idnode metadata. We don't need it here; passed through
   * to IdnodeEditor implicitly via the uuid lookup. */
  [key: string]: unknown
}

/* The read-only `active` property (PO_RDONLY | PO_NOSAVE | PO_NOUI),
 * a PT_BOOL serialised as JSON true / false: an enabled frontend or
 * CA slot, an adapter with at least one of those. Undefined when the
 * class has no such property, so no marker is drawn for it. */
function activeOf(h: HardwareNode): boolean | undefined {
  const value = h.params?.find((p) => p?.id === 'active')?.value
  return typeof value === 'boolean' ? value : undefined
}

/* PrimeVue Tree expects `key`, `label`, `children`, plus `leaf?` to
 * suppress the expand caret on terminal nodes. We map from the
 * server's HardwareNode shape into this. The `data` slot carries the
 * uuid back through to the click handler and the active state to
 * the node template. */
function toTreeNode(h: HardwareNode): TreeNode {
  /* If the server sent a populated children array, recurse; otherwise
   * present the node as expandable (children load lazily) — UNLESS
   * it's an empty array, which the server uses to mean "no kids". */
  const hasKnownChildren = Array.isArray(h.children)
  const children = hasKnownChildren ? (h.children as HardwareNode[]).map(toTreeNode) : undefined
  /* Three signals collapse into one `leaf` decision:
   *   1. Server's structural `leaf` flag — when truthy, this node's
   *      idclass has no `ic_get_childs` callback (frontends, CA
   *      slots, …). Definitive "no caret"; no probing lazy load
   *      needed. Was previously ignored, so every node got a
   *      caret even when the server knew it had nothing to drill
   *      into.
   *   2. Children fetched and empty — runtime confirmation that a
   *      class which COULD have children doesn't in this instance.
   *   3. Otherwise — children unknown for a class that might have
   *      them; keep the caret so the lazy expand can fetch. */
  const serverLeaf = h.leaf === true || h.leaf === 1
  const fetchedEmpty = hasKnownChildren && children!.length === 0
  return {
    key: h.uuid,
    label: (h.text ?? h.name ?? h.uuid) as string,
    leaf: serverLeaf || fetchedEmpty,
    children,
    data: { uuid: h.uuid, active: activeOf(h) },
  }
}

function nodeActive(node: TreeNode): boolean | undefined {
  return (node.data as { active?: boolean } | undefined)?.active
}

function activeLabel(active: boolean): string {
  return active ? t('Active') : /* i18n: new string */ t('Inactive')
}

/* PrimeVue sets the treeitem's `aria-label` to the bare node label,
 * which hides the marker from screen readers. The `node` pass-through
 * is bound after it (TreeNode.vue), so the state is appended here. */
const treePt: TreePassThroughOptions = {
  node: ({ context }) => {
    const node = context?.node
    if (!node) return {}
    const active = nodeActive(node)
    const label = node.label ?? ''
    return { 'aria-label': active === undefined ? label : `${label}, ${activeLabel(active)}` }
  },
}

const nodes = ref<TreeNode[]>([])
const loading = ref(false)
const error = ref<Error | null>(null)
const expandedKeys = ref<TreeExpandedKeys>({})
const selectionKeys = ref<TreeSelectionKeys>({})
const editingUuid = ref<string | null>(null)

/* ---- Expand-state persistence ----
 *
 * `expandedKeys` is in-memory only, so without this the tree resets to
 * fully collapsed every time the view unmounts (route change). Persist
 * the expanded UUIDs on every change and restore them on mount — see
 * `restoreExpansion`, which also replays the lazy child fetches so a
 * restored node actually shows its children. */
const EXPAND_STORAGE_KEY = 'tvh-config:dvb:adapters:expand'

function readSavedExpansion(): string[] {
  /* Corrupt JSON or storage unavailable (private mode) — start fresh. */
  const parsed = readStoredJson(EXPAND_STORAGE_KEY, (v): v is unknown[] =>
    Array.isArray(v),
  )
  if (!parsed) return []
  return parsed.filter((k): k is string => typeof k === 'string')
}

function persistExpansion() {
  /* Storage unavailable / over quota — expansion just won't persist. */
  const keys = Object.keys(expandedKeys.value).filter((k) => expandedKeys.value[k])
  writeStoredJson(EXPAND_STORAGE_KEY, keys)
}

/* Persist on every expand / collapse: PrimeVue's v-model emits a fresh
 * object on each toggle, and the comet refresh + on-mount restore
 * reassign `expandedKeys` too — all caught here. */
watch(expandedKeys, persistExpansion)

/* ---- Live refresh on adapter hot-plug ----
 *
 * hardware/tree is a point-in-time snapshot: an adapter plugged in
 * (or removed) after mount stays invisible until the view is
 * re-navigated. The server pushes a `hardware` notification on every
 * adapter add/remove — dual-port adapters emit two in quick
 * succession — so we subscribe to that class and rebuild the tree,
 * matching the Classic UI (tvadapters.js subscribes `comet: 'hardware'`).
 * The editor's `saved` event runs the same refresh, so a frontend
 * enabled or disabled in the drawer updates its dot and its adapter's.
 *
 * The handler debounces (one refresh per burst), reloads the root and
 * re-fetches children for nodes that were expanded so the tree does
 * not collapse under the user, and guards against overlapping runs
 * and against the component unmounting mid-refresh. */
const COMET_REFRESH_DEBOUNCE_MS = 150
let cometUnsub: (() => void) | null = null
let refreshTimer: ReturnType<typeof setTimeout> | null = null
let refreshing = false
let refreshPending = false
let isUnmounted = false

/* api/hardware/tree is the canonical endpoint per api_input.c:62.
 * The handler at api_idnode.c:506-574 requires a `uuid` param (line
 * 517-518; empty/missing returns EINVAL → HTTP 400). The literal
 * string `"root"` is the magic value that triggers the rootfn
 * callback (api_input_hw_tree) to enumerate the top-level adapter
 * list; any real UUID returns that node's children via
 * idnode_get_childs(). */
async function fetchRoot(): Promise<TreeNode[]> {
  const result = await apiCall<HardwareNode[]>('hardware/tree', {
    uuid: 'root',
  })
  return (result ?? []).map(toTreeNode)
}

/* Initial load: shows the "Loading…" status and, on failure, the
 * error screen. Later reloads go through the silent `refreshTree`. */
async function loadRoot() {
  loading.value = true
  error.value = null
  try {
    nodes.value = await fetchRoot()
  } catch (err) {
    error.value = err as Error
  } finally {
    loading.value = false
  }
}

async function loadChildren(node: TreeNode) {
  /* Already populated from a prior expand — no-op. */
  if (Array.isArray(node.children) && node.children.length > 0) return
  const uuid = node.key
  if (typeof uuid !== 'string') return
  try {
    const result = await apiCall<HardwareNode[]>('hardware/tree', { uuid })
    /* Mutate in place so PrimeVue re-renders the subtree. */
    node.children = (result ?? []).map(toTreeNode)
    /* If the server returns nothing, mark the node as a leaf so the
     * caret disappears on next render. */
    node.leaf = node.children.length === 0
  } catch (err) {
    /* Per-node error — leave children unset so the user can retry
     * by collapsing/expanding. */
    console.error('hardware/tree load failed for', uuid, err)
  }
}

/* Rebuild the tree from the server while preserving expansion:
 * snapshot the expanded keys, fetch the adapter list and the
 * children of every still-expanded node into a detached tree, then
 * swap it in and prune adapters that have gone away. Swapping once
 * keeps the expanded subtrees on screen during the round trips, so
 * a tree taller than the page keeps its scroll position.
 *
 * The refresh is silent: a failed root fetch keeps the current tree
 * rather than showing the error screen.
 *
 * `refreshing` serialises runs; a notification arriving mid-run sets
 * `refreshPending`, and the do/while re-runs once so the tree ends on
 * the latest server state. */
async function refreshTree() {
  if (isUnmounted) return
  if (refreshing) {
    refreshPending = true
    return
  }
  refreshing = true
  try {
    do {
      refreshPending = false
      const wasExpanded = Object.keys(expandedKeys.value).filter(
        (k) => expandedKeys.value[k],
      )
      let fresh: TreeNode[]
      try {
        fresh = await fetchRoot()
      } catch (err) {
        console.error('hardware/tree refresh failed:', err)
        continue
      }
      if (isUnmounted) return
      const live = await refetchExpandedChildren(wasExpanded, fresh)
      if (isUnmounted) return
      nodes.value = fresh
      error.value = null
      expandedKeys.value = pruneExpanded(wasExpanded, live)
    } while (refreshPending && !isUnmounted)
  } finally {
    refreshing = false
  }
}

/* True when a freshly-reloaded node was expanded before the reload
 * and so should have its children re-fetched. */
function wasNodeExpanded(node: TreeNode, wasExpanded: string[]): boolean {
  const key = typeof node.key === 'string' ? node.key : ''
  return key !== '' && !node.leaf && wasExpanded.includes(key)
}

/* Parents-first walk over a freshly-loaded tree: re-fetch the
 * children of every still-expanded node, enqueuing freshly loaded
 * children so deeper expanded nodes are covered in the same pass.
 * Returns the set of node keys that still exist on the server; stops
 * early (returning what it has) if the view unmounts mid-walk. */
async function refetchExpandedChildren(
  wasExpanded: string[],
  roots: TreeNode[],
): Promise<Set<string>> {
  const queue: TreeNode[] = [...roots]
  const live = new Set<string>()
  while (queue.length > 0) {
    const node = queue.shift() as TreeNode
    if (typeof node.key === 'string' && node.key !== '') live.add(node.key)
    if (wasNodeExpanded(node, wasExpanded)) {
      await loadChildren(node)
      if (isUnmounted) return live
    }
    if (Array.isArray(node.children)) queue.push(...node.children)
  }
  return live
}

/* The expanded keys whose nodes still exist, pruning any adapter
 * that has gone away. */
function pruneExpanded(wasExpanded: string[], live: Set<string>): TreeExpandedKeys {
  const pruned: TreeExpandedKeys = {}
  for (const key of wasExpanded) {
    if (live.has(key)) pruned[key] = true
  }
  return pruned
}

/* On-mount restore of the saved expansion over the tree `loadRoot`
 * just loaded: replay the lazy child fetches parents-first
 * (`refetchExpandedChildren`), then set `expandedKeys` to the keys
 * whose nodes still exist. */
async function restoreExpansion(wasExpanded: string[]) {
  const live = await refetchExpandedChildren(wasExpanded, nodes.value)
  if (isUnmounted) return
  expandedKeys.value = pruneExpanded(wasExpanded, live)
}

/* Debounced so a burst of `hardware` notifications (e.g. a dual-port
 * adapter's two messages) collapses into a single refreshTree run. */
function scheduleRefresh() {
  if (refreshTimer !== null) clearTimeout(refreshTimer)
  refreshTimer = globalThis.setTimeout(() => {
    refreshTimer = null
    void refreshTree()
  }, COMET_REFRESH_DEBOUNCE_MS)
}

function onNodeSelect(node: TreeNode) {
  const uuid = (node.data as { uuid?: string } | undefined)?.uuid
  if (typeof uuid === 'string') editingUuid.value = uuid
}

function closeEditor() {
  editingUuid.value = null
  /* Clear selection-highlight too, so reopening the same node fires
   * a fresh @node-select rather than being treated as already-selected. */
  selectionKeys.value = {}
}

const isEmpty = computed(() => !loading.value && !error.value && nodes.value.length === 0)

onMounted(async () => {
  /* Any `hardware` notification means an adapter was added or removed;
   * the payload is just a reload flag, so every message triggers a
   * (debounced) refresh. Subscribe before awaiting the initial load so
   * an adapter hot-plugged during the fetch isn't missed. */
  cometUnsub = cometClient.on('hardware', () => scheduleRefresh())
  /* Seed expansion from the previous visit before loading, so a comet
   * refresh racing the initial load restores the same set. */
  const saved = readSavedExpansion()
  if (saved.length > 0) {
    const seed: TreeExpandedKeys = {}
    for (const k of saved) seed[k] = true
    expandedKeys.value = seed
  }
  await loadRoot()
  /* Skip the restore on a failed load — the error screen replaces the
   * tree, and a wipe here would lose the saved expansion. A later
   * comet refresh restores it from `expandedKeys` (still seeded). */
  if (isUnmounted || error.value) return
  if (saved.length > 0) {
    /* Replay the lazy child fetches for the restored, still-present
     * nodes. Hold the loading state across these round-trips so the
     * tree paints once — fully expanded — rather than flickering as
     * each restored subtree's children arrive one request at a time. */
    loading.value = true
    try {
      await restoreExpansion(saved)
    } finally {
      loading.value = false
    }
  }
})

onBeforeUnmount(() => {
  isUnmounted = true
  cometUnsub?.()
  cometUnsub = null
  if (refreshTimer !== null) {
    clearTimeout(refreshTimer)
    refreshTimer = null
  }
})
</script>

<template>
  <section class="adapters">
    <div v-if="loading" class="adapters__status">{{ t('Loading adapters…') }}</div>
    <div v-else-if="error" class="adapters__status adapters__status--error">
      {{ t('Failed to load:') }} {{ error.message }}
    </div>
    <div v-else-if="isEmpty" class="adapters__status">{{ t('No DVB adapters detected on this server.') }}</div>
    <Tree
      v-else
      v-model:expanded-keys="expandedKeys"
      v-model:selection-keys="selectionKeys"
      :value="nodes"
      selection-mode="single"
      :meta-key-selection="false"
      class="adapters__tree"
      :pt="treePt"
      @node-expand="loadChildren"
      @node-select="onNodeSelect"
    >
      <template #default="{ node }">
        <span
          class="adapters__node"
          :class="{ 'adapters__node--inactive': nodeActive(node) === false }"
        >
          <span
            v-if="nodeActive(node) !== undefined"
            class="adapters__state"
            :class="nodeActive(node) ? 'adapters__state--on' : 'adapters__state--off'"
            :title="activeLabel(nodeActive(node) === true)"
          >
            <Circle :size="10" fill="currentColor" aria-hidden="true" />
          </span>
          {{ node.label }}
        </span>
      </template>
    </Tree>
    <IdnodeEditor
      :uuid="editingUuid"
      :level="'expert'"
      :title="t('Adapter / Frontend')"
      @saved="refreshTree"
      @close="closeEditor"
    />
  </section>
</template>

<style scoped>
.adapters {
  flex: 1 1 auto;
  min-height: 0;
  display: flex;
  flex-direction: column;
}

.adapters__tree {
  flex: 1 1 auto;
  min-height: 0;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

/* Leading on/off dot: green for an active node, red for an inactive
 * one, as in the classic tree. The inactive label is also muted so the
 * state does not rest on the dot colour alone. */
.adapters__node {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-2);
}

.adapters__node--inactive {
  color: var(--tvh-text-muted);
}

.adapters__state {
  display: inline-flex;
  flex: none;
}

.adapters__state--on {
  color: var(--tvh-success);
}

.adapters__state--off {
  color: var(--tvh-error);
}

.adapters__status {
  padding: var(--tvh-space-6);
  color: var(--tvh-text-muted);
  text-align: center;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
}

.adapters__status--error {
  color: var(--tvh-text);
  border-color: color-mix(in srgb, var(--tvh-primary) 40%, var(--tvh-border));
}
</style>
