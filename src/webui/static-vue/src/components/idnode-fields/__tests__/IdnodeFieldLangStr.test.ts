// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldLangStr — multilingual string editor (per-language
 * row + Add Language).
 *
 * Pins map shape on the wire (object keyed by 3-letter ISO
 * 639-2/B codes), per-row CRUD (add / remove / change language /
 * change text), the picker's used-language filtering, readonly
 * behaviour, and the empty-map default render.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldLangStr from '../IdnodeFieldLangStr.vue'
import type { IdnodeProp } from '@/types/idnode'

const LANGSTR_PROP: IdnodeProp = {
  id: 'title',
  type: 'langstr',
  caption: 'Title',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldLangStr — render', () => {
  it('renders one row per language entry', () => {
    const w = mount(IdnodeFieldLangStr, {
      props: {
        prop: LANGSTR_PROP,
        modelValue: { eng: 'Hello', ger: 'Hallo', fre: 'Bonjour' },
      },
    })
    expect(w.findAll('.ifld-langstr__row')).toHaveLength(3)
  })

  it('preserves wire-order for entries', () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { fre: 'X', eng: 'Y', ger: 'Z' } },
    })
    const langs = w.findAll('.ifld-langstr__lang').map((s) => (s.element as HTMLSelectElement).value)
    expect(langs).toEqual(['fre', 'eng', 'ger'])
  })

  it('shows an empty layout (just the Add button) for an empty map', () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: {} },
    })
    expect(w.findAll('.ifld-langstr__row')).toHaveLength(0)
    expect(w.find('.ifld-langstr__add').exists()).toBe(true)
  })

  it('coerces non-object modelValue (defensive) to empty', () => {
    const w = mount(IdnodeFieldLangStr, {
      /* Server bug or stale cache — just don't crash. */
      props: { prop: LANGSTR_PROP, modelValue: null as unknown as Record<string, string> },
    })
    expect(w.findAll('.ifld-langstr__row')).toHaveLength(0)
  })
})

describe('IdnodeFieldLangStr — text edit', () => {
  it('emits an updated map when a text input changes', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello' } },
    })
    await w.find('.ifld-langstr__text').setValue('Hi')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ eng: 'Hi' }])
  })

  it('preserves other languages when one is edited', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello', ger: 'Hallo' } },
    })
    /* Edit the first row's text input (eng row). */
    await w.findAll('.ifld-langstr__text')[0].setValue('Hi')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ eng: 'Hi', ger: 'Hallo' }])
  })
})

describe('IdnodeFieldLangStr — language change', () => {
  it('renames the key and preserves order when language changes', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello', ger: 'Hallo' } },
    })
    /* Change the first row's language from eng to fre. */
    await w.findAll('.ifld-langstr__lang')[0].setValue('fre')
    /* Order preserved: fre (was eng's slot), then ger. */
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ fre: 'Hello', ger: 'Hallo' }])
  })

  it('hides used languages from the per-row picker', () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello', ger: 'Hallo' } },
    })
    /* The first row should offer eng (its own) + every other
     * language EXCEPT ger (used elsewhere). */
    const firstRowLangs = w.findAll('.ifld-langstr__lang')[0]
      .findAll('option')
      .map((o) => (o.element as HTMLOptionElement).value)
    expect(firstRowLangs).toContain('eng')
    expect(firstRowLangs).not.toContain('ger')
    expect(firstRowLangs).toContain('fre')
  })
})

describe('IdnodeFieldLangStr — add / remove', () => {
  it('Add button defaults to eng when not yet used', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: {} },
    })
    await w.find('.ifld-langstr__add').trigger('click')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ eng: '' }])
  })

  it('Add button picks the next available common language when eng is taken', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello' } },
    })
    await w.find('.ifld-langstr__add').trigger('click')
    /* Next common after eng is ger per the COMMON_LANGS order. */
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ eng: 'Hello', ger: '' }])
  })

  it('Remove button drops the row from the map', async () => {
    const w = mount(IdnodeFieldLangStr, {
      props: { prop: LANGSTR_PROP, modelValue: { eng: 'Hello', ger: 'Hallo' } },
    })
    /* Remove the second row (ger). */
    await w.findAll('.ifld-langstr__remove')[1].trigger('click')
    expect(w.emitted('update:modelValue')?.[0]).toEqual([{ eng: 'Hello' }])
  })
})

describe('IdnodeFieldLangStr — readonly', () => {
  it('disables every select + input + hides Add / Remove', () => {
    const w = mount(IdnodeFieldLangStr, {
      props: {
        prop: { ...LANGSTR_PROP, rdonly: true },
        modelValue: { eng: 'Hello', ger: 'Hallo' },
      },
    })
    for (const s of w.findAll('.ifld-langstr__lang')) {
      expect((s.element as HTMLSelectElement).disabled).toBe(true)
    }
    for (const i of w.findAll('.ifld-langstr__text')) {
      expect((i.element as HTMLInputElement).disabled).toBe(true)
    }
    expect(w.find('.ifld-langstr__add').exists()).toBe(false)
    expect(w.find('.ifld-langstr__remove').exists()).toBe(false)
  })
})

describe('IdnodeFieldLangStr — niche language code', () => {
  it('renders a non-common language code as its own option', () => {
    const w = mount(IdnodeFieldLangStr, {
      /* `und` (Undetermined) is in TVH's full lang_codes table but
       * not in our common subset. The renderer still has to
       * display it. */
      props: { prop: LANGSTR_PROP, modelValue: { und: 'Mystery' } },
    })
    const select = w.find('.ifld-langstr__lang')
    expect((select.element as HTMLSelectElement).value).toBe('und')
  })
})
