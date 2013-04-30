/*
 *  IPTV Input
 *
 *  Copyright (C) 2013 Andreas Öman
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

#include "iptv_private.h"
#include "settings.h"

extern const idclass_t mpegts_service_class;

/*
 * Create
 */
iptv_service_t *
iptv_service_create
  ( const char *uuid, iptv_mux_t *im, uint16_t onid, uint16_t tsid )
{
  iptv_service_t *is = (iptv_service_t*)
    mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),
                           &mpegts_service_class, uuid,
                           (mpegts_mux_t*)im, onid, tsid);
  return is;
}

/*
 * Load
 */
static void
iptv_service_load_one ( iptv_service_t *is, htsmsg_t *c )
{
  /* Load core */
  mpegts_service_load_one((mpegts_service_t*)is, c);
}

void
iptv_service_load_all ( iptv_mux_t *im, const char *n )
{
  htsmsg_t *s, *c;
  htsmsg_field_t *f;
  iptv_service_t *is;

  if ((s = hts_settings_load_r(1, "input/mpegts/iptv/muxes/%s/services", n))) {
    HTSMSG_FOREACH(f, s) {
      if (!(c = htsmsg_get_map_by_field(f))) {
        tvhlog(LOG_ERR, "iptv", "failed to load svc config %s", f->hmf_name);
        continue;
      }

      /* Create */
      if (!(is = iptv_service_create(f->hmf_name, im, 0, 0))) {
        tvhlog(LOG_ERR, "iptv", "failed to load svc config %s", f->hmf_name);
        continue;
      }

      /* Load */
      iptv_service_load_one(is, c);
    }
  }
}

