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
#include "channels.h"

/*
 * Class
 */
extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;

static inline void
iptv_url_set0 ( char **url, char **sane_url,
                const char *url1, const char *sane_url1 )
{
  free(*url);
  free(*sane_url);
  *url = url1 ? strdup(url1) : NULL;
  *sane_url = sane_url1 ? strdup(sane_url1) : NULL;
}

int
iptv_url_set ( char **url, char **sane_url, const char *str, int allow_file, int allow_pipe )
{
  const char *x;
  char *buf, port[16] = "";
  size_t len;
  url_t u;

  if (strcmp(str ?: "", *url ?: "") == 0)
    return 0;

  if (str == NULL || *str == '\0') {
    iptv_url_set0(url, sane_url, NULL, NULL);
    return 1;
  }
  if (allow_file && !strncmp(str, "file://", 7)) {
    iptv_url_set0(url, sane_url, str, str);
    return 1;
  }
  if (allow_pipe && !strncmp(str, "pipe://", 7)) {
    x = str + strlen(str);
    while (x != str && *x <= ' ')
      x--;
    ((char *)x)[1] = '\0';
    iptv_url_set0(url, sane_url, str, str);
    return 1;
  }
  urlinit(&u);
  if (!urlparse(str, &u)) {
    len = (u.scheme ? strlen(u.scheme) + 3 : 0) +
          (u.host ? strlen(u.host) + 1 : 0) +
          /* port */ 16 +
          (u.path ? strlen(u.path) + 1 : 0) +
          (u.query ? strlen(u.query) + 2 : 0);
    buf = alloca(len);
    if (u.port > 0 && u.port <= 65535)
      snprintf(port, sizeof(port), ":%d", u.port);
    snprintf(buf, len, "%s%s%s%s%s%s%s",
             u.scheme ?: "", u.scheme ? "://" : "",
             u.host ?: "", port,
             u.path ?: "", (u.query && u.query[0]) ? "?" : "",
             u.query ?: "");
    iptv_url_set0(url, sane_url, str, buf);
    urlreset(&u);
    return 1;
  } else {
    if (*url == NULL || **url == '\0')
      iptv_url_set0(url, sane_url, "?", "?");
  }

  return 0;
}                                              

