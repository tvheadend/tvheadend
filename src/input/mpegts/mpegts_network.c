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

#include <assert.h>

/* ****************************************************************************
 * Class definition
 * ***************************************************************************/

static void
mpegts_network_class_save
  ( idnode_t *in )
{
  mpegts_network_t *mn = (mpegts_network_t*)in;
  mn->mn_config_save(mn);
}

const idclass_t mpegts_network_class =
{
  .ic_class      = "mpegts_network",
  .ic_caption    = "MPEGTS Network",
  .ic_save       = mpegts_network_class_save,
  .ic_properties = (const property_t[]){
    { PROPDEF1("networkname", "Network Name",
               PT_STR, mpegts_network_t, mn_network_name) },
    { PROPDEF1("nid", "Network ID (limit scanning)",
               PT_U16, mpegts_network_t, mn_nid) },
    { PROPDEF1("autodiscovery", "Network Discovery",
               PT_BOOL, mpegts_network_t, mn_autodiscovery) },
    { PROPDEF1("skipinitscan", "Skip Initial Scan",
               PT_BOOL, mpegts_network_t, mn_skipinitscan) },
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

/* ****************************************************************************
 * Scanning
 * ***************************************************************************/

static void
mpegts_network_initial_scan(void *aux)
{
  mpegts_network_t *mn = aux;
  mpegts_mux_t     *mm;

  tvhtrace("mpegts", "setup initial scan for %p", mn);
  while((mm = TAILQ_FIRST(&mn->mn_initial_scan_pending_queue)) != NULL) {
    assert(mm->mm_initial_scan_status == MM_SCAN_PENDING);
    if (mm->mm_start(mm, "initial scan", 1))
      break;
    assert(mm->mm_initial_scan_status == MM_SCAN_CURRENT);
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

void
mpegts_network_add_input ( mpegts_network_t *mn, mpegts_input_t *mi )
{
  char buf1[256], buf2[265];
  mi->mi_network = mn;
  LIST_INSERT_HEAD(&mn->mn_inputs, mi, mi_network_link);
  mn->mn_display_name(mn, buf1, sizeof(buf1));
  mi->mi_display_name(mi, buf2, sizeof(buf2));
  tvhdebug("mpegts", "%s - added input %s", buf1, buf2);
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
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
