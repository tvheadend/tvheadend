// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeFieldPerm — Unix-style permission editor (3×3 matrix +
 * editable octal text input + collapsed special-bits disclosure).
 *
 * Pins the bidirectional sync between the matrix and the octal
 * input, the canonical 4-digit emission shape, and the auto-open
 * behaviour for special bits.
 */
import { beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { setActivePinia, createPinia } from 'pinia'
import IdnodeFieldPerm from '../IdnodeFieldPerm.vue'
import type { IdnodeProp } from '@/types/idnode'

const PERM_PROP: IdnodeProp = {
  id: 'directory-permissions',
  type: 'perm',
  caption: 'Directory permissions',
}

beforeEach(() => {
  setActivePinia(createPinia())
})

describe('IdnodeFieldPerm — matrix rendering', () => {
  it('decomposes 0775 into the correct 9 checkboxes', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0775' },
    })
    const checks = w.findAll('.ifld-perm__matrix input[type="checkbox"]')
    expect(checks).toHaveLength(9)
    /* Matrix order: Owner R/W/X, Group R/W/X, Other R/W/X.
     * 0775 = rwx rwx r-x → all 9 except Other-Write. */
    const states = checks.map((c) => (c.element as HTMLInputElement).checked)
    expect(states).toEqual([true, true, true, true, true, true, true, false, true])
  })

  it('renders the canonical 4-digit octal in the text input', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0664' },
    })
    expect((w.find('.ifld-perm__octal-input').element as HTMLInputElement).value).toBe('0664')
  })
})

describe('IdnodeFieldPerm — checkbox → octal', () => {
  it('toggling Owner-Read on 0000 emits 0400', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0000' },
    })
    const ownerR = w.findAll('.ifld-perm__matrix input[type="checkbox"]')[0]
    await ownerR.setValue(true)
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['0400'])
  })

  it('toggling Other-Write off on 0666 emits 0664', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0666' },
    })
    const otherW = w.findAll('.ifld-perm__matrix input[type="checkbox"]')[7]
    await otherW.setValue(false)
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['0664'])
  })
})

describe('IdnodeFieldPerm — octal input → matrix', () => {
  it('typing a valid 4-digit octal emits canonical form', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0000' },
    })
    const input = w.find('.ifld-perm__octal-input')
    await input.setValue('0750')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['0750'])
  })

  it('typing a 3-digit octal pads to canonical 4-digit', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0000' },
    })
    const input = w.find('.ifld-perm__octal-input')
    await input.setValue('664')
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['0664'])
  })

  it('typing an invalid value (non-octal) emits nothing', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0664' },
    })
    const input = w.find('.ifld-perm__octal-input')
    await input.setValue('9XX')
    expect(w.emitted('update:modelValue')).toBeUndefined()
  })
})

describe('IdnodeFieldPerm — special bits disclosure', () => {
  it('auto-opens when the value carries any special bit', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '2755' /* setgid + 0755 */ },
    })
    const details = w.find('.ifld-perm__special')
    expect(details.attributes('open')).toBeDefined()
  })

  it('stays closed for plain perms', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0775' },
    })
    const details = w.find('.ifld-perm__special')
    expect(details.attributes('open')).toBeUndefined()
  })

  it('toggling setgid on 0755 emits 02755', async () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0755' },
    })
    /* Special-bits row order: setuid, setgid, sticky. */
    const setgid = w.findAll('.ifld-perm__special-row input[type="checkbox"]')[1]
    await setgid.setValue(true)
    expect(w.emitted('update:modelValue')?.[0]).toEqual(['02755'])
  })

  it('keeps the leading zero when a special bit pushes to 4 octal digits', async () => {
    /* The server saves via strtol(s, NULL, 0) — a bare "2775" would
     * parse as DECIMAL 2775 (= 0o5327) and corrupt the stored perms.
     * The emitted string must keep the `0` prefix so base-0 strtol
     * reads it as octal. */
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0775' },
    })
    const setgid = w.findAll('.ifld-perm__special-row input[type="checkbox"]')[1]
    await setgid.setValue(true)
    const emitted = w.emitted('update:modelValue')?.[0]?.[0] as string
    expect(emitted).toMatch(/^0[0-7]+$/)
    expect(emitted).toBe('02775')
  })

  it('round-trips a leading-zero special-bit value through the octal display', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '02755' },
    })
    expect((w.find('.ifld-perm__octal-input').element as HTMLInputElement).value).toBe('02755')
    /* setgid checkbox reflects the special bit. */
    const setgid = w.findAll('.ifld-perm__special-row input[type="checkbox"]')[1]
    expect((setgid.element as HTMLInputElement).checked).toBe(true)
  })
})

describe('IdnodeFieldPerm — readonly', () => {
  it('disables every checkbox + the octal input when prop.rdonly is true', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: { ...PERM_PROP, rdonly: true }, modelValue: '0664' },
    })
    const checks = w.findAll('input[type="checkbox"]')
    expect(checks.length).toBe(12) /* 9 matrix + 3 special */
    for (const c of checks) {
      expect((c.element as HTMLInputElement).disabled).toBe(true)
    }
    expect(
      (w.find('.ifld-perm__octal-input').element as HTMLInputElement).disabled,
    ).toBe(true)
  })
})

describe('IdnodeFieldPerm — tolerant input parsing', () => {
  it('accepts a decimal value like "493" and re-emits as 0755', async () => {
    /* The C side accepts any base on save via strtol(s, NULL, 0).
     * Server-emitted values are always octal but a hand-edited
     * config file might carry decimal; round-trip cleanly. */
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '493' /* decimal 493 = octal 0755 */ },
    })
    /* The display normalises to canonical octal regardless of input shape. */
    expect((w.find('.ifld-perm__octal-input').element as HTMLInputElement).value).toBe('0755')
  })

  it('accepts "0x1ED" hex and renders as 0755', () => {
    const w = mount(IdnodeFieldPerm, {
      props: { prop: PERM_PROP, modelValue: '0x1ED' /* hex 0x1ED = octal 0755 */ },
    })
    expect((w.find('.ifld-perm__octal-input').element as HTMLInputElement).value).toBe('0755')
  })
})
