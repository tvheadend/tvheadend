<script setup lang="ts">
/*
 * LevelMenu — toolbar dropdown for picking the view level.
 *
 * Trigger button + popover with three radio options (Basic /
 * Advanced / Expert). Mirrors the level section of GridSettingsMenu
 * but standalone: no columns section, no "Reset to defaults" footer.
 * Used by inline-form views (Configuration → General et al.) that
 * need a per-view view-level toggle without all the grid machinery.
 *
 * GridSettingsMenu remains the right component when both level AND
 * column visibility need to be exposed in one menu. This is the
 * level-only sibling for the case where there are no columns —
 * splitting the two keeps each component's contract honest, and the
 * styling rules are duplicated rather than shared because the
 * volume is small (~30 lines each) and a shared <Popover> primitive
 * isn't yet earned by the rest of the codebase.
 *
 * Strings ("View level", "Basic", "Advanced", "Expert") match the
 * existing translations in `intl/js/tvheadend.js.pot` so vue-i18n
 * will pick them up without re-translation when it lands.
 */
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { SlidersHorizontal } from 'lucide-vue-next'
import type { UiLevel } from '@/types/access'

defineProps<{
  /* Currently effective level (parent computes; we just display + emit). */
  effectiveLevel: UiLevel
  /*
   * Server-pinned lock flag — disables the level radio when true.
   * Driven by `access.locked` (config.uilevel_nochange). Matches
   * GridSettingsMenu's behaviour exactly: clicking a disabled option
   * is a no-op, and a help line below the radios explains why.
   */
  locked: boolean
}>()

const emit = defineEmits<{
  /* User picked a level. Parent owns the effective level and decides
   * whether to persist it (Pinia store, route meta, etc.). */
  setLevel: [level: UiLevel]
}>()

const levelOptions: { value: UiLevel; label: string }[] = [
  { value: 'basic', label: 'Basic' },
  { value: 'advanced', label: 'Advanced' },
  { value: 'expert', label: 'Expert' },
]

const open = ref(false)
const root = ref<HTMLElement | null>(null)

function toggle() {
  open.value = !open.value
}

function pickLevel(v: UiLevel, locked: boolean) {
  if (locked) return
  emit('setLevel', v)
  /* Close after selection — there's nothing else to do in this
   * popover. Differs from GridSettingsMenu where the menu stays
   * open because the user often picks a level AND toggles columns
   * in one session. */
  open.value = false
}

function onDocClick(ev: MouseEvent) {
  if (!root.value) return
  if (!root.value.contains(ev.target as Node)) open.value = false
}

onMounted(() => document.addEventListener('click', onDocClick))
onBeforeUnmount(() => document.removeEventListener('click', onDocClick))
</script>

<template>
  <div ref="root" class="level-menu">
    <button
      v-tooltip.bottom="'View level'"
      type="button"
      class="level-menu__btn"
      aria-label="View level"
      aria-haspopup="menu"
      :aria-expanded="open"
      @click="toggle"
    >
      <SlidersHorizontal :size="16" :stroke-width="2" />
    </button>
    <div v-if="open" class="level-menu__popover" role="menu">
      <div class="level-menu__section">
        <div class="level-menu__section-title">View level</div>
        <button
          v-for="opt in levelOptions"
          :key="opt.value"
          type="button"
          class="level-menu__option"
          :class="{
            'level-menu__option--active': effectiveLevel === opt.value,
          }"
          :disabled="locked"
          role="menuitemradio"
          :aria-checked="effectiveLevel === opt.value"
          @click="pickLevel(opt.value, locked)"
        >
          <span class="level-menu__radio" aria-hidden="true">{{
            effectiveLevel === opt.value ? '●' : '○'
          }}</span>
          {{ opt.label }}
        </button>
        <div v-if="locked" class="level-menu__locked-note">
          View level is fixed by the administrator
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.level-menu {
  position: relative;
  display: inline-flex;
}

.level-menu__btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: transparent;
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  transition: background var(--tvh-transition);
  cursor: pointer;
}

.level-menu__btn:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.level-menu__btn:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}

.level-menu__popover {
  position: absolute;
  top: calc(100% + 4px);
  right: 0;
  min-width: 200px;
  background: var(--tvh-bg-surface);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.12);
  padding: 4px 0;
  z-index: 50;
  display: flex;
  flex-direction: column;
}

.level-menu__section {
  display: flex;
  flex-direction: column;
  padding: 4px 0;
}

.level-menu__section-title {
  padding: 4px var(--tvh-space-3);
  font-size: 11px;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--tvh-text-muted);
}

.level-menu__option {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 6px var(--tvh-space-3);
  background: transparent;
  border: none;
  color: var(--tvh-text);
  font: inherit;
  font-size: 13px;
  text-align: left;
  cursor: pointer;
}

.level-menu__option:hover:not(:disabled) {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

.level-menu__option:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.level-menu__option:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: -2px;
}

.level-menu__option--active {
  font-weight: 500;
}

.level-menu__radio {
  color: var(--tvh-primary);
  width: 14px;
}

.level-menu__locked-note {
  padding: 4px var(--tvh-space-3) 6px;
  font-size: 11px;
  color: var(--tvh-text-muted);
  font-style: italic;
}

/* Phone — same triage rationale as GridSettingsMenu. Phone is locked
   to Basic; level customisation in that surface isn't useful. */
@media (max-width: 767px) {
  .level-menu {
    display: none;
  }
}
</style>
