<script setup lang="ts">
/*
 * EpgTagFilterSection — the "Tags" section inside an EPG view's
 * settings popover. Extracted from EpgViewOptions so it can be
 * reused by EpgTableOptions (Table view) without duplicating the
 * checkbox + bulk-action + (no tag) pseudo-row UI.
 *
 * Renders the section + the trailing divider together. When there's
 * nothing to filter against (zero user-facing tags AND no untagged
 * channels) the component renders nothing — including no divider —
 * so the parent doesn't need to coordinate visibility separately.
 *
 * State is owned by the parent (which holds the full `EpgViewOptions`
 * shape via `useEpgViewState`); we receive only the `tagFilter` slice
 * and emit a replacement on every change. UUIDs are stored EXCLUDED
 * (empty list = "show all") for the same reason as the parent: tag
 * additions on the server should default-include without forcing the
 * user to opt in.
 */
import { computed } from 'vue'
import type { TagFilter } from './epgViewOptions'
import type { ChannelTag } from '@/composables/useEpgViewState'

const props = defineProps<{
  /* Current filter state — two-way bound via v-model:tagFilter. */
  tagFilter: TagFilter
  /*
   * User-facing tag list (already filtered to non-internal +
   * enabled by the composable). Drives the per-tag checkbox rows
   * and the "Select all / none" bulk actions.
   */
  tags: ChannelTag[]
  /*
   * Whether at least one channel exists with zero tags. Drives the
   * "(no tag)" pseudo-checkbox — only render it when there's
   * something untagged to filter against.
   */
  hasUntaggedChannel: boolean
}>()

const emit = defineEmits<{
  'update:tagFilter': [value: TagFilter]
}>()

/* Toggle one tag's inclusion. Checking a box removes the UUID from
 * the excluded set; unchecking adds it. */
function setTagIncluded(uuid: string, included: boolean) {
  const current = props.tagFilter.excluded
  let excluded: string[]
  if (included) {
    excluded = current.filter((u) => u !== uuid)
  } else if (current.includes(uuid)) {
    excluded = current
  } else {
    excluded = [...current, uuid]
  }
  emit('update:tagFilter', { ...props.tagFilter, excluded })
}

function setIncludeUntagged(value: boolean) {
  emit('update:tagFilter', { ...props.tagFilter, includeUntagged: value })
}

function isTagIncluded(uuid: string): boolean {
  return !props.tagFilter.excluded.includes(uuid)
}

/* Bulk action — flip every real tag's checkbox AND the (no tag)
 * pseudo-checkbox at once. */
function setAllTags(included: boolean) {
  emit('update:tagFilter', {
    excluded: included ? [] : props.tags.map((t) => t.uuid),
    includeUntagged: included,
  })
}

/* "Only this tag" — select a single tag and exclude every other.
 * `includeUntagged` is left untouched on purpose: the (no tag)
 * pseudo-checkbox is independently controlled. If the user wants
 * "really only Sports, no untagged either", they uncheck the
 * (no tag) checkbox after. */
function setOnlyTag(uuid: string) {
  emit('update:tagFilter', {
    ...props.tagFilter,
    excluded: props.tags.map((t) => t.uuid).filter((u) => u !== uuid),
  })
}

/* Visual hints for the bulk-action links — muted when the action
 * is a no-op (already in target state). Buttons stay clickable;
 * muting just signals "nothing to do here". */
const allTagsSelected = computed(
  () => props.tagFilter.excluded.length === 0 && props.tagFilter.includeUntagged
)
const noTagsSelected = computed(
  () =>
    props.tagFilter.excluded.length === props.tags.length && !props.tagFilter.includeUntagged
)

/* Section visibility: hide entirely when there's nothing to filter
 * against. The filter UI would just be confusing in that case. */
const showSection = computed(() => props.tags.length > 0 || props.hasUntaggedChannel)
</script>

