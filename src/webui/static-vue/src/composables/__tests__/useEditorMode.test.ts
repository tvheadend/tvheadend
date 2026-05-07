/*
 * useEditorMode unit tests — covers the regression surface that was
 * previously inline boilerplate across UpcomingView / AutorecsView /
 * TimersView (and the edit-only variant in Finished/Failed/Removed).
 *
 * Each behaviour the inline copies relied on is asserted here so
 * the views can be refactored to consume the composable without
 * losing coverage. Composable is pure-Vue-reactivity (no Pinia, no
 * router, no DOM) — tests can drive it directly without mounting.
 */
import { computed, ref } from 'vue'
import { describe, expect, it } from 'vitest'
import { useEditorMode } from '../useEditorMode'

describe('useEditorMode', () => {
  describe('initial state', () => {
    it('starts with editingUuid=null, creatingBase=null, gridRef=null', () => {
      const editList = ref('a,b,c')
      const e = useEditorMode({ editList })
      expect(e.editingUuid.value).toBeNull()
      expect(e.creatingBase.value).toBeNull()
      expect(e.gridRef.value).toBeNull()
    })

    it('editorLevel falls back to "basic" when gridRef is null', () => {
      const e = useEditorMode({ editList: ref('a') })
      expect(e.editorLevel.value).toBe('basic')
    })
  })

  describe('editorLevel', () => {
    it('reflects gridRef.value.effectiveLevel when set', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.gridRef.value = { effectiveLevel: 'expert' }
      expect(e.editorLevel.value).toBe('expert')
      e.gridRef.value = { effectiveLevel: 'advanced' }
      expect(e.editorLevel.value).toBe('advanced')
    })

    it('falls back to "basic" if gridRef is unset again', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.gridRef.value = { effectiveLevel: 'expert' }
      e.gridRef.value = null
      expect(e.editorLevel.value).toBe('basic')
    })
  })

  describe('openEditor', () => {
    it('sets editingUuid for a single-row selection', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openEditor([{ uuid: 'row-1', title: 'foo' }])
      expect(e.editingUuid.value).toBe('row-1')
    })

    it('no-ops on empty selection', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openEditor([])
      expect(e.editingUuid.value).toBeNull()
    })

    it('no-ops on multi-row selection (Edit only enables for exactly one row)', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openEditor([{ uuid: 'a' }, { uuid: 'b' }])
      expect(e.editingUuid.value).toBeNull()
    })

    it('no-ops when the selected row has no uuid', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openEditor([{ title: 'no uuid here' }])
      expect(e.editingUuid.value).toBeNull()
    })

    it('no-ops when the selected row has a non-string uuid', () => {
      const e = useEditorMode({ editList: ref('a') })
      /* Defensive — server should never send a non-string uuid, but the
       * inline boilerplate has the type check, so we mirror it. */
      e.openEditor([{ uuid: 12345 } as unknown as { uuid: string }])
      expect(e.editingUuid.value).toBeNull()
    })
  })

  describe('openCreate', () => {
    it('sets creatingBase to options.createBase when configured', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'dvr/entry',
        createList: 'a,b',
      })
      e.openCreate()
      expect(e.creatingBase.value).toBe('dvr/entry')
    })

    it('is a no-op when createBase was not provided (edit-only views)', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openCreate()
      expect(e.creatingBase.value).toBeNull()
    })

    it('accepts an optional subclass and stores it on creatingSubclass', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'mpegts/network',
      })
      e.openCreate('dvb_network_dvbt')
      expect(e.creatingBase.value).toBe('mpegts/network')
      expect(e.creatingSubclass.value).toBe('dvb_network_dvbt')
      expect(e.creatingParentScope.value).toBeNull()
    })

    it('clears any prior parent-scope when called (mutually exclusive)', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'mpegts/network',
      })
      e.openCreateForParent({
        classEndpoint: 'mpegts/network/mux_class',
        createEndpoint: 'mpegts/network/mux_create',
        params: { uuid: 'net-1' },
      })
      e.openCreate('dvb_network_dvbt')
      expect(e.creatingSubclass.value).toBe('dvb_network_dvbt')
      expect(e.creatingParentScope.value).toBeNull()
    })
  })

  describe('openCreateForParent', () => {
    it('sets parent-scope and clears any prior subclass', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'mpegts/network',
      })
      e.openCreate('dvb_network_dvbt')
      e.openCreateForParent({
        classEndpoint: 'mpegts/network/mux_class',
        createEndpoint: 'mpegts/network/mux_create',
        params: { uuid: 'net-1' },
      })
      expect(e.creatingBase.value).toBe('mpegts/network')
      expect(e.creatingSubclass.value).toBeNull()
      expect(e.creatingParentScope.value).toEqual({
        classEndpoint: 'mpegts/network/mux_class',
        createEndpoint: 'mpegts/network/mux_create',
        params: { uuid: 'net-1' },
      })
    })

    it('is a no-op when createBase was not provided', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openCreateForParent({
        classEndpoint: 'x/class',
        createEndpoint: 'x/create',
        params: {},
      })
      expect(e.creatingBase.value).toBeNull()
      expect(e.creatingParentScope.value).toBeNull()
    })

    it('closeEditor clears the parent-scope', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'mpegts/network',
      })
      e.openCreateForParent({
        classEndpoint: 'mpegts/network/mux_class',
        createEndpoint: 'mpegts/network/mux_create',
        params: { uuid: 'net-1' },
      })
      e.closeEditor()
      expect(e.creatingBase.value).toBeNull()
      expect(e.creatingParentScope.value).toBeNull()
    })
  })

  describe('closeEditor', () => {
    it('nulls editingUuid (when in edit mode)', () => {
      const e = useEditorMode({ editList: ref('a') })
      e.openEditor([{ uuid: 'row-1' }])
      e.closeEditor()
      expect(e.editingUuid.value).toBeNull()
    })

    it('nulls creatingBase (when in create mode)', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'dvr/entry',
        createList: 'a',
      })
      e.openCreate()
      e.closeEditor()
      expect(e.creatingBase.value).toBeNull()
    })

    it('nulls both regardless of which was set (defensive)', () => {
      const e = useEditorMode({
        editList: ref('a'),
        createBase: 'dvr/entry',
        createList: 'a',
      })
      e.editingUuid.value = 'x'
      e.creatingBase.value = 'y'
      e.closeEditor()
      expect(e.editingUuid.value).toBeNull()
      expect(e.creatingBase.value).toBeNull()
    })
  })

  describe('editorList', () => {
    it('returns editList in edit mode (creatingBase null)', () => {
      const e = useEditorMode({
        editList: ref('edit-fields'),
        createBase: 'dvr/entry',
        createList: 'create-fields',
      })
      expect(e.editorList.value).toBe('edit-fields')
    })

    it('returns createList in create mode (creatingBase set)', () => {
      const e = useEditorMode({
        editList: ref('edit-fields'),
        createBase: 'dvr/entry',
        createList: 'create-fields',
      })
      e.openCreate()
      expect(e.editorList.value).toBe('create-fields')
    })

    it('falls back to editList when createList is missing (edit-only views)', () => {
      const e = useEditorMode({ editList: ref('edit-fields') })
      /* Even if creatingBase somehow gets set, we have no createList
       * to fall through to — return editList rather than undefined. */
      e.creatingBase.value = 'somehow-set'
      expect(e.editorList.value).toBe('edit-fields')
    })

    it('reactively reflects editList changes (admin toggle, etc.)', () => {
      const editList = ref('basic-fields')
      const e = useEditorMode({ editList })
      expect(e.editorList.value).toBe('basic-fields')
      editList.value = 'admin-fields'
      expect(e.editorList.value).toBe('admin-fields')
    })

    it('uses computed editList (real-world admin-aware shape)', () => {
      const isAdmin = ref(false)
      const editList = computed(() => (isAdmin.value ? 'fields,owner,creator' : 'fields'))
      const e = useEditorMode({ editList })
      expect(e.editorList.value).toBe('fields')
      isAdmin.value = true
      expect(e.editorList.value).toBe('fields,owner,creator')
    })
  })

  describe('openEditor / closeEditor / openCreate sequencing', () => {
    it('open Edit then Create switches modes (edit clears, create sets)', () => {
      const e = useEditorMode({
        editList: ref('e'),
        createBase: 'b',
        createList: 'c',
      })
      e.openEditor([{ uuid: 'r1' }])
      expect(e.editingUuid.value).toBe('r1')
      expect(e.creatingBase.value).toBeNull()

      /* Note: openCreate doesn't auto-clear editingUuid — that's the
       * caller's responsibility (they typically call closeEditor first
       * or rely on the drawer being mutually-exclusive at render time).
       * This test pins the documented "either-or" behaviour: both refs
       * can be non-null transiently but the IdnodeEditor's prop
       * resolution prefers createBase. */
      e.openCreate()
      expect(e.editingUuid.value).toBe('r1')
      expect(e.creatingBase.value).toBe('b')
    })

    it('closeEditor restores fully-closed state from any combination', () => {
      const e = useEditorMode({
        editList: ref('e'),
        createBase: 'b',
        createList: 'c',
      })
      e.openEditor([{ uuid: 'r1' }])
      e.openCreate()
      e.closeEditor()
      expect(e.editingUuid.value).toBeNull()
      expect(e.creatingBase.value).toBeNull()
    })
  })

  describe('flipToEdit', () => {
    it('flips create mode to edit mode against the new uuid', () => {
      const e = useEditorMode({
        editList: ref('e'),
        createBase: 'dvr/entry',
        createList: 'c',
      })
      e.openCreate()
      expect(e.creatingBase.value).toBe('dvr/entry')
      expect(e.editingUuid.value).toBeNull()

      /* Simulate IdnodeEditor's `created` emit after a successful
       * Apply in create mode. Mode flips: createBase clears, the new
       * uuid takes its place — the editor watches `[uuid, createBase]`
       * and reloads against the canonical server-side row. */
      e.flipToEdit('new-uuid-abc')
      expect(e.creatingBase.value).toBeNull()
      expect(e.editingUuid.value).toBe('new-uuid-abc')
    })

    it('still works when called from a non-create state (defensive)', () => {
      /* The composable doesn't gate flipToEdit on creatingBase being
       * set — the editor only emits `created` after a successful
       * create round-trip, but if some future caller invokes it
       * outside that flow, the result is still well-defined. */
      const e = useEditorMode({ editList: ref('e') })
      e.flipToEdit('uuid-x')
      expect(e.editingUuid.value).toBe('uuid-x')
      expect(e.creatingBase.value).toBeNull()
    })
  })
})
