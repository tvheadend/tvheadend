// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useMasterDetailActions — shared composables for the
 * Add / Clone / Delete toolbar handlers on master-detail
 * Configuration views.
 *
 * Each handler shares the same try/catch/toast/inflight-guard
 * shape; the per-view differences are the create endpoint, the
 * entity noun, and whether the Add path runs through a
 * subclass picker (single-class entity types skip the picker
 * and create directly).
 */
import { ref, type Ref } from 'vue'
import { apiCall } from '@/api/client'
import { cloneIdnode } from '@/api/cloneIdnode'
import { useToastNotify } from '@/composables/useToastNotify'
import { useConfirmDialog } from '@/composables/useConfirmDialog'
import { t } from '@/composables/useI18n'

export interface CloneActionOptions {
  /** Reactive ref holding the selected entity uuid; the composable
   *  writes the new clone's uuid into this ref on success. */
  selected: Ref<string | null>
  /** Create endpoint for the cloned entity, e.g. `'profile/create'`. */
  createEndpoint: string
  /** Summary line shown on the error toast. Defaults to `'Clone failed'`. */
  errorSummary?: string
}

export function useCloneAction(opts: CloneActionOptions) {
  const inflight = ref(false)
  const toast = useToastNotify()

  async function run() {
    if (!opts.selected.value || inflight.value) return
    const srcUuid = opts.selected.value
    inflight.value = true
    try {
      const result = await cloneIdnode(srcUuid, {
        createEndpoint: opts.createEndpoint,
      })
      opts.selected.value = result.uuid
      toast.success(t('Cloned as "{0}".', result.name))
    } catch (e) {
      toast.error(e instanceof Error ? e.message : String(e), {
        summary: opts.errorSummary ?? t('Clone failed'),
      })
    } finally {
      inflight.value = false
    }
  }

  return { inflight, run }
}

export interface DeleteActionOptions {
  selected: Ref<string | null>
  /** Body of the confirm dialog. */
  confirmText: string
  /** Summary line on the error toast. Defaults to `'Delete failed'`. */
  errorSummary?: string
}

export function useDeleteAction(opts: DeleteActionOptions) {
  const inflight = ref(false)
  const toast = useToastNotify()
  const confirmDialog = useConfirmDialog()

  async function run() {
    if (!opts.selected.value || inflight.value) return
    const ok = await confirmDialog.ask(opts.confirmText, { severity: 'danger' })
    if (!ok) return
    const srcUuid = opts.selected.value
    inflight.value = true
    try {
      await apiCall('idnode/delete', { uuid: srcUuid })
      /* No success toast: the row disappearing from the grid AND
       * the detail pane clearing are sufficient feedback. Add
       * and Clone keep their toasts because each carries
       * information the user can't read off the screen (the
       * "edit on the right" hint and the resolved clone name). */
      opts.selected.value = null
    } catch (e) {
      toast.error(e instanceof Error ? e.message : String(e), {
        summary: opts.errorSummary ?? t('Delete failed'),
      })
    } finally {
      inflight.value = false
    }
  }

  return { inflight, run }
}

export interface AddViaPickerOptions {
  selected: Ref<string | null>
  /** Create endpoint, e.g. `'profile/create'`. */
  createEndpoint: string
  /** Toast shown after a successful create. */
  successMessage: string
  /** Summary line on the error toast. Defaults to `'Add failed'`. */
  errorSummary?: string
  /** Body of the no-uuid error toast. Defaults to a generic message. */
  noUuidErrorMessage?: string
}

export function useAddViaPicker(opts: AddViaPickerOptions) {
  const pickerVisible = ref(false)
  const toast = useToastNotify()

  function onAddClick() {
    pickerVisible.value = true
  }

  async function onClassPicked(classKey: string) {
    pickerVisible.value = false
    try {
      const resp = await apiCall<{ uuid?: string }>(opts.createEndpoint, {
        class: classKey,
        conf: '{}',
      })
      if (typeof resp.uuid === 'string' && resp.uuid) {
        opts.selected.value = resp.uuid
        toast.success(opts.successMessage)
      } else {
        toast.error(
          opts.noUuidErrorMessage ?? t('Server returned no uuid.'),
          { summary: opts.errorSummary ?? t('Add failed') },
        )
      }
    } catch (e) {
      toast.error(e instanceof Error ? e.message : String(e), {
        summary: opts.errorSummary ?? t('Add failed'),
      })
    }
  }

  return { pickerVisible, onAddClick, onClassPicked }
}
