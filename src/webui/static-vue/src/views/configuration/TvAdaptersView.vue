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
 * Tree expand state resets on route change. Adding `localStorage`
 * persistence under e.g. `tvh-config:dvb:adapters:expand` would
 * survive navigation; default-reset is acceptable for now.
 */
import { onMounted, ref, computed } from 'vue'
import Tree from 'primevue/tree'
import type { TreeExpandedKeys, TreeSelectionKeys } from 'primevue/tree'
import type { TreeNode } from 'primevue/treenode'
import IdnodeEditor from '@/components/IdnodeEditor.vue'
import { apiCall } from '@/api/client'

/* The hardware/tree endpoint returns this shape (extracted from
 * api_input.c + ExtJS's idnode_tree consumption). Each node has a
 * uuid (the idnode UUID), a text label, an optional `children` array
 * (omitted when not yet fetched), and idnode metadata we mostly
 * ignore here — only `uuid` and the human label are used. */
interface HardwareNode {
  uuid: string
  text?: string
  /* Some payload variants put the human label in `name`; tolerate both. */
  name?: string
  /* When children have been loaded, this is an array. Absent when not
   * yet fetched. Empty array means "fetched, no children" (terminal
   * leaf — render without expand caret). */
  children?: HardwareNode[]
  /* Free-form idnode metadata. We don't need it here; passed through
   * to IdnodeEditor implicitly via the uuid lookup. */
  [key: string]: unknown
}

/* PrimeVue Tree expects `key`, `label`, `children`, plus `leaf?` to
 * suppress the expand caret on terminal nodes. We map from the
 * server's HardwareNode shape into this. The `data` slot carries the
 * uuid back through to the click handler. */
function toTreeNode(h: HardwareNode): TreeNode {
  /* If the server sent a populated children array, recurse; otherwise
   * present the node as expandable (children load lazily) — UNLESS
   * it's an empty array, which the server uses to mean "no kids". */
  const hasKnownChildren = Array.isArray(h.children)
  const children = hasKnownChildren ? (h.children as HardwareNode[]).map(toTreeNode) : undefined
  return {
    key: h.uuid,
    label: (h.text ?? h.name ?? h.uuid) as string,
    /* Mark as leaf only when children is an explicitly empty array.
     * Undefined children = "we don't know yet, allow expand". */
    leaf: hasKnownChildren && children!.length === 0,
    children,
    data: { uuid: h.uuid },
  }
}

const nodes = ref<TreeNode[]>([])
const loading = ref(false)
const error = ref<Error | null>(null)
const expandedKeys = ref<TreeExpandedKeys>({})
const selectionKeys = ref<TreeSelectionKeys>({})
const editingUuid = ref<string | null>(null)

/* api/hardware/tree is the canonical endpoint per api_input.c:62.
 * The handler at api_idnode.c:506-574 requires a `uuid` param (line
 * 517-518; empty/missing returns EINVAL → HTTP 400). The literal
 * string `"root"` is the magic value that triggers the rootfn
 * callback (api_input_hw_tree) to enumerate the top-level adapter
 * list; any real UUID returns that node's children via
 * idnode_get_childs(). */
async function loadRoot() {
  loading.value = true
  error.value = null
  try {
    const result = await apiCall<HardwareNode[]>('hardware/tree', {
      uuid: 'root',
    })
    nodes.value = (result ?? []).map(toTreeNode)
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
    console.error(`hardware/tree load for ${uuid} failed:`, err)
  }
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

onMounted(loadRoot)
</script>

<template>
  <section class="adapters">
    <div v-if="loading" class="adapters__status">Loading adapters…</div>
    <div v-else-if="error" class="adapters__status adapters__status--error">
      Failed to load: {{ error.message }}
    </div>
    <div v-else-if="isEmpty" class="adapters__status">No DVB adapters detected on this server.</div>
    <Tree
      v-else
      v-model:expanded-keys="expandedKeys"
      v-model:selection-keys="selectionKeys"
      :value="nodes"
      selection-mode="single"
      :meta-key-selection="false"
      class="adapters__tree"
      @node-expand="loadChildren"
      @node-select="onNodeSelect"
    />
    <IdnodeEditor
      :uuid="editingUuid"
      :level="'expert'"
      title="Adapter / Frontend"
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
