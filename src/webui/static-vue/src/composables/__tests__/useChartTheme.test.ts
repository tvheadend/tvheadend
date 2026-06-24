// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

import { afterEach, beforeEach, describe, expect, it } from 'vitest'
import { mount } from '@vue/test-utils'
import { defineComponent, h } from 'vue'
import { useChartTheme, type ChartTheme } from '../useChartTheme'

/* Inject a tiny stylesheet so getComputedStyle returns real values
 * during the test — happy-dom defaults to '' for unset custom
 * properties. */
function applyTheme(name: 'blue' | 'access'): void {
  const style = document.getElementById('test-theme-css') ?? document.createElement('style')
  style.id = 'test-theme-css'
  style.textContent =
    name === 'blue'
      ? `:root {
          --tvh-primary: #2563eb;
          --tvh-success: #16a34a;
          --tvh-warning: #d97706;
          --tvh-error: #dc2626;
          --tvh-text: #0f172a;
          --tvh-text-muted: #64748b;
          --tvh-border: #e2e8f0;
          --tvh-bg-surface: #ffffff;
        }`
      : `:root {
          --tvh-primary: #4d9aff;
          --tvh-success: #4ade80;
          --tvh-warning: #fbbf24;
          --tvh-error: #f87171;
          --tvh-text: #ffffff;
          --tvh-text-muted: #cbd5e1;
          --tvh-border: #3b3b3b;
          --tvh-bg-surface: #1a1a1a;
        }`
  if (!document.head.contains(style)) document.head.appendChild(style)
  document.documentElement.dataset.theme = name
}

beforeEach(() => {
  applyTheme('blue')
})

afterEach(() => {
  delete document.documentElement.dataset.theme
  document.getElementById('test-theme-css')?.remove()
})

function mountHarness(): { theme: () => ChartTheme; unmount: () => void } {
  let api!: ReturnType<typeof useChartTheme>
  const Harness = defineComponent({
    setup() {
      api = useChartTheme()
      return () => h('div')
    },
  })
  const w = mount(Harness)
  return { theme: () => api.theme.value, unmount: () => w.unmount() }
}

describe('useChartTheme — palette shape', () => {
  it('returns an 8-colour palette', () => {
    const { theme, unmount } = mountHarness()
    expect(theme().palette).toHaveLength(8)
    unmount()
  })

  it('first four palette entries are primary / success / warning / error', () => {
    const { theme, unmount } = mountHarness()
    const palette = theme().palette
    expect(palette[0]).toBe('#2563eb')
    expect(palette[1]).toBe('#16a34a')
    expect(palette[2]).toBe('#d97706')
    expect(palette[3]).toBe('#dc2626')
    unmount()
  })

  it('derived entries are CSS color-mix() expressions', () => {
    const { theme, unmount } = mountHarness()
    const palette = theme().palette
    for (let i = 4; i < palette.length; i++) {
      expect(palette[i]).toMatch(/^color-mix\(in srgb,/)
    }
    unmount()
  })

  it('reads axis / text / tooltip colours from theme tokens', () => {
    const { theme, unmount } = mountHarness()
    const t = theme()
    expect(t.axisColor).toBe('#e2e8f0')
    expect(t.textColor).toBe('#64748b')
    expect(t.tooltipBg).toBe('#ffffff')
    expect(t.tooltipFg).toBe('#0f172a')
    unmount()
  })
})

describe('useChartTheme — fallbacks', () => {
  it('falls back to safe defaults when no theme is applied', () => {
    delete document.documentElement.dataset.theme
    document.getElementById('test-theme-css')?.remove()
    const { theme, unmount } = mountHarness()
    const palette = theme().palette
    expect(palette[0]).toMatch(/^#/)
    expect(palette).toHaveLength(8)
    unmount()
  })
})
