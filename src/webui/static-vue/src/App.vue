<!--
  SPDX-License-Identifier: GPL-3.0-or-later
  Copyright (C) 2026 Tvheadend contributors
-->
<script setup lang="ts">
/*
 * Root component. Picks the layout shell based on the active
 * route:
 *
 *   - `/wizard/*` (any route with `meta.isWizard`) renders the
 *     setup wizard's full-viewport layout. NavRail / TopBar are
 *     bypassed entirely while the wizard is active — the wizard
 *     pre-empts the regular UI per ADR 0015 §1.
 *   - Everything else renders inside `AppShell` (NavRail +
 *     TopBar + main).
 *
 * The wizard's own RouterView is inside `WizardLayout`, so the
 * step components mount as children of that route. AppShell's
 * `<RouterView />` is gated by the `v-else` so the wizard
 * route's step components don't double-mount inside it.
 */
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import AppShell from '@/layouts/AppShell.vue'

const route = useRoute()
const isWizardRoute = computed(() => !!route.meta.isWizard)
</script>

<template>
  <!--
    Wizard routes mount the layout via the route's own component
    (the /wizard path resolves to WizardLayout, which itself
    carries a nested router-view for the step children). We rely
    on the plain router-view directive here rather than
    instantiating WizardLayout directly so vue-router resolves the
    route component, including its children, the normal way.
  -->
  <router-view v-if="isWizardRoute" />
  <AppShell v-else />
</template>
