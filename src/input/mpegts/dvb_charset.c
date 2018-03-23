/*
 *  tvheadend, charset list
 *  Copyright (C) 2012 Mariusz Białończyk
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include "tvheadend.h"
#include "settings.h"
#include "dvb_charset.h"
#include "input.h"

static LIST_HEAD(,dvb_charset) dvb_charset_list;

/*
 * Process a file
 */
static void _charset_load_file()
{
  htsmsg_t *l, *e;
  htsmsg_field_t *f;

  dvb_charset_t *enc;
  const char *charset;
  uint32_t tsid, onid, sid;
  int i = 0;

  l = hts_settings_load("charset");
  if (l)
  {
    HTSMSG_FOREACH(f, l) {
      if ((e = htsmsg_get_map_by_field(f))) {
        tsid = onid = sid = 0;
        htsmsg_get_u32(e, "onid", &onid);
        htsmsg_get_u32(e, "tsid", &tsid);
        htsmsg_get_u32(e, "sid",  &sid);
        charset = htsmsg_get_str(e, "charset");

        if (tsid == 0 || onid == 0 || !charset)
          continue;

        enc = calloc(1, sizeof(dvb_charset_t));
        if (enc)
        {
          enc->onid    = onid;
          enc->tsid    = tsid;
          enc->sid     = sid;
          enc->charset = strdup(charset);
          LIST_INSERT_HEAD(&dvb_charset_list, enc, link);
          i++;
        }
      }
    };
    htsmsg_destroy(l);
  };

  if (i > 0)
    tvhinfo(LS_CHARSET, "%d entries loaded", i);
}

/*
 * Initialise the charset list
 */
void dvb_charset_init ( void )
{
  _charset_load_file();
}

/*
 *
 */
void dvb_charset_done ( void )
{
  dvb_charset_t *enc;
  
  while ((enc = LIST_FIRST(&dvb_charset_list)) != NULL) {
    LIST_REMOVE(enc, link);
    free((void *)enc->charset);
    free(enc);
  }
}

/*
 * Find default charset
 */
const char *dvb_charset_find
  ( mpegts_network_t *mn, mpegts_mux_t *mm, mpegts_service_t *s )
{
  dvb_charset_t *ret = NULL, *enc;

  if (!mm && s)  mm = s->s_dvb_mux;
  if (!mn && mm) mn = mm->mm_network;

  /* User Overrides */
  if (s && s->s_dvb_charset && *s->s_dvb_charset)
    return s->s_dvb_charset;
  if (mm && mm->mm_charset && *mm->mm_charset)
    return mm->mm_charset;
  if (mn && mn->mn_charset && *mn->mn_charset)
    return mn->mn_charset;

  /* Global overrides */
  if (mm) {
    LIST_FOREACH(enc, &dvb_charset_list, link) {
      if (mm->mm_onid == enc->onid && mm->mm_tsid == enc->tsid) {
        if (s && service_id16(s) == enc->sid) {
          ret = enc;
          break;
        } else if (!enc->sid) {
          ret = enc;
        }
      }
    }
  }
  return ret ? ret->charset : NULL;
}

/*
 * List of available charsets
 */
htsmsg_t *
dvb_charset_enum ( void *p, const char *lang )
{
  int i;
  static const char *charsets[] = {
    "AUTO",
    "ISO-6937",
    "ISO-8859-1",
    "ISO-8859-2",
    "ISO-8859-3",
    "ISO-8859-4",
    "ISO-8859-5",
    "ISO-8859-6",
    "ISO-8859-7",
    "ISO-8859-8",
    "ISO-8859-9",
    "ISO-8859-10",
    "ISO-8859-11",
    "ISO-8859-12",
    "ISO-8859-13",
    "ISO-8859-14",
    "ISO-8859-15",
    "UTF-8",
    "GB2312",
    "UCS2",
    "AUTO_POLISH",
  };
  htsmsg_t *m = htsmsg_create_list();
  for ( i = 0; i < ARRAY_SIZE(charsets); i++)
    htsmsg_add_str(m, NULL, charsets[i]);
  return m;
}
