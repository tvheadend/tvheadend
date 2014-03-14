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

static void
iptv_service_config_save ( service_t *s )
{
  mpegts_service_t *ms = (mpegts_service_t*)s;
  htsmsg_t *c = htsmsg_create_map();
  service_save(s, c);
  hts_settings_save(c, "input/iptv/muxes/%s/services/%s",
                    idnode_uuid_as_str(&ms->s_dvb_mux->mm_id),
                    idnode_uuid_as_str(&ms->s_id));
  htsmsg_destroy(c);
}

/*
 * Create
 */
iptv_service_t *
iptv_service_create0
  ( iptv_mux_t *im, uint16_t sid, uint16_t pmt,
    const char *uuid, htsmsg_t *conf )
{
  iptv_service_t *is = (iptv_service_t*)
    mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),
                           &mpegts_service_class, uuid,
                           (mpegts_mux_t*)im, sid, pmt, conf);
  
  is->s_config_save = iptv_service_config_save;

  /* Set default service name */
  if (!is->s_dvb_svcname || !*is->s_dvb_svcname)
    if (im->mm_iptv_svcname)
      is->s_dvb_svcname = strdup(im->mm_iptv_svcname);

  return is;
}
