<script setup lang="ts">
/*
 * Placeholder for Configuration → Channel / EPG → EPG Grabber.
 * Maps the `epggrab` idnode class — global EPG grabber settings
 * (cron schedule, default language, channel rename/renumber
 * options, genre translations) plus toolbar buttons to
 * manually trigger grabber runs. Form-style page in Classic
 * (`static/app/epggrab.js:20-50`), not a grid — uses
 * `idnode_simple` rather than `idnode_grid`. Implementation
 * needs a form-style pattern we don't have yet; deferred.
 */
import SubviewPlaceholder from '@/components/SubviewPlaceholder.vue'
</script>

<template>
  <SubviewPlaceholder label="EPG Grabber" />
</template>
