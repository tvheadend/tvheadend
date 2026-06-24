// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

/*
 * `base` differs between build and dev:
 *
 *   build: '/gui/static/'   — production assets are served by
 *                              tvheadend's C `/gui/static` route
 *                              (page_static_file handler) so the
 *                              hashed URLs in index.html must carry
 *                              that prefix.
 *
 *   dev:   '/gui/'          — the Vite dev server has no `/static`
 *                              split; the SPA mounts at /gui/ to
 *                              match Vue Router's base. Without this,
 *                              router URLs like /gui/_dev/gridtest
 *                              would 404 in dev because Vite would
 *                              be serving at /gui/static/.
 */
export default defineConfig(({ command }) => ({
  base: command === 'build' ? '/gui/static/' : '/gui/',
  plugins: [vue()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    /*
     * Bumped from Vite's default `modules` (chrome87/edge88/es2020/
     * firefox78/safari14, all 2020/early-2021) to allow top-level
     * await in main.ts. Drops support for those very old browsers;
     * users still on them can fall back to the ExtJS UI at /extjs/.
     */
    target: 'es2022',
  },
  server: {
    proxy: {
      '/api': 'http://localhost:9981',
      '/comet': 'http://localhost:9981',
    },
  },
}))
