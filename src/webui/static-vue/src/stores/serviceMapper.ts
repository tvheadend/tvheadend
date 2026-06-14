// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useServiceMapperStore — reactive view onto the in-flight service-
 * mapping job. Backed by the singleton state in `service_mapper.c`
 * exposed via `service/mapper/status` (snapshot) + the
 * `'servicemapper'` Comet notification class (push).
 *
 * Lifecycle:
 *   - On first mount: fetch `service/mapper/status` so the user
 *     sees the current state immediately (handles the case where
 *     they reload mid-job — Comet only delivers post-connect
 *     events).
 *   - Subscribe to the `'servicemapper'` Comet class for live
 *     updates. Each notification carries
 *       { total, ok, fail, ignore, active? }
 *     emitted by `api_service_mapper_notify` at
 *     `src/api/api_service.c:66-70`. The `active` field is the uuid
 *     of the service being probed right now (omitted when idle).
 *
 * Active-service name resolution: the wire only carries the uuid;
 * we fetch `idnode/load?uuid=<uuid>` ONCE per uuid change and
 * cache the resolved name. Mirrors Classic's approach minus the
 * N+1-fetch (Classic fetches on every Comet tick — `static/app/
 * servicemapper.js:72-85`). For a 100-service job this saves
 * ~99 round-trips while keeping the same UX.
 *
 * Stop action: POST `service/mapper/stop` empties the queue
 * server-side; the resulting Comet notify (active=null, counters
 * frozen at the cancel point) flows through this store the same
 * way any other update does.
 */

import { computed, ref } from 'vue'
import { defineStore } from 'pinia'
import { apiCall } from '@/api/client'
import { cometClient } from '@/api/comet'
import type { NotificationMessage } from '@/types/comet'

export interface ServiceMapperStatus {
  /** Total services queued for the job. 0 when no job has run. */
  total: number
  /** Successfully mapped. */
  ok: number
  /** Could not be mapped (tune timeout, unable to decrypt, …). */
  fail: number
  /** Skipped (already mapped, disabled, type filter, …). */
  ignore: number
  /** UUID of the service currently being probed. Null when idle. */
  active: string | null
}

interface IdnodeLoadResponse {
  entries?: Array<{ text?: string; uuid?: string }>
}

interface StatusResponse {
  total?: number
  ok?: number
  fail?: number
  ignore?: number
  active?: string
}

const EMPTY_STATUS: ServiceMapperStatus = {
  total: 0,
  ok: 0,
  fail: 0,
  ignore: 0,
  active: null,
}

export function applyStatusUpdate(
  raw: NotificationMessage | StatusResponse,
): ServiceMapperStatus {
  const r = raw as StatusResponse
  return {
    total: typeof r.total === 'number' ? r.total : 0,
    ok: typeof r.ok === 'number' ? r.ok : 0,
    fail: typeof r.fail === 'number' ? r.fail : 0,
    ignore: typeof r.ignore === 'number' ? r.ignore : 0,
    /* Wire omits the `active` key entirely when idle; treat
     * missing / non-string as null. */
    active: typeof r.active === 'string' && r.active ? r.active : null,
  }
}

/* Job-active heuristic: any of the counters > 0 OR an active
 * uuid is set. The server zeroes counters between jobs, so a
 * fresh-start state has total=0 + active=null + processed=0
 * (idle). Once a job runs, counters stick at their final values
 * until the next job starts. We treat "active uuid set" as the
 * sole truthy signal of in-flight; the counters tell the user
 * what HAS happened, not what IS happening. */
export function jobIsActive(s: ServiceMapperStatus): boolean {
  return s.active !== null
}

/* Number of services processed so far (mapped + failed +
 * ignored). Used for the progress bar's numerator. */
export function processedCount(s: ServiceMapperStatus): number {
  return s.ok + s.fail + s.ignore
}

export const useServiceMapperStore = defineStore('serviceMapper', () => {
  const status = ref<ServiceMapperStatus>({ ...EMPTY_STATUS })
  const stopping = ref(false)
  const error = ref<string | null>(null)

  /* Active-service name cache. Keyed by uuid; populated lazily
   * when the wire delivers a new active uuid we haven't seen
   * before. Value `null` means "fetch in flight"; once resolved
   * the value is a string (or we drop the key on error so the
   * next tick can retry). */
  const activeNameCache = ref<Map<string, string>>(new Map())
  const activeFetchInFlight = ref<Set<string>>(new Set())

  const isActive = computed(() => jobIsActive(status.value))
  const processed = computed(() => processedCount(status.value))
  const progressFraction = computed(() => {
    const t = status.value.total
    if (t <= 0) return 0
    return Math.min(1, processed.value / t)
  })
  const activeServiceName = computed<string | null>(() => {
    const uuid = status.value.active
    if (!uuid) return null
    return activeNameCache.value.get(uuid) ?? null
  })

  async function fetchInitial() {
    error.value = null
    try {
      const res = await apiCall<StatusResponse>('service/mapper/status', {})
      status.value = applyStatusUpdate(res)
    } catch (e) {
      error.value = e instanceof Error ? e.message : `Failed to load: ${String(e)}`
    }
  }

  function applyMessage(msg: NotificationMessage) {
    status.value = applyStatusUpdate(msg)
    /* Kick off the async lookup for the new active uuid (if any)
     * outside the reactive update — avoids the await blocking
     * the comet pipeline. Errors are quiet; the cell falls back
     * to the raw uuid. */
    void resolveActiveName()
  }

  async function resolveActiveName() {
    const uuid = status.value.active
    if (!uuid) return
    if (activeNameCache.value.has(uuid)) return
    if (activeFetchInFlight.value.has(uuid)) return
    activeFetchInFlight.value.add(uuid)
    try {
      const res = await apiCall<IdnodeLoadResponse>('idnode/load', { uuid })
      const text = res.entries?.[0]?.text
      if (typeof text === 'string' && text) {
        /* Spread to break reactive sharing — Map mutations alone
         * don't always trigger Vue's deep tracker on consumers,
         * but a fresh ref assign always does. */
        const next = new Map(activeNameCache.value)
        next.set(uuid, text)
        activeNameCache.value = next
      }
    } catch {
      /* Quiet — UI falls back to raw uuid. Will retry next time
       * this uuid appears in `active`. */
    } finally {
      activeFetchInFlight.value.delete(uuid)
    }
  }

  async function stop() {
    if (stopping.value) return
    stopping.value = true
    error.value = null
    try {
      await apiCall('service/mapper/stop', {})
      /* Comet will deliver the post-stop status. Don't optimistic-
       * clear — the server's view is authoritative. */
    } catch (e) {
      error.value = e instanceof Error ? e.message : `Failed to stop: ${String(e)}`
    } finally {
      stopping.value = false
    }
  }

  /* Subscribe once at store creation. Pinia + cometClient's
   * single-subscription-per-class semantics mean one listener
   * for the app lifetime; the store lives until page reload, so
   * there's no teardown path. */
  cometClient.on('servicemapper', applyMessage)

  return {
    status,
    stopping,
    error,
    isActive,
    processed,
    progressFraction,
    activeServiceName,
    fetchInitial,
    stop,
  }
})
