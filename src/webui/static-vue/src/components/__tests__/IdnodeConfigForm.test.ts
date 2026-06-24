// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeConfigForm unit tests. Focused on the `lockLevel` prop —
 * whose presence pins the displayed view level and hides the
 * `<LevelMenu>` chooser. Mirrors the legacy ExtJS
 * `idnode_simple({ uilevel: 'expert', … })` semantic
 * (config.js:111 / idnode.js:2953) for pages whose entire field set
 * is single-level-gated server-side (e.g. Image Cache, every field
 * `PO_EXPERT`).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import { defineComponent, h } from 'vue'
import IdnodeConfigForm from '../IdnodeConfigForm.vue'
import { useAccessStore } from '@/stores/access'
import { setupApiMockReset } from './__helpers__/idnodeEditorTestUtils'

/* PrimeVue Select can't mount bare in happy-dom without the
 * PrimeVue plugin (its baseinput layer reads `$variant` off the
 * runtime config). IdnodeFieldEnum renders one for every enum
 * field on the form, so every IdnodeConfigForm-mounting test
 * needs a passthrough stub. Mirrors the SELECT_STUB pattern in
 * EnumFilterControl.test.ts + IdnodeFieldEnum.test.ts. */
const SELECT_STUB = defineComponent({
  name: 'SelectStub',
  props: {
    modelValue: { type: [String, Number, null], default: null },
    options: { type: Array, default: () => [] },
    optionValue: { type: String, default: 'value' },
    optionLabel: { type: String, default: 'label' },
    filter: { type: Boolean, default: false },
    disabled: { type: Boolean, default: false },
    ariaLabel: { type: String, default: '' },
    inputId: { type: String, default: '' },
  },
  emits: ['update:modelValue'],
  setup(props, { emit }) {
    return () => {
      type Opt = { key: string | number; val: string }
      const rows = (props.options as Opt[]).map((opt) =>
        h(
          'button',
          {
            class: 'sel-stub__opt',
            'data-key': String(opt.key),
            type: 'button',
            onClick: () => emit('update:modelValue', opt.key),
          },
          opt.val,
        ),
      )
      return h(
        'div',
        {
          class: 'sel-stub',
          'data-filter': String(props.filter),
          'data-value': String(props.modelValue ?? ''),
        },
        rows,
      )
    }
  },
})

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* vue-router mock — IdnodeConfigForm calls `useRoute()` to read
 * the hash for its field-focus watcher (settings palette pushes
 * `#field=<id>`). Tests drive the hash via the module-level
 * `mockHash` variable so each spec can pin a target without
 * spinning up a real router. */
let mockHash = ''
vi.mock('vue-router', () => ({
  useRoute: () => ({
    get hash() {
      return mockHash
    },
  }),
}))

setupApiMockReset(apiMock)

/* Mixed-level field set so the test can prove that the rendered
 * level governs which fields appear: a Basic-level field and an
 * Expert-level field. `propLevel()` reads `p.expert` / `p.advanced`
 * boolean flags directly (see `types/idnode.ts:140`). */
const MIXED_PARAMS = [
  { id: 'name', type: 'str', caption: 'Name', value: '' },
  { id: 'expert_only', type: 'str', caption: 'Expert', value: '', expert: true },
] as const

async function mountWithParams(
  params: ReadonlyArray<Record<string, unknown>>,
  formProps: Partial<{
    lockLevel: 'basic' | 'advanced' | 'expert'
    disabledFor: (id: string, vals: Record<string, unknown>) => boolean
    saveLabel: string
    saveTooltip: string
    preselect: Record<string, unknown>
    alwaysDirty: boolean
    mandatoryFields: ReadonlyArray<string>
  }> = {},
  meta?: { groups?: ReadonlyArray<Record<string, unknown>> }
) {
  apiMock.mockResolvedValueOnce({ entries: [{ params, meta }] })
  const wrapper = mount(IdnodeConfigForm, {
    props: {
      loadEndpoint: 'imagecache/config/load',
      saveEndpoint: 'imagecache/config/save',
      ...formProps,
    },
    global: {
      stubs: { Select: SELECT_STUB },
    },
  })
  await flushPromises()
  return wrapper
}

