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

static htsmsg_t *
mpegts_network_class_save
  ( idnode_t *in, char *filename, size_t fsize )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;
  if (mn->mn_config_save)
    return mn->mn_config_save(mn, filename, fsize);
  return NULL;
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
  static int n;
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

static htsmsg_t *
mpegts_network_discovery_enum ( void *o, const char *lang )
{
  static const struct strtab tab[] = {
    { N_("Disable"),                  MN_DISCOVERY_DISABLE },
    { N_("New muxes only"),           MN_DISCOVERY_NEW },
    { N_("New muxes + change muxes"), MN_DISCOVERY_CHANGE },
  };
  return strtab2htsmsg(tab, 1, lang);
}

CLASS_DOC(mpegts_network)

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = N_("DVB Inputs - Networks"),
  .ic_doc        = tvh_doc_mpegts_network_class,
  .ic_event      = "mpegts_network",
  .ic_perm_def   = ACCESS_ADMIN,
  .ic_save       = mpegts_network_class_save,
  .ic_get_title  = mpegts_network_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "networkname",
      .name     = N_("Network name"),
      .desc     = N_("Name of the network."),
      .off      = offsetof(mpegts_network_t, mn_network_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_STR,
      .id       = "pnetworkname",
      .name     = N_("Provider network name"),
      .desc     = N_("Provider's network name."),
      .off      = offsetof(mpegts_network_t, mn_provider_network_name),
      .opts     = PO_ADVANCED | PO_HIDDEN,
    },
    {
      .type     = PT_U16,
      .id       = "nid",
      .name     = N_("Network ID (limit scanning)"),
      .desc     = N_("Limited/limit scanning to this network ID only."),
      .opts     = PO_ADVANCED,
      .off      = offsetof(mpegts_network_t, mn_nid),
    },
    {
      .type     = PT_INT,
      .id       = "autodiscovery",
      .name     = N_("Network discovery"),
      .desc     = N_("Discover more muxes using the Network "
                     "Information Table (if available)."),
      .off      = offsetof(mpegts_network_t, mn_autodiscovery),
      .list     = mpegts_network_discovery_enum,
      .opts     = PO_ADVANCED,
      .def.i    = MN_DISCOVERY_NEW
    },
    {
      .type     = PT_BOOL,
      .id       = "skipinitscan",
      .name     = N_("Skip initial scan"),
      .desc     = N_("Skip scanning known muxes when Tvheadend starts. "
                     "If \"initial scan\" is allowed and new muxes are "
                     "found then they will still be scanned. See Help for "
                     "more details."),
      .off      = offsetof(mpegts_network_t, mn_skipinitscan),
      .opts     = PO_EXPERT,
      .def.i    = 1
    },
    {
      .type     = PT_BOOL,
      .id       = "idlescan",
      .name     = N_("Idle scan muxes"),
      .desc     = N_("When nothing else is happening Tvheadend will "
                     "continuously rotate among all muxes and tune to "
                     "them to verify that they are still working when "
                     "the inputs are not used for streaming. If your "
                     "adapters have problems with lots of (endless) "
                     "tuning, disable this. Note that this option "
                     "should be OFF for the normal operation. This type "
                     "of mux probing is not required and it may cause "
                     "issues for SAT>IP (limited number of PID filters)."),
      .off      = offsetof(mpegts_network_t, mn_idlescan),
      .def.i    = 0,
      .notify   = mpegts_network_class_idlescan_notify,
      .opts     = PO_EXPERT | PO_HIDDEN,
    },
    {
      .type     = PT_BOOL,
      .id       = "sid_chnum",
      .name     = N_("Use service IDs as channel numbers"),
      .desc     = N_("Use the provider's service IDs as channel numbers."),
      .off      = offsetof(mpegts_network_t, mn_sid_chnum),
      .opts     = PO_EXPERT,
      .def.i    = 0,
    },
    {
      .type     = PT_BOOL,
      .id       = "ignore_chnum",
      .name     = N_("Ignore provider's channel numbers"),
      .desc     = N_("Don't use the provider's channel numbers."),
      .off      = offsetof(mpegts_network_t, mn_ignore_chnum),
      .opts     = PO_ADVANCED,
      .def.i    = 0,
    },
