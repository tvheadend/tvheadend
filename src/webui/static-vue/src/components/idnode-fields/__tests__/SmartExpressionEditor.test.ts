// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * SmartExpressionEditor — the autorec expression field-group widget.
 *
 * Covered:
 *   - typing emits update('expression', text)
 *   - external groupValues changes replace the local text
 *   - a parse error renders with line:col; the tree is absent
 *   - the branch tree renders one row per node with depth
 *   - a skip toggle rewrites the text minimally (comments intact)
 *     and emits the updated expression
 *   - the root row has no skip toggle
 *   - the all-skipped hint appears when the tree prunes away
 *   - Preview posts the full form context (values + uuid) plus the
 *     current expression text and renders entries / counts
 *   - a server-side expression error from preview is shown verbatim
 */
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { nextTick } from 'vue'
import { setActivePinia, createPinia } from 'pinia'
import SmartExpressionEditor from '../SmartExpressionEditor.vue'
import type { IdnodeProp } from '@/types/idnode'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...a: unknown[]) => apiMock(...a),
}))

const EXPR_PROP: IdnodeProp = {
  id: 'expression',
  type: 'str',
  caption: 'Smart expression',
  multiline: true,
}

const SIMPLE = '{ "any": [ { "title": "polar" }, ' +
  '// beta branch\n { "title": "bears" } ] }'

function make(expression = '', formContext?: {
  uuid: string | null
  values: Record<string, unknown>
}) {
  return mount(SmartExpressionEditor, {
    props: {
      groupProps: { expression: EXPR_PROP },
      groupValues: { expression },
      formContext,
    },
  })
}

beforeEach(() => {
  setActivePinia(createPinia())
  apiMock.mockReset()
})

