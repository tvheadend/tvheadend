// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { describe, expect, it } from 'vitest'
import {
  audioColumns,
  caColumns,
  otherColumns,
  subtitColumns,
  teletextColumns,
  videoColumns,
} from '../esfilterColumns'
import type { ColumnDef } from '@/types/column'

/* These tests pin the per-class field set to the C property
 * tables in `src/esfilter.c`. The grids are intentionally NOT
 * generated from class metadata — we curate columns to control
 * width / ordering / cell components — so the wire-shape can
 * drift if the C side adds a field. The smoke-test catches
 * drift in the most failure-prone direction: structural shape
 * (which fields exist on which class). Cell rendering and
 * server-side filter wiring are runtime concerns (eyeball /
 * future view-mount tests). */

function fieldsOf(cols: ColumnDef[]): string[] {
  return cols.map((c) => c.field)
}

describe('esfilterColumns — common columns', () => {
  /* Every class includes these eight; verify each list contains
   * them so a renamed C field surfaces here, not at runtime. */
  const COMMON = ['enabled', 'type', 'service', 'pid', 'action', 'log', 'comment']

  it.each([
    ['video', videoColumns],
    ['audio', audioColumns],
    ['teletext', teletextColumns],
    ['subtit', subtitColumns],
    ['ca', caColumns],
    ['other', otherColumns],
  ])('%s exposes the common field set', (_label, cols) => {
    const fields = fieldsOf(cols)
    for (const f of COMMON) expect(fields).toContain(f)
  })
})

describe('esfilterColumns — per-class shape', () => {
  it('video / audio / teletext / subtit have language + sindex (no CA fields)', () => {
    for (const cols of [videoColumns, audioColumns, teletextColumns, subtitColumns]) {
      const fields = fieldsOf(cols)
      expect(fields).toContain('language')
      expect(fields).toContain('sindex')
      expect(fields).not.toContain('CAid')
      expect(fields).not.toContain('CAprovider')
    }
  })

  it('CA omits language; adds CAid + CAprovider; keeps sindex', () => {
    const fields = fieldsOf(caColumns)
    expect(fields).not.toContain('language')
    expect(fields).toContain('CAid')
    expect(fields).toContain('CAprovider')
    expect(fields).toContain('sindex')
  })

  it('Other has language; lacks sindex (and no CA fields)', () => {
    const fields = fieldsOf(otherColumns)
    expect(fields).toContain('language')
    expect(fields).not.toContain('sindex')
    expect(fields).not.toContain('CAid')
    expect(fields).not.toContain('CAprovider')
  })
})

describe('esfilterColumns — phone visibility', () => {
  /* Across all six classes the phone-card view surfaces the same
   * four-field set: type (bold headline), enabled + action
   * (2-up secondary row), service (full-width trailer). Inline-
   * static enums (`type`, `action`) decode via the metadata
   * key→label map; `service` resolves through the deferred-list
   * fetch. The rest stay desktop-only — diagnostics behind a
   * tap. */
  it.each([
    ['video', videoColumns],
    ['audio', audioColumns],
    ['teletext', teletextColumns],
    ['subtit', subtitColumns],
    ['ca', caColumns],
    ['other', otherColumns],
  ])('%s makes type / enabled / action / service phone-visible', (_label, cols) => {
    const phoneVisible = cols
      .filter((c) => c.minVisible === 'phone')
      .map((c) => c.field)
    expect(phoneVisible.sort()).toEqual(['action', 'enabled', 'service', 'type'])
  })

  it.each([
    ['video', videoColumns],
    ['audio', audioColumns],
    ['teletext', teletextColumns],
    ['subtit', subtitColumns],
    ['ca', caColumns],
    ['other', otherColumns],
  ])('%s elects type as the primary phone-card field', (_label, cols) => {
    const primaries = cols.filter((c) => c.phoneRole === 'primary').map((c) => c.field)
    expect(primaries).toEqual(['type'])
  })
})

describe('esfilterColumns — enum wiring', () => {
  /* Deferred enums (not auto-resolved by class metadata) need
   * explicit cellComponent + enumSource. Pin the wiring so a
   * future change that drops the enumSource doesn't regress to
   * raw uuids in the cells. */
  it('language column wires the language/locale deferred enum where present', () => {
    for (const cols of [videoColumns, audioColumns, teletextColumns, subtitColumns, otherColumns]) {
      const lang = cols.find((c) => c.field === 'language')
      expect(lang).toBeDefined()
      expect(lang?.cellComponent).toBeDefined()
      expect(lang?.enumSource).toMatchObject({ type: 'api', uri: 'language/locale' })
    }
  })

  it('service column wires the service/list deferred enum on every class', () => {
    for (const cols of [videoColumns, audioColumns, teletextColumns, subtitColumns, caColumns, otherColumns]) {
      const svc = cols.find((c) => c.field === 'service')
      expect(svc).toBeDefined()
      expect(svc?.cellComponent).toBeDefined()
      expect(svc?.enumSource).toMatchObject({
        type: 'api',
        uri: 'service/list',
        params: { enum: 1 },
      })
    }
  })
})
