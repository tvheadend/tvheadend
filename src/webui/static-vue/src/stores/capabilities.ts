/*
 * Capabilities store — compile-time feature flags (e.g. 'tvadapters',
 * 'libav', 'timeshift', 'caclient'). Loaded once at startup via
 * POST /api/config/capabilities; never updated thereafter.
 */

import { defineStore } from 'pinia'
import { ref } from 'vue'
import { apiCall } from '@/api/client'

export const useCapabilitiesStore = defineStore('capabilities', () => {
  const list = ref<string[]>([])
  const loaded = ref(false)
  const error = ref<Error | null>(null)

  async function load() {
    try {
      const result = await apiCall<string[]>('config/capabilities')
      list.value = Array.isArray(result) ? result : []
      loaded.value = true
    } catch (err) {
      error.value = err as Error
      console.error('capabilities load failed:', err)
    }
  }

  function has(cap: string): boolean {
    return list.value.includes(cap)
  }

  return { list, loaded, error, load, has }
})
