<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CollapsibleSection — one accordion section inside a SettingsPopover.
 *
 * Renders as:
 *   ▶ Title              [Summary chip]     (collapsed)
 *   ▼ Title              [Summary chip]     (expanded, slot below)
 *
 * The chip's colour reflects whether the section is in a non-default
 * state — accent-coloured when `isDefault === false`, neutral
 * otherwise. The chip + the auto-open seed both consume the same
 * `isDefault` predicate so the visual story matches: the section
 * pre-opened on popover open is the one whose chip is accent-tinted.
 *
 * Opt-in: SettingsPopover consumers wrap each section in
 * <CollapsibleSection> to gain accordion behaviour. Consumers that
 * keep using `<div class="settings-popover__section">` directly
 * render as before, just without collapse. This keeps existing
 * popovers (EpgViewOptions, PhoneSortPopover, ColumnHeaderMenu)
 * unaffected.
 *
 * State model (owned by SettingsPopover):
 *   - Popover opens → wait nextTick (children mount + register) →
 *     compute `topmostNonDefault` (first registered section with
 *     `!isDefault()`) → seed openSections with that one id.
 *   - User clicks a section header → toggle in openSections.
 *   - Popover closes → discard openSections.
 *   - Popover reopens → recompute seed. Previous user clicks don't
 *     carry over — open state lives only while the popover is open.
 */
import { computed, inject, onBeforeUnmount, onMounted } from 'vue'
import { ChevronRight, ChevronDown } from 'lucide-vue-next'
import {
  SETTINGS_POPOVER_KEY,
  type SettingsPopoverContext,
} from './settingsPopoverContext'

const props = defineProps<{
  /* Stable id used as the key in openSections.
   * Must be unique within one popover. */
  id: string
  /* Header label rendered next to the chevron. */
  title: string
  /* True when the section is in its zero / default state. Drives
   * both the summary-chip colour AND the auto-open priority — the
   * topmost non-default section opens automatically on popover open. */
  isDefault: boolean
  /* Short text rendered as the right-aligned summary chip. Examples:
   * "Off", "Basic", "12 of 16", "2 active", "Channel". Empty string
   * suppresses the chip. */
  summary: string
}>()

/* Inject the popover's context. Falls back to a degenerate no-op
 * context when the component is used outside a SettingsPopover
 * (test isolation, future surfaces) so it always renders SOMETHING
 * — just not accordion-controlled. */
const ctx = inject<SettingsPopoverContext | null>(SETTINGS_POPOVER_KEY, null)

const isOpen = computed(
  () => ctx?.openSections.value.has(props.id) ?? true,
)

/* Register on mount; unregister on unmount. The registry order
 * matches mount order = template declaration order, which the
 * popover uses to compute "topmost non-default". */
onMounted(() => {
  if (ctx) ctx.registerSection(props.id, () => props.isDefault)
})

onBeforeUnmount(() => {
  if (ctx) ctx.unregisterSection(props.id)
})

function clickHeader() {
  if (ctx) ctx.toggleSection(props.id)
}
</script>

<template>
  <div class="collapsible-section">
    <button
      type="button"
      class="collapsible-section__header"
      :aria-expanded="isOpen"
      :aria-controls="`collapsible-${id}`"
      @click="clickHeader"
    >
      <span class="collapsible-section__chevron" aria-hidden="true">
        <ChevronDown v-if="isOpen" :size="14" :stroke-width="2" />
        <ChevronRight v-else :size="14" :stroke-width="2" />
      </span>
      <span class="collapsible-section__title">{{ title }}</span>
      <span
        v-if="summary"
        class="collapsible-section__chip"
        :class="{ 'collapsible-section__chip--accent': !isDefault }"
      >
        {{ summary }}
      </span>
    </button>
    <div
      v-if="isOpen"
      :id="`collapsible-${id}`"
      class="collapsible-section__body"
    >
      <slot />
    </div>
  </div>
</template>

<style>
/*
 * Non-scoped: slot content is the consumer's settings-popover__*
 * chrome; padding / alignment chrome must apply to children. The
 * class hooks are deliberately specific to avoid collisions.
 */
.collapsible-section {
  margin-bottom: var(--tvh-space-2);
}

.collapsible-section:last-of-type {
  margin-bottom: 0;
}

.collapsible-section__header {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
  padding: 6px 6px;
  background: transparent;
  border: none;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  font: inherit;
  font-size: var(--tvh-text-sm);
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  text-align: left;
  cursor: pointer;
}

.collapsible-section__header:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.collapsible-section__header:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.collapsible-section__chevron {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex: 0 0 auto;
  width: 14px;
  height: 14px;
}

.collapsible-section__title {
  /* Title is short and constant ("Sort by", "Columns", etc.) —
   * pinned to content width so a long chip can't push the label
   * onto a second line. White-space:nowrap is the belt-and-braces
   * against any future label that contains a soft break point. */
  flex: 0 0 auto;
  white-space: nowrap;
}

.collapsible-section__chip {
  /* Chip is the variable-width side of the header. `flex: 0 1 auto`
   * lets it shrink to a smaller box (and via overflow + ellipsis
   * lose characters) when the chip + title + chevron + padding
   * would otherwise exceed the popover's max-width. `margin-left:
   * auto` pushes the chip to the right edge regardless of how
   * narrow it ends up — the title stays left-aligned next to the
   * chevron. */
  flex: 0 1 auto;
  min-width: 0;
  margin-left: auto;
  padding: 1px 6px;
  font-size: var(--tvh-text-xs);
  font-weight: 500;
  text-transform: none;
  letter-spacing: 0;
  color: var(--tvh-text-muted);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.collapsible-section__chip--accent {
  color: var(--tvh-primary);
  border-color: color-mix(in srgb, var(--tvh-primary) 40%, var(--tvh-border));
  background: color-mix(in srgb, var(--tvh-primary) 10%, var(--tvh-bg-page));
  font-weight: 600;
}

.collapsible-section__body {
  /* Pull slot content slightly inset under the header for a clear
   * visual grouping; matches the existing `.settings-popover__section`
   * padding so swapping a div for a CollapsibleSection doesn't shift
   * anything else. */
  padding-left: 4px;
}
</style>
