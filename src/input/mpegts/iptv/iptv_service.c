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
  mpegts_mux_t     *mm = ((mpegts_service_t *)s)->s_dvb_mux;
  htsmsg_t         *c  = htsmsg_create_map();
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  service_save(s, c);
  hts_settings_save(c, "input/iptv/networks/%s/muxes/%s/services/%s",
                    idnode_uuid_as_sstr(&mm->mm_network->mn_id),
                    idnode_uuid_as_str(&mm->mm_id, ubuf1),
                    idnode_uuid_as_str(&s->s_id, ubuf2));
  htsmsg_destroy(c);
}

static void
iptv_service_delete ( service_t *s, int delconf )
{
  mpegts_mux_t     *mm = ((mpegts_service_t *)s)->s_dvb_mux;
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Remove config */
  if (delconf)
    hts_settings_remove("input/iptv/networks/%s/muxes/%s/services/%s",
                        idnode_uuid_as_sstr(&mm->mm_network->mn_id),
                        idnode_uuid_as_str(&mm->mm_id, ubuf1),
                        idnode_uuid_as_str(&s->s_id, ubuf2));

  /* Note - do no pass the delconf flag - another file location */
  mpegts_service_delete(s, 0);
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
  is->s_delete      = iptv_service_delete;

  /* Set default service name */
  if (!is->s_dvb_svcname || !*is->s_dvb_svcname)
    if (im->mm_iptv_svcname)
      is->s_dvb_svcname = strdup(im->mm_iptv_svcname);

  return is;
}
