/*
 * Top-channel scroll helper — shared logic for restoring the
 * persisted "top channel" after nav-away/return. Used by both
 * Timeline (vertical channel-list axis) and Magazine
 * (horizontal channel-row axis).
 *
 * The view supplies an offset accessor `(uuid) => number | null`.
 * Returning null means "this channel isn't reachable" — the
 * helper falls through to the first reachable channel in
 * `channels` order. Index-based views (Timeline / Magazine
 * today) compute `index * rowHeight`; DOM-based views could
 * read `element.offsetTop`. Same shape either way.
 *
 * Pure logic; no Vue reactivity. Easier to test (and read) than
 * two view-local impls of the same fallback walk.
 */

export type ScrollAxis = 'vertical' | 'horizontal'

export interface ChannelRowLike {
  uuid: string
}

/* Returns the scroll offset (px) at which the channel's leading
 * edge sits, or null when the channel isn't reachable (tag-
 * filtered out, virtual-scroller hasn't rendered it yet, etc.). */
export type RowOffsetAccessor = (uuid: string) => number | null

/*
 * Position the scroll element so that the row matching `uuid`
 * sits at the leading edge of the visible viewport (top for
 * vertical, left for horizontal). When the row's offset isn't
 * available, fall back to the first channel in `channels` whose
 * accessor returns a non-null offset.
 *
 * No-op when `scrollEl` or `channels` are null/empty, or when no
 * row is reachable.
 *
 * Returns the uuid actually scrolled to (the requested one, the
 * fallback's uuid, or null when no row was reachable). The
 * caller may persist the chosen uuid back so that subsequent
 * fallbacks don't keep retrying the same dead uuid.
 */
export function restoreTopChannel(opts: {
  uuid: string
  channels: ReadonlyArray<ChannelRowLike>
  axis: ScrollAxis
  scrollEl: HTMLElement | null
  getRowOffset: RowOffsetAccessor
}): string | null {
  const { uuid, channels, axis, scrollEl, getRowOffset } = opts
  if (!scrollEl || channels.length === 0) return null

  /* Try the requested uuid first. */
  const requested = getRowOffset(uuid)
  if (requested !== null) {
    applyOffset(scrollEl, requested, axis)
    return uuid
  }

  /* Fallback: walk channels in order, pick the first whose
   * accessor returns a usable offset. */
  for (const ch of channels) {
    const off = getRowOffset(ch.uuid)
    if (off !== null) {
      applyOffset(scrollEl, off, axis)
      return ch.uuid
    }
  }
  return null
}

function applyOffset(scrollEl: HTMLElement, offsetPx: number, axis: ScrollAxis): void {
  if (axis === 'vertical') {
    scrollEl.scrollTop = offsetPx
  } else {
    scrollEl.scrollLeft = offsetPx
  }
}
