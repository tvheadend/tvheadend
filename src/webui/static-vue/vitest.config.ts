/// <reference types="vitest" />
import { fileURLToPath, URL } from 'node:url'
import { defineConfig } from 'vitest/config'
import vue from '@vitejs/plugin-vue'

/*
 * Vitest config kept separate from vite.config.ts so the production
 * build doesn't carry test-specific globals or env. Tests run via
 * `npm test` (one-shot) or `npm run test:watch` (HMR-style).
 *
 * happy-dom over jsdom: lighter, faster, sufficient for our component
 * tests. If a PrimeVue component ever needs APIs happy-dom doesn't
 * implement, swap to jsdom (drop-in switch in `environment`).
 */
export default defineConfig({
  plugins: [vue()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  test: {
    globals: true,
    environment: 'happy-dom',
    include: ['src/**/__tests__/**/*.test.ts'],
  },
})
