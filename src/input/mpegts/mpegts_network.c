/*
 *  Tvheadend - MPEGTS input source
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

#include "input.h"
#include "subscriptions.h"
#include "channels.h"
#include "access.h"
#include "dvb_charset.h"

#include <assert.h>

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

static void
mpegts_network_class_save
  ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;
  if (mn->mn_config_save)
    mn->mn_config_save(mn);
}

static const char *
mpegts_network_class_get_title ( idnode_t *in, const char *lang )
{
  static char buf[256];
  mpegts_network_t *mn = (mpegts_network_t*)in;
  *buf = 0;
  if (mn->mn_display_name)
    mn->mn_display_name(mn, buf, sizeof(buf));
  return buf;
}

static const void *
mpegts_network_class_get_num_mux ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm;
  mpegts_network_t *mn = ptr;

  n = 0;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    n++;

  return &n;
}

static const void *
mpegts_network_class_get_num_svc ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm;
  mpegts_service_t *s;
  mpegts_network_t *mn = ptr;

  n = 0;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
      n++;

  return &n;
}

static const void *
mpegts_network_class_get_num_chn ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm;
  mpegts_service_t *s;
  mpegts_network_t *mn = ptr;
  idnode_list_mapping_t *ilm;

  n = 0;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    LIST_FOREACH(s, &mm->mm_services, s_dvb_mux_link)
      LIST_FOREACH(ilm, &s->s_channels, ilm_in1_link)
        n++;

  return &n;
}

static const void *
mpegts_network_class_get_scanq_length ( void *ptr )
{
  static __thread int n;
  mpegts_mux_t *mm;
  mpegts_network_t *mn = ptr;

  n = 0;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    if (mm->mm_scan_state != MM_SCAN_STATE_IDLE)
      n++;

  return &n;
}

static void
mpegts_network_class_idlescan_notify ( void *p, const char *lang )
{
  mpegts_network_t *mn = p;
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
    if (mn->mn_idlescan)
      mpegts_network_scan_queue_add(mm, SUBSCRIPTION_PRIO_SCAN_IDLE,
                                    SUBSCRIPTION_IDLESCAN, 0);
    else if (mm->mm_scan_state  == MM_SCAN_STATE_PEND &&
             mm->mm_scan_weight == SUBSCRIPTION_PRIO_SCAN_IDLE) {
      mm->mm_scan_flags = 0;
      mpegts_network_scan_queue_del(mm);
    }
  }
}

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = N_("MPEG-TS Network"),
  .ic_event      = "mpegts_network",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = mpegts_network_class_save,
  .ic_get_title  = mpegts_network_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "networkname",
      .name     = N_("Network Name"),
      .off      = offsetof(mpegts_network_t, mn_network_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_U16,
      .id       = "nid",
      .name     = N_("Network ID (limit scanning)"),
      .opts     = PO_ADVANCED,
      .off      = offsetof(mpegts_network_t, mn_nid),
    },
    {
      .type     = PT_BOOL,
      .id       = "autodiscovery",
      .name     = N_("Network Discovery"),
      .off      = offsetof(mpegts_network_t, mn_autodiscovery),
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "skipinitscan",
      .name     = N_("Skip Initial Scan"),
      .off      = offsetof(mpegts_network_t, mn_skipinitscan),
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "idlescan",
      .name     = N_("Idle Scan Muxes"),
      .off      = offsetof(mpegts_network_t, mn_idlescan),
      .def.i    = 0,
      .notify   = mpegts_network_class_idlescan_notify,
      .opts     = PO_ADVANCED | PO_HIDDEN,
    },
    {
      .type     = PT_BOOL,
      .id       = "sid_chnum",
      .name     = N_("Service IDs as Channel Numbers"),
      .off      = offsetof(mpegts_network_t, mn_sid_chnum),
      .def.i    = 0,
    },
    {
      .type     = PT_BOOL,
      .id       = "ignore_chnum",
      .name     = N_("Ignore Provider's Channel Numbers"),
      .off      = offsetof(mpegts_network_t, mn_ignore_chnum),
      .def.i    = 0,
    },
#if ENABLE_SATIP_SERVER
    {
      .type     = PT_U16,
      .id       = "satip_source",
      .name     = N_("SAT>IP Source Number"),
      .off      = offsetof(mpegts_network_t, mn_satip_source),
    },
#endif
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character Set"),
      .off      = offsetof(mpegts_network_t, mn_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_BOOL,
      .id       = "localtime",
      .name     = N_("EIT Local Time"),
      .off      = offsetof(mpegts_network_t, mn_localtime),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "num_mux",
      .name     = N_("# Muxes"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_mux,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = N_("# Services"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_svc,
    },
    {
      .type     = PT_INT,
      .id       = "num_chn",
      .name     = N_("# Mapped Channels"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_chn,
    },
    {
      .type     = PT_INT,
      .id       = "scanq_length",
      .name     = N_("Scan Queue length"),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_scanq_length,
    },
    {}
  }
};

/* ****************************************************************************
 * Class methods
 * ***************************************************************************/

static void
mpegts_network_display_name
  ( mpegts_network_t *mn, char *buf, size_t len )
{
  strncpy(buf, mn->mn_network_name ?: "unknown", len);
}

static void
mpegts_network_config_save
  ( mpegts_network_t *mn )
{
  // Nothing - leave to child classes
}

static mpegts_mux_t *
mpegts_network_create_mux
  ( mpegts_network_t *mn, void *origin, uint16_t sid, uint16_t tsid,
    void *aux, int force )
{
  return NULL;
}

static mpegts_service_t *
mpegts_network_create_service
 ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return NULL;
}

static const idclass_t *
mpegts_network_mux_class
  ( mpegts_network_t *mn )
{
  extern const idclass_t mpegts_mux_class;
  return &mpegts_mux_class;
}

