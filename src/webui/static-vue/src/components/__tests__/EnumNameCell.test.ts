/*
 * EnumNameCell unit tests. Covers both shapes the cell handles:
 *   - scalar value (single enum key)
 *   - array value (multiple enum keys)
 * The component auto-detects via Array.isArray, so the same set
 * of cases applies to both — fetcher mock + lifecycle expectations
 * are identical.
 *
 * `fetchDeferredEnum` is the module-level cached fetcher; we mock
 * it so the test doesn't make a real network call and so we can
 * control resolution timing via a deferred promise.
 */

import { describe, expect, it, vi, beforeEach } from 'vitest'
import { mount, flushPromises } from '@vue/test-utils'
import EnumNameCell from '../EnumNameCell.vue'
import type { ColumnDef } from '@/types/column'
import type { Option } from '../idnode-fields/deferredEnum'

vi.mock('../idnode-fields/deferredEnum', async (importOriginal) => {
  const actual = await importOriginal<typeof import('../idnode-fields/deferredEnum')>()
  return {
    ...actual,
    fetchDeferredEnum: vi.fn<(d: unknown) => Promise<Option[]>>(),
  }
})

import { fetchDeferredEnum } from '../idnode-fields/deferredEnum'

const mockedFetch = vi.mocked(fetchDeferredEnum)

const COL_SCALAR: ColumnDef = {
  field: 'channel',
  enumSource: { type: 'api', uri: 'channel/list' },
}

const COL_ARRAY: ColumnDef = {
  field: 'profile',
  enumSource: { type: 'api', uri: 'profile/list' },
}

const FIXTURE_SCALAR: Option[] = [
  { key: 'uuid-1', val: 'BBC One HD' },
  { key: 'uuid-2', val: 'ITV1' },
]

const FIXTURE_ARRAY: Option[] = [
  { key: 'uuid-1', val: 'Pass' },
  { key: 'uuid-2', val: 'MKV' },
  { key: 'uuid-3', val: 'Audio Only' },
]

beforeEach(() => {
  mockedFetch.mockReset()
})

describe('EnumNameCell — scalar value', () => {
  it('renders the resolved name once the fetch settles', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-1', col: COL_SCALAR },
    })
    /* In-flight: should show the raw key as a fallback so the cell
     * isn't visibly empty during the fetch window. */
    expect(wrapper.text()).toBe('uuid-1')
    await flushPromises()
    expect(wrapper.text()).toBe('BBC One HD')
  })

  it('renders an em-dash for an unknown key after the fetch settled', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-missing', col: COL_SCALAR },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('renders an em-dash for null / empty values without fetching at all', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_SCALAR)
    const wrapper = mount(EnumNameCell, {
      props: { value: null, col: COL_SCALAR },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('falls back gracefully when the deferred fetch rejects', async () => {
    mockedFetch.mockRejectedValueOnce(new Error('network down'))
    const wrapper = mount(EnumNameCell, {
      props: { value: 'uuid-1', col: COL_SCALAR },
    })
    expect(wrapper.text()).toBe('uuid-1')
    await flushPromises()
    /* After rejection, options resolves to empty list → unknown key
     * → em-dash. The raw key during the fetch window is honest, the
     * em-dash post-failure signals "no resolution available". */
    expect(wrapper.text()).toBe('—')
  })
})

describe('EnumNameCell — array value', () => {
  it('renders an em-dash for an empty array without fetching', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: [], col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('renders an em-dash for null / non-array values', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: null, col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('—')
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('shows raw keys joined during the in-flight fetch window', async () => {
    /* Fetch never resolves — keep the component in the in-flight
     * state to verify the raw-keys-joined fallback. */
    mockedFetch.mockReturnValueOnce(new Promise(() => {}))
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    expect(wrapper.text()).toBe('uuid-1, uuid-2')
  })

  it('renders the resolved labels joined with comma-space once the fetch settles', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Pass, MKV')
  })

  it('preserves cardinality when one key is missing post-fetch', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-missing', 'uuid-3'], col: COL_ARRAY },
    })
    await flushPromises()
    /* Missing key shows as '—' inside the join so the user sees
     * that one of the three references is stale. */
    expect(wrapper.text()).toBe('Pass, —, Audio Only')
  })

  it('collapses a forest of em-dashes when every key is missing', async () => {
    mockedFetch.mockResolvedValueOnce(FIXTURE_ARRAY)
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-x', 'uuid-y'], col: COL_ARRAY },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })

  it('falls back to a single em-dash when the deferred fetch rejects', async () => {
    mockedFetch.mockRejectedValueOnce(new Error('network down'))
    const wrapper = mount(EnumNameCell, {
      props: { value: ['uuid-1', 'uuid-2'], col: COL_ARRAY },
    })
    /* In-flight: raw keys joined. */
    expect(wrapper.text()).toBe('uuid-1, uuid-2')
    await flushPromises()
    /* After rejection, options resolves to empty list → every key
     * is unknown → forest-collapse rule kicks in. */
    expect(wrapper.text()).toBe('—')
  })
})

describe('EnumNameCell — inline enum source', () => {
  /* Inline static enum: option list is provided directly on the
   * column descriptor (no `fetchDeferredEnum` round-trip). Used
   * for small fixed enums like the mux `enabled` tri-state
   * (Ignore / Disable / Enable). The cell should resolve labels
   * synchronously without any in-flight phase. */

  const TRI_STATE: Option[] = [
    { key: -1, val: 'Ignore' },
    { key: 0, val: 'Disable' },
    { key: 1, val: 'Enable' },
  ]
  const COL_TRI: ColumnDef = { field: 'enabled', enumSource: TRI_STATE }

  /* Note on `await flushPromises()`: even though the inline-enum
   * path sets `options.value` synchronously in `onMounted`, Vue's
   * computed-driven re-render is queued to a microtask. The
   * deferred-fetch tests above don't need a flush BEFORE asserting
   * the raw-key fallback because that's the initial render state;
   * inline tests assert the POST-mount label, which requires the
   * reactive flush. */

  it('resolves a scalar value without calling fetchDeferredEnum', async () => {
    const wrapper = mount(EnumNameCell, {
      props: { value: 1, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Enable')
    expect(mockedFetch).not.toHaveBeenCalled()
  })

  it('resolves tri-state PT_INT enum (value=-1 → "Ignore", not "Enable")', async () => {
    /* BooleanCell would render the same green tick for value=1
     * (Enable) and value=-1 (Ignore) because both are truthy.
     * EnumNameCell + an inline enum surfaces the correct label
     * for each. */
    const wrapper = mount(EnumNameCell, {
      props: { value: -1, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('Ignore')
  })

  it('renders an em-dash for an unknown key against an inline enum', async () => {
    const wrapper = mount(EnumNameCell, {
      props: { value: 99, col: COL_TRI },
    })
    await flushPromises()
    expect(wrapper.text()).toBe('—')
  })
})
