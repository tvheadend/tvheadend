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

static int
iptv_mux_url_set ( void *p, const void *v )
{
  iptv_mux_t *im = p;
  const char *str = v, *x;
  char *buf, port[16] = "";
  size_t len;
  url_t url;

  if (strcmp(str ?: "", im->mm_iptv_url ?: "")) {
    if (str == NULL || *str == '\0') {
      free(im->mm_iptv_url);
      free(im->mm_iptv_url_sane);
      im->mm_iptv_url = NULL;
      im->mm_iptv_url_sane = NULL;
      return 1;
    }
    if (!strncmp(str, "pipe://", 7)) {
      x = str + strlen(str);
      while (x != str && *x <= ' ')
        x--;
      ((char *)x)[1] = '\0';
      free(im->mm_iptv_url);
      free(im->mm_iptv_url_sane);
      im->mm_iptv_url = strdup(str);
      im->mm_iptv_url_sane = strdup(str);
      return 1;
    }
    memset(&url, 0, sizeof(url));
    if (!urlparse(str, &url)) {
      free(im->mm_iptv_url);
      free(im->mm_iptv_url_sane);
      im->mm_iptv_url = strdup(str);
      if (im->mm_iptv_url) {
        len = (url.scheme ? strlen(url.scheme) + 3 : 0) +
              (url.host ? strlen(url.host) + 1 : 0) +
              /* port */ 16 +
              (url.path ? strlen(url.path) + 1 : 0) +
              (url.query ? strlen(url.query) + 2 : 0);
        buf = alloca(len);
        if (url.port > 0 && url.port <= 65535)
          snprintf(port, sizeof(port), ":%d", url.port);
        snprintf(buf, len, "%s%s%s%s%s%s%s",
                 url.scheme ?: "", url.scheme ? "://" : "",
                 url.host ?: "", port,
                 url.path ?: "", url.query ? "?" : "",
                 url.query ?: "");
        im->mm_iptv_url_sane = strdup(buf);
      } else {
        im->mm_iptv_url_sane = NULL;
      }
      urlreset(&url);
      return 1;
    }
    urlreset(&url);
  }
  return 0;
}                                              

static htsmsg_t *
iptv_muxdvr_class_kill_list ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("SIGKILL"),   IPTV_KILL_KILL },
    { N_("SIGTERM"),   IPTV_KILL_TERM },
    { N_("SIGINT"),    IPTV_KILL_INT, },
    { N_("SIGHUP"),    IPTV_KILL_HUP },
    { N_("SIGUSR1"),   IPTV_KILL_USR1 },
    { N_("SIGUSR2"),   IPTV_KILL_USR2 },
  };
  return strtab2htsmsg(tab, 1, lang);
}

const idclass_t iptv_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "iptv_mux",
  .ic_caption    = N_("IPTV Multiplex"),
  .ic_properties = (const property_t[]){
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .off      = offsetof(iptv_mux_t, mm_iptv_priority),
      .def.i    = 0,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "spriority",
      .name     = N_("Streaming Priority"),
      .off      = offsetof(iptv_mux_t, mm_iptv_streaming_priority),
      .def.i    = 0,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "iptv_url",
      .name     = N_("URL"),
      .off      = offsetof(iptv_mux_t, mm_iptv_url),
      .set      = iptv_mux_url_set,
      .opts     = PO_MULTILINE
    },
    {
      .type     = PT_STR,
      .id       = "iptv_interface",
      .name     = N_("Interface"),
      .off      = offsetof(iptv_mux_t, mm_iptv_interface),
    },
    {
      .type     = PT_BOOL,
      .id       = "iptv_atsc",
      .name     = N_("ATSC"),
      .off      = offsetof(iptv_mux_t, mm_iptv_atsc),
    },
    {
      .type     = PT_STR,
      .id       = "iptv_muxname",
      .name     = N_("Mux Name"),
      .off      = offsetof(iptv_mux_t, mm_iptv_muxname),
    },
    {
      .type     = PT_STR,
      .id       = "iptv_sname",
      .name     = N_("Service Name"),
      .off      = offsetof(iptv_mux_t, mm_iptv_svcname),
    },
    {
      .type     = PT_BOOL,
      .id       = "iptv_respawn",
      .name     = N_("Respawn (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_respawn),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "iptv_kill",
      .name     = N_("Kill signal (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_kill),
      .list     = iptv_muxdvr_class_kill_list,
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_INT,
      .id       = "iptv_kill_timeout",
      .name     = N_("Kill timeout (pipe/secs)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_kill_timeout),
      .opts     = PO_ADVANCED,
      .def.i    = 5
    },
    {
      .type     = PT_STR,
      .id       = "iptv_env",
      .name     = N_("Environment (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_env),
      .opts     = PO_ADVANCED | PO_MULTILINE
    },
    {}
  }
};

static void
iptv_mux_config_save ( mpegts_mux_t *mm )
{
  char ubuf[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  hts_settings_save(c, "input/iptv/networks/%s/muxes/%s/config",
                    idnode_uuid_as_sstr(&mm->mm_network->mn_id),
                    idnode_uuid_as_str(&mm->mm_id, ubuf));
  htsmsg_destroy(c);
}

static void
iptv_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  char *url, *url_sane, *muxname;
  iptv_mux_t *im = (iptv_mux_t*)mm;
  char ubuf[UUID_HEX_SIZE];

  if (delconf)
    hts_settings_remove("input/iptv/networks/%s/muxes/%s/config",
                        idnode_uuid_as_sstr(&mm->mm_network->mn_id),
                        idnode_uuid_as_str(&mm->mm_id, ubuf));

  url = im->mm_iptv_url; // Workaround for silly printing error
  url_sane = im->mm_iptv_url_sane;
  muxname = im->mm_iptv_muxname;
  free(im->mm_iptv_interface);
  free(im->mm_iptv_svcname);
  free(im->mm_iptv_env);
  mpegts_mux_delete(mm, delconf);
  free(url);
  free(url_sane);
  free(muxname);
}

static void
iptv_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  iptv_mux_t *im = (iptv_mux_t*)mm;
  if(im->mm_iptv_muxname) {
    strncpy(buf, im->mm_iptv_muxname, len);
    buf[len-1] = '\0';
  } else if(im->mm_iptv_url_sane) {
    strncpy(buf, im->mm_iptv_url_sane, len);
    buf[len-1] = '\0';
  } else
    *buf = 0;
}

/*
 * Create
 */
iptv_mux_t *
iptv_mux_create0 ( iptv_network_t *in, const char *uuid, htsmsg_t *conf )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  char ubuf[UUID_HEX_SIZE];

  /* Create Mux */
  iptv_mux_t *im =
    mpegts_mux_create(iptv_mux, uuid, (mpegts_network_t*)in,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf);

  /* Callbacks */
  im->mm_display_name     = iptv_mux_display_name;
  im->mm_config_save      = iptv_mux_config_save;
  im->mm_delete           = iptv_mux_delete;

  if (!im->mm_iptv_kill_timeout)
    im->mm_iptv_kill_timeout = 5;

  /* Create Instance */
  (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL,
                                   (mpegts_input_t*)iptv_input,
                                   (mpegts_mux_t*)im);

  /* Services */
  c = hts_settings_load_r(1, "input/iptv/networks/%s/muxes/%s/services",
                          idnode_uuid_as_sstr(&in->mn_id),
                          idnode_uuid_as_str(&im->mm_id, ubuf));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      (void)iptv_service_create0(im, 0, 0, f->hmf_name, e);
    }
  }
  htsmsg_destroy(c);

  return im;
}
