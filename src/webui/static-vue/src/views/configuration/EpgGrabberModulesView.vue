<script setup lang="ts">
/*
 * Placeholder for Configuration → Channel / EPG → EPG Grabber
 * Modules. Maps the `epggrab_mod` idnode class — admins
 * enable / disable / configure individual EPG grabber modules
 * (EIT, OpenTV, XMLTV external, internal grabbers, etc.) with
 * priorities and per-module settings. Form-grid hybrid in
 * Classic (`static/app/epggrab.js:73-101`) — `idnode_form_grid`
 * with per-module status icons and a detail panel revealed by
 * row selection. Page-level UI is `uilevel: 'advanced'` in
 * Classic; the hybrid layout pattern isn't built yet, deferred.
 */
import SubviewPlaceholder from '@/components/SubviewPlaceholder.vue'
</script>

<template>
  <SubviewPlaceholder label="EPG Grabber Modules" />
</template>