<template>
  <template v-if="showSection">
    <div class="settings-popover__section">
      <div class="settings-popover__section-title">Tags</div>
      <!--
        Bulk-action row: flip every checkbox at once. Hidden when
        there are zero real tags (only the "(no tag)" pseudo-row
        is visible) since "Select all / none" of one item is
        pointless. Muted styling on whichever side is already
        satisfied gives a subtle "no-op" hint without disabling
        the button.
      -->
      <div v-if="tags.length > 0" class="epg-tag-filter__bulk-actions">
        <button
          type="button"
          class="epg-tag-filter__bulk-link"
          :class="{ 'epg-tag-filter__bulk-link--muted': allTagsSelected }"
          @click="setAllTags(true)"
        >
          Select all
        </button>
        <span class="epg-tag-filter__bulk-sep">·</span>
        <button
          type="button"
          class="epg-tag-filter__bulk-link"
          :class="{ 'epg-tag-filter__bulk-link--muted': noTagsSelected }"
          @click="setAllTags(false)"
        >
          Select none
        </button>
      </div>
      <div class="epg-tag-filter__list">
        <!--
          Each tag row is a flex container so the trailing "Only"
          link can sit at the right edge without disturbing the
          checkbox alignment. "Only" appears on row hover (CSS
          opacity transition) — keeps the row clean at rest.
        -->
        <label
          v-for="t in tags"
          :key="t.uuid"
          class="settings-popover__option epg-tag-filter__row"
        >
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="isTagIncluded(t.uuid)"
            @change="setTagIncluded(t.uuid, ($event.target as HTMLInputElement).checked)"
          />
          {{ t.name ?? t.uuid }}
          <button
            type="button"
            class="epg-tag-filter__only"
            :title="`Show only ${t.name ?? t.uuid}`"
            @click.prevent="setOnlyTag(t.uuid)"
          >
            Only
          </button>
        </label>
        <label v-if="hasUntaggedChannel" class="settings-popover__option">
          <input
            type="checkbox"
            class="settings-popover__checkbox"
            :checked="tagFilter.includeUntagged"
            @change="setIncludeUntagged(($event.target as HTMLInputElement).checked)"
          />
          (no tag)
        </label>
      </div>
    </div>
    <hr class="settings-popover__divider" />
  </template>
</template>

<style scoped>
/*
 * Scroll the tag list when there are many configured tags. Caps the
 * popover at a sensible height instead of growing unbounded down the
 * page. The 200 px budget fits ~10 tag rows at the
 * `settings-popover__option`'s 30 px line-height — beyond that the
 * user scrolls within the section.
 */
.epg-tag-filter__list {
  display: flex;
  flex-direction: column;
  max-height: 200px;
  overflow-y: auto;
}

/*
 * Bulk-action row sits between the section title and the tag list.
 * Compact link-style buttons + a centred middle-dot separator —
 * smaller font (12 px) than the per-tag rows (13 px) so the row
 * reads as a secondary affordance, not another tag entry.
 */
.epg-tag-filter__bulk-actions {
  display: flex;
  align-items: center;
  gap: var(--tvh-space-2);
  padding: 0 var(--tvh-space-2) var(--tvh-space-2);
  font-size: 12px;
}

.epg-tag-filter__bulk-link {
  background: none;
  border: none;
  padding: 0;
  color: var(--tvh-primary);
  cursor: pointer;
  text-decoration: underline;
  font: inherit;
}

.epg-tag-filter__bulk-link:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}

/* Muted variant — applied when the action is a no-op (form already
 * in the target state). Button stays clickable; the visual hint is
 * just "nothing to do here right now". */
.epg-tag-filter__bulk-link--muted {
  color: var(--tvh-text-muted);
  text-decoration: none;
}

.epg-tag-filter__bulk-sep {
  color: var(--tvh-text-muted);
}

/*
 * Per-tag row layout. Override the default flex direction from
 * `.settings-popover__option` so the "Only" button can sit at the
 * trailing edge without disturbing the checkbox + caption flow.
 */
.epg-tag-filter__row {
  display: flex;
  align-items: center;
}

/*
 * Trailing "Only" link — fades in on row hover. `margin-left: auto`
 * pushes it to the right edge so short tag captions don't pull it
 * inward. Slightly smaller font and underlined to read as a link
 * rather than another control. `:focus-visible` reveals it for
 * keyboard users without requiring hover.
 */
.epg-tag-filter__only {
  margin-left: auto;
  background: none;
  border: none;
  padding: 0 4px;
  color: var(--tvh-primary);
  cursor: pointer;
  font: inherit;
  font-size: 11px;
  text-decoration: underline;
  opacity: 0;
  transition: opacity 0.15s;
}

.epg-tag-filter__row:hover .epg-tag-filter__only,
.epg-tag-filter__only:focus-visible {
  opacity: 1;
}

/* Touch / no-hover devices: always show the "Only" link. Without
 * this, real phones and tablets (where the primary input doesn't
 * fire `:hover`) leave the affordance permanently invisible. Width-
 * based phone detection is the wrong axis here — a small desktop
 * window still has a working mouse pointer and shouldn't get the
 * always-on treatment. `@media (hover: none)` matches the actual
 * "this device can't hover" condition. */
@media (hover: none) {
  .epg-tag-filter__only {
    opacity: 1;
  }
}

.epg-tag-filter__only:focus-visible {
  outline: 2px solid var(--tvh-primary);
  outline-offset: 2px;
  border-radius: var(--tvh-radius-sm);
}
</style>