static mpegts_mux_t *
mpegts_network_mux_create2
  ( mpegts_network_t *mn, htsmsg_t *conf )
{
  return NULL;
}

static void
mpegts_network_link_delete ( mpegts_network_link_t *mnl )
{
  idnode_notify_changed(&mnl->mnl_input->ti_id);
  LIST_REMOVE(mnl, mnl_mn_link);
  LIST_REMOVE(mnl, mnl_mi_link);
  free(mnl);
}

void
mpegts_network_delete
  ( mpegts_network_t *mn, int delconf )
{
  mpegts_mux_t *mm;
  mpegts_network_link_t *mnl;

  /* Remove from global list */
  LIST_REMOVE(mn, mn_global_link);

  /* Delete all muxes */
  while ((mm = LIST_FIRST(&mn->mn_muxes))) {
    mm->mm_delete(mm, delconf);
  }

  /* Disarm scanning */
  gtimer_disarm(&mn->mn_scan_timer);

  /* Remove from input */
  while ((mnl = LIST_FIRST(&mn->mn_inputs)))
    mpegts_network_link_delete(mnl);

  /* Free memory */
  idnode_unlink(&mn->mn_id);
  free(mn->mn_network_name);
  free(mn->mn_charset);
  free(mn);
}

/* ****************************************************************************
 * Creation/Config
 * ***************************************************************************/

mpegts_network_list_t mpegts_network_all;

mpegts_network_t *
mpegts_network_create0
  ( mpegts_network_t *mn, const idclass_t *idc, const char *uuid,
    const char *netname, htsmsg_t *conf )
{
  char buf[256];

  /* Setup idnode */
  if (idnode_insert(&mn->mn_id, uuid, idc, 0)) {
    if (uuid)
      tvherror("mpegts", "invalid network uuid '%s'", uuid);
    free(mn);
    return NULL;
  }

  /* Default callbacks */
  mn->mn_display_name   = mpegts_network_display_name;
  mn->mn_config_save    = mpegts_network_config_save;
  mn->mn_create_mux     = mpegts_network_create_mux;
  mn->mn_create_service = mpegts_network_create_service;
  mn->mn_mux_class      = mpegts_network_mux_class;
  mn->mn_mux_create2    = mpegts_network_mux_create2;

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_network_all, mn, mn_global_link);

  /* Initialise scanning */
  TAILQ_INIT(&mn->mn_scan_pend);
  TAILQ_INIT(&mn->mn_scan_active);
  gtimer_arm(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);

  /* Defaults */
  mn->mn_satpos = INT_MAX;

  /* Load config */
  if (conf)
    idnode_load(&mn->mn_id, conf);

  /* Name */
  if (netname) mn->mn_network_name = strdup(netname);
  mn->mn_display_name(mn, buf, sizeof(buf));
  tvhtrace("mpegts", "created network %s", buf);

  return mn;
}

void
mpegts_network_class_delete(const idclass_t *idc, int delconf)
{
  mpegts_network_t *mn, *n;

  for (mn = LIST_FIRST(&mpegts_network_all); mn != NULL; mn = n) {
    n = LIST_NEXT(mn, mn_global_link);
    if (mn->mn_id.in_class == idc)
      mpegts_network_delete(mn, delconf);
  }
}

int
mpegts_network_set_nid
  ( mpegts_network_t *mn, uint16_t nid )
{
  char buf[256];
  if (mn->mn_nid == nid)
    return 0;
  mn->mn_nid = nid;
  mn->mn_display_name(mn, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - set nid %04X (%d)", buf, nid, nid);
  return 1;
}

int
mpegts_network_set_network_name
  ( mpegts_network_t *mn, const char *name )
{
  char buf[256];
  if (mn->mn_network_name) return 0;
  if (!name || name[0] == '\0' || !strcmp(name, mn->mn_network_name ?: ""))
    return 0;
  tvh_str_update(&mn->mn_network_name, name);
  mn->mn_display_name(mn, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - set name %s", buf, name);
  return 1;
}

void
mpegts_network_scan ( mpegts_network_t *mn )
{
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
}

/******************************************************************************
 * Network classes/creation
 *****************************************************************************/

mpegts_network_builder_list_t mpegts_network_builders;

void
mpegts_network_register_builder
  ( const idclass_t *idc,
    mpegts_network_t *(*build) (const idclass_t *idc, htsmsg_t *conf) )
{
  mpegts_network_builder_t *mnb = calloc(1, sizeof(mpegts_network_builder_t));
  mnb->idc   = idc;
  mnb->build = build;
  LIST_INSERT_HEAD(&mpegts_network_builders, mnb, link);
}

void
mpegts_network_unregister_builder
  ( const idclass_t *idc )
{
  mpegts_network_builder_t *mnb;
  LIST_FOREACH(mnb, &mpegts_network_builders, link) {
    if (mnb->idc == idc) {
      LIST_REMOVE(mnb, link);
      free(mnb);
      return;
    }
  }
}

mpegts_network_t *
mpegts_network_build
  ( const char *clazz, htsmsg_t *conf )
{
  mpegts_network_builder_t *mnb;
  LIST_FOREACH(mnb, &mpegts_network_builders, link) {
    if (!strcmp(mnb->idc->ic_class, clazz))
      return mnb->build(mnb->idc, conf);
  }
  return NULL;
}

/******************************************************************************
 * Search
 *****************************************************************************/

mpegts_mux_t *
mpegts_network_find_mux
  ( mpegts_network_t *mn, uint16_t onid, uint16_t tsid )
{
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
    if (mm->mm_onid && onid && mm->mm_onid != onid) continue;
    if (mm->mm_tsid == tsid)
      break;
  }
  return mm;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