describe('IdnodeConfigForm — lockLevel', () => {
  it('renders the LevelMenu chooser when lockLevel is not set', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(MIXED_PARAMS)

    expect(wrapper.find('.level-menu').exists()).toBe(true)
  })

  it('hides the LevelMenu chooser when lockLevel is set', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(MIXED_PARAMS, { lockLevel: 'expert' })

    expect(wrapper.find('.level-menu').exists()).toBe(false)
  })

  it('renders enabled inputs when disabledFor is undefined', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams([
      { id: 'unlimited_period', type: 'bool', caption: 'Unlimited time', value: false },
      { id: 'max_period', type: 'u32', caption: 'Maximum period (mins)', value: 60 },
    ])

    /* No disabledFor → max_period input renders enabled. */
    const maxPeriod = wrapper.find('input[type="number"]')
    expect(maxPeriod.exists()).toBe(true)
    expect(maxPeriod.attributes('disabled')).toBeUndefined()
  })

  it('disables fields whose id matches a true disabledFor predicate', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'unlimited_period', type: 'bool', caption: 'Unlimited time', value: true },
        { id: 'max_period', type: 'u32', caption: 'Maximum period (mins)', value: 60 },
      ],
      {
        disabledFor: (id, vals) =>
          id === 'max_period' && Boolean(vals.unlimited_period),
      }
    )

    const maxPeriod = wrapper.find('input[type="number"]')
    expect(maxPeriod.exists()).toBe(true)
    expect(maxPeriod.attributes('disabled')).toBeDefined()
  })

  it('preserves server-side rdonly even when disabledFor returns false (no re-enabling)', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'frozen', type: 'str', caption: 'Frozen', value: 'x', rdonly: true },
      ],
      {
        /* Predicate intentionally returns false; server's rdonly
         * must still apply — disabledFor is additive only. */
        disabledFor: () => false,
      }
    )

    const frozen = wrapper.find('input[type="text"]')
    expect(frozen.exists()).toBe(true)
    expect(frozen.attributes('disabled')).toBeDefined()
  })

  it('reactively re-evaluates disabledFor when a trigger field changes', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'unlimited_period', type: 'bool', caption: 'Unlimited time', value: false },
        { id: 'max_period', type: 'u32', caption: 'Maximum period (mins)', value: 60 },
      ],
      {
        disabledFor: (id, vals) =>
          id === 'max_period' && Boolean(vals.unlimited_period),
      }
    )

    /* Initial: trigger is false, dependent should be enabled. */
    let maxPeriod = wrapper.find('input[type="number"]')
    expect(maxPeriod.attributes('disabled')).toBeUndefined()

    /* Toggle the trigger checkbox → predicate flips → dependent
     * should disable on re-render. The checkbox is the only
     * checkbox-typed input in the fixture. */
    const trigger = wrapper.find('input[type="checkbox"]')
    expect(trigger.exists()).toBe(true)
    await trigger.setValue(true)
    await flushPromises()

    maxPeriod = wrapper.find('input[type="number"]')
    expect(maxPeriod.attributes('disabled')).toBeDefined()
  })

  it('pins the displayed level to lockLevel even when access.uilevel is lower', async () => {
    /* User's effective level is 'basic'. With lockLevel='expert' the
     * page should render expert-gated fields anyway — the whole
     * point of the prop. Without lockLevel, the same field set
     * would show only the basic field. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const lockedWrapper = await mountWithParams(MIXED_PARAMS, { lockLevel: 'expert' })
    const lockedHtml = lockedWrapper.html()
    expect(lockedHtml).toContain('Name')
    expect(lockedHtml).toContain('Expert')

    apiMock.mockReset()
    setActivePinia(createPinia())
    const access2 = useAccessStore()
    access2.data = { admin: true, dvr: true, uilevel: 'basic' }

    const unlockedWrapper = await mountWithParams(MIXED_PARAMS)
    const unlockedHtml = unlockedWrapper.html()
    expect(unlockedHtml).toContain('Name')
    expect(unlockedHtml).not.toContain('Expert')
  })
})

describe('IdnodeConfigForm — mandatoryFields', () => {
  /* IdnodeConfigForm synthesises `prop.mandatory: true` on the
   * listed field ids inside effectiveProp. IdnodeFieldEnum reads
   * that flag to suppress its `(none)` clear-to-null option for
   * str-typed enum singletons (language_ui / theme_ui) that have
   * no clearable equivalent in Classic. Manual allowlist today,
   * future PO_MANDATORY flag tomorrow. */
  const STR_ENUM_PARAMS = [
    {
      id: 'theme_ui',
      type: 'str',
      caption: 'Theme',
      value: 'blue',
      enum: [
        { key: 'blue', val: 'Blue' },
        { key: 'gray', val: 'Gray' },
      ],
    },
  ] as const

  it('renders the `(none)` clear option by default for str-typed enums', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(STR_ENUM_PARAMS)
    const opts = wrapper.findAll('.sel-stub__opt')
    /* Without mandatoryFields the synthetic clear-to-null option
     * still renders, alongside the two real options. */
    expect(opts).toHaveLength(3)
    expect(opts[0].text()).toBe('(none)')
  })

  it('suppresses the `(none)` clear option for listed field ids', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(STR_ENUM_PARAMS, {
      mandatoryFields: ['theme_ui'],
    })
    const opts = wrapper.findAll('.sel-stub__opt')
    /* Only the two real options render — `(none)` is gone. */
    expect(opts).toHaveLength(2)
    for (const o of opts) {
      expect(o.text()).not.toBe('(none)')
    }
  })
})

