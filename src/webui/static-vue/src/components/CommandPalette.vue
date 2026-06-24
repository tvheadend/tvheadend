<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CommandPalette — the Cmd-K overlay. PrimeVue `<Dialog>` shell
 * with a search input at the top and a grouped result list below;
 * mounted once at `AppShell` level so every route gets it.
 *
 * Lifecycle:
 *   - Visibility is two-way-bound to `useCommandPalette().isOpen`
 *     via a computed proxy (matches `HelpDialog`'s pattern). The
 *     setter routes through `close()` so dismissal via Escape,
 *     backdrop click, or any PrimeVue-driven path fires the same
 *     side-effects (query reset).
 *   - Input focus is grabbed when the dialog opens (via the
 *     `@show` event PrimeVue emits after mount + animation).
 *   - On close, focus returns to whichever element was focused
 *     when the palette opened (the trigger pill, the Cmd-K
 *     keypress target, etc.).
 *
 * Keyboard inside the dialog:
 *   - ↑ / ↓ move the highlight through the result list
 *   - Enter executes the highlighted command
 *   - Esc closes (PrimeVue's default keypress handler picks this
 *     up; we don't need to wire it)
 *
 * A11y: dialog gets `role="dialog"` and `aria-modal` from
 * PrimeVue. The input owns the focus surface and uses
 * `aria-activedescendant` to point at the highlighted result
 * row, with an `aria-live="polite"` region announcing the result
 * count so screen-reader users get feedback on each keystroke.
 */
import { computed, nextTick, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import Dialog from 'primevue/dialog'
import { Search } from 'lucide-vue-next'
import { useCommandPalette } from '@/composables/useCommandPalette'
import { useAccessStore } from '@/stores/access'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { useEntityEditor } from '@/composables/useEntityEditor'
import { useI18n } from '@/composables/useI18n'
import { useToastNotify } from '@/composables/useToastNotify'
import { useWizardStore } from '@/stores/wizard'
import {
  SUGGESTED_COMMAND_IDS,
  buildActionCommands,
  buildRouteCommands,
} from '@/commands/commandRegistry'
import {
  ensureChannelsLoaded,
  getChannelCommands,
} from '@/commands/channelSource'
import {
  getEpgEventCommands,
  updateEpgQuery,
} from '@/commands/epgEventSource'
import {
  ensureRecordingsLoaded,
  getRecordingCommands,
} from '@/commands/recordingSource'
import {
  ensureAutorecsLoaded,
  getAutorecCommands,
} from '@/commands/autorecSource'
import {
  ensureSettingsLoaded,
  getSettingsCommands,
} from '@/commands/settingsSource'
import { rankCommands, type RankedResult } from '@/commands/commandRanker'
import type { Command } from '@/commands/commandRegistry'

const { t } = useI18n()
const palette = useCommandPalette()
const access = useAccessStore()
const router = useRouter()
const toast = useToastNotify()
const wizard = useWizardStore()
const entityEditor = useEntityEditor()
const confirm = useConfirmDialog()

/* Reactive channel-command list, populated by the lazy fetch
 * below. Empty until the first palette open kicks off the fetch;
 * subsequent opens within the cache TTL get the result immediately. */
const channelCommands = getChannelCommands()

/* Reactive EPG-event-command list, populated by the debounced
 * title search below. Empty until the user types 3+ characters;
 * cleared when the query falls below the threshold. */
const epgEventCommands = getEpgEventCommands()

/* Reactive recording-command list, populated lazily on first
 * palette open (same shape as channelSource — 60s cache). Stays
 * empty for users without dvr permission (the server returns no
 * entries for them). */
const recordingCommands = getRecordingCommands()

/* Reactive autorec-command list. Same lazy-fetch + 60s cache
 * shape as recordings. Stays empty for non-dvr users (server
 * gate). The toggle/delete handlers invalidate the cache on
 * success so the next open re-fetches with the latest state. */
const autorecCommands = getAutorecCommands()

/* Reactive settings-command list — every editable field on the
 * six singleton-config pages. Lazy-fetched on first open, 60 s
 * cache, admin-gated at the source (non-admins pay zero cost). */
const settingsCommands = getSettingsCommands()

/* `previouslyFocused` — the element that owned focus when the
 * palette opened. Restored on close so the user's tab order
 * doesn't reset to the top of the page. */
const previouslyFocused = ref<HTMLElement | null>(null)
const inputEl = ref<HTMLInputElement | null>(null)
/* Highlighted row index within the flat result list. -1 means
 * "nothing highlighted" (which only happens for an empty result
 * list); Enter is a no-op in that case. */
const highlightIndex = ref(0)

/* Keyboard-vs-mouse interaction guard.
 *
 * Arrow keys trigger `scrollIntoView`, which moves DOM rows under
 * a stationary mouse cursor. The newly-positioned row fires
 * `mouseenter` and yanks the highlight to the wrong place —
 * exactly the "lines jump randomly" symptom. Linear / Raycast fix:
 * remember which input drove the last highlight change; ignore
 * `mouseenter` until an actual `mousemove` proves the user is
 * back on the pointer. Real cursor movement resets the flag and
 * mouse-driven highlight resumes naturally.
 *
 * Initial value `true` so the first keystroke right after open
 * doesn't get clobbered by whatever row the cursor happens to be
 * over (the user-clicked-the-pill-and-started-typing flow).
 */
const lastInteractionKeyboard = ref(true)

/* PrimeVue's visible-binding goes through this proxy so dismissal
 * via Escape / backdrop / X all run our `close()` side-effects.
 * Same shape as HelpDialog.vue. */
const visibleProxy = computed({
  get: () => palette.isOpen.value,
  set: (v: boolean) => {
    if (!v) palette.close()
  },
})

/* Composed command list. Sources stay separate so each builder
 * can be unit-tested in isolation; the palette glues them together
 * by concatenating. Route + action commands are both static per
 * page load (routes don't change at runtime, and the action set is
 * hardcoded) so caching once is enough. */
const allCommands = computed<readonly Command[]>(() => [
  ...buildRouteCommands(router),
  ...buildActionCommands({ toast, wizard, router, access, confirm }),
  ...channelCommands.value,
  ...recordingCommands.value,
  ...autorecCommands.value,
  ...epgEventCommands.value,
  ...settingsCommands.value,
])

const results = computed<RankedResult[]>(() => {
  const ranked = rankCommands({
    query: palette.query.value,
    commands: allCommands.value,
    hasPermission: (k) => access.has(k),
    mruRank: (id) => palette.mruRank(id),
    suggestedIds: SUGGESTED_COMMAND_IDS,
  })
  /* Reorder so items of the same section are adjacent in flat order.
   * The grouped-rendering layout puts all Channels rows together
   * regardless of fuse's interleave; without this reorder, the
   * highlight's flat index doesn't match what the user sees on
   * screen — pressing ↓ on a Channels row could teleport the
   * highlight to an Actions row rendered several rows below
   * because that's the fuse-ranked next item. Section order
   * preserves first-appearance (so the best-scoring match's
   * section stays on top); within a section, items keep their
   * fuse-score order. */
  const bySection = new Map<string, RankedResult[]>()
  for (const r of ranked) {
    const label = groupLabelOf(r)
    if (!bySection.has(label)) bySection.set(label, [])
    bySection.get(label)!.push(r)
  }
  return [...bySection.values()].flat()
})

/* Reset the highlight to the first result whenever the result
 * list changes (new query, MRU shift after an execution, etc.).
 * Without this, the highlight could point past the new list and
 * Enter would no-op.
 *
 * Also flag this as a keyboard-driven moment so the mouseenter
 * the DOM rebuild fires (on a row that ends up under the stationary
 * cursor) gets ignored by `onRowMouseEnter`. Without that flag
 * flip, the watch's pre-flush highlight=0 reset gets immediately
 * overwritten by the spurious mouseenter — symptom: after typing
 * a new query, the highlight lands on whatever row sits under
 * the cursor rather than on the top result. */
watch(results, () => {
  highlightIndex.value = 0
  lastInteractionKeyboard.value = true
})

/* Kick off the channel-list fetch on every palette open. The
 * channelSource module caches for 60s, so this is a no-op when the
 * cache is still warm; the first open of a session pays the
 * one-time fetch (~100ms) and the channel commands appear in the
 * result list as soon as the response lands. */
watch(palette.isOpen, (open) => {
  if (open) {
    ensureChannelsLoaded({ entityEditor, router, access }).catch(() => undefined)
    ensureRecordingsLoaded({ entityEditor, router, access, toast, confirm }).catch(() => undefined)
    ensureAutorecsLoaded({ entityEditor, router, access, toast, confirm }).catch(() => undefined)
    ensureSettingsLoaded({ router, access }).catch(() => undefined)
  } else {
    /* Palette closed — bump the EPG-search debounce by sending an
     * empty query, which cancels any in-flight timer and clears
     * the result list. Stops a background fetch from completing
     * after the user dismissed and overwriting nothing useful. */
    updateEpgQuery('', { router, access, toast })
  }
})

/* Drive the EPG title-search source from the palette's query.
 * Only runs while the palette is open — sending the empty string
 * on close (above) cancels any in-flight debounce. The
 * epgEventSource handles its own min-length gating (3 chars) and
 * debouncing (300 ms) — we just feed it the live query string. */
watch(palette.query, (q) => {
  if (!palette.isOpen.value) return
  updateEpgQuery(q, { router, access, toast })
})

/*
 * Focus seamlessness on open.
 *
 * The pill trigger LOOKS like an input field; users expect that
 * clicking it lets them type immediately. Previously the focus
 * happened on PrimeVue's `@show` event — which fires AFTER the
 * dialog's open animation completes. Keystrokes typed during that
 * window were dropped on the floor.
 *
 * Running on the `isOpen` flip (with one `nextTick` for the dialog
 * template to mount) closes that gap to within a single microtask,
 * so a fast click-and-type lands in the input.
 *
 * Snapshot of `previouslyFocused` happens FIRST, before the dialog
 * grabs focus, so `onHide` can restore it. The previous `@show` /
 * `@hide` PrimeVue bindings are removed since this watcher now
 * carries the lifecycle on both edges.
 */
watch(palette.isOpen, async (open) => {
  if (open) {
    previouslyFocused.value = document.activeElement instanceof HTMLElement
      ? document.activeElement
      : null
    highlightIndex.value = 0
    /* Reset the mouse-ignore flag so the first DOM-rebuild
     * mouseenter (rows appearing under whatever the cursor is
     * over from the previous session) can't yank the highlight. */
    lastInteractionKeyboard.value = true
    await nextTick()
    inputEl.value?.focus()
  } else {
    previouslyFocused.value?.focus()
    previouslyFocused.value = null
  }
})

/* Group adjacent results by header for visual rendering. The
 * ranker returns a flat list — we keep that order (so MRU and
 * fuse score determine position) but insert headers when the
 * group changes from one item to the next.
 *
 * Group label preference: when the ranker has emitted an
 * `emptyStateGroup` ('Recent' / 'Suggested'), that wins so the
 * empty-query view reads as "Recent / Suggested" rather than the
 * underlying Actions / Navigation sections. For non-empty queries
 * the ranker leaves `emptyStateGroup` undefined and we fall back
 * to `command.section`. */
interface ResultGroup {
  section: string
  items: { result: RankedResult; flatIndex: number }[]
}

function groupLabelOf(result: RankedResult): string {
  return result.emptyStateGroup ?? result.command.section
}

/* Build groups keyed by section label so same-section results
 * collapse into a single header even when fuse interleaves them
 * with other sections (e.g. typing "news" can rank Channel A →
 * Navigation hit → Channel B, which without dedup would render
 * "Channels / Navigation / Channels" with two Channels headers).
 *
 * Section ORDER is first-appearance order — keeps the user's
 * best match at the top while keeping each section header unique.
 * Items within a section keep their fuse score order.
 */
const groups = computed<ResultGroup[]>(() => {
  const bySection = new Map<string, ResultGroup>()
  results.value.forEach((result, flatIndex) => {
    const label = groupLabelOf(result)
    let group = bySection.get(label)
    if (!group) {
      group = { section: label, items: [] }
      bySection.set(label, group)
    }
    group.items.push({ result, flatIndex })
  })
  return [...bySection.values()]
})

function rowId(flatIndex: number): string {
  return `command-palette-row-${flatIndex}`
}

function onInput(event: Event): void {
  palette.query.value = (event.target as HTMLInputElement).value
}

function move(delta: number): void {
  if (results.value.length === 0) return
  /* Arrow key — mark this turn as keyboard-driven so the scroll
   * that's about to happen doesn't cause a stray mouseenter to
   * overwrite the highlight. */
  lastInteractionKeyboard.value = true
  const next = highlightIndex.value + delta
  /* Wrap around the ends so ↑ from the top jumps to the bottom
   * and vice versa — matches the Slack / Linear behaviour. */
  if (next < 0) highlightIndex.value = results.value.length - 1
  else if (next >= results.value.length) highlightIndex.value = 0
  else highlightIndex.value = next
  /* Scroll the highlighted row into view if it would be clipped
   * by the scrollable list. */
  void nextTick(() => {
    const el = document.getElementById(rowId(highlightIndex.value))
    el?.scrollIntoView({ block: 'nearest' })
  })
}

/* Row `mouseenter` handler. Honours the keyboard-vs-mouse guard:
 * when the last interaction was the keyboard, a row firing
 * mouseenter is almost certainly a scrollIntoView artifact (DOM
 * moved under the stationary cursor) — not a deliberate hover — so
 * we ignore it. As soon as the user actually moves the mouse, the
 * list-level `mousemove` resets the flag and hover-driven highlight
 * resumes naturally. */
function onRowMouseEnter(flatIndex: number): void {
  if (lastInteractionKeyboard.value) return
  highlightIndex.value = flatIndex
}

/* Action tiers — three keyboard combos drive three handler slots.
 * Tertiary is opt-in per command (used by channels + recordings);
 * commands without a given tier no-op on its keystroke rather than
 * silently falling back to primary — keeps modifier semantics
 * honest. */
type ActionTier = 'primary' | 'secondary' | 'tertiary'

function executeHighlighted(tier: ActionTier = 'primary'): void {
  const target = results.value[highlightIndex.value]
  if (!target) return
  execute(target.command, tier)
}

function execute(command: Command, tier: ActionTier = 'primary'): void {
  /* Look up the handler for the requested tier. A missing
   * secondary or tertiary makes the modifier-Enter a no-op
   * rather than a silent fallback to primary. */
  let handler: (() => void | Promise<void>) | undefined
  if (tier === 'primary') handler = command.action
  else if (tier === 'secondary') handler = command.secondaryAction?.handler
  else handler = command.tertiaryAction?.handler
  if (!handler) return
  palette.recordExecution(command.id)
  palette.close()
  /* Run after close so the dialog doesn't briefly flash the new
   * route as it tears down. The `.catch` makes the chained
   * promise non-floating without needing the `void` operator —
   * individual commands surface their own errors via toasts /
   * dialogs (e.g. apiCall surfacing through useErrorDialog), so
   * we just swallow here. */
  Promise.resolve(handler()).catch(() => undefined)
}

/* Detect macOS for the chip's modifier symbol (Cmd vs Ctrl) and to
 * pick the right key on Enter. Same heuristic as
 * CommandPaletteTrigger.vue. navigator.platform is deprecated but
 * still ships everywhere; the modern userAgentData isn't on Safari
 * yet. */
const isMac = computed<boolean>(() => {
  const p = globalThis.navigator?.platform ?? ''
  return /Mac|iPhone|iPad|iPod/i.test(p)
})

const secondaryModifierSymbol = computed<string>(() => (isMac.value ? '⌘' : 'Ctrl'))

/* Tertiary modifier symbol. Mac convention is `⇧⌘` (Shift + Cmd
 * glued); Win/Linux convention is the literal "Shift+Ctrl". Both
 * read as "the modifier-stack before ↵". */
const tertiaryModifierSymbol = computed<string>(() =>
  isMac.value ? '⇧⌘' : 'Shift+Ctrl',
)

/* Footer-bar action hints derived from the currently-highlighted
 * result. Empty when no result is highlighted. The footer carries
 * BOTH primary and secondary (when present) so visual weight is
 * balanced — the previous per-row chip only showed the secondary,
 * which mis-cued attention to the less-common path. Linear / Raycast
 * pattern. */
interface FooterHint {
  modifier: string | null
  label: string
}

function defaultPrimaryLabel(section: string): string {
  if (section === 'Actions') return 'Run'
  return 'Open'
}

const footerHints = computed<FooterHint[]>(() => {
  const target = results.value[highlightIndex.value]
  if (!target) return []
  const hints: FooterHint[] = []
  hints.push({
    modifier: null,
    label: target.command.actionLabel ?? defaultPrimaryLabel(target.command.section),
  })
  if (target.command.secondaryAction) {
    hints.push({
      modifier: secondaryModifierSymbol.value,
      label: target.command.secondaryAction.label,
    })
  }
  if (target.command.tertiaryAction) {
    hints.push({
      modifier: tertiaryModifierSymbol.value,
      label: target.command.tertiaryAction.label,
    })
  }
  return hints
})

function onKeyDown(event: KeyboardEvent): void {
  if (event.key === 'ArrowDown') {
    event.preventDefault()
    move(1)
  } else if (event.key === 'ArrowUp') {
    event.preventDefault()
    move(-1)
  } else if (event.key === 'Enter') {
    event.preventDefault()
    /* Three-tier modifier-Enter mapping:
     *   ⇧⌘↵ / Ctrl+Shift+↵ — tertiary
     *   ⌘↵   / Ctrl+↵        — secondary
     *   ↵                    — primary
     *
     * The modifier check is independent of useGlobalShortcuts'
     * eitherMetaOrCtrl convention because here we're inside the
     * palette and the dialog's own listener — no registry layer
     * in the way. Tertiary tier checked FIRST so the shift-bit
     * isn't swallowed by the secondary branch. */
    const modifierHeld = isMac.value ? event.metaKey : event.ctrlKey
    let tier: ActionTier = 'primary'
    if (modifierHeld && event.shiftKey) tier = 'tertiary'
    else if (modifierHeld) tier = 'secondary'
    executeHighlighted(tier)
  }
  /* Esc handled by PrimeVue's dialog-keydown listener — no
   * explicit binding needed. Tab / Shift-Tab work naturally as
   * focus navigation within the dialog. */
}

/* `onShow` / `onHide` lifecycle is handled by the
 * `watch(palette.isOpen, …)` above — fires on the isOpen flip,
 * which beats PrimeVue's animation-end events to focus the input
 * before any user keystroke can drop. The @show / @hide PrimeVue
 * bindings are removed too. */
</script>

<template>
  <Dialog
    v-model:visible="visibleProxy"
    modal
    :draggable="false"
    :dismissable-mask="true"
    :show-header="false"
    position="top"
    class="command-palette"
    :style="{ width: '640px', maxWidth: 'calc(100vw - 32px)', marginTop: '12vh' }"
    :breakpoints="{ '768px': '100vw' }"
    aria-label="Command palette"
    :pt="{
      root: { style: 'overflow: hidden;' },
      content: {
        class: 'command-palette__content',
        style: 'padding: 0; display: flex; flex-direction: column; max-height: 70vh; overflow: hidden;',
      },
    }"
  >
    <div class="command-palette__inner" @keydown="onKeyDown">
      <div class="command-palette__input-row">
        <Search :size="18" :stroke-width="2" class="command-palette__input-icon" />
        <input
          ref="inputEl"
          type="text"
          class="command-palette__input"
          :aria-label="t('Search')"
          :placeholder="t('Search anywhere · {0}K', secondaryModifierSymbol)"
          :value="palette.query.value"
          autocomplete="off"
          spellcheck="false"
          role="combobox"
          aria-expanded="true"
          aria-controls="command-palette-listbox"
          :aria-activedescendant="results.length > 0 ? rowId(highlightIndex) : undefined"
          @input="onInput"
        />
      </div>

      <div
        id="command-palette-listbox"
        class="command-palette__list"
        role="listbox"
        :aria-label="t('Commands')"
        @mousemove="lastInteractionKeyboard = false"
      >
        <div v-if="results.length === 0" class="command-palette__empty">
          {{ t('No results') }}
        </div>
        <template v-else>
          <div
            v-for="group in groups"
            :key="group.section"
            class="command-palette__group"
          >
            <p class="command-palette__group-header">{{ group.section }}</p>
            <div
              v-for="{ result, flatIndex } in group.items"
              :id="rowId(flatIndex)"
              :key="result.command.id"
              role="option"
              :aria-selected="flatIndex === highlightIndex"
              class="command-palette__row"
              :class="{ 'command-palette__row--active': flatIndex === highlightIndex }"
              @mouseenter="onRowMouseEnter(flatIndex)"
              @click="execute(result.command)"
            >
              <component
                :is="result.command.icon"
                v-if="result.command.icon"
                :size="16"
                :stroke-width="2"
                class="command-palette__row-icon"
              />
              <div class="command-palette__row-text">
                <span class="command-palette__row-label">{{ result.command.label }}</span>
                <span
                  v-if="result.command.description"
                  class="command-palette__row-description"
                >{{ result.command.description }}</span>
              </div>
            </div>
          </div>
        </template>
      </div>

      <!-- Footer bar — Raycast/Linear pattern. Shows the
           highlighted row's primary and (optional) secondary
           action labels with their keyboard shortcuts. Empty when
           no row is highlighted (no results / loading). -->
      <div v-if="footerHints.length > 0" class="command-palette__footer">
        <span
          v-for="(hint, idx) in footerHints"
          :key="idx"
          class="command-palette__footer-hint"
        >
          <kbd v-if="hint.modifier">{{ hint.modifier }}</kbd>
          <kbd>↵</kbd>
          <span class="command-palette__footer-hint-label">{{ hint.label }}</span>
        </span>
      </div>

      <div class="command-palette__live" aria-live="polite">
        <template v-if="palette.query.value">{{ results.length }} {{ t('results') }}</template>
      </div>
    </div>
  </Dialog>
</template>

<style scoped>
/* The dialog content slot already manages overflow + flex via the
 * :pt overrides. The inner wrapper just provides the column flex
 * for input + list + live region. */
.command-palette__inner {
  display: flex;
  flex-direction: column;
  min-height: 0;
  flex: 1 1 auto;
}

.command-palette__input-row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-3) var(--tvh-space-4);
  border-bottom: 1px solid var(--tvh-border);
  flex-shrink: 0;
}

