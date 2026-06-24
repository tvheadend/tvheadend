<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * SettingsPopover — shared shell for toolbar dropdown menus that hold
 * groups of view options.
 *
 * Provides:
 *   - The trigger button (sliders icon + tooltip + aria wiring).
 *   - Open / close state with click-outside dismissal.
 *   - The popover panel itself, anchored below the trigger.
 *   - A "Reset to defaults" footer button (gated on the consumer's
 *     `defaultsActive` prop — disabled when current state already
 *     equals defaults). Emits `reset` on click and closes the panel.
 *
 * Each consumer (today: `GridSettingsMenu`, `TimelineViewOptions`)
 * fills the default slot with its own sections. Two `<style>`
 * blocks: the scoped one styles the trigger button + panel + reset
 * footer; the non-scoped one exposes class hooks (`.settings-popover__
 * section`, `__option`, `__divider`, …) that consumers compose inside
 * the slot. Vue's scoping doesn't reach slotted content, so non-scoped
 * is the right tool for shared section chrome.
 *
 * Owns the shared trigger + panel + reset chrome so each consumer
 * focuses on its own sections; new settings popovers (per-section
 * configuration on the configuration L3 leaves, etc.) drop in here.
 */
import { computed, onBeforeUnmount, onMounted, provide, ref, useSlots, watch } from 'vue'
import { SlidersHorizontal } from 'lucide-vue-next'
import {
  SETTINGS_POPOVER_KEY,
  type SettingsPopoverContext,
} from './settingsPopoverContext'
/* Import the free `t` function rather than the composable's
 * destructured `t` so it's a module-scope symbol — Vue's
 * compiler-sfc macro analyzer rejects setup-scope references
 * inside `withDefaults` defaults ("cannot reference locally
 * declared variables"). The free function reads the same
 * `tvh_locale` global; only the reactivity wrapper differs,
 * and prop defaults don't need reactivity anyway. */
import { t } from '@/composables/useI18n'

withDefaults(
  defineProps<{
    /**
     * Aria label on the trigger button. Same string also drives the
     * default tooltip when `tooltipText` is unset. Defaults to
     * "View options" so a consumer that doesn't pass anything still
     * gets a sensible label.
     */
    ariaLabel?: string
    /**
     * Tooltip-bottom hint shown on hover over the trigger button.
     * Defaults to the same value as `ariaLabel`.
     */
    tooltipText?: string
    /**
     * When true, the "Reset to defaults" button is disabled — the
     * current state already matches defaults so resetting would be
     * a no-op. Mirrors the existing `GridSettingsMenu` behaviour
     * where the reset button is always present but visibly inert
     * when nothing's been customised.
     */
    defaultsActive?: boolean
  }>(),
  {
    ariaLabel: t('View options'),
    tooltipText: t('View options'),
    defaultsActive: false,
  }
)

const emit = defineEmits<{
  /* User clicked Reset to defaults. Consumer is expected to revert
   * its state; the popover closes itself afterwards. */
  reset: []
}>()

const open = ref(false)
const root = ref<HTMLElement | null>(null)

/* True when the consumer passes default-slot content. Drives the
 * footer divider — single-action consumers (drawer "View options"
 * with only a Reset button) get a clean panel without a stray
 * top-edge `<hr>`. Most consumers (GridSettingsMenu,
 * TimelineViewOptions) keep the divider because they HAVE sections
 * above the reset row. */
const slots = useSlots()
const hasDefaultSlot = computed(
  () => !!slots.default && (slots.default()?.length ?? 0) > 0
)

/*
 * Accordion section registry. Each CollapsibleSection child registers
 * itself on mount with its id + isDefault predicate; the registry
 * order matches mount order = template declaration order. The popover
 * uses this to compute "topmost non-default" when seeding the open
 * set on each popover-open transition.
 *
 * Consumers that DON'T wrap content in CollapsibleSection (the legacy
 * pattern: plain `<div class="settings-popover__section">`) leave
 * the registry empty. Nothing else watches the registry, so they're
 * unaffected.
 */
const registry = ref<{ id: string; isDefault: () => boolean }[]>([])
const openSections = ref<Set<string>>(new Set())

function registerSection(id: string, isDefault: () => boolean) {
  registry.value.push({ id, isDefault })
}

function unregisterSection(id: string) {
  const i = registry.value.findIndex((s) => s.id === id)
  if (i >= 0) registry.value.splice(i, 1)
}

function toggleSection(id: string) {
  const next = new Set(openSections.value)
  if (next.has(id)) next.delete(id)
  else next.add(id)
  openSections.value = next
}

function toggle() {
  open.value = !open.value
}

/*
 * Reset the accordion open-set every time the popover closes.
 * Section state lives only while the popover is visible — the next
 * open starts fully collapsed and the user clicks whichever section
 * they want to see. Accent chips on the collapsed headers flag
 * non-default sections at a glance, so the extra click is paid for
 * by a cleaner zero state on open.
 *
 * Watching `open` (not gating inside `toggle`) means this fires for
 * every code path that flips the state — click-outside dismissal,
 * `defineExpose.close()`, the reset button — not just user clicks
 * on the trigger.
 */
watch(open, (now, prev) => {
  if (!now && prev) {
    openSections.value = new Set()
  }
})

const context: SettingsPopoverContext = {
  openSections,
  popoverOpen: open,
  registerSection,
  unregisterSection,
  toggleSection,
}
provide(SETTINGS_POPOVER_KEY, context)

function clickReset() {
  emit('reset')
  open.value = false
}

