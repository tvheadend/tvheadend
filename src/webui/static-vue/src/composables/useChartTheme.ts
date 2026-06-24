// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * useChartTheme — theme-token-driven palette + base Chart.js
 * option presets for the bandwidth chart.
 *
 * Reads CSS custom properties from the document root so the chart
 * picks up the active theme (Blue / Gray / Access) and re-runs on
 * theme switch via a MutationObserver on `[data-theme]`.
 *
 * The palette is an 8-entry array sourced from the theme's state
 * colours (`--tvh-primary`, `--tvh-success`, `--tvh-warning`,
 * `--tvh-error`) plus four derived hues built via `color-mix` so
 * every theme gets a unique 8-colour rotation without us hard-
 * coding hex values that fight the brand. Series colour assignment
 * is `palette[idx % palette.length]`, so a row's colour stays
 * stable across re-renders as long as its index in the selection
 * doesn't shift.
 *
 * Grid / axis / tooltip styling reads the muted-text and border
 * tokens so the chart looks at home alongside the form chrome on
 * the same surface.
 */

import { onBeforeUnmount, onMounted, ref, type Ref } from 'vue'

export interface ChartTheme {
  /** 8 series colours; index into via `palette[idx % palette.length]`. */
  palette: string[]
  /** Axis line / grid line / tooltip border colour. */
  axisColor: string
  /** Tick / axis label colour. */
  textColor: string
  /** Tooltip background. */
  tooltipBg: string
  /** Tooltip text colour. */
  tooltipFg: string
}

/* Read a CSS custom property from <html>, falling back to a
 * caller-supplied default. happy-dom's getComputedStyle returns
 * '' for unset properties; defend by falling back. */
function readToken(name: string, fallback: string): string {
  if (typeof document === 'undefined') return fallback
  const value = getComputedStyle(document.documentElement)
    .getPropertyValue(name)
    .trim()
  return value === '' ? fallback : value
}

/* Build the palette from theme tokens. The four state colours sit
 * up front for the common 1-4 row case (most-likely first colour
 * is the brand primary). Beyond that we derive four supplementary
 * hues via `color-mix` against the muted-text axis colour so they
 * stay readable on every theme without us hand-picking values that
 * clash. */
function buildPalette(): string[] {
  const primary = readToken('--tvh-primary', '#2563eb')
  const success = readToken('--tvh-success', '#16a34a')
  const warning = readToken('--tvh-warning', '#d97706')
  const error = readToken('--tvh-error', '#dc2626')
  /* Derived hues: rotate the primary toward muted-text to land in
   * a different region of the colour wheel. `color-mix` is a CSS
   * function — Chart.js accepts CSS colour strings directly so we
   * can pass these through verbatim. The browser computes the
   * mix per-render. */
  return [
    primary,
    success,
    warning,
    error,
    `color-mix(in srgb, ${primary} 60%, ${success})`,
    `color-mix(in srgb, ${primary} 60%, ${warning})`,
    `color-mix(in srgb, ${success} 60%, ${error})`,
    `color-mix(in srgb, ${warning} 60%, ${primary})`,
  ]
}

function buildTheme(): ChartTheme {
  return {
    palette: buildPalette(),
    axisColor: readToken('--tvh-border', '#e2e8f0'),
    textColor: readToken('--tvh-text-muted', '#64748b'),
    tooltipBg: readToken('--tvh-bg-surface', '#ffffff'),
    tooltipFg: readToken('--tvh-text', '#0f172a'),
  }
}

export interface UseChartThemeReturn {
  theme: Ref<ChartTheme>
}

/**
 * Composable returning the live chart theme. Subscribes to
 * `data-theme` attribute mutations on `<html>` and republishes the
 * theme so the chart redraws with the new palette without
 * re-creating its canvas.
 */
export function useChartTheme(): UseChartThemeReturn {
  const theme = ref<ChartTheme>(buildTheme())
  let observer: MutationObserver | null = null

  onMounted(() => {
    if (typeof MutationObserver === 'undefined' || typeof document === 'undefined') return
    observer = new MutationObserver(() => {
      theme.value = buildTheme()
    })
    observer.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ['data-theme'],
    })
  })

  onBeforeUnmount(() => {
    observer?.disconnect()
    observer = null
  })

  return { theme }
}
