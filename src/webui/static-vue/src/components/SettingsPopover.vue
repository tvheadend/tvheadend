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
 * Replaces the inline trigger + panel + reset chrome that
 * `GridSettingsMenu` used to ship on its own. Future settings
 * popovers (per-section configuration on the configuration L3
 * leaves, etc.) drop in here.
 */
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { SlidersHorizontal } from 'lucide-vue-next'

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
    ariaLabel: 'View options',
    tooltipText: 'View options',
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

function toggle() {
  open.value = !open.value
}

function clickReset() {
  emit('reset')
  open.value = false
}

/* Click-outside dismissal. Listener on the document so a click on
 * any other UI element closes the panel; checks the `root` element's
 * subtree to know whether the click was inside our own chrome. */
function onDocClick(ev: MouseEvent) {
  if (!root.value) return
  if (!root.value.contains(ev.target as Node)) open.value = false
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
      <hr class="settings-popover__divider" />
      <button
        type="button"
        class="settings-popover__reset"
        :disabled="defaultsActive"
        @click="clickReset"
      >
        Reset to defaults
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
  min-width: 240px;
  max-width: 320px;
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
  font-size: 13px;
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
  font-size: 12px;
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
  font-size: 14px;
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
  font-size: 12px;
  color: var(--tvh-text-muted);
}
</style>
