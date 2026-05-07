<script setup lang="ts">
/*
 * PageTabs — generic L2 navigation strip used by sectioned views like
 * DVR or (later) Configuration.
 *
 * Desktop / tablet: horizontal row of router-link tabs with a
 * GitHub-style 2 px underline + bold-weight indicator on the active
 * tab. Inactive tabs use the muted text color.
 *
 * Phone (<768 px): tab row collapses into a `<select>` dropdown that
 * navigates on change. Saves horizontal space for narrow screens and
 * stays consistent with the IdnodeGrid sort dropdown so users see one
 * "select to navigate" idiom across the UI rather than two.
 *
 * Active-tab matching: any tab whose `to` is a prefix of the current
 * route path counts as active. This means deeper routes (e.g. a
 * `/dvr/upcoming/<uuid>` detail route) keep the Upcoming tab
 * visually active.
 */
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'

interface PageTab {
  to: string
  label: string
}

const props = defineProps<{
  tabs: PageTab[]
}>()

const route = useRoute()
const router = useRouter()

const activeTo = computed(() => {
  /*
   * Choose the longest matching prefix so nested routes don't
   * accidentally light up a parent's sibling tab. e.g. for
   * /dvr/upcoming we want to match the `/dvr/upcoming` tab, not
   * `/dvr` if it ever existed as a tab too.
   */
  const matches = props.tabs.filter((t) => route.path === t.to || route.path.startsWith(t.to + '/'))
  matches.sort((a, b) => b.to.length - a.to.length)
  return matches[0]?.to ?? props.tabs[0]?.to ?? ''
})

const PHONE_MAX_WIDTH = 768
const isPhone = ref(
  globalThis.window !== undefined && globalThis.window.innerWidth < PHONE_MAX_WIDTH
)
function checkPhone() {
  isPhone.value = globalThis.window.innerWidth < PHONE_MAX_WIDTH
}
onMounted(() => globalThis.window.addEventListener('resize', checkPhone))
onBeforeUnmount(() => globalThis.window.removeEventListener('resize', checkPhone))

function onSelectChange(ev: Event) {
  const target = ev.target as HTMLSelectElement
  if (target.value && target.value !== route.path) {
    router.push(target.value)
  }
}
</script>

<template>
  <nav class="page-tabs" :class="{ 'page-tabs--phone': isPhone }" aria-label="Section navigation">
    <select v-if="isPhone" class="page-tabs__select" :value="activeTo" @change="onSelectChange">
      <option v-for="t in tabs" :key="t.to" :value="t.to">
        {{ t.label }}
      </option>
    </select>
    <template v-else>
      <router-link
        v-for="t in tabs"
        :key="t.to"
        :to="t.to"
        class="page-tabs__tab"
        :class="{ 'page-tabs__tab--active': activeTo === t.to }"
      >
        {{ t.label }}
      </router-link>
    </template>
  </nav>
</template>

<style scoped>
.page-tabs {
  display: flex;
  gap: var(--tvh-space-4);
  align-items: stretch;
  border-bottom: 1px solid var(--tvh-border);
  margin-bottom: var(--tvh-space-4);
  /*
   * Allow the row to scroll horizontally when there are many tabs
   * rather than wrapping; wrapping breaks the underline alignment
   * and makes the row visually jump as new tabs are added.
   */
  overflow-x: auto;
  scrollbar-width: thin;
  /*
   * Never let the flex container above us shrink this nav bar.
   * Without this, when the sibling L3 view has a very tall intrinsic
   * content height (e.g. ConfigGeneralBaseView's form with ~10 detail
   * groups stacking to 1500px+), the flex algorithm in the parent
   * column proportionally shrinks every flex-shrink:1 item — and the
   * default flex value for a non-explicit child is `0 1 auto`. The
   * shrinkage combined with the implicit `overflow-y: auto` (forced
   * by `overflow-x: auto` per CSS Overflow Module Level 3) clips the
   * tabs vertically and the sibling appears to butt against the
   * sliced-off bottom. Pinning shrink to 0 is the right fix because
   * a fixed-height nav bar should never participate in flex shrinkage
   * — its content height is its size.
   */
  flex-shrink: 0;
}

.page-tabs__tab {
  display: inline-flex;
  align-items: center;
  padding: var(--tvh-space-3) 0;
  color: var(--tvh-text-muted);
  text-decoration: none;
  font-weight: 500;
  font-size: 14px;
  border-bottom: 2px solid transparent;
  /* Pull the tab's bottom border 1 px down so it overlaps the row's
     border, producing a clean continuous underline rather than two
     stacked lines. */
  margin-bottom: -1px;
  white-space: nowrap;
  transition:
    color var(--tvh-transition),
    border-color var(--tvh-transition);
}

.page-tabs__tab:hover {
  color: var(--tvh-text);
}

.page-tabs__tab:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}

.page-tabs__tab--active {
  color: var(--tvh-text);
  font-weight: 600;
  border-bottom-color: var(--tvh-primary);
}

/* Phone — replace the tab row with a select dropdown. */
.page-tabs--phone {
  border-bottom: none;
  margin-bottom: var(--tvh-space-3);
  overflow: visible;
}

.page-tabs__select {
  flex: 1;
  background: var(--tvh-bg-page);
  color: var(--tvh-text);
  border: 1px solid var(--tvh-border);
  border-radius: var(--tvh-radius-sm);
  padding: 8px var(--tvh-space-3);
  font: inherit;
  min-height: 40px;
}

.page-tabs__select:focus {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 1px;
}
</style>