describe('IdnodeConfigForm — sub-group merging', () => {
  /* Mirrors the dvr_config layout: parent group "Filename / Tagging
   * Settings" (number 4) plus an unnamed sub-group (number 5,
   * parent: 4) carrying secondary filename fields. The legacy
   * ExtJS form nests the sub-group inside the parent fieldset; we
   * flatten by merging sub-group fields into the parent's bucket
   * so users see one collapsible "Filename / Tagging" section
   * containing all 4 fields rather than two adjacent sections
   * (one named, one orphaned). */
  it('merges fields from a sub-group (parent set) into the parent group', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'channel-in-title', type: 'bool', caption: 'Include channel name', value: false, group: 4 },
        { id: 'omit-title', type: 'bool', caption: 'Omit title', value: false, group: 5 },
      ],
      {},
      {
        groups: [
          { number: 4, name: 'Filename / Tagging Settings' },
          { number: 5, name: '', parent: 4 },
        ],
      }
    )

    const html = wrapper.html()
    /* Both fields render. */
    expect(html).toContain('Include channel name')
    expect(html).toContain('Omit title')

    /* Exactly one rendered <details> for the parent group; the
     * sub-group does NOT produce a sibling <section>. */
    const sections = wrapper.findAll('.idnode-config-form__group')
    expect(sections.length).toBe(1)
    /* The single rendered section is the named parent. */
    expect(sections[0].html()).toContain('Filename / Tagging Settings')

    /* Both fields live inside that section — confirms the merge
     * (sub-group field didn't escape to a separate top-level
     * section). */
    expect(sections[0].html()).toContain('Include channel name')
    expect(sections[0].html()).toContain('Omit title')
  })

  it('preserves declaration order: parent fields first, then sub-group fields', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'parent-field', type: 'str', caption: 'AAAParent', value: '', group: 4 },
        { id: 'child-field', type: 'str', caption: 'ZZZChild', value: '', group: 5 },
      ],
      {},
      {
        groups: [
          { number: 4, name: 'Group 4' },
          { number: 5, name: '', parent: 4 },
        ],
      }
    )

    const html = wrapper.html()
    const aaaIdx = html.indexOf('AAAParent')
    const zzzIdx = html.indexOf('ZZZChild')
    expect(aaaIdx).toBeGreaterThan(-1)
    expect(zzzIdx).toBeGreaterThan(-1)
    /* Parent field comes first because we iterate visibleFields in
     * declaration order; the sub-group field appends after. */
    expect(aaaIdx).toBeLessThan(zzzIdx)
  })

  it('falls back gracefully when a sub-group references a missing parent', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    /* Sub-group declares parent: 99 but no group 99 exists. The
     * field should still render somewhere — defensive bound on
     * the parent-walk prevents an infinite loop and the fallback
     * renders the orphan field at its declared group number. */
    const wrapper = await mountWithParams(
      [
        { id: 'orphan-field', type: 'str', caption: 'Orphan', value: '', group: 5 },
      ],
      {},
      {
        groups: [{ number: 5, name: 'Group 5', parent: 99 }],
      }
    )

    expect(wrapper.html()).toContain('Orphan')
  })
})