static int
iptv_mux_url_set ( void *p, const void *v )
{
  iptv_mux_t *im = p;
  return iptv_url_set(&im->mm_iptv_url, &im->mm_iptv_url_sane, v, 1, 1);
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
      .name     = N_("Streaming priority"),
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
      .type     = PT_BOOL,
      .id       = "iptv_substitute",
      .name     = N_("Substitute formatters"),
      .off      = offsetof(iptv_mux_t, mm_iptv_substitute),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "iptv_interface",
      .name     = N_("Interface"),
      .off      = offsetof(iptv_mux_t, mm_iptv_interface),
      .list     = network_interfaces_enum,
      .opts     = PO_ADVANCED
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
      .name     = N_("Mux name"),
      .off      = offsetof(iptv_mux_t, mm_iptv_muxname),
    },
    {
      .type     = PT_S64,
      .intextra = CHANNEL_SPLIT,
      .id       = "channel_number",
      .name     = N_("Channel number"),
      .off      = offsetof(iptv_mux_t, mm_iptv_chnum),
    },
    {
      .type     = PT_STR,
      .id       = "iptv_sname",
      .name     = N_("Service name"),
      .off      = offsetof(iptv_mux_t, mm_iptv_svcname),
    },
    {
      .type     = PT_STR,
      .id       = "iptv_epgid",
      .name     = N_("EPG name"),
      .off      = offsetof(iptv_mux_t, mm_iptv_epgid),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_STR,
      .id       = "iptv_icon",
      .name     = N_("Icon URL"),
      .off      = offsetof(iptv_mux_t, mm_iptv_icon),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_BOOL,
      .id       = "iptv_respawn",
      .name     = N_("Respawn (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_respawn),
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_INT,
      .id       = "iptv_kill",
      .name     = N_("Kill signal (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_kill),
      .list     = iptv_muxdvr_class_kill_list,
      .opts     = PO_EXPERT
    },
    {
      .type     = PT_INT,
      .id       = "iptv_kill_timeout",
      .name     = N_("Kill timeout (pipe/secs)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_kill_timeout),
      .opts     = PO_EXPERT,
      .def.i    = 5
    },
    {
      .type     = PT_STR,
      .id       = "iptv_env",
      .name     = N_("Environment (pipe)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_env),
      .opts     = PO_EXPERT | PO_MULTILINE
    },
    {
      .type     = PT_STR,
      .id       = "iptv_hdr",
      .name     = N_("Custom HTTP headers"),
      .off      = offsetof(iptv_mux_t, mm_iptv_hdr),
      .opts     = PO_EXPERT | PO_MULTILINE
    },
    {
      .type     = PT_STR,
      .id       = "iptv_tags",
      .name     = N_("Channel tags"),
      .off      = offsetof(iptv_mux_t, mm_iptv_tags),
      .opts     = PO_ADVANCED | PO_MULTILINE
    },
    {
      .type     = PT_U32,
      .id       = "iptv_satip_dvbt_freq",
      .name     = N_("SAT>IP DVB-T frequency (Hz)"),
      .off      = offsetof(iptv_mux_t, mm_iptv_satip_dvbt_freq),
      .opts     = PO_ADVANCED
    },
    {
      .type     = PT_U32,
      .id       = "iptv_buffer_limit",
      .name     = N_("Buffering limit (ms)"),
      .desc     = N_("Specifies the incoming buffering limit in "
                     "milliseconds (PCR based). If the PCR time "
                     "difference from the system clock is higher than "
                     "this, the incoming stream is paused."),
      .off      = offsetof(iptv_mux_t, mm_iptv_buffer_limit),
      .opts     = PO_ADVANCED,
    },
    {}
  }
};

static htsmsg_t *
iptv_mux_config_save ( mpegts_mux_t *mm, char *filename, size_t fsize )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  snprintf(filename, fsize, "input/iptv/networks/%s/muxes/%s",
           idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
           idnode_uuid_as_str(&mm->mm_id, ubuf2));
  return c;
}

static void
iptv_mux_free ( mpegts_mux_t *mm )
{
  iptv_mux_t *im = (iptv_mux_t *)mm;
  free(im->mm_iptv_url);
  free(im->mm_iptv_url_sane);
  free(im->mm_iptv_url_raw);
  free(im->mm_iptv_muxname);
  free(im->mm_iptv_interface);
  free(im->mm_iptv_svcname);
  free(im->mm_iptv_env);
  free(im->mm_iptv_hdr);
  free(im->mm_iptv_tags);
  free(im->mm_iptv_icon);
  free(im->mm_iptv_epgid);
  mpegts_mux_free(mm);
}

static void
iptv_mux_delete ( mpegts_mux_t *mm, int delconf )
{
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  if (delconf)
    hts_settings_remove("input/iptv/networks/%s/muxes/%s",
                        idnode_uuid_as_str(&mm->mm_network->mn_id, ubuf1),
                        idnode_uuid_as_str(&mm->mm_id, ubuf2));

  mpegts_mux_delete(mm, delconf);
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
  htsmsg_t *c, *c2, *e;
  htsmsg_field_t *f;
  iptv_service_t *ms;
  char ubuf1[UUID_HEX_SIZE];
  char ubuf2[UUID_HEX_SIZE];

  /* Create Mux */
  iptv_mux_t *im =
    mpegts_mux_create(iptv_mux, uuid, (mpegts_network_t*)in,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf);

  /* Callbacks */
  im->mm_display_name     = iptv_mux_display_name;
  im->mm_config_save      = iptv_mux_config_save;
  im->mm_delete           = iptv_mux_delete;
  im->mm_free             = iptv_mux_free;

  if (!im->mm_iptv_kill_timeout)
    im->mm_iptv_kill_timeout = 5;

  sbuf_init(&im->mm_iptv_buffer);

  /* Create Instance */
  (void)mpegts_mux_instance_create(mpegts_mux_instance, NULL,
                                   (mpegts_input_t*)iptv_input,
                                   (mpegts_mux_t*)im);

  /* Services */
  c2 = NULL;
  c = htsmsg_get_map(conf, "services");
  if (c == NULL)
    c = c2 = hts_settings_load_r(1, "input/iptv/networks/%s/muxes/%s/services",
                                 idnode_uuid_as_str(&in->mn_id, ubuf1),
                                 idnode_uuid_as_str(&im->mm_id, ubuf2));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      (void)iptv_service_create0(im, 0, 0, f->hmf_name, e);
    }
  } else if (in->in_service_id) {
    conf = htsmsg_create_map();
    htsmsg_add_u32(conf, "sid", in->in_service_id);
    htsmsg_add_u32(conf, "dvb_servicetype", 1); /* SDTV */
    ms = iptv_service_create0(im, 0, 0, NULL, conf);
    ms->s_pmt_pid = SERVICE_PMT_AUTO;
    htsmsg_destroy(conf);
    if (ms)
      mpegts_network_bouquet_trigger((mpegts_network_t *)in, 0);
  }
  htsmsg_destroy(c2);

  return im;
}
