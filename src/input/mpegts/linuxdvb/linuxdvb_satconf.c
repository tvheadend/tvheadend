/*
 *  Tvheadend - Linux DVB satconf
 *
 *  Copyright (C) 2013 Adam Sutton
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

#include "tvheadend.h"
#include "linuxdvb_private.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/dvb/dmx.h>

/* **************************************************************************
 * Class definition
 * *************************************************************************/

extern const idclass_t linuxdvb_hardware_class;

static const char*
linuxdvb_satconf_class_network_get(void *o)
{
  linuxdvb_satconf_t *ls = o;
  if (ls->mi_network)
    return idnode_uuid_as_str(&ls->mi_network->mn_id);
  return NULL;
}

static int
linuxdvb_satconf_class_network_set(void *o, const char *s)
{
  mpegts_input_t   *mi = o;
  mpegts_network_t *mn = mi->mi_network;
  linuxdvb_network_t *ln = (linuxdvb_network_t*)mn;

  if (mi->mi_network && !strcmp(idnode_uuid_as_str(&mn->mn_id), s ?: ""))
    return 0;

  if (ln->ln_type != FE_QPSK) {
    tvherror("linuxdvb", "attempt to set network of wrong type");
    return 0;
  }

  mpegts_input_set_network(mi, s ? mpegts_network_find(s) : NULL);
  return 1;
}

static htsmsg_t *
linuxdvb_satconf_class_network_enum(void *o)
{
  extern const idclass_t linuxdvb_network_class;
  int i;
  linuxdvb_network_t *ln;
  htsmsg_t *m = htsmsg_create_list();
  idnode_set_t *is = idnode_find_all(&linuxdvb_network_class);
  for (i = 0; i < is->is_count; i++) {
    ln = (linuxdvb_network_t*)is->is_array[i];
    if (ln->ln_type == FE_QPSK) {
      htsmsg_t *e = htsmsg_create_map();
      htsmsg_add_str(e, "key", idnode_uuid_as_str(&ln->mn_id));
      htsmsg_add_str(e, "val", ln->mn_network_name);
      htsmsg_add_msg(m, NULL, e);
    }
  }
  idnode_set_free(is);
  return m;
}

static const char *
linuxdvb_satconf_class_frontend_get ( void *o )
{
  linuxdvb_satconf_t *ls = o;
  if (ls->ls_frontend)
    return idnode_uuid_as_str(&ls->ls_frontend->mi_id);
  return NULL;
}

static int
linuxdvb_satconf_class_frontend_set ( void *o, const char *u )
{
  extern const idclass_t linuxdvb_frontend_dvbs_class;
  linuxdvb_satconf_t *ls = o;
  mpegts_input_t *lfe = idnode_find(u, &linuxdvb_frontend_dvbs_class);
  if (lfe && ls->ls_frontend != lfe) {
    // TODO: I probably need to clean up the existing linkages
    ls->ls_frontend = lfe;
    return 1;
  }
  return 0;
}

static htsmsg_t *
linuxdvb_satconf_class_frontend_enum (void *o)
{
  extern const idclass_t linuxdvb_frontend_dvbs_class;
  int i;
  char buf[512];
  htsmsg_t *m = htsmsg_create_list();
  idnode_set_t *is = idnode_find_all(&linuxdvb_frontend_dvbs_class);
  for (i = 0; i < is->is_count; i++) {
    mpegts_input_t *mi = (mpegts_input_t*)is->is_array[i];
    htsmsg_t *e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idnode_uuid_as_str(&mi->mi_id));
    *buf = 0;
    mi->mi_display_name(mi, buf, sizeof(buf));
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(m, NULL, e);
  }
  idnode_set_free(is);
  return m;
}

const idclass_t linuxdvb_satconf_class =
{
  .ic_super      = &linuxdvb_hardware_class,
  .ic_class      = "linuxdvb_satconf",
  .ic_caption    = "Linux DVB Satconf",
  //.ic_get_title  = linuxdvb_satconf_class_get_title,
  //.ic_save       = linuxdvb_satconf_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_STR,
      .id       = "frontend",
      .name     = "Frontend",
      .str_get  = linuxdvb_satconf_class_frontend_get,
      .str_set  = linuxdvb_satconf_class_frontend_set,
      .str_enum = linuxdvb_satconf_class_frontend_enum
    },
    {
      .type     = PT_STR,
      .id       = "network",
      .name     = "Network",
      .str_get  = linuxdvb_satconf_class_network_get,
      .str_set  = linuxdvb_satconf_class_network_set,
      .str_enum = linuxdvb_satconf_class_network_enum
    },
    {}
  }
};

/* **************************************************************************
 * Class methods
 * *************************************************************************/

static void
linuxdvb_satconf_display_name ( mpegts_input_t* mi, char *buf, size_t len )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->mi_display_name(ls->ls_frontend, buf, len);
}

static const idclass_t *
linuxdvb_satconf_network_class ( mpegts_input_t *mi )
{
  extern const idclass_t linuxdvb_network_class;
  return &linuxdvb_network_class;
}

static mpegts_network_t *
linuxdvb_satconf_network_create ( mpegts_input_t *mi, htsmsg_t *conf )
{
  return (mpegts_network_t*)
    linuxdvb_network_create0(NULL, FE_QPSK, conf);
}

static int
linuxdvb_satconf_is_enabled ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_is_enabled(ls->ls_frontend);
}

static int
linuxdvb_satconf_is_free ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_is_free(ls->ls_frontend);
}

static int
linuxdvb_satconf_current_weight ( mpegts_input_t *mi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_current_weight(ls->ls_frontend);
}

static void
linuxdvb_satconf_stop_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_stop_mux(ls->ls_frontend, mmi);
}

static int
linuxdvb_satconf_start_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  mi = ls->ls_frontend;
  // TODO: need more here!
  return mi->mi_start_mux(mi, mmi);
}

static void
linuxdvb_satconf_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_open_service(ls->ls_frontend, s);
}

static void
linuxdvb_satconf_close_service
  ( mpegts_input_t *mi, mpegts_service_t *s )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_close_service(ls->ls_frontend, s);
}

static void
linuxdvb_satconf_create_mux_instance
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_create_mux_instance(ls->ls_frontend, mm);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/
 
linuxdvb_satconf_t *
linuxdvb_satconf_create0
  ( const char *uuid, htsmsg_t *conf )
{
  linuxdvb_satconf_t *ls = mpegts_input_create(linuxdvb_satconf, uuid, conf);

  /* Input callbacks */
  ls->mi_is_enabled          = linuxdvb_satconf_is_enabled;
  ls->mi_is_free             = linuxdvb_satconf_is_free;
  ls->mi_current_weight      = linuxdvb_satconf_current_weight;
  ls->mi_display_name        = linuxdvb_satconf_display_name;
  ls->mi_start_mux           = linuxdvb_satconf_start_mux;
  ls->mi_stop_mux            = linuxdvb_satconf_stop_mux;
  ls->mi_open_service        = linuxdvb_satconf_open_service;
  ls->mi_close_service       = linuxdvb_satconf_close_service;
  ls->mi_network_class       = linuxdvb_satconf_network_class;
  ls->mi_network_create      = linuxdvb_satconf_network_create;
  ls->mi_create_mux_instance = linuxdvb_satconf_create_mux_instance;

  return ls;
}

#if 0
void
linuxdvb_satconf_save ( linuxdvb_satconf_t *lfe, htsmsg_t *m )
{
  //mpegts_input_save((mpegts_input_t*)lfe, m);
 // htsmsg_add_str(m, "type", dvb_type2str(lfe->lfe_info.type));
}
#endif

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