describe('IdnodeConfigForm — level-bucket fallback', () => {
  /* Mirrors Classic `idnode_simple` at `static/app/idnode.js:
   * 1047-1059`: when the class declares no server-defined
   * groups, fall back to bucketing fields by view level into
   * "Basic Settings" / "Advanced Settings" / "Expert Settings"
   * synthetic fieldsets. Special case: if only the Basic bucket
   * has fields, render a single unnamed section. This is what
   * the user expects for the Timeshift page (no `ic_groups`
   * declared on `timeshift_conf_class`; mixed basic / advanced
   * / expert fields). */
  it('renders titled sections (Basic / Advanced / Expert) when no server groups declared', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams([
      { id: 'enabled', type: 'bool', caption: 'Enabled', value: false },
      { id: 'path', type: 'str', caption: 'Storage path', value: '', advanced: true },
      { id: 'ondemand', type: 'bool', caption: 'On-demand', value: false, expert: true },
    ])

    const html = wrapper.html()
    expect(html).toContain('Basic Settings')
    expect(html).toContain('Advanced Settings')
    expect(html).toContain('Expert Settings')

    /* Section ordering: Basic before Advanced before Expert. */
    const basicIdx = html.indexOf('Basic Settings')
    const advancedIdx = html.indexOf('Advanced Settings')
    const expertIdx = html.indexOf('Expert Settings')
    expect(basicIdx).toBeLessThan(advancedIdx)
    expect(advancedIdx).toBeLessThan(expertIdx)
  })

  it('renders single unnamed section when only Basic-level fields are present', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams([
      { id: 'name', type: 'str', caption: 'Name', value: '' },
      { id: 'comment', type: 'str', caption: 'Comment', value: '' },
    ])

    const html = wrapper.html()
    /* Special case fires: no titled "Basic Settings" header. */
    expect(html).not.toContain('Basic Settings')
    expect(html).not.toContain('Advanced Settings')
    expect(html).not.toContain('Expert Settings')
    /* Both fields still rendered. */
    expect(html).toContain('Name')
    expect(html).toContain('Comment')
  })

  it('renders only the Expert Settings section for an all-expert class (Image cache shape)', async () => {
    /* Image cache: every field is PO_EXPERT. lockLevel='expert'
     * so basic/advanced level filters don't suppress anything.
     * Classic renders this as a single "Expert Settings"
     * fieldset (line 1054 of idnode.js fires; lines 1052/1054/
     * 1056 are if-not-else-if so each adds independently — only
     * Expert fires here because df / af are empty). */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    const wrapper = await mountWithParams(
      [
        { id: 'enabled', type: 'bool', caption: 'Enabled', value: false, expert: true },
        { id: 'expire', type: 'u32', caption: 'Expire (sec)', value: 0, expert: true },
      ],
      { lockLevel: 'expert' }
    )

    const html = wrapper.html()
    expect(html).not.toContain('Basic Settings')
    expect(html).not.toContain('Advanced Settings')
    expect(html).toContain('Expert Settings')
    expect(html).toContain('Enabled')
    expect(html).toContain('Expire')
  })

  it('hides level sections whose fields are filtered out by the active uilevel', async () => {
    /* User at uilevel='basic' with mixed-level fields: only
     * Basic-level fields visible; Advanced + Expert sections
     * shouldn't render at all. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams([
      { id: 'enabled', type: 'bool', caption: 'Enabled', value: false },
      { id: 'path', type: 'str', caption: 'Storage path', value: '', advanced: true },
      { id: 'ondemand', type: 'bool', caption: 'On-demand', value: false, expert: true },
    ])

    const html = wrapper.html()
    /* Only basic-level field is visible; falls to the special
     * unnamed-single-section case. */
    expect(html).not.toContain('Advanced Settings')
    expect(html).not.toContain('Expert Settings')
    expect(html).toContain('Enabled')
    expect(html).not.toContain('Storage path')
    expect(html).not.toContain('On-demand')
  })

  it('does not apply level-bucket fallback when server groups ARE declared', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    /* With explicit server-side groups, the standard group-
     * based path runs even when fields have mixed levels.
     * Classic does the same — `meta.groups` presence flips
     * idnode_editor_form into group-rendering mode. */
    const wrapper = await mountWithParams(
      [
        { id: 'enabled', type: 'bool', caption: 'Enabled', value: false, group: 1 },
        { id: 'path', type: 'str', caption: 'Storage', value: '', advanced: true, group: 1 },
      ],
      {},
      {
        groups: [{ number: 1, name: 'General Settings' }],
      }
    )

    const html = wrapper.html()
    expect(html).toContain('General Settings')
    expect(html).not.toContain('Basic Settings')
    expect(html).not.toContain('Advanced Settings')
  })
})

