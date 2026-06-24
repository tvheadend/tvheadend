// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors

/*
 * formatBytes — render a byte count in binary units (KiB / MiB /
 * GiB / TiB). Shared by the rail's storage info area and the Home
 * dashboard's health line for disk-space figures.
 *
 * One decimal place from KiB up; bare bytes below 1 KiB.
 */
export function formatBytes(b: number): string {
  if (b >= 1024 ** 4) return `${(b / 1024 ** 4).toFixed(1)} TiB`
  if (b >= 1024 ** 3) return `${(b / 1024 ** 3).toFixed(1)} GiB`
  if (b >= 1024 ** 2) return `${(b / 1024 ** 2).toFixed(1)} MiB`
  if (b >= 1024) return `${(b / 1024).toFixed(1)} KiB`
  return `${b} B`
}