describe('SmartExpressionEditor', () => {
  it('emits update on typing', async () => {
    const w = make('')
    await w.find('textarea').setValue('{ "title": "x" }')
    const ev = w.emitted('update')!
    expect(ev[ev.length - 1]).toEqual(['expression', '{ "title": "x" }'])
  })

  it('follows external value replacement', async () => {
    const w = make('{ "title": "x" }')
    await w.setProps({ groupValues: { expression: '{ "title": "y" }' } })
    expect((w.find('textarea').element as HTMLTextAreaElement).value)
      .toBe('{ "title": "y" }')
  })

  it('shows a parse error with position and hides the tree', () => {
    const w = make('{ "title": "x", "year": { "min": 2 } }')
    expect(w.find('.sxe__error').text()).toContain('exactly one')
    expect(w.find('.sxe__error').text()).toMatch(/1:\d+/)
    expect(w.find('.sxe__tree').exists()).toBe(false)
  })

  it('renders the branch tree with depths', () => {
    const w = make(SIMPLE)
    const rows = w.findAll('.sxe__tree-row')
    expect(rows).toHaveLength(3)
    expect(rows[0]!.text()).toContain('any')
    expect(rows[1]!.text()).toContain('title: "polar"')
    expect(rows[2]!.text()).toContain('title: "bears"')
  })

  it('root row has no skip toggle; children have one', () => {
    const w = make(SIMPLE)
    const rows = w.findAll('.sxe__tree-row')
    expect(rows[0]!.find('input[type=checkbox]').exists()).toBe(false)
    expect(rows[1]!.find('input[type=checkbox]').exists()).toBe(true)
  })

  it('root row carries the master enabled toggle with form context', async () => {
    const w = make(SIMPLE, { uuid: 'abcd', values: { enabled: true } })
    const root = w.findAll('.sxe__tree-row')[0]!
    const box = root.find('input[type=checkbox]')
    expect(box.exists()).toBe(true)
    expect((box.element as HTMLInputElement).checked).toBe(true)
    await box.setValue(false)
    const ev = w.emitted('update')!
    /* the settings field, never the expression text */
    expect(ev[ev.length - 1]).toEqual(['enabled', false])
  })

  it('a disabled rule dims the tree without striking it through', () => {
    const w = make(SIMPLE, { uuid: 'abcd', values: { enabled: false } })
    expect(w.find('.sxe__tree--disabled').exists()).toBe(true)
    expect(w.findAll('.sxe__tree-row--skipped')).toHaveLength(0)
  })

  it('skip toggle rewrites minimally, keeps comments, emits', async () => {
    const w = make(SIMPLE)
    const box = w.findAll('.sxe__tree-row')[1]!.find('input[type=checkbox]')
    await box.setValue(false)
    const ev = w.emitted('update')!
    const text = ev[ev.length - 1]![1] as string
    expect(text).toContain('"title": "polar", "skip": true')
    expect(text).toContain('// beta branch')
    /* the row renders struck-through */
    expect(w.findAll('.sxe__tree-row--skipped')).toHaveLength(1)
  })

  it('shows the all-skipped hint when the tree prunes away', () => {
    const w = make('{ "any": [ { "title": "x", "skip": true } ] }')
    expect(w.find('.sxe__hint').text()).toContain('matches nothing')
  })

  it('preview posts full form context and renders results', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { eventId: 7, channelName: 'Nature', title: 'Polar Bear Diaries',
          subtitle: 'On thin ice', start: 1784181600, stop: 1784185200,
          disposition: 'record' },
      ],
      matched: 1,
      scanned: 125,
    })
    const w = make('{ "title": "polar" }', {
      uuid: 'abcd',
      values: { name: 'rule', comment: '', expression: 'stale' },
    })
    await w.find('.sxe__preview-btn').trigger('click')
    await nextTick()
    await nextTick()
    expect(apiMock).toHaveBeenCalledTimes(1)
    const [endpoint, params] = apiMock.mock.calls[0]!
    expect(endpoint).toBe('dvr/autorec/preview')
    const conf = JSON.parse((params as { conf: string }).conf)
    /* the live textarea text wins over the (stale) form value; empty
     * fields are omitted; the uuid rides along for edit previews */
    expect(conf.expression).toBe('{ "title": "polar" }')
    expect(conf.name).toBe('rule')
    expect(conf.comment).toBeUndefined()
    expect(conf.uuid).toBe('abcd')
    expect(w.find('.sxe__counts').text()).toContain('1')
    expect(w.find('.sxe__results').text()).toContain('Polar Bear Diaries')
    expect(w.find('.sxe__results').text()).toContain('will record')
  })

  it('marks duplicate-handling skips with the provisional label', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { eventId: 7, channelName: 'Nature', title: 'Polar Bear Diaries',
          start: 1784181600, stop: 1784185200, disposition: 'record' },
        { eventId: 8, channelName: 'Nature +1', title: 'Polar Bear Diaries',
          start: 1784185200, stop: 1784188800, disposition: 'record',
          duplicate: 1 },
      ],
      matched: 2,
      scanned: 125,
    })
    const w = make('{ "title": "polar" }')
    await w.find('.sxe__preview-btn').trigger('click')
    await nextTick()
    await nextTick()
    const rows = w.findAll('.sxe__results tbody tr')
    expect(rows).toHaveLength(2)
    expect(rows[0]!.text()).toContain('will record')
    expect(rows[0]!.find('.sxe__dup').exists()).toBe(false)
    expect(rows[1]!.find('.sxe__dup').text()).toBe('duplicate')
    /* the tooltip carries the provisional, this-rule-only semantics,
     * phrased after the ExtJS details dialog's rerun note */
    expect(rows[1]!.find('.sxe__dup').attributes('title'))
      .toContain('Will be skipped because it is a rerun')
  })

  it('clears preview results when the expression changes', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        { eventId: 7, channelName: 'Nature', title: 'Polar Bear Diaries',
          start: 1784181600, stop: 1784185200, disposition: 'record' },
      ],
      matched: 1,
      scanned: 125,
    })
    const w = make('{ "title": "polar" }')
    await w.find('.sxe__preview-btn').trigger('click')
    await nextTick()
    await nextTick()
    expect(w.find('.sxe__results').exists()).toBe(true)
    /* stale results must not survive an edit */
    await w.find('textarea').setValue('{ "title": "bears" }')
    await nextTick()
    expect(w.find('.sxe__results').exists()).toBe(false)
  })

  it('shows a server-side expression error verbatim', async () => {
    apiMock.mockResolvedValueOnce({ error: 'unknown operator or leaf "foo"' })
    const w = make('{ "foo": 1 }')
    await w.find('.sxe__preview-btn').trigger('click')
    await nextTick()
    await nextTick()
    expect(w.find('.sxe__error').text())
      .toContain('unknown operator or leaf "foo"')
  })
})