describe('IdnodeConfigForm — Save button overrides', () => {
  /* `saveLabel` + `saveTooltip` let pages tweak the Save button's
   * wording without slot scaffolding. The Debugging page uses
   * "Apply configuration (run-time only)" with a tooltip
   * explaining changes are runtime-only — matching Classic's
   * `saveText` / `saveTooltip` pattern at `static/app/tvhlog.js:
   * 369-371`. */
  it('renders the default "Save" label when saveLabel is unset', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams([
      { id: 'name', type: 'str', caption: 'Name', value: '' },
    ])

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.text()).toBe('Save')
  })

  it('renders a custom saveLabel when provided', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: '' }],
      { saveLabel: 'Apply configuration (run-time only)' }
    )

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.text()).toBe('Apply configuration (run-time only)')
  })

  it('still flips to "Saving…" while saving regardless of saveLabel', async () => {
    /* The in-flight signal needs to stay consistent so users
     * can't mistake a long-saving form for a stale render. The
     * label override only affects the idle state. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    /* Block the save round-trip on a never-resolving promise so
     * the saving flag stays true while we assert. */
    apiMock.mockResolvedValueOnce({ entries: [{ params: [{ id: 'name', type: 'str', value: 'x' }] }] })
    const wrapper = mount(IdnodeConfigForm, {
      props: {
        loadEndpoint: 'imagecache/config/load',
        saveEndpoint: 'imagecache/config/save',
        saveLabel: 'Apply configuration (run-time only)',
      },
    })
    await flushPromises()

    /* Mark dirty so the save handler proceeds. */
    const input = wrapper.find('input[type="text"]')
    await input.setValue('y')
    await flushPromises()

    /* Mock the save to a never-resolving promise so saving
     * stays true through the assertion. */
    apiMock.mockReturnValueOnce(new Promise(() => {}))
    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    await saveBtn.trigger('click')
    await flushPromises()

    expect(saveBtn.text()).toBe('Saving…')
  })
})

describe('IdnodeConfigForm — preselect', () => {
  /* `preselect` lets the caller override loaded values for
   * specific fields without rewriting baseline. Used for
   * caller-driven preselects that the server doesn't persist
   * itself — Service Mapper's `services` field is the canonical
   * consumer (preselected from grid selection on Channels /
   * DVB Services pages). */
  it('overrides loaded values for matching field IDs', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [
        { id: 'name', type: 'str', caption: 'Name', value: 'server-name' },
        { id: 'comment', type: 'str', caption: 'Comment', value: 'server-comment' },
      ],
      { preselect: { name: 'preselected-name' } },
    )

    /* The first text input (Name) should reflect the
     * preselected value, not the loaded one. The second
     * (Comment) is untouched. */
    const inputs = wrapper.findAll('input[type="text"]')
    expect((inputs[0].element as HTMLInputElement).value).toBe('preselected-name')
    expect((inputs[1].element as HTMLInputElement).value).toBe('server-comment')
  })

  it('lights up the dirty marker on overridden fields (Save enables immediately)', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: 'server-name' }],
      { preselect: { name: 'preselected-name' } },
    )

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('ignores preselect keys that don\'t match a loaded field id', async () => {
    /* Defensive — preselects from a stale URL or a server schema
     * change shouldn't pollute currentValues with phantom keys. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: 'server-name' }],
      { preselect: { name: 'preselected-name', ghost: 'should-not-appear' } },
    )

    /* Form still works + only the real field is overridden;
     * ghost key has no row to render against. */
    const inputs = wrapper.findAll('input[type="text"]')
    expect((inputs[0].element as HTMLInputElement).value).toBe('preselected-name')
  })

  it('with no preselect, behaviour is unchanged (no dirty fields after load)', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams([
      { id: 'name', type: 'str', caption: 'Name', value: 'server-name' },
    ])

    /* Save disabled because nothing's dirty — the regression
     * gate. */
    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeDefined()
  })
})

