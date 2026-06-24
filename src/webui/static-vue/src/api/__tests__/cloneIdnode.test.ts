// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

/* `cloneIdnode` calls `apiCall` twice (load + create). Mock the
 * module so each test can inspect the calls + drive the response
 * shapes deterministically. */

const apiCallMock = vi.fn<(...args: unknown[]) => Promise<unknown>>()

vi.mock('@/api/client', () => ({
  apiCall: apiCallMock,
}))

beforeEach(() => {
  apiCallMock.mockReset()
})

afterEach(() => {
  vi.restoreAllMocks()
})

/* Standard load-response shape — `entries[0].params` array of
 * `{ id, value }` objects, mirroring what the server emits via
 * `prop_serialize0`. The top-level `class` field is what
 * `idnode_serialize0` (`src/idnode.c:1550`) emits as a sibling
 * of `params` for every entry. */
function loadResp(
  params: Array<{ id: string; value: unknown }>,
  topLevelClass?: string,
) {
  const entry: {
    uuid: string
    class?: string
    params: Array<{ id: string; value: unknown }>
  } = { uuid: 'src-uuid', params }
  if (topLevelClass !== undefined) entry.class = topLevelClass
  return { entries: [entry] }
}

describe('cloneIdnode', () => {
  it('happy path: loads source, flattens params, mutates name, posts create, returns new uuid', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          { id: 'class', value: 'profile-mpegts' },
          { id: 'name', value: 'BBC One' },
          { id: 'enabled', value: true },
          { id: 'priority', value: 5 },
        ])
      )
      .mockResolvedValueOnce({ uuid: 'new-uuid' })

    const result = await cloneIdnode('src-uuid', {
      createEndpoint: 'profile/create',
    })

    expect(result).toEqual({ uuid: 'new-uuid', name: 'BBC One (copy)' })

    /* Load call. */
    expect(apiCallMock.mock.calls[0]).toEqual(['idnode/load', { uuid: 'src-uuid' }])

    /* Create call. Class is top-level; conf carries the rest
     * (with the renamed `name` and without `class` / `uuid`). */
    const [createEndpoint, createBody] = apiCallMock.mock.calls[1]
    expect(createEndpoint).toBe('profile/create')
    expect((createBody as { class?: string }).class).toBe('profile-mpegts')
    const conf = JSON.parse((createBody as { conf: string }).conf)
    expect(conf).toEqual({
      name: 'BBC One (copy)',
      enabled: true,
      priority: 5,
    })
    expect(conf).not.toHaveProperty('class')
    expect(conf).not.toHaveProperty('uuid')
  })

  it('drops a uuid field from the conf even if it slipped into params', async () => {
    /* The server normally emits uuid as a top-level entry field,
     * not inside params, but defend in depth — the create
     * endpoint would reject a uuid in conf anyway. */
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          { id: 'class', value: 'profile-mpegts' },
          { id: 'uuid', value: 'should-be-stripped' },
          { id: 'name', value: 'X' },
        ])
      )
      .mockResolvedValueOnce({ uuid: 'new-uuid' })

    await cloneIdnode('src-uuid', { createEndpoint: 'profile/create' })

    const conf = JSON.parse(
      (apiCallMock.mock.calls[1][1] as { conf: string }).conf
    )
    expect(conf).not.toHaveProperty('uuid')
    expect(conf.name).toBe('X (copy)')
  })

  it('honours custom nameSuffix', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          { id: 'class', value: 'profile-mpegts' },
          { id: 'name', value: 'foo' },
        ])
      )
      .mockResolvedValueOnce({ uuid: 'new' })

    const result = await cloneIdnode('src-uuid', {
      createEndpoint: 'profile/create',
      nameSuffix: '-clone',
    })

    expect(result.name).toBe('foo-clone')
    const conf = JSON.parse(
      (apiCallMock.mock.calls[1][1] as { conf: string }).conf
    )
    expect(conf.name).toBe('foo-clone')
  })

  it('honours custom classField', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          /* Class lives in a non-default field for this entity. */
          { id: 'kind', value: 'special-kind' },
          { id: 'name', value: 'thing' },
        ])
      )
      .mockResolvedValueOnce({ uuid: 'new' })

    await cloneIdnode('src-uuid', {
      createEndpoint: 'thing/create',
      classField: 'kind',
    })

    const [, body] = apiCallMock.mock.calls[1]
    expect((body as { class?: string }).class).toBe('special-kind')
    const conf = JSON.parse((body as { conf: string }).conf)
    /* `kind` was extracted as the class — it shouldn't also
     * appear in the conf body. */
    expect(conf).not.toHaveProperty('kind')
  })

  it('honours custom nameField', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          { id: 'class', value: 'profile-mpegts' },
          { id: 'title', value: 'My Title' },
        ])
      )
      .mockResolvedValueOnce({ uuid: 'new' })

    const result = await cloneIdnode('src-uuid', {
      createEndpoint: 'profile/create',
      nameField: 'title',
    })

    expect(result.name).toBe('My Title (copy)')
    const conf = JSON.parse(
      (apiCallMock.mock.calls[1][1] as { conf: string }).conf
    )
    expect(conf.title).toBe('My Title (copy)')
  })

  it('throws when load returns no entries', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock.mockResolvedValueOnce({ entries: [] })

    await expect(
      cloneIdnode('src-uuid', { createEndpoint: 'profile/create' })
    ).rejects.toThrow(/Failed to load source/)
  })

  it('throws when source has no class field at top level OR in params', async () => {
    /* Defensive against a malformed server response — neither
     * the top-level `entry.class` (emitted by idnode_serialize0
     * for normal loads) nor a `class` property in params is
     * present. */
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock.mockResolvedValueOnce(
      loadResp([{ id: 'name', value: 'orphan' }] /* no top-level class */)
    )

    await expect(
      cloneIdnode('src-uuid', { createEndpoint: 'profile/create' })
    ).rejects.toThrow(/missing required class field/)
  })

  it('reads class from top-level entry.class for single-class types', async () => {
    /* dvr_config_class doesn't expose `class` as a property, so
     * params has no `class` entry. The top-level `entry.class`
     * (always emitted by idnode_serialize0) is what cloneIdnode
     * picks up. */
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp(
          [
            { id: 'name', value: 'My profile' },
            { id: 'enabled', value: true },
          ],
          'dvrconfig'
        )
      )
      .mockResolvedValueOnce({ uuid: 'new-dvr-uuid' })

    const result = await cloneIdnode('src-uuid', {
      createEndpoint: 'dvr/config/create',
    })

    expect(result).toEqual({ uuid: 'new-dvr-uuid', name: 'My profile (copy)' })
    const [, body] = apiCallMock.mock.calls[1]
    expect((body as { class?: string }).class).toBe('dvrconfig')
    const conf = JSON.parse((body as { conf: string }).conf)
    expect(conf).toEqual({ name: 'My profile (copy)', enabled: true })
    expect(conf).not.toHaveProperty('class')
  })

  it('top-level entry.class wins over a stale class entry in params', async () => {
    /* Belt-and-braces: if both shapes are present (profile-class
     * shape, where `class` is duplicated as a property), the
     * top-level field is authoritative. */
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp(
          [
            { id: 'class', value: 'stale-value' },
            { id: 'name', value: 'X' },
          ],
          'profile-mpegts'
        )
      )
      .mockResolvedValueOnce({ uuid: 'new' })

    await cloneIdnode('src-uuid', { createEndpoint: 'profile/create' })

    const [, body] = apiCallMock.mock.calls[1]
    expect((body as { class?: string }).class).toBe('profile-mpegts')
    const conf = JSON.parse((body as { conf: string }).conf)
    expect(conf).not.toHaveProperty('class')
  })

  it('throws when create returns no uuid', async () => {
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([
          { id: 'class', value: 'profile-mpegts' },
          { id: 'name', value: 'foo' },
        ])
      )
      .mockResolvedValueOnce({}) /* no uuid in response */

    await expect(
      cloneIdnode('src-uuid', { createEndpoint: 'profile/create' })
    ).rejects.toThrow(/returned no uuid/)
  })

  it('handles a missing source name with a placeholder default', async () => {
    /* Defensive: if the source somehow has no name, the suffix
     * still appends to a placeholder so the create POST has a
     * valid string for the name field. */
    const { cloneIdnode } = await import('../cloneIdnode')
    apiCallMock
      .mockResolvedValueOnce(
        loadResp([{ id: 'class', value: 'profile-mpegts' }])
      )
      .mockResolvedValueOnce({ uuid: 'new' })

    const result = await cloneIdnode('src-uuid', {
      createEndpoint: 'profile/create',
    })
    expect(result.name).toBe('unnamed (copy)')
  })
})
