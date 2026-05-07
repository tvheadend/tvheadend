/*
 * Module-level shared state for `<ColumnHeaderMenu>`.
 *
 * Each instance allocates a unique numeric id at setup time and
 * derives its open state from `currentlyOpenInstanceId`. Setting
 * the shared ref opens that one instance; setting null closes
 * all. Result: opening any kebab automatically closes any other
 * that was open.
 *
 * Lives in a separate `.ts` so the state is genuinely module-
 * scoped (Vue's `<script setup>` makes top-level vars setup-
 * local, not module-level) AND so tests can import + reset it
 * between cases.
 */

import { ref } from 'vue'

let nextInstanceId = 0

export const currentlyOpenInstanceId = ref<number | null>(null)

export function allocColumnHeaderMenuInstanceId(): number {
  return nextInstanceId++
}

/* Test helper — call from beforeEach to reset module state so a
 * stale `currentlyOpenInstanceId` from a previous test doesn't
 * affect the next one. Underscored to mark it as not part of the
 * production surface. */
export function _resetColumnHeaderMenuStateForTests(): void {
  nextInstanceId = 0
  currentlyOpenInstanceId.value = null
}