describe('IdnodeConfigForm — alwaysDirty (trigger forms)', () => {
  /* `alwaysDirty=true` is the escape hatch for forms whose
   * Save button represents "fire this action" rather than
   * "persist these field changes". Service Mapper is the
   * canonical consumer — Map services should always be
   * clickable, even when the form's options match the server's
   * last-saved state. Mirrors Classic's
   * `idnode_editor_win({ alwaysDirty: true })` flag. */
  it('Save button stays enabled with alwaysDirty=true on a clean form', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: 'unchanged' }],
      { alwaysDirty: true },
    )

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('save() proceeds with alwaysDirty=true on a clean form', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: 'unchanged' }],
      { alwaysDirty: true },
    )

    /* Mock the save POST + the post-save reload load(). */
    apiMock.mockResolvedValueOnce({})
    apiMock.mockResolvedValueOnce({
      entries: [{ params: [{ id: 'name', type: 'str', caption: 'Name', value: 'unchanged' }] }],
    })

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    await saveBtn.trigger('click')
    await flushPromises()

    /* apiMock calls: [0] = initial load, [1] = save. */
    expect(apiMock.mock.calls.length).toBeGreaterThanOrEqual(2)
    const [endpoint, body] = apiMock.mock.calls[1]
    expect(endpoint).toBe('imagecache/config/save')
    expect((body as { node: string }).node).toContain('"name":"unchanged"')
  })

  it('default (alwaysDirty=false) preserves the existing dirty gate', async () => {
    /* Regression — non-trigger forms must still suppress
     * round-trips when nothing changed. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams([
      { id: 'name', type: 'str', caption: 'Name', value: 'unchanged' },
    ])

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeDefined()
  })
})

describe('IdnodeConfigForm — saved emit', () => {
  /* Symmetric with `IdnodeEditor.vue`'s `saved` emit. Lets parent
   * views react to a successful save without sniffing a comet
   * channel. Service Mapper Dialog uses this to close + emit
   * `started` to the grid view (which then surfaces a toast). */
  it('emits "saved" after a successful save POST', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: '' }],
      { alwaysDirty: true },
    )

    apiMock.mockResolvedValueOnce({}) /* save */
    apiMock.mockResolvedValueOnce({
      entries: [{ params: [{ id: 'name', type: 'str', caption: 'Name', value: '' }] }],
    }) /* reload */

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('saved')).toBeTruthy()
    expect(wrapper.emitted('saved')?.length).toBe(1)
  })

  it('does NOT emit "saved" when the save POST rejects', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(
      [{ id: 'name', type: 'str', caption: 'Name', value: '' }],
      { alwaysDirty: true },
    )

    apiMock.mockRejectedValueOnce(new Error('server error'))

    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    expect(wrapper.emitted('saved')).toBeFalsy()
  })
})

