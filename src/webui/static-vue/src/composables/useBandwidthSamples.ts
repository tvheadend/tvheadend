// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useBandwidthSamples — per-row ring buffers driven by the live
 * `useStatusStore.entries` array.
 *
 * The chart's data pump. Watches a getter that returns the current
 * row set (each row carrying one or more bandwidth-metric fields)
 * and snapshots each tick into per-row / per-metric ring buffers.
 * The `<BandwidthChart>` consumer reads `samples` reactively and
 * Chart.js redraws on every change.
 *
 * Why not subscribe to Comet directly: `useStatusStore` already
 * does. Each Comet `input_status` / `subscriptions` notification
 * triggers a silent refetch that mutates the entries array in
 * place via `mergeByKey`, preserving row identity. We just react
 * to those mutations — no parallel subscription, no duplicate
 * fetches.
 *
 * Sampling cadence is the server's natural 1 Hz tick. We dedupe
 * by timestamp inside a 500 ms gate so multiple back-to-back
 * mutations (e.g. a Comet burst causing two refetches in quick
 * succession) don't double-stamp the buffer with the same
 * server-side value.
 */

import { computed, ref, watch, type ComputedRef, type Ref } from 'vue'

export interface Sample {
  /** Wall-clock timestamp in ms when the sample landed. */
  t: number
  /** Sample value in the source row's native units (bits or bytes
   *  per second — see `formatBitrate.toBitsPerSecond` for the
   *  conversion to display units). */
  v: number
}

export interface UseBandwidthSamplesOptions<Row extends Record<string, unknown>> {
  /** Reactive getter for the selected rows. Recomputes on every
   *  Comet-driven entries refresh so we sample fresh values. */
  rows: () => Row[]
  /** Field on the row that identifies it across ticks
   *  (`'uuid'` for inputs, `'id'` for subscriptions). */
  keyField: keyof Row
  /** Numeric fields to sample per row. Streams pass `['bps']`,
   *  subscriptions pass `['in', 'out']`. */
  metrics: readonly string[]
  /** Ring-buffer window in seconds. Trims oldest samples beyond
   *  this horizon on every push. Reactive so the user can change
   *  it live (30 / 60 / 300 in the drawer toolbar). */
  windowSec: Ref<number> | ComputedRef<number>
  /** When true, sampling is suspended; existing samples stay put.
   *  Drives the drawer's pause button. */
  paused: Ref<boolean>
}

/* Minimum gap between samples for a given row+metric. Server
 * sends 1 Hz; we accept anything down to 500 ms but reject bursts
 * tighter than that. */
const DEDUP_WINDOW_MS = 500

export function useBandwidthSamples<Row extends Record<string, unknown>>(
  opts: UseBandwidthSamplesOptions<Row>,
) {
  /* Per-row / per-metric ring buffers. Outer Map keyed by row's
   * identifier (uuid or id), inner Map keyed by metric name. */
  const buffers = ref<Map<string | number, Map<string, Sample[]>>>(new Map())

  function getRowKey(row: Row): string | number | null {
    const v = row[opts.keyField]
    if (typeof v === 'string' || typeof v === 'number') return v
    return null
  }

  function ensureBuffer(key: string | number, metric: string): Sample[] {
    let perRow = buffers.value.get(key)
    if (!perRow) {
      perRow = new Map()
      buffers.value.set(key, perRow)
    }
    let buf = perRow.get(metric)
    if (!buf) {
      buf = []
      perRow.set(metric, buf)
    }
    return buf
  }

  function trim(buf: Sample[], now: number): void {
    const horizon = now - opts.windowSec.value * 1000
    while (buf.length > 0 && buf[0].t < horizon) buf.shift()
  }

  /* Append (or dedup-overwrite) a single metric sample and trim
   * to the active window. Extracted from `sample()` so the outer
   * function stays below the cognitive-complexity cap. */
  function recordMetric(key: string | number, metric: string, value: number, now: number): void {
    const buf = ensureBuffer(key, metric)
    const last = buf[buf.length - 1]
    if (last && now - last.t < DEDUP_WINDOW_MS) {
      /* Same server tick observed twice (e.g. burst Comet refetch)
       * — overwrite the last sample with the freshest value rather
       * than appending a near-duplicate. */
      last.t = now
      last.v = value
    } else {
      buf.push({ t: now, v: value })
    }
    trim(buf, now)
  }

  function readMetricValue(row: Row, metric: string): number {
    const raw = row[metric]
    return typeof raw === 'number' && Number.isFinite(raw) ? raw : 0
  }

  function sample(): void {
    if (opts.paused.value) return
    const now = Date.now()
    const seenKeys = new Set<string | number>()
    for (const row of opts.rows()) {
      const key = getRowKey(row)
      if (key === null) continue
      seenKeys.add(key)
      for (const metric of opts.metrics) {
        recordMetric(key, metric, readMetricValue(row, metric), now)
      }
    }
    /* Drop buffers for rows no longer in the selection so series
     * disappear from the chart when the user deselects. */
    for (const key of buffers.value.keys()) {
      if (!seenKeys.has(key)) buffers.value.delete(key)
    }
    /* Force a reactive notification on the outer ref — mutating
     * nested Maps doesn't trigger by default. */
    buffers.value = new Map(buffers.value)
  }

  /* Re-sample on every entries change. The store mutates rows in
   * place via Object.assign (mergeByKey), so a deep watcher fires
   * on each cell update. */
  watch(opts.rows, sample, { deep: true, immediate: true })
  /* React to window changes — shrinking the window must drop
   * out-of-horizon samples immediately, not wait for the next
   * server tick. */
  watch(opts.windowSec, () => {
    const now = Date.now()
    for (const perRow of buffers.value.values()) {
      for (const buf of perRow.values()) trim(buf, now)
    }
    buffers.value = new Map(buffers.value)
  })

  const samples = computed(() => buffers.value)

  /** Latest value per row+metric, for the legend's "now" column.
   *  Empty buffer reads as 0 — drives the legend's zero-state. */
  const currentValues = computed<Map<string | number, Map<string, number>>>(() => {
    const out = new Map<string | number, Map<string, number>>()
    for (const [key, perRow] of buffers.value) {
      const inner = new Map<string, number>()
      for (const [metric, buf] of perRow) {
        inner.set(metric, buf.length > 0 ? buf[buf.length - 1].v : 0)
      }
      out.set(key, inner)
    }
    return out
  })

  /** Aggregate samples = `Σ rows[i].metric` per timestamp bin.
   *  Used by Aggregate mode. Buckets by 1-second timestamps so
   *  buffers that ticked at slightly different wall-clocks line up. */
  const aggregate = computed<Map<string, Sample[]>>(() => {
    const out = new Map<string, Sample[]>()
    for (const metric of opts.metrics) {
      const bucket = new Map<number, number>()
      for (const perRow of buffers.value.values()) {
        const buf = perRow.get(metric)
        if (!buf) continue
        for (const s of buf) {
          const tBucket = Math.floor(s.t / 1000) * 1000
          bucket.set(tBucket, (bucket.get(tBucket) ?? 0) + s.v)
        }
      }
      const series: Sample[] = []
      for (const t of [...bucket.keys()].sort((a, b) => a - b)) {
        series.push({ t, v: bucket.get(t) ?? 0 })
      }
      out.set(metric, series)
    }
    return out
  })

  /** Wipe every buffer — drives the drawer's reset button. */
  function clear(): void {
    buffers.value = new Map()
  }

  return { samples, currentValues, aggregate, clear }
}