#if ENABLE_SATIP_SERVER
    {
      .type     = PT_U16,
      .id       = "satip_source",
      .name     = N_("SAT>IP source number"),
      .desc     = N_("The SAT>IP source number."),
      .off      = offsetof(mpegts_network_t, mn_satip_source),
      .opts     = PO_ADVANCED
    },
#endif
    {
      .type     = PT_STR,
      .id       = "charset",
      .name     = N_("Character set"),
      .desc     = N_("The character encoding for this network "
                     "(e.g. UTF-8)."),
      .off      = offsetof(mpegts_network_t, mn_charset),
      .list     = dvb_charset_enum,
      .opts     = PO_ADVANCED | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "localtime",
      .name     = N_("EIT time offset"),
      .desc     = N_("Select the time offset for EIT events."),
      .off      = offsetof(mpegts_network_t, mn_localtime),
      .list     = dvb_timezone_enum,
      .opts     = PO_EXPERT | PO_DOC_NLIST,
    },
    {
      .type     = PT_INT,
      .id       = "num_mux",
      .name     = N_("# Muxes"),
      .desc     = N_("Total number of muxes found on this network."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_mux,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = N_("# Services"),
      .desc     = N_("Total number of services found on this network."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_svc,
    },
    {
      .type     = PT_INT,
      .id       = "num_chn",
      .name     = N_("# Mapped channels"),
      .desc     = N_("Total number of mapped channels on this network."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_chn,
    },
    {
      .type     = PT_INT,
      .id       = "scanq_length",
      .name     = N_("Scan queue length"),
      .desc     = N_("The number of muxes left to scan on this network."),
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_scanq_length,
    },
    {
       .type     = PT_BOOL,
       .id       = "wizard",
       .name     = N_("Wizard"),
       .off      = offsetof(mpegts_network_t, mn_wizard),
       .opts     = PO_NOUI
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

static htsmsg_t *
mpegts_network_config_save
  ( mpegts_network_t *mn, char *filename, size_t size )
{
  // Nothing - leave to child classes
  return NULL;
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

  idnode_save_check(&mn->mn_id, delconf);

  /* Remove from global list */
  LIST_REMOVE(mn, mn_global_link);

  /* Delete all muxes */
  while ((mm = LIST_FIRST(&mn->mn_muxes))) {
    mm->mm_delete(mm, delconf);
  }

  /* Disarm scanning */
  mtimer_disarm(&mn->mn_scan_timer);

  /* Remove from input */
  while ((mnl = LIST_FIRST(&mn->mn_inputs)))
    mpegts_network_link_delete(mnl);

  /* Free memory */
  idnode_unlink(&mn->mn_id);
  free(mn->mn_network_name);
  free(mn->mn_provider_network_name);
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
  mn->mn_delete         = mpegts_network_delete;

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_network_all, mn, mn_global_link);

  /* Initialise scanning */
  TAILQ_INIT(&mn->mn_scan_pend);
  TAILQ_INIT(&mn->mn_scan_active);
  mtimer_arm_rel(&mn->mn_scan_timer, mpegts_network_scan_timer_cb, mn, 0);

  /* Defaults */
  mn->mn_satpos = INT_MAX;
  mn->mn_skipinitscan = 1;
  mn->mn_autodiscovery = MN_DISCOVERY_NEW;

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
      mn->mn_delete(mn, delconf);
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
  int save = 0;
  if (mn->mn_network_name == NULL || mn->mn_network_name[0] == '\0') {
    if (name && name[0] && strcmp(name, mn->mn_network_name ?: "")) {
      tvh_str_update(&mn->mn_network_name, name);
      mn->mn_display_name(mn, buf, sizeof(buf));
      tvhdebug("mpegts", "%s - set name %s", buf, name);
      save = 1;
    }
  }
  if (strcmp(name ?: "", mn->mn_network_name ?: "")) {
    tvh_str_update(&mn->mn_provider_network_name, name ?: "");
    save = 1;
  }
  return save;
}

void
mpegts_network_scan ( mpegts_network_t *mn )
{
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link)
    mpegts_mux_scan_state_set(mm, MM_SCAN_STATE_PEND);
}

void
mpegts_network_get_type_str( mpegts_network_t *mn, char *buf, size_t buflen )
{
  const char *s = "IPTV";
#if ENABLE_MPEGTS_DVB
  dvb_fe_type_t ftype;
  ftype = dvb_fe_type_by_network_class(mn->mn_id.in_class);
  if (ftype != DVB_TYPE_NONE)
    s = dvb_type2str(ftype);
#endif
  snprintf(buf, buflen, "%s", s);
}

/******************************************************************************
 * Wizard
 *****************************************************************************/

htsmsg_t *
mpegts_network_wizard_get
  ( mpegts_input_t *mi, const idclass_t *idc,
    mpegts_network_t *mn, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map(), *l, *e;
  char ubuf[UUID_HEX_SIZE], buf[256];

  if (mi && idc) {
    mi->mi_display_name(mi, buf, sizeof(buf));
    htsmsg_add_str(m, "input_name", buf);
    l = htsmsg_create_list();
    e = htsmsg_create_map();
    htsmsg_add_str(e, "key", idc->ic_class);
    htsmsg_add_str(e, "val", idclass_get_caption(idc, lang));
    htsmsg_add_msg(l, NULL, e);
    htsmsg_add_msg(m, "mpegts_network_types", l);
    if (mn)
      htsmsg_add_str(m, "mpegts_network", idnode_uuid_as_str(&mn->mn_id, ubuf));
  }
  return m;
}

void
mpegts_network_wizard_create
  ( const char *clazz, htsmsg_t **nlist, const char *lang )
{
  char buf[256];
  mpegts_network_t *mn;
  mpegts_network_builder_t *mnb;
  htsmsg_t *conf;

  if (nlist)
    *nlist = NULL;

  mnb = mpegts_network_builder_find(clazz);
  if (mnb == NULL)
    return;

  /* only one network per type */
  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link)
    if (mn->mn_id.in_class == mnb->idc && mn->mn_wizard)
      goto found;

  conf = htsmsg_create_map();
  htsmsg_add_str(conf, "networkname", idclass_get_caption(mnb->idc, lang));
  htsmsg_add_bool(conf, "wizard", 1);
  mn = mnb->build(mnb->idc, conf);
  htsmsg_destroy(conf);
  if (mn)
    idnode_changed(&mn->mn_id);

found:
  if (mn && nlist) {
    *nlist = htsmsg_create_list();
    htsmsg_add_str(*nlist, NULL, idnode_uuid_as_str(&mn->mn_id, buf));
  }
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
  idclass_register(idc);
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

mpegts_network_builder_t *
mpegts_network_builder_find
  ( const char *clazz )
{
  mpegts_network_builder_t *mnb;
  if (clazz == NULL)
    return NULL;
  LIST_FOREACH(mnb, &mpegts_network_builders, link) {
    if (!strcmp(mnb->idc->ic_class, clazz))
      return mnb;
  }
  return NULL;
}

mpegts_network_t *
mpegts_network_build
  ( const char *clazz, htsmsg_t *conf )
{
  mpegts_network_builder_t *mnb;
  mnb = mpegts_network_builder_find(clazz);
  if (mnb)
    return mnb->build(mnb->idc, conf);
  return NULL;
}

/******************************************************************************
 * Search
 *****************************************************************************/

mpegts_mux_t *
mpegts_network_find_mux
  ( mpegts_network_t *mn, uint16_t onid, uint16_t tsid, int check )
{
  mpegts_mux_t *mm;
  LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
    if (mm->mm_onid && onid && mm->mm_onid != onid) continue;
    if (mm->mm_tsid == tsid) {
      if (!check || mm->mm_enabled == MM_ENABLE)
        break;
    }
  }
  return mm;
}

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
