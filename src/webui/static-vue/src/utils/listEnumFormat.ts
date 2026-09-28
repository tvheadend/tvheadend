// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Cell formatter for an idnode list prop with an inline enum
 * (PT_INT or PT_STR `islist` with an `idnode_slist` or strtab list,
 * e.g. the Bouquets `mapopt` / `chtag` options).
 *
 * The grid endpoint returns the raw option ids (`["mapradio",
 * "encrypted"]`); the labels only come with the class metadata
 * (`prop.enum`, already localised by the server). IdnodeGrid's
 * automatic enum labelling skips list props, so such columns pass
 * this as their `format`.
 *
 * `getMeta` is read on every call, so a cell rendered before the
 * class metadata arrived shows the labels once it does. Until then,
 * and for ids the metadata does not list, the raw id is shown.
 */
import type { IdnodeClassMeta } from '@/types/idnode'

export function listEnumFormat(
  getMeta: () => IdnodeClassMeta | null | undefined,
  field: string,
): (value: unknown) => string {
  return (value: unknown): string => {
    if (!Array.isArray(value) || value.length === 0) return ''
    const options = getMeta()?.props.find((p) => p.id === field)?.enum
    const labels = new Map<string, string>()
    if (Array.isArray(options)) {
      for (const o of options) labels.set(String(o.key), o.val)
    }
    return value.map((k) => labels.get(String(k)) ?? String(k)).join(', ')
  }
}
