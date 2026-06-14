// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * IdnodeEditor unit tests. Mocks api/client to control load/save
 * behavior. Drives the editor via the `uuid` prop (null = closed,
 * string = open and load that uuid).
 */
import { describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import PrimeVue from 'primevue/config'
import IdnodeEditor from '../IdnodeEditor.vue'
import { useAccessStore } from '@/stores/access'
import {
  TOOLTIP_DIRECTIVE_STUB,
  makeDrawerStub,
  setupApiMockReset,
} from './__helpers__/idnodeEditorTestUtils'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

/* IdnodeEditor calls `useConfirmDialog()` (themed PrimeVue
 * <ConfirmDialog>) on the discard-unsaved-changes path. PrimeVue's
 * underlying `useConfirm()` reads from the app-level inject tree
 * populated by `app.use(ConfirmationService)` in main.ts; this test
 * doesn't bootstrap the app, so we mock the wrapper module instead.
 * Default `ask` returns true so any test that triggers attemptClose
 * with a dirty form proceeds to emit('close'). */
vi.mock('@/composables/useConfirmDialog', () => ({
  useConfirmDialog: () => ({ ask: vi.fn(async () => true) }),
}))

setupApiMockReset(apiMock)

type UiLevel = 'basic' | 'advanced' | 'expert'

function mountEditor(
  propOverrides: Partial<{
    uuid: string | null
    createBase: string | null
    level: UiLevel
    title: string
    list: string
  }> = {}
) {
  return mount(IdnodeEditor, {
    props: {
      uuid: 'test-uuid',
      level: 'expert',
      ...propOverrides,
    },
    global: {
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

/*
 * Helper for "edit-mode" tests: queue an `idnode/load` response with
 * the given params, mount the editor against uuid='x', flush the
 * load promise, return the wrapper. Removes the per-test 4-line
 * boilerplate that was repeated across nearly every it() in the
 * suite. `editorProps` allows per-test overrides
 * (e.g. `level: 'basic'`).
 */
async function mountWithLoadedParams(
  params: Array<Record<string, unknown>>,
  editorProps: Partial<{
    uuid: string | null
    level: UiLevel
    list: string
  }> = {}
) {
  apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params }] })
  const wrapper = mountEditor({ uuid: 'x', ...editorProps })
  await flushPromises()
  return wrapper
}

/*
 * Helper for "create-mode" tests: queue the class-fetch response and
 * the create response, mount the editor with `createBase`, flush
 * promises, return the wrapper. The save tests then mutate
 * `currentValues` and click Save.
 */
const DEFAULT_CREATE_RESPONSE: Record<string, unknown> = { uuid: 'new-uuid' }

async function mountCreateMode(
  props: Array<Record<string, unknown>>,
  createResponse: Record<string, unknown> = DEFAULT_CREATE_RESPONSE
) {
  apiMock.mockResolvedValueOnce({ class: 'dvrentry', props })
  apiMock.mockResolvedValueOnce(createResponse)
  const wrapper = mountEditor({ uuid: null, createBase: 'dvr/entry' })
  await flushPromises()
  return wrapper
}

async function mountWithGroups(
  params: Array<Record<string, unknown>>,
  groups: Array<{ number: number; name: string; parent?: number }>,
  editorProps: Partial<{ level: UiLevel }> = {}
) {
  apiMock.mockResolvedValueOnce({
    entries: [{ uuid: 'x', params, meta: { groups } }],
  })
  const wrapper = mountEditor({ uuid: 'x', level: 'expert', ...editorProps })
  await flushPromises()
  return wrapper
}

/* Mount with the PrimeVue plugin installed ($primevue config) —
 * needed by fields that render real PrimeVue widgets (the enum
 * field's Select), unlike the str-only fixtures elsewhere here. */
function mountWithPrimeVue(uuid: string) {
  return mount(IdnodeEditor, {
    props: { uuid, level: 'expert' },
    global: {
      plugins: [[PrimeVue, {}]],
      directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
      stubs: { Drawer: makeDrawerStub() },
    },
  })
}

describe('IdnodeEditor', () => {
  it('does not load when uuid is null', () => {
    const wrapper = mountEditor({ uuid: null })
    expect(apiMock).not.toHaveBeenCalled()
    expect(wrapper.find('.drawer-stub').exists()).toBe(false)
  })

  it('fetches via api/idnode/load on mount when uuid is set', () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params: [] }] })
    mountEditor({ uuid: 'x' })
    expect(apiMock).toHaveBeenCalledWith('idnode/load', {
      uuid: 'x',
      meta: 1,
    })
  })

  it('forwards the `list` prop to api/idnode/load when set', () => {
    apiMock.mockResolvedValueOnce({ entries: [{ uuid: 'x', params: [] }] })
    mountEditor({ uuid: 'x', list: 'comment,pri' })
    expect(apiMock).toHaveBeenCalledWith('idnode/load', {
      uuid: 'x',
      meta: 1,
      list: 'comment,pri',
    })
  })

  it('routes rdonly fields into the Read-only Info section (collapsed)', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: '' },
            { id: 'uri', type: 'str', caption: 'URI', rdonly: true, value: 'abc' },
          ],
        },
      ],
    })
    const wrapper = mountEditor({ uuid: 'x', level: 'expert' })
    await flushPromises()
    /* Find the Read-only Info section and check that uri lives inside
     * it while comment lives in the Basic Settings section. */
    const sections = wrapper.findAll('details')
    const readOnly = sections.find((d) => d.find('summary').text().includes('Read-only Info'))
    const basic = sections.find((d) => d.find('summary').text().includes('Basic Settings'))
    expect(readOnly).toBeTruthy()
    expect(basic).toBeTruthy()
    /* Read-only Info collapses by default (no `open` attribute). */
    expect(readOnly?.element.hasAttribute('open')).toBe(false)
    expect(basic?.element.hasAttribute('open')).toBe(true)
    expect(readOnly?.html()).toContain('URI')
    expect(basic?.html()).toContain('Comment')
    expect(basic?.html()).not.toContain('URI')
  })

  it('renders a placeholder for unsupported field types', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [
            /* Every known type tag now has a renderer (str / int /
             * bool / time / perm / hexa / intsplit / langstr / enum
             * variants). Cover the defensive null-return path with
             * a deliberately-unknown type tag, simulating a future
             * server-side type the client doesn't recognise yet. */
            {
              id: 'title',
              type: 'unknown_future_type',
              caption: 'Title',
              value: '',
            },
          ],
        },
      ],
    })
    const wrapper = mountEditor({ uuid: 'x' })
    await flushPromises()
    const placeholder = wrapper.find('.idnode-editor__placeholder')
    expect(placeholder.exists()).toBe(true)
    expect(placeholder.text()).toContain('Title')
    expect(placeholder.text()).toContain('not implemented')
  })

  it('renders one input per editable visible field', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'hello' },
            { id: 'pri', type: 'int', caption: 'Priority', value: 2 },
            { id: 'enabled', type: 'bool', caption: 'Enabled', value: true },
          ],
        },
      ],
    })
    const wrapper = mountEditor({ uuid: 'x' })
    await flushPromises()
    /* Each field renders an input/textarea/select. We assert by label
     * presence, which is the user-visible bit. */
    expect(wrapper.html()).toContain('Comment')
    expect(wrapper.html()).toContain('Priority')
    expect(wrapper.html()).toContain('Enabled')
  })

  it('respects view-level filtering on fields (basic hides advanced/expert)', async () => {
    const wrapper = await mountWithLoadedParams(
      [
        { id: 'a', type: 'str', caption: 'AlwaysShown', value: '' },
        { id: 'b', type: 'str', caption: 'AdvancedOnly', advanced: true, value: '' },
        { id: 'c', type: 'str', caption: 'ExpertOnly', expert: true, value: '' },
      ],
      { level: 'basic' }
    )
    expect(wrapper.html()).toContain('AlwaysShown')
    expect(wrapper.html()).not.toContain('AdvancedOnly')
    expect(wrapper.html()).not.toContain('ExpertOnly')
  })

  it('skips noui and phidden fields entirely', async () => {
    const wrapper = await mountWithLoadedParams(
      [
        { id: 'a', type: 'str', caption: 'Visible', value: '' },
        { id: 'b', type: 'str', caption: 'NoUI', noui: true, value: '' },
        { id: 'c', type: 'str', caption: 'Phidden', phidden: true, value: '' },
      ],
      { level: 'expert' }
    )
    expect(wrapper.html()).toContain('Visible')
    expect(wrapper.html()).not.toContain('NoUI')
    expect(wrapper.html()).not.toContain('Phidden')
  })

  it('save sends only modified fields and emits close on success', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [
            { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
            { id: 'pri', type: 'int', caption: 'Priority', value: 1 },
          ],
        },
      ],
    })
    apiMock.mockResolvedValueOnce({}) /* save response */
    const wrapper = mountEditor({ uuid: 'x' })
    await flushPromises()

    /* Twiddle the comment field via the exposed currentValues state.
     * Flush reactivity before the click — the Save button is gated
     * on `!dirty` in edit mode, and Vue Test Utils' `trigger('click')`
     * respects the `disabled` attribute. Without the await, the
     * click would fire BEFORE Vue re-evaluates `:disabled` against
     * the new dirty=true state, and would be swallowed. */
    const editorVm = wrapper.vm as unknown as {
      currentValues: Record<string, unknown>
    }
    editorVm.currentValues.comment = 'updated'
    await flushPromises()

    await wrapper.find('.idnode-editor__btn--save').trigger('click')
    await flushPromises()

    /* The second apiMock call is the save. Only the modified field
     * (comment) should be in the node payload — pri stays initial,
     * gets omitted. */
    const saveCall = apiMock.mock.calls[1]
    expect(saveCall[0]).toBe('idnode/save')
    const node = JSON.parse(saveCall[1].node as string)
    expect(node.uuid).toBe('x')
    expect(node.comment).toBe('updated')
    expect(node.pri).toBeUndefined()

    expect(wrapper.emitted('close')).toBeTruthy()
    expect(wrapper.emitted('saved')).toBeTruthy()
  })

  it('Save is disabled in edit mode when nothing is dirty', async () => {
    apiMock.mockResolvedValueOnce({
      entries: [
        {
          uuid: 'x',
          params: [{ id: 'comment', type: 'str', caption: 'Comment', value: 'hi' }],
        },
      ],
    })
    const wrapper = mountEditor({ uuid: 'x' })
    await flushPromises()

    /* No edits — Save should be disabled (the new edit-mode gate
     * forbids no-op submits; replaces the previous "Save click on
     * clean form just emits close" behaviour). */
    const saveBtn = wrapper.find<HTMLButtonElement>('.idnode-editor__btn--save')
    expect(saveBtn.element.disabled).toBe(true)
    /* Only the load call fired; the click is swallowed and no
     * save / close round-trip happens. */
    expect(apiMock).toHaveBeenCalledTimes(1)
    expect(wrapper.emitted('saved')).toBeFalsy()
  })

  it('shows the load error message and disables Save', async () => {
    apiMock.mockRejectedValueOnce(new Error('network down'))
    const wrapper = mountEditor({ uuid: 'x' })
    await flushPromises()
    expect(wrapper.html()).toContain('network down')
    const saveBtn = wrapper.find<HTMLButtonElement>('.idnode-editor__btn--save')
    expect(saveBtn.element.disabled).toBe(true)
  })

  /*
   * Create-mode tests — `createBase` triggers a different load path
   * (api/<base>/class instead of api/idnode/load) and a different
   * save path (api/<base>/create with conf=<JSON>, no diff).
   */
  describe('create mode', () => {
    it('loads class metadata from <base>/class on mount', () => {
      apiMock.mockResolvedValueOnce({ class: 'dvrentry', props: [] })
      mountEditor({ uuid: null, createBase: 'dvr/entry' })
      expect(apiMock).toHaveBeenCalledWith('dvr/entry/class', {})
    })

    it('forwards the `list` prop on the class fetch', () => {
      apiMock.mockResolvedValueOnce({ class: 'dvrentry', props: [] })
      mountEditor({
        uuid: null,
        createBase: 'dvr/entry',
        list: 'enabled,channel',
      })
      expect(apiMock).toHaveBeenCalledWith('dvr/entry/class', {
        list: 'enabled,channel',
      })
    })

    it('renders rdonly fields in the Read-only Info section AND nosave fields in their normal bucket', async () => {
      apiMock.mockResolvedValueOnce({
        class: 'dvrentry',
        props: [
          { id: 'channel', type: 'str', caption: 'Channel', value: '' },
          { id: 'uri', type: 'str', caption: 'URI', rdonly: true, value: '' },
          { id: 'disp_title', type: 'str', caption: 'Title', nosave: true, value: '' },
        ],
      })
      const wrapper = mountEditor({ uuid: null, createBase: 'dvr/entry' })
      await flushPromises()
      /* All three fields appear — matches ExtJS, which only skips
       * `noui` for create mode. PO_NOSAVE fields like `disp_title`
       * accept writes via their prop's `set` callback even though
       * they aren't persisted directly; skipping them broke entry
       * creation (no title). Rdonly fields go to the collapsed
       * Read-only Info section, same as in edit mode. */
      expect(wrapper.html()).toContain('Channel')
      expect(wrapper.html()).toContain('Title')
      expect(wrapper.html()).toContain('URI')
      /* The Read-only Info section exists (uri lives there). */
      const sections = wrapper.findAll('details')
      const readOnly = sections.find((d) => d.find('summary').text().includes('Read-only Info'))
      expect(readOnly).toBeTruthy()
    })

    it('save POSTs to <base>/create with conf=<JSON of filled fields>', async () => {
      /* `dvrentry` requires start + stop + channel per
       * validationRules.ts (dvr_db.c:1045-1052). Tests that exercise
       * the create-save round-trip fill all three so the validation
       * gate doesn't refuse to submit. */
      const wrapper = await mountCreateMode([
        { id: 'start', type: 'time', caption: 'Start', value: 0 },
        { id: 'stop', type: 'time', caption: 'Stop', value: 0 },
        { id: 'channel', type: 'str', caption: 'Channel', value: '' },
        { id: 'comment', type: 'str', caption: 'Comment', value: '' },
      ])
      const editorVm = wrapper.vm as unknown as {
        currentValues: Record<string, unknown>
      }
      editorVm.currentValues.start = 1000
      editorVm.currentValues.stop = 2000
      editorVm.currentValues.channel = 'channel-uuid-123'
      editorVm.currentValues.comment = 'test'
      /* Flush so Save's reactive `:disabled` recomputes before the
       * click — with the required fields now filled it goes enabled.
       * Direct currentValues mutation needs a tick to reach the DOM;
       * a real user's per-keystroke input would already have. */
      await flushPromises()

      await wrapper.find('.idnode-editor__btn--save').trigger('click')
      await flushPromises()

      const createCall = apiMock.mock.calls[1]
      expect(createCall[0]).toBe('dvr/entry/create')
      const conf = JSON.parse(createCall[1].conf as string)
      expect(conf.channel).toBe('channel-uuid-123')
      expect(conf.comment).toBe('test')

      expect(wrapper.emitted('close')).toBeTruthy()
      expect(wrapper.emitted('saved')).toBeTruthy()
    })

    it('save omits empty/null fields from the conf payload', async () => {
      const wrapper = await mountCreateMode([
        { id: 'start', type: 'time', caption: 'Start', value: 0 },
        { id: 'stop', type: 'time', caption: 'Stop', value: 0 },
        { id: 'channel', type: 'str', caption: 'Channel', value: '' },
        { id: 'comment', type: 'str', caption: 'Comment', value: '' },
      ])
      const editorVm = wrapper.vm as unknown as {
        currentValues: Record<string, unknown>
      }
      editorVm.currentValues.start = 1000
      editorVm.currentValues.stop = 2000
      editorVm.currentValues.channel = 'channel-uuid-123'
      /* comment stays empty string — should be omitted from conf. */
      /* Flush so Save's `:disabled` recomputes (required fields filled). */
      await flushPromises()

      await wrapper.find('.idnode-editor__btn--save').trigger('click')
      await flushPromises()

      const createCall = apiMock.mock.calls[1]
      const conf = JSON.parse(createCall[1].conf as string)
      expect(conf.channel).toBe('channel-uuid-123')
      expect(conf.comment).toBeUndefined()
    })

    it('save is gated by validation — refuses to submit when required fields are missing', async () => {
      /* dvrentry requires start + stop + channel; mount without
       * filling start/stop and confirm the create POST never fires. */
      const wrapper = await mountCreateMode([
        { id: 'start', type: 'time', caption: 'Start', value: 0 },
        { id: 'stop', type: 'time', caption: 'Stop', value: 0 },
        { id: 'channel', type: 'str', caption: 'Channel', value: '' },
      ])
      const editorVm = wrapper.vm as unknown as {
        currentValues: Record<string, unknown>
      }
      editorVm.currentValues.channel = 'channel-uuid-123'
      /* start + stop stay null — required-field validation fails. */

      await wrapper.find('.idnode-editor__btn--save').trigger('click')
      await flushPromises()

      /* apiMock.mock.calls[0] is the class-fetch from mountCreateMode;
       * a second call would be the create round-trip. The validation
       * gate refuses, so no second call. */
      expect(apiMock).toHaveBeenCalledTimes(1)
      expect(wrapper.emitted('close')).toBeFalsy()
    })
  })

  /* Regression: `currentLevel = ref(props.level)` captured
   * `props.level` once at setup, so create-mode dialogs froze
   * at the initialisation-time fallback. A watcher on
   * `props.level` re-syncs reactively. */
  describe('view level — props.level reactivity', () => {
    it('re-syncs currentLevel when props.level changes after mount (create mode)', async () => {
      /* Seed access uilevel to Expert so every level is reachable
       * from the select. */
      const access = useAccessStore()
      access.data = { admin: true, dvr: true, uilevel: 'expert' }

      /* Class metadata carries one basic + one advanced field
       * so we can observe the level change through field
       * visibility (the user-facing effect of currentLevel). */
      apiMock.mockResolvedValueOnce({
        class: 'dvrentry',
        props: [
          { id: 'a', type: 'str', caption: 'BasicField', value: '' },
          { id: 'b', type: 'str', caption: 'AdvancedField', advanced: true, value: '' },
        ],
      })

      /* Mount with level='basic' — this simulates the buggy
       * scenario where the grid's effectiveLevel hasn't
       * propagated yet and `editorLevel` is still on its
       * 'basic' fallback. */
      const wrapper = mountEditor({
        uuid: null,
        createBase: 'dvr/entry',
        level: 'basic',
      })
      await flushPromises()

      /* Initially Advanced field hidden under Basic level. */
      expect(wrapper.html()).toContain('BasicField')
      expect(wrapper.html()).not.toContain('AdvancedField')

      /* Parent's level prop now flips to 'advanced' (the
       * grid's level signal landing reactively). Without the
       * fix, currentLevel stays at 'basic' and AdvancedField
       * stays hidden. */
      await wrapper.setProps({ level: 'advanced' })
      await flushPromises()

      expect(wrapper.html()).toContain('AdvancedField')
    })

    it('re-syncs currentLevel when props.level changes after mount (edit mode)', async () => {
      /* Same shape, edit-path variant. Pre-fix edit happened to
       * work because the `uuid` watcher fires on open, but a
       * later parent-driven level change (e.g. user toggles the
       * global LevelMenu while the drawer is open) was equally
       * broken. The fix covers both paths via a single watcher. */
      const access = useAccessStore()
      access.data = { admin: true, dvr: true, uilevel: 'expert' }

      apiMock.mockResolvedValueOnce({
        entries: [
          {
            uuid: 'x',
            params: [
              { id: 'a', type: 'str', caption: 'BasicField', value: '' },
              { id: 'b', type: 'str', caption: 'ExpertField', expert: true, value: '' },
            ],
          },
        ],
      })

      const wrapper = mountEditor({ uuid: 'x', level: 'advanced' })
      await flushPromises()
      expect(wrapper.html()).not.toContain('ExpertField')

      await wrapper.setProps({ level: 'expert' })
      await flushPromises()
      expect(wrapper.html()).toContain('ExpertField')
    })

    it('keeps user-overridden level when props.level does NOT change', async () => {
      /* Negative guard: if the parent's level prop is steady,
       * the user's in-drawer LevelMenu selection (writing to
       * currentLevel) must NOT get clobbered by the new
       * watcher firing spuriously. Initial sync only — no
       * setProps after mount. */
      const access = useAccessStore()
      access.data = { admin: true, dvr: true, uilevel: 'expert' }

      apiMock.mockResolvedValueOnce({
        class: 'dvrentry',
        props: [
          { id: 'a', type: 'str', caption: 'BasicField', value: '' },
          { id: 'b', type: 'str', caption: 'AdvancedField', advanced: true, value: '' },
        ],
      })

      const wrapper = mountEditor({
        uuid: null,
        createBase: 'dvr/entry',
        level: 'basic',
      })
      await flushPromises()
      expect(wrapper.html()).not.toContain('AdvancedField')
      /* No setProps — props.level steady at 'basic'. The
       * advanced field stays hidden; the watcher hasn't
       * fired because props.level didn't change. */
      expect(wrapper.html()).not.toContain('AdvancedField')
    })
  })

  /*
   * Esc handling is owned solely by the editor's document keydown
   * listener; the Drawer's built-in closeOnEscape must be disabled.
   * With both active, one press ran attemptClose twice — double
   * 'close' emit on a clean form, doubled discard-confirm on a
   * dirty one. The real Drawer binds its Esc listener inside its
   * enter-transition hook, which never completes under happy-dom,
   * so the duplication can't reproduce in this environment — the
   * contract is pinned via the prop the editor passes to Drawer
   * plus the single-emit behaviour of the component's own listener.
   */
  describe('Escape key', () => {
    /* Stub declaring closeOnEscape as a prop so the value the
     * editor binds is observable via findComponent().props(). */
    const ESC_DRAWER_STUB = {
      template:
        '<div class="drawer-stub" v-if="visible"><slot /><slot name="footer" /></div>',
      props: ['visible', 'closeOnEscape'],
    }

    function mountForEsc() {
      apiMock.mockResolvedValueOnce({
        entries: [
          {
            uuid: 'x',
            params: [{ id: 'comment', type: 'str', caption: 'Comment', value: 'hi' }],
          },
        ],
      })
      return mount(IdnodeEditor, {
        props: { uuid: 'x', level: 'expert' },
        global: {
          directives: { tooltip: TOOLTIP_DIRECTIVE_STUB },
          stubs: { Drawer: ESC_DRAWER_STUB },
        },
      })
    }

    it('disables the Drawer built-in Esc handler', async () => {
      const wrapper = mountForEsc()
      await flushPromises()
      expect(wrapper.findComponent(ESC_DRAWER_STUB).props('closeOnEscape')).toBe(false)
      wrapper.unmount()
    })

    it('a single Esc press emits exactly one close', async () => {
      const wrapper = mountForEsc()
      await flushPromises()

      document.dispatchEvent(
        new KeyboardEvent('keydown', { key: 'Escape', code: 'Escape', bubbles: true }),
      )
      await flushPromises()

      expect(wrapper.emitted('close')).toHaveLength(1)
      wrapper.unmount()
    })
  })

  /*
   * Pristine values are exempt from validation in edit mode. The
   * server legitimately stores values the client-side rules reject
   * (e.g. a timerec start/stop getter returns the literal "Any" —
   * dvr_timerec.c:407 — while the enum list only carries a
   * translated entry, dvr_timerec.c:426-429). An untouched field
   * must not produce an error and lock Save/Apply; a user-edited
   * field still validates fully.
   */
  describe('validation — pristine values are not validated', () => {
    const OFF_LIST_PARAMS = [
      {
        id: 'start',
        type: 'str',
        caption: 'Start',
        value: 'Any',
        enum: [
          { key: 'Invalid', val: 'Invalid' },
          { key: '07:00', val: '07:00' },
        ],
      },
      { id: 'comment', type: 'str', caption: 'Comment', value: 'orig' },
    ]

    it('an untouched off-list enum value produces no error and the entry stays editable', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'x', class: 'dvrtimerec', params: OFF_LIST_PARAMS }],
      })
      apiMock.mockResolvedValueOnce({}) /* save response */
      const wrapper = mountWithPrimeVue('x')
      await flushPromises()

      /* Untouched — no error anywhere, despite 'Any' not being in
       * the enum list. */
      expect(wrapper.find('.ifld-row--invalid').exists()).toBe(false)

      /* Edit an unrelated field; the pristine off-list value must
       * not block the save round-trip. */
      const editorVm = wrapper.vm as unknown as {
        currentValues: Record<string, unknown>
      }
      editorVm.currentValues.comment = 'updated'
      await flushPromises()

      await wrapper.find('.idnode-editor__btn--save').trigger('click')
      await flushPromises()

      const saveCall = apiMock.mock.calls[1]
      expect(saveCall[0]).toBe('idnode/save')
      const node = JSON.parse(saveCall[1].node as string)
      expect(node.comment).toBe('updated')
      expect(wrapper.emitted('saved')).toBeTruthy()
    })

    it('editing the field to another off-list value surfaces the error', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'x', class: 'dvrtimerec', params: OFF_LIST_PARAMS }],
      })
      const wrapper = mountWithPrimeVue('x')
      await flushPromises()

      /* User changes the value — the field is now dirty and
       * validates fully; another off-list value gets flagged. */
      const editorVm = wrapper.vm as unknown as {
        onFieldChange: (id: string, value: unknown) => void
      }
      editorVm.onFieldChange('start', '99:99')
      await flushPromises()

      const errorEl = wrapper.find('.ifld-row__error')
      expect(errorEl.exists()).toBe(true)
      expect(errorEl.text()).toContain('not in allowed list')
      /* Save is gated while the dirty form carries an error. */
      const saveBtn = wrapper.find<HTMLButtonElement>('.idnode-editor__btn--save')
      expect(saveBtn.element.disabled).toBe(true)
    })

    it('reverting the edit back to the loaded value clears the error', async () => {
      apiMock.mockResolvedValueOnce({
        entries: [{ uuid: 'x', class: 'dvrtimerec', params: OFF_LIST_PARAMS }],
      })
      const wrapper = mountWithPrimeVue('x')
      await flushPromises()

      const editorVm = wrapper.vm as unknown as {
        onFieldChange: (id: string, value: unknown) => void
      }
      editorVm.onFieldChange('start', '99:99')
      await flushPromises()
      expect(wrapper.find('.ifld-row__error').exists()).toBe(true)

      /* Back to the server's value — pristine again, error gone. */
      editorVm.onFieldChange('start', 'Any')
      await flushPromises()
      expect(wrapper.find('.ifld-row__error').exists()).toBe(false)
    })
  })

  /*
   * Server-declared named property groups (idclass_t.ic_groups,
   * serialised by idnode.c:idclass_get_property_groups). When the
   * load response carries `meta.groups`, the editor renders one
   * `<details>` per root group instead of the level-based four-
   * bucket layout. Sub-groups (entries with `parent` set) flat-merge
   * into their root parent's fields list. When no groups are
   * declared, the level-bucket fallback (see earlier tests) still
   * applies.
   */
  describe('class-defined groups', () => {
    it('renders one section per root group in declaration order', async () => {
      const wrapper = await mountWithGroups(
        [
          { id: 'a', type: 'str', caption: 'AField', group: 1, value: '' },
          { id: 'b', type: 'str', caption: 'BField', group: 2, value: '' },
        ],
        [
          { number: 1, name: 'General' },
          { number: 2, name: 'Output' },
        ]
      )
      const sections = wrapper.findAll('details')
      expect(sections).toHaveLength(2)
      expect(sections[0].find('summary').text()).toBe('General')
      expect(sections[1].find('summary').text()).toBe('Output')
      expect(sections[0].html()).toContain('AField')
      expect(sections[0].html()).not.toContain('BField')
      expect(sections[1].html()).toContain('BField')
    })

    it('flat-merges sub-groups into their parent (single section per root)', async () => {
      const wrapper = await mountWithGroups(
        [
          { id: 'a', type: 'str', caption: 'ParentField', group: 4, value: '' },
          { id: 'b', type: 'str', caption: 'ChildField', group: 5, value: '' },
          /* Second root so we render as multi-section with visible
           * <summary> titles — lets us assert directly that there's
           * exactly one Filename/Tagging section, not two. */
          { id: 'c', type: 'str', caption: 'OtherField', group: 6, value: '' },
        ],
        [
          { number: 4, name: 'Filename/Tagging' },
          /* Sub-group with parent=4 and an empty name — Classic
           * dvr_config.c declares the secondary filename block this
           * way; should merge into the Filename/Tagging fieldset. */
          { number: 5, name: '', parent: 4 },
          { number: 6, name: 'Other' },
        ]
      )
      const sections = wrapper.findAll('details')
      expect(sections).toHaveLength(2)
      const summaries = sections.map((d) => d.find('summary').text())
      expect(summaries).toEqual(['Filename/Tagging', 'Other'])
      /* Both parent and child fields land in the Filename/Tagging
       * section — sub-group flat-merge worked. */
      expect(sections[0].html()).toContain('ParentField')
      expect(sections[0].html()).toContain('ChildField')
      expect(sections[1].html()).toContain('OtherField')
    })

    it('sub-group with missing parent surfaces as its own section', async () => {
      const wrapper = await mountWithGroups(
        [
          { id: 'a', type: 'str', caption: 'OrphanField', group: 5, value: '' },
          /* Second root so multi-section rendering exposes summaries. */
          { id: 'b', type: 'str', caption: 'OtherField', group: 6, value: '' },
        ],
        [
          /* Parent number 4 is referenced but not declared — the
           * sub-group acts as a root, its own name shows as the
           * section header. */
          { number: 5, name: 'Strays', parent: 4 },
          { number: 6, name: 'Other' },
        ]
      )
      const sections = wrapper.findAll('details')
      expect(sections).toHaveLength(2)
      const summaries = sections.map((d) => d.find('summary').text())
      expect(summaries).toContain('Strays')
      expect(sections.find((d) => d.find('summary').text() === 'Strays')?.html()).toContain(
        'OrphanField'
      )
    })

    it('rdonly fields render inline in their declared group (no separate Read-only Info)', async () => {
      const wrapper = await mountWithGroups(
        [
          { id: 'name', type: 'str', caption: 'Name', group: 1, value: 'profile-x' },
          { id: 'uuid', type: 'str', caption: 'UUID', group: 1, rdonly: true, value: 'abc-123' },
          /* Second root so multi-section rendering exposes summaries
           * — lets us assert "no summary contains Read-only Info"
           * (vs. asserting on raw HTML, which matches template
           * comments). */
          { id: 'other', type: 'str', caption: 'Other', group: 2, value: '' },
        ],
        [
          { number: 1, name: 'General' },
          { number: 2, name: 'More' },
        ]
      )
      /* No section is the synthetic Read-only Info bucket — assert
       * via <summary> text rather than raw HTML so template comments
       * in the editor source don't false-positive. */
      const summaries = wrapper.findAll('summary').map((s) => s.text())
      expect(summaries).not.toContain('Read-only Info')
      /* The rdonly field renders inside its declared group, not in
       * a separate Read-only Info bucket. */
      const general = wrapper
        .findAll('details')
        .find((d) => d.find('summary').text() === 'General')
      expect(general).toBeTruthy()
      expect(general?.html()).toContain('Name')
      expect(general?.html()).toContain('UUID')
    })

    it('hides a group whose visible-field bucket goes empty under the level filter', async () => {
      const wrapper = await mountWithGroups(
        [
          /* The General group only has a basic field. */
          { id: 'a', type: 'str', caption: 'BasicField', group: 1, value: '' },
          /* The Tuning group only has an expert field — hidden at level basic. */
          { id: 'b', type: 'str', caption: 'ExpertField', group: 2, expert: true, value: '' },
        ],
        [
          { number: 1, name: 'General' },
          { number: 2, name: 'Tuning' },
        ],
        { level: 'basic' }
      )
      /* Only General has any visible field → single-section short-
       * circuit (no <details>). Tuning is hidden entirely. */
      expect(wrapper.html()).toContain('BasicField')
      expect(wrapper.html()).not.toContain('ExpertField')
      expect(wrapper.html()).not.toContain('Tuning')
    })

    it('falls back to level buckets when meta.groups is empty', async () => {
      /* Same shape as the existing "rdonly fields → Read-only Info"
       * test, but with an explicit empty meta.groups array — confirms
       * the fallback path stays active when the server's class has
       * no `ic_groups` declaration. */
      apiMock.mockResolvedValueOnce({
        entries: [
          {
            uuid: 'x',
            params: [
              { id: 'comment', type: 'str', caption: 'Comment', value: '' },
              { id: 'uri', type: 'str', caption: 'URI', rdonly: true, value: 'abc' },
            ],
            meta: { groups: [] },
          },
        ],
      })
      const wrapper = mountEditor({ uuid: 'x', level: 'expert' })
      await flushPromises()
      const sections = wrapper.findAll('details')
      const readOnly = sections.find((d) => d.find('summary').text().includes('Read-only Info'))
      const basic = sections.find((d) => d.find('summary').text().includes('Basic Settings'))
      expect(readOnly).toBeTruthy()
      expect(basic).toBeTruthy()
    })
  })
})