.command-palette__input-icon {
  color: var(--tvh-text-muted);
  flex-shrink: 0;
}

.command-palette__input {
  flex: 1;
  background: none;
  border: 0;
  outline: 0;
  font: inherit;
  font-size: var(--tvh-text-lg);
  color: var(--tvh-text);
}

.command-palette__input::placeholder {
  color: var(--tvh-text-muted);
}

/* Scrollable result region. `min-height: 0` lets it shrink within
 * the dialog's `max-height: 70vh` so the input row stays pinned. */
.command-palette__list {
  flex: 1;
  overflow-y: auto;
  min-height: 0;
  padding: var(--tvh-space-2) 0;
}

.command-palette__empty {
  padding: var(--tvh-space-4);
  text-align: center;
  color: var(--tvh-text-muted);
}

.command-palette__group + .command-palette__group {
  margin-top: var(--tvh-space-2);
}

.command-palette__group-header {
  margin: 0;
  padding: var(--tvh-space-1) var(--tvh-space-4);
  font-size: var(--tvh-text-xs);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
}

.command-palette__row {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-3);
  padding: var(--tvh-space-2) var(--tvh-space-4);
  cursor: pointer;
  transition: background var(--tvh-transition);
}

.command-palette__row--active {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-active-strength), transparent);
}

