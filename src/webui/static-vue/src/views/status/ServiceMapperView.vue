<script setup lang="ts">
/*
 * ServiceMapperView — placeholder for the Status > Service Mapper
 * tab. The ExtJS UI has a richer "service mapper" panel showing the
 * progress of the mapper job (jobs in progress, queue length, etc.).
 * Porting it touches APIs we don't otherwise use yet, so it lands
 * later.
 *
 * The tab itself is included in StatusLayout's strip so the routing
 * is in place; this body just signals the intent.
 */
import SubviewPlaceholder from '@/components/SubviewPlaceholder.vue'
</script>

<template>
  <SubviewPlaceholder label="Service Mapper" />
</template>
