// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useCommandPalette — module-level singleton for the global Cmd-K
 * command palette. Owns the open/close state, the user's typed
 * query, and the most-recently-used (MRU) ordering that boosts
 * recently-executed commands in the empty-query view.
 *
 * Same shape as `useHelp`: a single instance shared across the
 * app; every entry point toggles the same dialog.
 *
 * The MRU list lives in `localStorage` under
 * `tvh:command-palette:mru`. Each successful command execution
 * moves its id to the front; the list is capped at MRU_LIMIT so
 * it stays small. Reads tolerate a malformed entry (cleared on
 * mismatch) so a user's localStorage edit doesn't break the
 * palette.
 */
import { ref } from 'vue'
import { readStoredJson, writeStoredJson } from '@/utils/storage'

const MRU_STORAGE_KEY = 'tvh:command-palette:mru'
const MRU_LIMIT = 20

/* Persisted flag the Home dashboard reads to auto-dismiss the
 * "Try the command palette" tile. Set on the very first
 * `open()` call so any path that opens the palette — Cmd-K,
 * the pill button, the Home tile itself — counts as discovery
 * and the tile disappears on the next Home visit. */
const SEEN_STORAGE_KEY = 'tvh:home:seen-palette'

const isOpen = ref(false)
const query = ref('')
const mru = ref<string[]>(loadMru())
/* Reactive mirror of the "user has discovered the palette"
 * localStorage flag. Initialized from storage at module load so
 * a returning user starts with `true`; flipped to `true` the
 * first time `open()` runs in the current session. Drives the
 * Home dashboard's "Try the command palette" tile (auto-hides
 * the moment the palette opens, no reload needed). */
const seenPalette = ref<boolean>(loadSeenPalette())

function loadMru(): string[] {
  const parsed = readStoredJson(MRU_STORAGE_KEY, (v): v is unknown[] =>
    Array.isArray(v),
  )
  if (!parsed) return []
  /* Guard against non-string entries from a hand-edited
   * localStorage. Filter rather than throw — palette stays
   * usable even with corrupt persisted state. */
  return parsed.filter((v): v is string => typeof v === 'string').slice(0, MRU_LIMIT)
}

function saveMru(): void {
  /* Quota or private-browsing failures are swallowed by the
   * helper. MRU still works for the current session via the
   * in-memory ref. */
  writeStoredJson(MRU_STORAGE_KEY, mru.value)
}

function open(): void {
  isOpen.value = true
  markPaletteSeen()
}

/* Persist the "user has discovered the palette" flag AND flip the
 * reactive ref. Single-write (we never clear it) so this is a
 * no-op after the first open of the user's lifetime. Tolerant of
 * localStorage being unavailable (private browsing, quota) — the
 * worst case is the tile shows again on a fresh load, never a
 * hard failure. */
function markPaletteSeen(): void {
  if (seenPalette.value) return
  seenPalette.value = true
  if (globalThis.localStorage === undefined) return
  try {
    globalThis.localStorage.setItem(SEEN_STORAGE_KEY, '1')
  } catch {
    /* Silent — same rationale as saveMru above. */
  }
}

function loadSeenPalette(): boolean {
  if (globalThis.localStorage === undefined) return false
  try {
    return globalThis.localStorage.getItem(SEEN_STORAGE_KEY) === '1'
  } catch {
    return false
  }
}

function close(): void {
  isOpen.value = false
  /* Reset the query so the next open shows MRU first instead of
   * resuming whatever the user typed last. Matches the Slack /
   * Linear convention. */
  query.value = ''
}

function toggle(): void {
  if (isOpen.value) close()
  else open()
}

/*
 * Move a command id to the front of the MRU list. Called by the
 * palette after a successful execution. Deduplicates so re-running
 * the same command doesn't grow the list. Caps at MRU_LIMIT so a
 * power user's persistent state stays small.
 */
function recordExecution(commandId: string): void {
  /* Drop any existing occurrence, then unshift to the front. */
  const next = mru.value.filter((id) => id !== commandId)
  next.unshift(commandId)
  if (next.length > MRU_LIMIT) next.length = MRU_LIMIT
  mru.value = next
  saveMru()
}

/*
 * Position of a command id in the MRU list, or -1 if absent.
 * Lower index = more recent. The ranker reads this to boost
 * results that the user has already executed.
 */
function mruRank(commandId: string): number {
  return mru.value.indexOf(commandId)
}

/* Test helper — reset the module state so leaked entries from one
 * test don't bleed into another. Not part of the public surface. */
export function __resetCommandPaletteForTests(): void {
  isOpen.value = false
  query.value = ''
  mru.value = []
  seenPalette.value = false
  if (globalThis.localStorage !== undefined) {
    globalThis.localStorage.removeItem(MRU_STORAGE_KEY)
    globalThis.localStorage.removeItem(SEEN_STORAGE_KEY)
  }
}

export function useCommandPalette() {
  return {
    isOpen,
    query,
    mru,
    seenPalette,
    open,
    close,
    toggle,
    recordExecution,
    mruRank,
  }
}
