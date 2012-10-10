/*
 *  tvheadend, encoding list
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
#include "encoding.h"
#include "../../settings.h"

/*
 * Process a file
 */
static void _encoding_load_file()
{
  htsmsg_t *l, *e;
  htsmsg_field_t *f;

  encoding_t *enc;
  unsigned int tsid, onid;
  int i = 0;

  l = hts_settings_load("encoding");
  if (l)
  {
    HTSMSG_FOREACH(f, l) {
      if ((e = htsmsg_get_map_by_field(f))) {
        tsid = onid = 0;
        htsmsg_get_u32(e, "tsid", &tsid);
        htsmsg_get_u32(e, "onid", &onid);

        if (tsid == 0 || onid == 0)
          continue;

        enc = calloc(1, sizeof(encoding_t));
        if (enc)
        {
          enc->tsid = tsid;
          enc->onid = onid;
          LIST_INSERT_HEAD(&encoding_list, enc, link);
          i++;
        }
      }
    };
    htsmsg_destroy(l);
  };

  if (i > 0)
    tvhlog(LOG_INFO, "encoding", "%d entries loaded", i);
}

/*
 * Initialise the encoding list
 */
void encoding_init ( void )
{
  _encoding_load_file();
}
