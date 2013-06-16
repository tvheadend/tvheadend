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
#include "diseqc.h"

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

extern const idclass_t mpegts_input_class;

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
  extern const idclass_t linuxdvb_network_class;
  mpegts_input_t   *mi = o;
  mpegts_network_t *mn = mi->mi_network;

  if (mi->mi_network && !strcmp(idnode_uuid_as_str(&mn->mn_id), s ?: ""))
    return 0;

  mn = s ? idnode_find(s, &linuxdvb_network_class) : NULL;

  if (mn && ((linuxdvb_network_t*)mn)->ln_type != FE_QPSK) {
    tvherror("linuxdvb", "attempt to set network of wrong type");
    return 0;
  }

  mpegts_input_set_network(mi, mn);
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
  .ic_super      = &mpegts_input_class,
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
  ls->ls_frontend->mi_display_name(ls->ls_frontend, buf, len);
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
  int r = ls->ls_frontend->mi_is_free(ls->ls_frontend);
  return r;
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
  int r;
  uint32_t f;
  linuxdvb_satconf_t   *ls = (linuxdvb_satconf_t*)mi;
  linuxdvb_frontend_t *lfe = (linuxdvb_frontend_t*)(mi = ls->ls_frontend);
  linuxdvb_mux_t      *lm  = (linuxdvb_mux_t*)mmi->mmi_mux;

  /* Test run */
  // Note: basically this ensures the tuning params are acceptable
  //       for the FE, so that if they're not we don't have to wait
  //       for things like rotors and switches
  if (!ls->ls_lnb)
    return SM_CODE_TUNING_FAILED;
  f = ls->ls_lnb->lnb_frequency(ls->ls_lnb, lm);
  if (f == (uint32_t)-1)
    return SM_CODE_TUNING_FAILED;
  r = linuxdvb_frontend_tune0(lfe, mmi, f);
  if (r) return r;
  
  /* Switch */

  /* Rotor */

  /* LNB */
  if (ls->ls_lnb->lnb_tune(ls->ls_lnb, lm, lfe->lfe_fe_fd))
    return SM_CODE_TUNING_FAILED;

  /* Tune */
  return linuxdvb_frontend_tune1(lfe, mmi, f);
}

static void
linuxdvb_satconf_open_service
  ( mpegts_input_t *mi, mpegts_service_t *s, int init )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_open_service(ls->ls_frontend, s, init);
}

static void
linuxdvb_satconf_close_service
  ( mpegts_input_t *mi, mpegts_service_t *s )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_close_service(ls->ls_frontend, s);
}

static void
linuxdvb_satconf_started_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_started_mux(ls->ls_frontend, mmi);
}

static void
linuxdvb_satconf_stopped_mux
  ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  ls->ls_frontend->mi_stopped_mux(ls->ls_frontend, mmi);
}

static int
linuxdvb_satconf_has_subscription
  ( mpegts_input_t *mi, mpegts_mux_t *mm )
{
  linuxdvb_satconf_t *ls = (linuxdvb_satconf_t*)mi;
  return ls->ls_frontend->mi_has_subscription(ls->ls_frontend, mm);
}

static int
linuxdvb_satconf_open_pid
  ( linuxdvb_frontend_t *lfe, int pid, const char *name )
{
  linuxdvb_satconf_t  *ls  = (linuxdvb_satconf_t*)lfe;
  lfe = (linuxdvb_frontend_t*)ls->ls_frontend;
  return lfe->lfe_open_pid(lfe, pid, name);
}

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

static uint32_t uni_freq
  ( linuxdvb_lnb_t *lnb, linuxdvb_mux_t *lm )
{
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  if (p->frequency > 11700000)
    return abs(p->frequency - 10600000);
  else
    return abs(p->frequency - 9750000);
}

static int uni_tune
  ( linuxdvb_lnb_t *lnb, linuxdvb_mux_t *lm, int fd )
{
  dvb_mux_conf_t *dmc = &lm->lm_tuning;
  struct dvb_frontend_parameters *p = &dmc->dmc_fe_params;
  return diseqc_setup(fd, 0, 0, p->frequency > 11700000, 0, 0);
}
 
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
  ls->mi_started_mux         = linuxdvb_satconf_started_mux;
  ls->mi_stopped_mux         = linuxdvb_satconf_stopped_mux;
  ls->mi_has_subscription    = linuxdvb_satconf_has_subscription;
  ls->lfe_open_pid           = linuxdvb_satconf_open_pid;

  /* Unoversal LMB */
  ls->ls_lnb = calloc(1, sizeof(linuxdvb_lnb_t));
  ls->ls_lnb->lnb_frequency = uni_freq;
  ls->ls_lnb->lnb_tune      = uni_tune;

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
