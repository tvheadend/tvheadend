/*
 * useEpgViewWrapper — shared script-setup glue for the route-
 * level wrappers TimelineView and MagazineView. Both views
 * derive identical bits from `state = useEpgViewState()`:
 *
 *   - `now` cursor (1-second tick).
 *   - DVR-overlay-bar click → singleton AppShell DVR editor.
 *   - Channel + event mappings for the rendering component
 *     (passes through the relevant per-event fields).
 *   - `loadingDays` Set serialised to an array prop.
 *   - `pxPerMinute` and `titleOnly` density-derived knobs.
 *
 * The initial scroll-to-now latch lives in a sibling
 * `useEpgInitialScrollToNow` composable — separate so the view
 * can wire the `scrollToNow` callback (which depends on
 * `pxPerMinute`, returned from this wrapper) without
 * declaration-order acrobatics.
 */
import { computed } from 'vue'
import { useDvrEditor } from './useDvrEditor'
import { useNowCursor } from './useNowCursor'
import { isTitleOnlyDensity, pxPerMinuteFor } from '@/views/epg/epgViewOptions'
import type { useEpgViewState } from './useEpgViewState'

export function useEpgViewWrapper(
  state: ReturnType<typeof useEpgViewState>,
  /*
   * Per-view density slot. Density is persisted per-view
   * (`viewOptions.density: { timeline, magazine }`) so the
   * wrapper has to know which slot to read for `pxPerMinute`
   * and `titleOnly`. Caller (TimelineView / MagazineView)
   * passes a literal.
   */
  which: 'timeline' | 'magazine',
) {
  /* DVR overlay-bar click → singleton AppShell IdnodeEditor. URL
   * gains `?editUuid=…` (router.replace, no history pollution);
   * on close the user is still on the originating EPG view at the
   * same day + scroll position. */
  const dvrEditor = useDvrEditor()
  function onDvrClick(uuid: string) {
    dvrEditor.open(uuid)
  }

  const { now } = useNowCursor()

  /* Density-derived layout knobs. Renderer stays generic — it
   * accepts numeric / boolean props and doesn't know about the
   * Density enum. Magazine's column-width and Timeline's
   * row-height come from separate density helpers in the
   * caller — they're per-axis quantities the wrapper doesn't
   * own. */
  const pxPerMinute = computed(() => pxPerMinuteFor(state.viewOptions.value.density[which]))
  const titleOnly = computed(() => isTitleOnlyDensity(state.viewOptions.value.density[which]))

  /* loadingDays Set → array for the prop. Vue tracks the Set
   * reactively because the source ALWAYS reassigns with a new
   * Set, never mutates in place. */
  const loadingDaysArray = computed(() => [...state.loadingDays.value])

  /* Channel + event mapping for the rendering component. Both
   * Timeline and Magazine consume the same row shapes; the
   * difference is purely visual (positioning). */
  const channels = computed(() =>
    state.filteredChannels.value.map((c) => ({
      uuid: c.uuid,
      name: c.name,
      number: c.number,
      icon: c.icon_public_url,
    })),
  )

  const events = computed(() =>
    state.filteredEvents.value.map((e) => ({
      eventId: e.eventId,
      channelUuid: e.channelUuid,
      start: e.start,
      stop: e.stop,
      title: e.title,
      subtitle: e.subtitle,
      summary: e.summary,
      description: e.description,
    })),
  )

  return {
    now,
    pxPerMinute,
    titleOnly,
    onDvrClick,
    loadingDaysArray,
    channels,
    events,
  }
}
