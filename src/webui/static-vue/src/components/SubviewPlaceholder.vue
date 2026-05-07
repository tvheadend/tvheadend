<script setup lang="ts">
/*
 * SubviewPlaceholder — informational card shown in place of a view
 * whose real content isn't built in this UI yet. Carries an escape-
 * hatch link that opens the classic ExtJS interface (`/`, redirected
 * server-side to extjs.html in webui.c) in a new tab so users can
 * still reach the feature.
 *
 * Lives as a component (not duplicated copy in each placeholder view)
 * so consistency stays automatic; each view's body can be swapped
 * for a real implementation without touching this scaffolding.
 *
 * Note: `href="/"` doesn't honour `tvheadend_webroot` prefixes — the
 * v2 UI as a whole doesn't yet (Vite `base` is hardcoded). Tracked
 * in BACKLOG.md under Infrastructure / cross-cutting.
 */
defineProps<{
  /* User-facing label of the absent view, e.g. "Users", "Networks". */
  label: string
}>()
</script>

<template>
  <section class="subview-placeholder">
    <p class="subview-placeholder__message">
      <strong>{{ label }}</strong> isn't available in this interface yet.
    </p>
    <p class="subview-placeholder__action">
      <a
        class="subview-placeholder__link"
        href="/"
        target="_blank"
        rel="noopener noreferrer"
      >
        Open the classic interface in a new tab
        <span class="subview-placeholder__arrow" aria-hidden="true">↗</span>
      </a>
    </p>
  </section>
</template>

<style scoped>
.subview-placeholder {
  padding: var(--tvh-space-6);
  background: var(--tvh-bg-surface);
  border: 1px dashed var(--tvh-border);
  border-radius: var(--tvh-radius-md);
  color: var(--tvh-text-muted);
}

.subview-placeholder strong {
  color: var(--tvh-text);
}

.subview-placeholder__message {
  margin: 0;
}

.subview-placeholder__action {
  margin: var(--tvh-space-3) 0 0;
}

.subview-placeholder__link {
  color: var(--tvh-primary);
  text-decoration: none;
}

.subview-placeholder__link:hover,
.subview-placeholder__link:focus-visible {
  text-decoration: underline;
}

.subview-placeholder__arrow {
  margin-left: 2px;
}
</style>