.command-palette__row-icon {
  color: var(--tvh-text-muted);
  flex-shrink: 0;
}

.command-palette__row-text {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.command-palette__row-label {
  color: var(--tvh-text);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.command-palette__row-description {
  font-size: var(--tvh-text-xs);
  color: var(--tvh-text-muted);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/*
 * Footer bar — Raycast / Linear pattern. Pinned below the result
 * list; shows the highlighted row's primary and (optional)
 * secondary action labels next to their keyboard shortcuts.
 * Single source of action hints, balanced visual weight for
 * primary vs secondary, and a natural place for tertiary actions
 * to land later.
 */
.command-palette__footer {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-4);
  flex-shrink: 0;
  padding: var(--tvh-space-2) var(--tvh-space-4);
  border-top: 1px solid var(--tvh-border);
  background: var(--tvh-bg-surface);
}

.command-palette__footer-hint {
  display: inline-flex;
  align-items: center;
  gap: var(--tvh-space-1);
  color: var(--tvh-text-muted);
  font-size: var(--tvh-text-xs);
}

.command-palette__footer-hint kbd {
  display: inline-block;
  min-width: 16px;
  padding: 1px 5px;
  font: inherit;
  font-size: var(--tvh-text-xs);
  font-family: inherit;
  text-align: center;
  color: var(--tvh-text-muted);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: 3px;
  line-height: 1.2;
}

.command-palette__footer-hint-label {
  margin-left: 2px;
  white-space: nowrap;
}

/* Screen-reader-only live region. Visually hidden but announced
 * by assistive tech when its content changes. */
.command-palette__live {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}
</style>
