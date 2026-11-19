// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Tvheadend contributors
/*
 * clientIcons — which client a viewer is, from the name it gives.
 */
import { describe, expect, it } from 'vitest'
import { BRAND_ICONS, clientKind, clientLabel } from '../clientIcons'

describe('clientKind', () => {
  it('knows the media centres and players by the names they give', () => {
    expect(clientKind('Kodi Media Center (192.168.80.89)')).toBe('kodi')
    expect(clientKind('XBMC Media Center')).toBe('kodi')
    expect(clientKind('Jellyfin (192.168.80.20)')).toBe('jellyfin')
    expect(clientKind('Emby Server')).toBe('emby')
    expect(clientKind('Plex Media Server')).toBe('plex')
    expect(clientKind('VLC/3.0.21 LibVLC/3.0.21')).toBe('vlc')
    expect(clientKind('TVHClient (192.168.80.30)')).toBe('tvhclient')
  })

  it('tells browsers apart by their user agents, the more particular names first', () => {
    const ua = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)'
    expect(clientKind(`${ua} Chrome/130.0.0.0 Safari/537.36`)).toBe('chrome')
    expect(clientKind(`${ua} Chrome/130.0.0.0 Safari/537.36 Edg/130.0`)).toBe('edge')
    expect(clientKind(`${ua} Chrome/130.0.0.0 Safari/537.36 OPR/115.0`)).toBe('opera')
    expect(clientKind(`${ua} Chrome/130.0.0.0 Safari/537.36 Vivaldi/7.0`)).toBe('vivaldi')
    expect(clientKind('Mozilla/5.0 (X11; rv:133.0) Gecko/20100101 Firefox/133.0')).toBe('firefox')
    expect(clientKind('Mozilla/5.0 (Macintosh) Version/18.1 Safari/605.1.15')).toBe('safari')
    expect(clientKind('Mozilla/5.0 (compatible; SomeBrowser)')).toBe('browser')
  })

  it('takes anything else as a client', () => {
    expect(clientKind('Lounge TV (192.168.1.21)')).toBe('other')
    expect(clientKind(undefined)).toBe('other')
    expect(clientKind('')).toBe('other')
  })

  it('says a browser by its name and address, and any other client as given', () => {
    expect(clientLabel('Mozilla/5.0 (X11) Chrome/130.0 (192.168.80.88)', 'Web browser')).toBe(
      'Chrome (192.168.80.88)'
    )
    expect(clientLabel('Mozilla/5.0 (compatible) (192.168.80.88)', 'Web browser')).toBe(
      'Web browser (192.168.80.88)'
    )
    expect(clientLabel('Kodi Media Center (192.168.80.89)', 'Web browser')).toBe(
      'Kodi Media Center (192.168.80.89)'
    )
  })

  it('has an outline and a colour for every brand it knows', () => {
    for (const b of Object.values(BRAND_ICONS)) {
      expect(b.path).toMatch(/^M/)
      expect(b.color).toMatch(/^#[0-9A-F]{6}$/i)
    }
  })
})
