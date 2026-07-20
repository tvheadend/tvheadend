// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Content-type (EIT genre) filter options for the EPG Table view,
 * shared by the view-options popover and the "Content Type" column
 * filter so both offer the same list from one source.
 *
 * Options are sourced from the shared EPG content-types store (the
 * same store the event drawer's Classification block uses); `ensure()`
 * is idempotent and lazily triggers the first fetch. Before the labels
 * arrive the list is empty and the MultiSelect just shows its
 * placeholder.
 */
import { computed, type ComputedRef } from 'vue'
import { useEpgContentTypeStore } from '@/stores/epgContentTypes'

export interface GenreOption {
  value: number
  label: string
}

export function useEpgGenreOptions(): ComputedRef<GenreOption[]> {
  const contentTypes = useEpgContentTypeStore()
  contentTypes.ensure()

  return computed<GenreOption[]>(() => {
    const out: GenreOption[] = []
    for (const [code, name] of contentTypes.labels.entries()) {
      /* Restrict to MAJOR-group codes (low nibble zero — 0x10 / 0x20
       * / ... / 0xF0). The server's `epg_genre_list_contains`
       * (`src/epg.c:2256`) widens the match mask to `0xF0` only when
       * the selected code has a zero low nibble (`partial && !(code &
       * 0x0F)`), so picking a major group catches every event tagged
       * with any subtype underneath it. Picking a subtype (e.g.
       * "Detective" = 0x11) requires an exact match — most
       * broadcasters only tag the major group, so subtype picks
       * return empty results in practice. Cell + drawer rendering
       * keep using the full label map for lookup.
       *
       * No explicit "Any" entry — MultiSelect represents the
       * empty-filter state via an empty model-value array. */
      if (code & 0x0f) continue
      out.push({ value: code, label: name })
    }
    return out
  })
}
