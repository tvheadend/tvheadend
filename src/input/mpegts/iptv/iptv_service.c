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
  char ubuf0[UUID_HEX_SIZE];
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  service_save(s, c);
  hts_settings_save(c, "input/iptv/networks/%s/muxes/%s/services/%s",
                    idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf0),
                    idnode_uuid_as_str(&mm->mm_id, ubuf1),
                    idnode_uuid_as_str(&s->s_id, ubuf2));
  htsmsg_destroy(c);
}

static void
iptv_service_delete ( service_t *s, int delconf )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  mpegts_mux_t     *mm = is->s_dvb_mux;
  char ubuf0[UUID_HEX_SIZE];
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Remove config */
  if (delconf)
    hts_settings_remove("input/iptv/networks/%s/muxes/%s/services/%s",
                        idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf0),
                        idnode_uuid_as_str(&mm->mm_id, ubuf1),
                        idnode_uuid_as_str(&s->s_id, ubuf2));

  /* Note - do no pass the delconf flag - another file location */
  mpegts_service_delete(s, 0);
}

static const char *
iptv_service_channel_name ( service_t *s )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  iptv_mux_t       *im = (iptv_mux_t *)is->s_dvb_mux;
  if (im->mm_iptv_svcname && im->mm_iptv_svcname[0])
    return im->mm_iptv_svcname;
  return is->s_dvb_svcname;
}

static int64_t
iptv_service_channel_number ( service_t *s )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  iptv_mux_t       *im = (iptv_mux_t *)is->s_dvb_mux;
  if (im->mm_iptv_chnum)
    return im->mm_iptv_chnum;
  return mpegts_service_channel_number(s);
}

static const char *
iptv_service_channel_icon ( service_t *s )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  iptv_mux_t       *im = (iptv_mux_t *)is->s_dvb_mux;
  iptv_network_t   *in = (iptv_network_t *)im->mm_network;
  const char       *ic = im->mm_iptv_icon;
  if (ic && ic[0]) {
    if (strncmp(ic, "http://", 7) == 0 ||
        strncmp(ic, "https://", 8) == 0)
      return ic;
    if (strncmp(ic, "file:///", 8) == 0)
      return ic + 7;
    if (strncmp(ic, "file://", 7) == 0)
      ic += 7;
    if (in && in->in_icon_url && in->in_icon_url[0]) {
      snprintf(prop_sbuf, PROP_SBUF_LEN, "%s/%s", in->in_icon_url, ic);
      return prop_sbuf;
    }
  }
  return NULL;
}

static const char *
iptv_service_channel_epgid ( service_t *s )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  iptv_mux_t       *im = (iptv_mux_t *)is->s_dvb_mux;
  return im->mm_iptv_epgid;
}

static htsmsg_t *
iptv_service_channel_tags ( service_t *s )
{
  iptv_service_t   *is = (iptv_service_t *)s;
  iptv_mux_t       *im = (iptv_mux_t *)is->s_dvb_mux;
  char             *p = im->mm_iptv_tags, *x;
  htsmsg_t         *r = NULL;
  if (p) {
    r = htsmsg_create_list();
    while (*p) {
      while (*p && *p <= ' ') p++;
      x = p;
      while (*p && *p >= ' ') p++;
      if (*p) { *p = '\0'; p++; }
      if (*x)
        htsmsg_add_str(r, NULL, x);
    }
  }
  return r;
}

/*
 * Create
 */
iptv_service_t *
iptv_service_create0
  ( iptv_mux_t *im, uint16_t sid, uint16_t pmt,
    const char *uuid, htsmsg_t *conf )
{
  iptv_network_t *in = (iptv_network_t *)im->mm_network;
  iptv_service_t *is = (iptv_service_t*)
    mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),
                           &mpegts_service_class, uuid,
                           (mpegts_mux_t*)im, sid, pmt, conf);
  
  is->s_config_save    = iptv_service_config_save;
  is->s_delete         = iptv_service_delete;
  is->s_channel_name   = iptv_service_channel_name;
  is->s_channel_number = iptv_service_channel_number;
  is->s_channel_icon   = iptv_service_channel_icon;
  is->s_channel_epgid  = iptv_service_channel_epgid;
  is->s_channel_tags   = iptv_service_channel_tags;

  /* Set default service name */
  if (!is->s_dvb_svcname || !*is->s_dvb_svcname)
    if (im->mm_iptv_svcname)
      is->s_dvb_svcname = strdup(im->mm_iptv_svcname);

  if (in->in_bouquet)
    iptv_bouquet_trigger(in, 1);

  return is;
}
