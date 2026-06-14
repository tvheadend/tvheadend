// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors


// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Channel-list deferred-enum descriptor helper.
 *
 * When a deferred-enum descriptor targets the `channel/list`
 * endpoint, merge in the user's chname display preferences
 * (`numbers` / `sources`) so client-built descriptors render the
 * same channel-name string as the server-built ones.
 *
 * Server-side `channel_class_get_list` (`src/channels.c:248-268`)
 * auto-applies these params when the server builds the descriptor
 * for an idnode field. The client-side hand-built descriptors
 * used by `EnumNameCell` for grid cells (DVR Upcoming's Channel
 * column, the EpgGrabber Channels view, the DVB Services view)
 * bypassed that helper and missed both flags. Symptom: a grid
 * cell showed "Channel One" while the same column's edit dropdown
 * showed "DVB-T: Channel One" — inconsistent.
 *
 * Reads from `useAccessStore()`. Safe to call from any Vue setup
 * / onMounted / computed context where Pinia is active. The
 * deferred-enum cache (`./components/idnode-fields/deferredEnum.ts`)
 * keys on URI + params-json, so flipping either flag invalidates
 * the cache slot and triggers a refetch on the next render — but
 * in practice the General config save handler force-reloads the
 * page (chname_num is in `RELOAD_FIELDS`), so the second-tier
 * refresh path doesn't see use.
 */
import {
  isDeferredEnum,
  type EnumSource,
} from '@/components/idnode-fields/deferredEnum'
import { useAccessStore } from '@/stores/access'

export function resolveChannelListDescriptor(src: EnumSource | undefined): EnumSource | undefined {
  if (!src || !isDeferredEnum(src)) return src
  if (src.uri !== 'channel/list') return src
  const access = useAccessStore()
  const extras: Record<string, unknown> = {}
  if (access.chnameNum) extras.numbers = 1
  if (access.chnameSrc) extras.sources = 1
  if (Object.keys(extras).length === 0) return src
  return { ...src, params: { ...src.params, ...extras } }
}
