/*
 * IdnodeEditor unit tests. Mocks api/client to control load/save
 * behavior. Drives the editor via the `uuid` prop (null = closed,
 * string = open and load that uuid).
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createPinia, setActivePinia } from 'pinia'
import IdnodeEditor from '../IdnodeEditor.vue'

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

const tooltipDirectiveStub = {
  mounted() {},
  updated() {},
  unmounted() {},
}

beforeEach(() => {
  apiMock.mockReset()
  /* IdnodeEditor reads `useAccessStore()` for the level-picker access
   * cap; tests need an active Pinia instance to satisfy that. Fresh
   * pinia per test keeps state from leaking across cases. */
  setActivePinia(createPinia())
})

afterEach(() => {
  apiMock.mockReset()
})

function mountEditor(
  propOverrides: Partial<{
    uuid: string | null
    createBase: string | null
    level: 'basic' | 'advanced' | 'expert'
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
      directives: { tooltip: tooltipDirectiveStub },
      stubs: {
        /*
         * Drawer is PrimeVue's, full of teleports + transitions that
         * don't exercise our logic. Stub it as a passthrough so the
         * children render in-place and assertions can find them.
         */
        Drawer: {
          template: '<div class="drawer-stub" v-if="visible"><slot /><slot name="footer" /></div>',
          props: ['visible'],
        },
      },
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
    level: 'basic' | 'advanced' | 'expert'
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
            /* langstr is in the unsupported-field-types set — should
             * render the placeholder row instead of an input. */
            { id: 'title', type: 'langstr', caption: 'Title', value: {} },
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
})
