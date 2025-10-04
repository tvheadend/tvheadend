/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * tvheadend, HTML/XML helper routines
 */

#include <string.h>

#include "webui/webui.h"
#include "hts_strtab.h"

/* Escape chars */

static struct {
  char        chr;
  const char *esc;
} html_esc_codes[] = {
  { '>',  "&gt;"    },
  { '<',  "&lt;"    },
  { '&',  "&amp;"   },
  { '\'', "&apos;"  },
  { '"',  "&quot;" }
};

static const char *html_escape_char ( const char chr )
{
  int i;
  for ( i = 0; i < sizeof(html_esc_codes) / sizeof(html_esc_codes[0]); i++ ) {
    if (html_esc_codes[i].chr == chr) return html_esc_codes[i].esc;
  }
  return NULL;
}

/**
 * Escape characters that will interfere with xml.
 * Count how many bytes str would contain if it would be rss escapped
 */

size_t
html_escaped_len(const char *src)
{
  size_t len = 0;
  const char *esc;
  while (*src) {
    if ((esc = html_escape_char(*src))) {
      len += strlen(esc);
    } else {
      len++;
    }
    src++;
  }
  return len;
}

/*
 * http (xml) escape a string
 */

const char*
html_escape(char *dst, const char *src, size_t len)
{
  const char *esc;
  len--; // for NUL
  while (*src && len) {
    if ((esc = html_escape_char(*src))) {
      while (*esc && len) {
        *dst = *esc;
        len--; dst++; esc++;
      }
    } else {
      *dst = *src;
      dst++; len--;
    }
    src++;
  }
  *dst = '\0';

  return dst;
}