/* PrimeVue overlay panels that, when used inside the popover,
 * render outside our root DOM subtree (they default to
 * `appendTo: 'body'`). Without this whitelist the popover's
 * outside-click dismissal would fire on every checkbox /
 * option click inside such a panel — closing the popover the
 * moment the user interacts with an embedded MultiSelect,
 * Select, DatePicker, etc.
 *
 * Each entry is a class selector PrimeVue 4 stamps on the
 * overlay's root element. Add new ones here as additional
 * floating widgets get embedded in SettingsPopover content. */
const PRIMEVUE_OVERLAY_SELECTORS = [
  '.p-multiselect-overlay',
  '.p-select-overlay',
  '.p-datepicker-panel',
  '.p-tooltip',
]

/* Click-outside dismissal. Listener on the document so a click on
 * any other UI element closes the panel.
 *
 * Uses `composedPath()` instead of `contains(ev.target)` because
 * Vue may re-render and detach the clicked node before this handler
 * fires (e.g. the accordion's chevron icon swap on header click) —
 * a stale `ev.target` then no longer lives under `root`, falsely
 * triggering dismissal. `composedPath()` captures the chain at
 * event-dispatch time, before any re-renders.
 *
 * The PrimeVue-overlay whitelist (above) ignores clicks whose
 * path includes a floating widget panel that's logically part
 * of the popover's interaction surface but DOM-mounted under
 * `<body>`. */
function pathIncludesOverlay(path: EventTarget[]): boolean {
  for (const node of path) {
    if (!(node instanceof Element)) continue
    for (const sel of PRIMEVUE_OVERLAY_SELECTORS) {
      if (node.matches(sel)) return true
    }
  }
  return false
}

function onDocClick(ev: MouseEvent) {
  if (!root.value) return
  const path = ev.composedPath()
  if (path.includes(root.value)) return
  if (pathIncludesOverlay(path)) return
  open.value = false
}

onMounted(() => document.addEventListener('click', onDocClick))
onBeforeUnmount(() => document.removeEventListener('click', onDocClick))

/* Exposed for tests + consumers that want to programmatically close
 * the popover (e.g. after committing a value). */
defineExpose({
  close: () => {
    open.value = false
  },
})
</script>

<template>
  <div ref="root" class="settings-popover">
    <button
      v-tooltip.bottom="tooltipText"
      type="button"
      class="settings-popover__btn"
      :aria-label="ariaLabel"
      aria-haspopup="menu"
      :aria-expanded="open"
      @click="toggle"
    >
      <SlidersHorizontal :size="16" :stroke-width="2" />
    </button>
    <div v-if="open" class="settings-popover__panel" role="menu">
      <slot />
      <hr v-if="hasDefaultSlot" class="settings-popover__divider" />
      <button
        type="button"
        class="settings-popover__reset"
        :disabled="defaultsActive"
        @click="clickReset"
      >
        {{ t('Reset to defaults') }}
      </button>
    </div>
  </div>
</template>

<style scoped>
/*
 * Scoped: trigger + panel + reset chrome. These elements are owned
 * by SettingsPopover's template and aren't subject to slotted-content
 * styling concerns.
 */
.settings-popover {
  position: relative;
  display: inline-flex;
}

.settings-popover__btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  cursor: pointer;
}

.settings-popover__btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), var(--tvh-bg-page));
}

.settings-popover__btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.settings-popover__panel {
  position: absolute;
  top: calc(100% + 4px);
  right: 0;
  /* Pinned panel width — the previous min/max range (240..320 px)
   * meant the panel grew with a wide summary chip up to its
   * max-width, which read as "the menu jumps wider when this
   * particular column is sorted." Pinning to a single value makes
   * the panel feel stable, and the CollapsibleSection header's
   * chip ellipsis kicks in cleanly within this fixed budget. */
  width: 300px;
  padding: var(--tvh-space-2);
  background: var(--tvh-bg-surface);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.18);
  z-index: 10;
}

.settings-popover__reset {
  display: block;
  width: 100%;
  padding: 6px 8px;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  font: inherit;
  font-size: var(--tvh-text-md);
  text-align: center;
  cursor: pointer;
}

.settings-popover__reset:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.settings-popover__reset:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.settings-popover__reset:disabled {
  cursor: default;
  opacity: 0.5;
}
</style>

<style>
/*
 * Non-scoped: section chrome shared with slotted content. Vue's
 * scoped styles don't propagate into slot content, so these classes
 * live in a regular <style> block. Names are deliberately specific
 * (`settings-popover__*`) so global namespace collisions are
 * vanishingly unlikely.
 */
.settings-popover__section {
  margin-bottom: var(--tvh-space-2);
}

.settings-popover__section:last-of-type {
  margin-bottom: 0;
}

.settings-popover__section-title {
  padding: 4px 6px;
  font-size: var(--tvh-text-sm);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
}

.settings-popover__option {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
  padding: 6px 8px;
  background: transparent;
  border: none;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  font: inherit;
  font-size: var(--tvh-text-lg);
  text-align: left;
  cursor: pointer;
}

.settings-popover__option:hover:not(.settings-popover__option--disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.settings-popover__option:focus-visible,
.settings-popover__option input:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.settings-popover__option--active {
  font-weight: 600;
}

.settings-popover__option--disabled {
  cursor: default;
  opacity: 0.5;
}

.settings-popover__radio {
  display: inline-block;
  width: 1em;
  text-align: center;
  color: var(--tvh-primary);
}

.settings-popover__checkbox {
  flex: 0 0 auto;
  margin: 0;
}

.settings-popover__divider {
  border: none;
  border-top: 1px solid var(--tvh-border);
  margin: var(--tvh-space-2) 0;
}

.settings-popover__note {
  padding: 4px 8px;
  font-size: var(--tvh-text-sm);
  color: var(--tvh-text-muted);
}
</style>
