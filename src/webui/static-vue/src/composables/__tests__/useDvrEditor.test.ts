/*
 * useDvrEditor — singleton DVR-editor handle. Tests cover:
 *   - open() / close() round trip
 *   - URL sync writes editUuid on EPG routes
 *   - URL sync does NOT touch the URL on non-EPG routes (the
 *     anti-conflict guard that lets DVR list views' own
 *     useEditorMode keep owning editUuid on /dvr/*)
 *   - URL → state: pasting ?editUuid on an EPG route opens the
 *     drawer
 *   - Route change auto-closes the open drawer
 *
 * The composable wires its watchers lazily on first call (router
 * composables need a component context) — tests need to mount a
 * tiny harness component that calls useDvrEditor() so the
 * router context is available. Module re-imported per test so
 * the singleton state and `wired` flag start fresh each time.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { defineComponent, h, nextTick, reactive } from 'vue'
import { mount, type VueWrapper } from '@vue/test-utils'

/* Synthetic reactive route + router. Tests mutate `route.path` /
 * `route.query` to simulate navigation; assertions read
 * `replaceCalls` to verify the composable wrote (or didn't write)
 * to the URL. */
const route = reactive<{ path: string; query: Record<string, string | undefined> }>({
  path: '/epg/timeline',
  query: {},
})

const replaceCalls: Array<{ query: Record<string, string | undefined> }> = []

vi.mock('vue-router', () => ({
  useRoute: () => route,
  useRouter: () => ({
    replace: (target: { query: Record<string, string | undefined> }) => {
      replaceCalls.push(target)
      /* Reflect the URL write back into the reactive route, so the
       * "URL → editor" watch sees the same end state a real router
       * would after navigation completes. */
      route.query = { ...target.query }
      return Promise.resolve()
    },
  }),
}))

beforeEach(() => {
  /* Fresh module + reset shared state per test. vitest's `resetModules`
   * is what makes the composable's module-level singleton + `wired`
   * flag start clean each time. */
  vi.resetModules()
  route.path = '/epg/timeline'
  route.query = {}
  replaceCalls.length = 0
})

afterEach(() => {
  replaceCalls.length = 0
})

async function mountHarness(): Promise<{
  wrapper: VueWrapper
  api: { editingUuid: { value: string | null }; open: (u: string) => void; close: () => void }
}> {
  const mod = await import('../useDvrEditor')
  let captured!: ReturnType<typeof mod.useDvrEditor>
  const Harness = defineComponent({
    setup() {
      captured = mod.useDvrEditor()
      return () => h('div')
    },
  })
  const wrapper = mount(Harness)
  await nextTick()
  return { wrapper, api: captured }
}

describe('useDvrEditor', () => {
  it('starts with editingUuid=null', async () => {
    const { api } = await mountHarness()
    expect(api.editingUuid.value).toBeNull()
  })

  it('open(uuid) sets editingUuid and writes editUuid to the URL on EPG routes', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')
    expect(replaceCalls).toHaveLength(1)
    expect(replaceCalls[0].query.editUuid).toBe('abc-123')
  })

  it('close() clears editingUuid and strips editUuid from the URL on EPG routes', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    replaceCalls.length = 0

    api.close()
    await nextTick()
    expect(api.editingUuid.value).toBeNull()
    expect(replaceCalls).toHaveLength(1)
    expect(replaceCalls[0].query.editUuid).toBeUndefined()
  })

  it('open(uuid) on a non-EPG route sets state but does NOT touch the URL', async () => {
    route.path = '/dvr/upcoming'
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')
    /* Critical: useEditorMode in UpcomingView owns editUuid on
     * /dvr/* routes. We must not write it from here, or the two
     * editors would race to open. */
    expect(replaceCalls).toHaveLength(0)
  })

  it('URL → state: pasting ?editUuid on an EPG route opens the editor', async () => {
    const { api } = await mountHarness()
    expect(api.editingUuid.value).toBeNull()

    route.query = { editUuid: 'pasted-uuid' }
    await nextTick()

    expect(api.editingUuid.value).toBe('pasted-uuid')
  })

  it('URL → state: ?editUuid on a non-EPG route does NOT open the editor', async () => {
    route.path = '/dvr/upcoming'
    const { api } = await mountHarness()

    route.query = { editUuid: 'pasted-uuid' }
    await nextTick()

    expect(api.editingUuid.value).toBeNull()
  })

  it('navigating away from the EPG route closes an open editor', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()
    expect(api.editingUuid.value).toBe('abc-123')

    route.path = '/status/connections'
    await nextTick()

    expect(api.editingUuid.value).toBeNull()
  })

  it('navigating between two EPG routes also auto-closes (consistent with the route-change rule)', async () => {
    const { api } = await mountHarness()
    api.open('abc-123')
    await nextTick()

    route.path = '/epg/magazine'
    await nextTick()

    expect(api.editingUuid.value).toBeNull()
  })
})