describe('IdnodeConfigForm — hideFields', () => {
  const TWO_FIELDS = [
    { id: 'keep_me', type: 'str', caption: 'Keep me', value: 'k' },
    { id: 'hide_me', type: 'str', caption: 'Hide me', value: 'h' },
  ] as const

  it('renders every field when hideFields is unset', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    const wrapper = await mountWithParams(TWO_FIELDS)

    expect(wrapper.text()).toContain('Keep me')
    expect(wrapper.text()).toContain('Hide me')
  })

  it('suppresses listed field ids from the rendered form', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    apiMock.mockResolvedValueOnce({ entries: [{ params: TWO_FIELDS }] })
    const wrapper = mount(IdnodeConfigForm, {
      props: {
        loadEndpoint: 'imagecache/config/load',
        saveEndpoint: 'imagecache/config/save',
        hideFields: ['hide_me'],
      },
    })
    await flushPromises()

    expect(wrapper.text()).toContain('Keep me')
    expect(wrapper.text()).not.toContain('Hide me')
  })

  it('still threads hidden field values through currentValues', async () => {
    /* hideFields only suppresses RENDER. The field's loaded value
     * still rides currentValues so a custom editor wired through
     * the template ref can read + mutate it, and the next save
     * POST includes it. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }

    apiMock.mockResolvedValueOnce({ entries: [{ params: TWO_FIELDS }] })
    const wrapper = mount(IdnodeConfigForm, {
      props: {
        loadEndpoint: 'imagecache/config/load',
        saveEndpoint: 'imagecache/config/save',
        hideFields: ['hide_me'],
      },
    })
    await flushPromises()

    const vals = (wrapper.vm as unknown as {
      currentValues: Record<string, unknown>
    }).currentValues
    expect(vals.keep_me).toBe('k')
    expect(vals.hide_me).toBe('h')
  })
})

/* Validation wire-up — mirrors IdnodeEditor's `applyClassRules` +
 * per-field `validateField` pipeline. `errors` is the merged map;
 * `visibleError(id)` gates display on touched / submitAttempted;
 * Save button is disabled while `hasErrors === true`.
 *
 * The CLASS_RULES map in `validationRules.ts` currently carries
 * only DVR classes — we exercise it here by mounting against
 * `class: 'dvrentry'` which declares `required: ['start','stop','channel']`.
 * Once Configuration-leaf classes get their own rules (Commits 2-7),
 * those exercise the same wire-up. */
describe('IdnodeConfigForm — validation wire-up', () => {
  it('keeps Save clickable on a clean form with errors so the user can surface them', async () => {
    /* Mirrors IdnodeEditor's permissive gate: only `dirty && hasErrors`
     * blocks the button. A fresh-loaded form with required fields empty
     * stays clickable; clicking flips submitAttempted and reveals the
     * inline error messages. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          class: 'dvrentry',
          params: [
            { id: 'start', type: 'time', caption: 'Start', value: null },
            { id: 'stop', type: 'time', caption: 'Stop', value: null },
            { id: 'channel', type: 'str', caption: 'Channel', value: '' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: {
        uuid: 'test-uuid',
        alwaysDirty: true,
      },
    })
    await flushPromises()

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('enables Save once required fields are non-empty', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          class: 'dvrentry',
          params: [
            { id: 'start', type: 'time', caption: 'Start', value: 1000 },
            { id: 'stop', type: 'time', caption: 'Stop', value: 2000 },
            { id: 'channel', type: 'str', caption: 'Channel', value: 'ch1' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: {
        uuid: 'test-uuid',
        alwaysDirty: true,
      },
    })
    await flushPromises()

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })

  it('surfaces a cross-field error message on the right field after touch', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    /* dvrentry crossField rule: stop must be after start. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          class: 'dvrentry',
          params: [
            { id: 'start', type: 'time', caption: 'Start', value: 2000 },
            { id: 'stop', type: 'time', caption: 'Stop', value: 1000 },
            { id: 'channel', type: 'str', caption: 'Channel', value: 'ch1' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: { uuid: 'test-uuid', alwaysDirty: true },
    })
    await flushPromises()

    /* Errors stay hidden until touched OR submitAttempted. Click
     * Save first — the gate flips submitAttempted → true even
     * though the save itself is blocked. */
    await wrapper.find('.idnode-config-form__btn--save').trigger('click')
    await flushPromises()

    /* The cross-field error binds to `stop`; visibleError() should
     * now return the cross-field message. */
    const errorEls = wrapper.findAll('.ifld-row__error')
    const messages = errorEls.map((e) => e.text())
    expect(messages.some((m) => m.includes('Stop time must be after start time'))).toBe(true)
  })

  it('does NOT call apiCall when Save is invoked while errors exist', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          class: 'dvrentry',
          params: [
            { id: 'start', type: 'time', caption: 'Start', value: null },
            { id: 'stop', type: 'time', caption: 'Stop', value: null },
            { id: 'channel', type: 'str', caption: 'Channel', value: '' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: { uuid: 'test-uuid', alwaysDirty: true },
    })
    await flushPromises()

    /* Reset the mock so we can assert that no save-call is made. */
    apiMock.mockClear()

    /* Saving the form via the exposed save() should be a no-op
     * because hasErrors === true. */
    const vm = wrapper.vm as unknown as { save: () => Promise<void> }
    await vm.save()
    await flushPromises()

    expect(apiMock).not.toHaveBeenCalled()
  })

  it('marks required-field captions with the --required class', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    apiMock.mockResolvedValueOnce({
      entries: [
        {
          class: 'dvrentry',
          params: [
            { id: 'start', type: 'time', caption: 'Start', value: 1000 },
            { id: 'stop', type: 'time', caption: 'Stop', value: 2000 },
            { id: 'channel', type: 'str', caption: 'Channel', value: 'ch1' },
            { id: 'comment', type: 'str', caption: 'Comment', value: '' },
          ],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: { uuid: 'test-uuid', alwaysDirty: true },
    })
    await flushPromises()

    /* dvrentry requires start/stop/channel; comment is optional. */
    const rows = wrapper.findAll('.ifld-row')
    const requiredCount = rows.filter((r) => r.classes().includes('ifld-row--required')).length
    expect(requiredCount).toBe(3)
  })

  it('treats null currentClass as "no class-level rules" (graceful fallback)', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'expert' }

    /* No `class` field — applyClassRules returns an empty error
     * map, save stays gated only by isDirty + per-field validators. */
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          params: [{ id: 'name', type: 'str', caption: 'Name', value: 'foo' }],
        },
      ],
    })
    const wrapper = mount(IdnodeConfigForm, {
      props: { uuid: 'test-uuid', alwaysDirty: true },
    })
    await flushPromises()

    const saveBtn = wrapper.find('.idnode-config-form__btn--save')
    expect(saveBtn.attributes('disabled')).toBeUndefined()
  })
})

