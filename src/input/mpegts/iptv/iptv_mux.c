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
    { PROPDEF1("iptv_url",       "URL",
               PT_STR, iptv_mux_t, mm_iptv_url) },
#if 0
    { PROPDEF1("iptv_interface", "Interface",
               PT_STR, iptv_mux_t, mm_iptv_interface) },
#endif
  }
};

/*
 * Create
 */
iptv_mux_t *
iptv_mux_create ( const char *uuid, const char *url )
{
  /* Create Mux */
  iptv_mux_t *im =
    mpegts_mux_create(iptv_mux, NULL,
                      (mpegts_network_t*)&iptv_network,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE);
  if (url)
    im->mm_iptv_url = strdup(url);

  /* Create Instance */
  mpegts_mux_instance_create0(&im->mm_iptv_instance,
                              &mpegts_mux_instance_class,
                              NULL,
                              (mpegts_input_t*)&iptv_input,
                              (mpegts_mux_t*)im);

  return im;
}

/*
 * Load
 */
static void
iptv_mux_load_one ( iptv_mux_t *im, htsmsg_t *c )
{
  const char *str;

  /* Load core */
  mpegts_mux_load_one((mpegts_mux_t*)im, c);

  /* URL */
  if ((str = htsmsg_get_str(c, "iptv_url")))
    tvh_str_update(&im->mm_iptv_url, str);
}

void
iptv_mux_load_all ( void )
{
  htsmsg_t *s, *m;
  htsmsg_field_t *f;
  iptv_mux_t *im;

  if ((s = hts_settings_load_r(1, "input/mpegts/iptv/muxes"))) {
    HTSMSG_FOREACH(f, s) {
      if (!(m = htsmsg_get_map_by_field(f)) || !(m = htsmsg_get_map(m, "config"))) {
        tvhlog(LOG_ERR, "iptv", "failed to load mux config %s", f->hmf_name);
        continue;
      }
      printf("UUID: %s\n", f->hmf_name);
      printf("CONFIG:\n");
      htsmsg_print(m);

      /* Create */
      im = iptv_mux_create(f->hmf_name, NULL);
      if (!im) {
        tvhlog(LOG_ERR, "iptv", "failed to load mux config %s", f->hmf_name);
        continue;
      }

      /* Configure */
      iptv_mux_load_one(im, m);

      /* Load services */
      iptv_service_load_all(im, f->hmf_name);
    }
  }
}
