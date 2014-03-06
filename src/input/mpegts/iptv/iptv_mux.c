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

/*
 * Class
 */
extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;

const idclass_t iptv_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "iptv_mux",
  .ic_caption    = "IPTV Multiplex",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "iptv_url",
      .name     = "URL",
      .off      = offsetof(iptv_mux_t, mm_iptv_url),
    },
    {
      .type     = PT_STR,
      .id       = "iptv_interface",
      .name     = "Interface",
      .off      = offsetof(iptv_mux_t, mm_iptv_interface),
    },
    {
      .type     = PT_BOOL,
      .id       = "iptv_atsc",
      .name     = "ATSC",
      .off      = offsetof(iptv_mux_t, mm_iptv_atsc),
    },
    {}
  }
};

static void
iptv_mux_config_save ( mpegts_mux_t *mm )
{
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  hts_settings_save(c, "input/iptv/muxes/%s/config",
                    idnode_uuid_as_str(&mm->mm_id));
  htsmsg_destroy(c);
}

static void
iptv_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  if (delconf)
    hts_settings_remove("input/iptv/muxes/%s/config",
                      idnode_uuid_as_str(&mm->mm_id));

  mpegts_mux_delete(mm, delconf);
}

static void
iptv_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  iptv_mux_t *im = (iptv_mux_t*)mm;
  if(im->mm_iptv_url)
    strncpy(buf, im->mm_iptv_url, len);
  else
    *buf = 0;
}

/*
 * Create
 */
iptv_mux_t *
iptv_mux_create ( const char *uuid, htsmsg_t *conf )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;

  /* Create Mux */
  iptv_mux_t *im =
    mpegts_mux_create(iptv_mux, uuid,
                      (mpegts_network_t*)iptv_network,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf);

  /* Callbacks */
  im->mm_display_name     = iptv_mux_display_name;
  im->mm_config_save      = iptv_mux_config_save;
  im->mm_delete           = iptv_mux_delete;

  /* Create Instance */
  (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL,
                                   (mpegts_input_t*)iptv_input,
                                   (mpegts_mux_t*)im);

  /* Services */
  c = hts_settings_load_r(1, "input/iptv/muxes/%s/services",
                          idnode_uuid_as_str(&im->mm_id));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      (void)iptv_service_create0(im, 0, 0, f->hmf_name, e);
    }
  }
  

  return im;
}

/*
 * Load
 */
void
iptv_mux_load_all ( void )
{
  htsmsg_t *s, *e;
  htsmsg_field_t *f;

  if ((s = hts_settings_load_r(1, "input/iptv/muxes"))) {
    HTSMSG_FOREACH(f, s) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      (void)iptv_mux_create(f->hmf_name, e);
    }
  }
}
