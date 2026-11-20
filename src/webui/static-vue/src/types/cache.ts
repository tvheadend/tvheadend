// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * Shapes of api/status/caches (src/api/api_status.c): the shared
 * per-channel timeshift caches, one entry per service holding one,
 * and the budgets they draw on (the response's `budget` member,
 * which the status store keeps as `meta.budget`).
 */

/* Where the data of one slot of a cache's map lives — the
 * SVCBUF_LOC_* values of src/timeshift/timeshift_svcbuf.h. */
export const CacheLoc = {
  gap: 0,
  ram: 1,
  staging: 2,
  disk: 3,
} as const
export type CacheLocCode = (typeof CacheLoc)[keyof typeof CacheLoc]

export interface CacheEntry extends Record<string, unknown> {
  /* The service holding the cache: the row key. */
  uuid: string
  channel?: string
  service?: string
  /* Bytes held, and where: the timeshift RAM budget, RAM waiting for
   * the disk writer, or only on disk. */
  size: number
  ram: number
  staging: number
  disk: number
  segments: number
  /* Unix seconds of the oldest and newest data, and the span between. */
  oldest: number
  newest: number
  span: number
  gap: boolean
  write_error: boolean
  ram_only: boolean
  /* Bytes dropped since the cache last had to start a fresh timeline. */
  dropped: number
  /* Seconds behind live, one per client watching through the cache --
   * live ones at 0 -- and one per recording fed from it. */
  viewers: number[]
  recordings: number[]
  /* Whether each of `viewers` is paused, and who it is -- its client,
   * or user, and address: "Kodi Media Center (192.168.1.20)" --, in
   * the same order. */
  paused?: boolean[]
  clients?: string[]
  /* The programme of each recording, [start, stop] in Unix seconds,
   * padding included: the part of the cache it is made of. */
  recorded?: [number, number][]
  /* Bytes of the cache within those programmes, by where they live. */
  recorded_bytes?: { ram: number; staging: number; disk: number }
  /* The cache's own readers and replay gates, counted. */
  readers: number
  gates: number
  /* The span, oldest first, as slots of CacheLocCode. */
  map: CacheLocCode[]
  keepalive?: boolean
  keepalive_expires?: number
  /* Subscriptions on the service other than a keepalive. */
  subscriptions: number
}

export interface CacheBudget {
  enabled: boolean
  ram_only: boolean
  unlimited: boolean
  size_used: number
  size_max: number
  ram_used: number
  ram_max: number
  staging_used: number
  staging_max: number
  /* Of all that, the bytes the caches' recordings are made of. */
  rec_ram?: number
  rec_staging?: number
  rec_disk?: number
  /* Minutes an idle channel's cache is kept, 0 for off. */
  keepalive: number
  /* Seconds a cache grows to (the timeshift period), 0 for unlimited. */
  period: number
}

/* What a subscription has to do with its channel's cache — the
 * `cache_role` field of api/status/subscriptions rows. */
export type CacheRole = 'viewer' | 'recording' | 'warmup' | 'keepalive'