describe('IdnodeConfigForm — hash-driven field focus', () => {
  /* Each spec sets `mockHash` BEFORE mountWithParams kicks off
   * the load + the watcher firing. afterEach clears it so a leak
   * doesn't bleed into the next spec.
   *
   * scrollIntoView isn't implemented in happy-dom; stub on
   * HTMLElement.prototype so the watcher's call doesn't throw.
   * CSS.escape IS implemented natively in happy-dom (no stub
   * needed). */
  const scrollSpy = vi.fn()
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  ;(globalThis as any).HTMLElement.prototype.scrollIntoView = scrollSpy

  beforeEach(() => {
    mockHash = ''
    scrollSpy.mockClear()
  })

  afterEach(() => {
    mockHash = ''
  })

  it('scrolls and highlights the row matching the field hash', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    mockHash = '#field=name'

    const wrapper = await mountWithParams(MIXED_PARAMS)
    /* The watcher waits one nextTick after the level promote
     * + the row paint before calling scrollIntoView. */
    await flushPromises()

    expect(scrollSpy).toHaveBeenCalledTimes(1)
    const row = wrapper.find('#field-name')
    expect(row.exists()).toBe(true)
    expect(row.classes()).toContain('ifld-row--targeted')
  })

  it('does nothing when the hash does not match the `#field=<id>` format', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    mockHash = '#some-other-hash'

    await mountWithParams(MIXED_PARAMS)
    await flushPromises()

    expect(scrollSpy).not.toHaveBeenCalled()
  })

  it('does nothing when the hash targets an unknown field id', async () => {
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    mockHash = '#field=does_not_exist'

    await mountWithParams(MIXED_PARAMS)
    await flushPromises()

    expect(scrollSpy).not.toHaveBeenCalled()
  })

  it('auto-promotes the display level when the targeted field is advanced/expert', async () => {
    /* User starts at Basic; the targeted field is expert-only.
     * Without promotion the row would be filtered out by the
     * level rule and the scroll would no-op. The watcher should
     * bump the local currentLevel to expert before the scroll
     * fires so the row is in the DOM. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    mockHash = '#field=expert_only'

    const wrapper = await mountWithParams(MIXED_PARAMS)
    await flushPromises()

    expect(scrollSpy).toHaveBeenCalledTimes(1)
    /* Row is now rendered AND targeted; without the promote it
     * wouldn't be in the DOM at all because Basic level filters
     * out expert-flagged fields. */
    const row = wrapper.find('#field-expert_only')
    expect(row.exists()).toBe(true)
    expect(row.classes()).toContain('ifld-row--targeted')
  })

  it('does NOT promote the level when the page is lockLevel-pinned', async () => {
    /* Image Cache and similar lock-level pages have committed to
     * a single level; auto-promoting would break that contract
     * (e.g. an admin-pinned-basic page should not silently flip
     * to expert just because a search landed there). The page's
     * displayed level stays at the lock value; if the field is
     * outside the lock's coverage the row isn't rendered and the
     * scroll no-ops. */
    const access = useAccessStore()
    access.data = { admin: true, dvr: true, uilevel: 'basic' }
    mockHash = '#field=expert_only'

    /* Page is pinned to basic; the expert_only field is filtered out. */
    const wrapper = await mountWithParams(MIXED_PARAMS, { lockLevel: 'basic' })
    await flushPromises()

    expect(scrollSpy).not.toHaveBeenCalled()
    expect(wrapper.find('#field-expert_only').exists()).toBe(false)
  })
})
