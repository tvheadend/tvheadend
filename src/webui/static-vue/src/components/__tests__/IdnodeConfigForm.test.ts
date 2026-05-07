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
import IdnodeConfigForm from '../IdnodeConfigForm.vue'
import { useAccessStore } from '@/stores/access'

const apiMock = vi.fn()
vi.mock('@/api/client', () => ({
  apiCall: (...args: unknown[]) => apiMock(...args),
}))

beforeEach(() => {
  apiMock.mockReset()
  setActivePinia(createPinia())
})

afterEach(() => {
  apiMock.mockReset()
})

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
  formProps: Partial<{ lockLevel: 'basic' | 'advanced' | 'expert' }> = {}
) {
  apiMock.mockResolvedValueOnce({ entries: [{ params }] })
  const wrapper = mount(IdnodeConfigForm, {
    props: {
      loadEndpoint: 'imagecache/config/load',
      saveEndpoint: 'imagecache/config/save',
      ...formProps,
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
