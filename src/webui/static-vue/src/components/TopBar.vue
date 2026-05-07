<script setup lang="ts">
import { Menu } from 'lucide-vue-next'

defineEmits<{ toggleRail: [] }>()

/*
 * NOTE: cross-UI reference. Bound as an expression (not a literal `src`)
 * so Vite's Vue plugin doesn't try to bundle the asset — the URL is
 * served at runtime by tvheadend's existing /static handler from the
 * ExtJS bundle (src/webui/static/img/logo.png, registered in
 * webui_init() in src/webui/webui.c). When the ExtJS UI is retired,
 * copy the asset into static-vue/src/assets/ and replace this with a
 * Vite-imported reference.
 */
const logoUrl = '/static/img/logo.png'
</script>

<template>
  <header class="top-bar">
    <button
      class="top-bar__menu-toggle"
      aria-label="Toggle navigation"
      @click="$emit('toggleRail')"
    >
      <Menu :size="20" :stroke-width="2" />
    </button>
    <img :src="logoUrl" alt="" class="top-bar__logo" />
    <span class="top-bar__title">Tvheadend</span>
    <div class="top-bar__spacer" />
  </header>
</template>

<style scoped>
.top-bar {
  height: var(--tvh-topbar-height);
  flex-shrink: 0;
  background: var(--tvh-bg-surface);
  border-bottom: 1px solid var(--tvh-border);
  display: flex;
  align-items: center;
  padding: 0 var(--tvh-space-4);
  gap: var(--tvh-space-3);
}

.top-bar__menu-toggle {
  display: none;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  border-radius: var(--tvh-radius-sm);
  color: var(--tvh-text);
  transition: background var(--tvh-transition);
}

.top-bar__menu-toggle:hover {
  background: color-mix(in srgb, var(--tvh-primary) var(--tvh-hover-strength), transparent);
}

@media (max-width: 767px) {
  .top-bar__menu-toggle {
    display: flex;
  }
}

.top-bar__logo {
  height: 28px;
  width: auto;
}

.top-bar__title {
  font-weight: 600;
  font-size: 15px;
  letter-spacing: -0.01em;
}

.top-bar__spacer {
  flex: 1;
}
</style>
