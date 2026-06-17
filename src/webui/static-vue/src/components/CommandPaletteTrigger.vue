<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * CommandPaletteTrigger — clickable affordance that opens the
 * command palette. Two visual variants picked via the `variant`
 * prop:
 *
 *   variant="pill" — wide pill labelled "Search… ⌘K" for the
 *     desktop chrome (rendered in NavRail's brand row). Touch-only
 *     desktop devices use this to open the palette without a
 *     keyboard; keyboard users also click it when they prefer
 *     pointer over shortcut, the same way Slack's top-bar search
 *     is reachable both ways.
 *   variant="icon" — magnifier icon for the phone TopBar where
 *     horizontal real estate is tight. Same click handler.
 *
 * The shortcut hint shows `⌘K` on macOS / iOS and `Ctrl K` on
 * everything else, via navigator-platform sniff. Conventional
 * symbology — same heuristic Slack / Linear / GitHub use — so
 * the hint matches what the user actually presses on their
 * physical keyboard.
 */
import { computed } from 'vue'
import { Search } from 'lucide-vue-next'
import { useCommandPalette } from '@/composables/useCommandPalette'
import { useAccessStore } from '@/stores/access'
import { useI18n } from '@/composables/useI18n'

const { t } = useI18n()

withDefaults(
  defineProps<{
    variant?: 'pill' | 'icon'
  }>(),
  { variant: 'pill' },
)

const palette = useCommandPalette()
const access = useAccessStore()

/* Hide the trigger when the user is anonymous — opening the
 * palette eagerly loads several admin-gated command sources
 * (recordings via `dvr/entry/grid_finished`, autorecs via
 * `dvr/autorec/grid`, settings via `config/load`), all of which
 * return 401 and trip the browser's native Digest dialog the
 * instant the palette opens. ExtJS-equivalent behaviour: don't
 * offer the entry-point until the user has chosen to sign in.
 * The same predicate hides the Home `discover-palette` /
 * `discover-palette-touch` discoverability tiles so we don't
 * point at a missing button. */
const visible = computed(() => access.authMode !== 'anonymous')

/* navigator.platform is technically deprecated but every modern
 * browser still returns it for backward compat — the modern
 * `navigator.userAgentData.platform` isn't shipping on Safari
 * yet. String contains rather than equality because the values
 * vary ("MacIntel", "iPhone", "iPad", …). */
const isMac = computed<boolean>(() => {
  const p = globalThis.navigator?.platform ?? ''
  return /Mac|iPhone|iPad|iPod/i.test(p)
})

const modifierLabel = computed<string>(() => (isMac.value ? '⌘' : 'Ctrl'))

function onClick(): void {
  palette.toggle()
}
</script>

<template>
  <button
    v-if="visible && variant === 'pill'"
    type="button"
    class="cmd-trigger cmd-trigger--pill"
    :title="t('Search anywhere — {0}K', modifierLabel)"
    :aria-label="t('Open command palette')"
    @click="onClick"
  >
    <Search :size="16" :stroke-width="2" class="cmd-trigger__icon" />
    <span class="cmd-trigger__label">{{ t('Search…') }}</span>
    <span class="cmd-trigger__shortcut" aria-hidden="true">
      <kbd>{{ modifierLabel }}</kbd>
      <kbd>K</kbd>
    </span>
  </button>
  <button
    v-else-if="visible"
    type="button"
    class="cmd-trigger cmd-trigger--icon"
    :title="t('Search anywhere — {0}K', modifierLabel)"
    :aria-label="t('Open command palette')"
    @click="onClick"
  >
    <Search :size="20" :stroke-width="2" />
  </button>
</template>

<style scoped>
/* Pill variant — same chrome shape as `.nav-rail__toggle` and the
 * search bars in Linear / Slack: muted background, hover lift,
 * focus ring. Sits in the NavRail brand row so the height matches
 * the brand-row height (48px) via vertical centering. */
.cmd-trigger {
  background: none;
  border: 0;
  color: var(--tvh-text);
  font: inherit;
  cursor: pointer;
  transition:
    background var(--tvh-transition),
    border-color var(--tvh-transition);
}

.cmd-trigger--pill {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  width: 100%;
  height: 32px;
  padding: 0 var(--tvh-space-3);
  background: var(--tvh-bg-page);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text-muted);
  text-align: left;
}

.cmd-trigger--pill:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), var(--tvh-bg-page));
  border-color: color-mix(in srgb, var(--tvh-primary) 40%, var(--tvh-border));
}

.cmd-trigger--pill:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.cmd-trigger__icon {
  color: var(--tvh-text-muted);
  flex-shrink: 0;
}

.cmd-trigger__label {
  flex: 1;
  font-size: var(--tvh-text-sm);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.cmd-trigger__shortcut {
  display: inline-flex;
  align-items: center;
  gap: 2px;
  flex-shrink: 0;
}

.cmd-trigger__shortcut kbd {
  display: inline-block;
  min-width: 18px;
  padding: 1px 5px;
  font: inherit;
  font-size: var(--tvh-text-xs);
  font-family: inherit;
  text-align: center;
  color: var(--tvh-text-muted);
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: 3px;
  line-height: 1.2;
}

/* Icon variant — phone TopBar real estate. Matches the
 * `.top-bar__menu-toggle` chrome (hamburger) so the two buttons
 * read as a pair. */
.cmd-trigger--icon {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  flex-shrink: 0;
}

.cmd-trigger--icon:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.cmd-trigger--icon:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
