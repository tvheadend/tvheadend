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
#include "dvb/dvb_charset.h"

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
    tvhlog(LOG_INFO, "charset", "%d entries loaded", i);
}

/*
 * Initialise the charset list
 */
void dvb_charset_init ( void )
{
  _charset_load_file();
}

/*
 * Find default charset
 */
const char *dvb_charset_find
  ( uint16_t onid, uint16_t tsid, uint16_t sid )
{
  dvb_charset_t *ret = NULL, *enc;
  LIST_FOREACH(enc, &dvb_charset_list, link) {
    if (onid == enc->onid && tsid == enc->tsid) {
      if (sid == enc->sid) {
         ret = enc;
        break;
      } else if (!enc->sid) {
        ret = enc;
      }
    }
  }
  return ret ? ret->charset : NULL;
}
