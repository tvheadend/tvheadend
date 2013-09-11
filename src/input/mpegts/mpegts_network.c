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

#include "input/mpegts.h"
#include "subscriptions.h"

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
mpegts_network_class_get_title ( idnode_t *in )
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
mpegts_network_class_get_scanq_length ( void *ptr )
{
  static int n;
  mpegts_mux_t *mm;
  mpegts_network_t *mn = ptr;

  n = 0;
  TAILQ_FOREACH(mm, &mn->mn_initial_scan_pending_queue, mm_initial_scan_link)
    n++;
  TAILQ_FOREACH(mm, &mn->mn_initial_scan_current_queue, mm_initial_scan_link)
    n++;

  return &n;
}

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = "MPEGTS Network",
  .ic_save       = mpegts_network_class_save,
  .ic_event      = "mpegts_network",
  .ic_get_title  = mpegts_network_class_get_title,
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "networkname",
      .name     = "Network Name",
      .off      = offsetof(mpegts_network_t, mn_network_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_U16,
      .id       = "nid",
      .name     = "Network ID (limit scanning)",
      .off      = offsetof(mpegts_network_t, mn_nid),
    },
    {
      .type     = PT_BOOL,
      .id       = "autodiscovery",
      .name     = "Network Discovery",
      .off      = offsetof(mpegts_network_t, mn_autodiscovery),
    },
    {
      .type     = PT_BOOL,
      .id       = "skipinitscan",
      .name     = "Skip Initial Scan",
      .off      = offsetof(mpegts_network_t, mn_skipinitscan),
    },
    {
      .type     = PT_INT,
      .id       = "num_mux",
      .name     = "# Muxes",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_mux,
    },
    {
      .type     = PT_INT,
      .id       = "num_svc",
      .name     = "# Services",
      .opts     = PO_RDONLY | PO_NOSAVE,
      .get      = mpegts_network_class_get_num_svc,
    },
    {
      .type     = PT_INT,
      .id       = "scanq_length",
      .name     = "Scan Q length",
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
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t tsid, dvb_mux_conf_t *aux )
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

void
mpegts_network_delete
  ( mpegts_network_t *mn )
{
  mpegts_input_t *mi;
  mpegts_mux_t *mm;

  /* Remove from global list */
  LIST_REMOVE(mn, mn_global_link);

  /* Delete all muxes */
  while ((mm = LIST_FIRST(&mn->mn_muxes))) {
    mm->mm_delete(mm);
  }

  /* Check */
  assert(TAILQ_FIRST(&mn->mn_initial_scan_pending_queue) == NULL);
  assert(TAILQ_FIRST(&mn->mn_initial_scan_current_queue) == NULL);
  

  /* Disable timer */
  gtimer_disarm(&mn->mn_initial_scan_timer);

  /* Remove from input */
  while ((mi = LIST_FIRST(&mn->mn_inputs)))
    mpegts_input_set_network(mi, NULL);

  /* Free memory */
  idnode_unlink(&mn->mn_id);
  free(mn->mn_network_name);
  free(mn);
}

/* ****************************************************************************
 * Scanning
 * ***************************************************************************/

static void
mpegts_network_initial_scan(void *aux)
{
  int r;
  mpegts_network_t  *mn = aux;
  mpegts_mux_t      *mm, *mark = NULL;

  tvhtrace("mpegts", "setup initial scan for %p", mn);
  while((mm = TAILQ_FIRST(&mn->mn_initial_scan_pending_queue)) != NULL) {

    /* Stop */
    if (mm == mark) break;

    assert(mm->mm_initial_scan_status == MM_SCAN_PENDING);
    r = mpegts_mux_subscribe(mm, "initscan", 1);

    /* Stop scanning here */
    if (r == SM_CODE_NO_FREE_ADAPTER)
      break;

    /* Started */
    if (!r) {
      assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
      continue;
    }

    TAILQ_REMOVE(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);

    /* Available tuners can't be used
     * Note: this is subtly different it does not imply there are no free
     *       tuners, just that none of the free ones can service this mux.
     *       therefore we move this to the back of the queue and see if we
     *       can find one we can tune
     */
    if (r == SM_CODE_NO_VALID_ADAPTER) {
      if (!mark) mark = mm;
      TAILQ_INSERT_TAIL(&mn->mn_initial_scan_pending_queue, mm, mm_initial_scan_link);
      continue;
    }

    /* Remove */
    mpegts_mux_initial_scan_fail(mm);
  }
  gtimer_arm(&mn->mn_initial_scan_timer, mpegts_network_initial_scan, mn, 10);
}

void
mpegts_network_schedule_initial_scan ( mpegts_network_t *mn )
{
  gtimer_arm(&mn->mn_initial_scan_timer, mpegts_network_initial_scan, mn, 0);
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
  idnode_insert(&mn->mn_id, uuid, idc);
  if (conf)
    idnode_load(&mn->mn_id, conf);

  /* Default callbacks */
  mn->mn_display_name   = mpegts_network_display_name;
  mn->mn_config_save    = mpegts_network_config_save;
  mn->mn_create_mux     = mpegts_network_create_mux;
  mn->mn_create_service = mpegts_network_create_service;
  mn->mn_mux_class      = mpegts_network_mux_class;
  mn->mn_mux_create2    = mpegts_network_mux_create2;

  /* Network name */
  if (netname) mn->mn_network_name = strdup(netname);

  /* Init Qs */
  TAILQ_INIT(&mn->mn_initial_scan_pending_queue);
  TAILQ_INIT(&mn->mn_initial_scan_current_queue);

  /* Add to global list */
  LIST_INSERT_HEAD(&mpegts_network_all, mn, mn_global_link);

  mn->mn_display_name(mn, buf, sizeof(buf));
  tvhtrace("mpegts", "created network %s", buf);

  return mn;
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
  if (!name || !strcmp(name, mn->mn_network_name ?: ""))
    return 0;
  tvh_str_update(&mn->mn_network_name, name);
  mn->mn_display_name(mn, buf, sizeof(buf));
  tvhdebug("mpegts", "%s - set name %s", buf, name);
  return 1;
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
