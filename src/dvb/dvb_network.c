/*
 *  TV Input - Linux DVB interface
 *  Copyright (C) 2007 - 2012 Andreas Öman
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

#include <assert.h>

#include "tvheadend.h"
#include "packet.h"
#include "dvb.h"
#include "epggrab.h"
#include "settings.h"
#include "dvb_support.h"

struct dvb_network_list dvb_networks;

static idnode_t **dvb_network_get_childs(struct idnode *self);

static const idclass_t dvb_network_class = {
  .ic_class = "dvbnetwork",
  .ic_get_childs = dvb_network_get_childs,
  .ic_properties = {
    {
      "autodiscovery", "Auto discovery", PT_BOOL,
      offsetof(dvb_network_t, dn_autodiscovery)
    }, {
      "nitoid", "NIT OID", PT_INT,
      offsetof(dvb_network_t, dn_nitoid)
    }, {
      "disable_pmt_monitor", "Disable PMT monitor", PT_BOOL,
      offsetof(dvb_network_t, dn_disable_pmt_monitor)
    }, {
    }},
};

/**
 *
 */
dvb_network_t *
dvb_network_create(int fe_type, const char *uuid)
{
  dvb_network_t *dn = calloc(1, sizeof(dvb_network_t));
  if(idnode_insert(&dn->dn_id, uuid, &dvb_network_class)) {
    free(dn);
    return NULL;
  }

  dn->dn_fe_type = fe_type;
  TAILQ_INIT(&dn->dn_initial_scan_pending_queue);
  TAILQ_INIT(&dn->dn_initial_scan_current_queue);

  dn->dn_autodiscovery = fe_type != FE_QPSK;
  LIST_INSERT_HEAD(&dvb_networks, dn, dn_global_link);
  return dn;
}



/**
 *
 */
static int
muxsortcmp(const void *A, const void *B)
{
  const dvb_mux_t *a = *(dvb_mux_t **)A;
  const dvb_mux_t *b = *(dvb_mux_t **)B;
  if(a->dm_conf.dmc_fe_params.frequency < b->dm_conf.dmc_fe_params.frequency)
    return -1;
  if(a->dm_conf.dmc_fe_params.frequency > b->dm_conf.dmc_fe_params.frequency)
    return 1;
  return a->dm_conf.dmc_polarisation - b->dm_conf.dmc_polarisation;
}


/**
 *
 */
static idnode_t **
dvb_network_get_childs(struct idnode *self)
{
  dvb_network_t *dn = (dvb_network_t *)self;
  dvb_mux_t *dm;
  int cnt = 1;
  LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link)
    cnt++;

  idnode_t **v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  LIST_FOREACH(dm, &dn->dn_muxes, dm_network_link)
    v[cnt++] = (idnode_t *)dm;
  qsort(v, cnt, sizeof(idnode_t *), muxsortcmp);
  v[cnt] = NULL;
  return v;
}


/**
 *
 */
static void
dvb_network_load(htsmsg_t *m, const char *uuid)
{
  uint32_t fetype;

  if(htsmsg_get_u32(m, "fetype", &fetype))
    return;

  dvb_network_t *dn = dvb_network_create(fetype, uuid);
  if(dn == NULL)
    return;

  htsmsg_get_u32(m, "autodiscovery",       &dn->dn_autodiscovery);
  htsmsg_get_u32(m, "nitoid",              &dn->dn_nitoid);
  htsmsg_get_u32(m, "disable_pmt_monitor", &dn->dn_disable_pmt_monitor);

  dvb_mux_load(dn);

  dvb_network_schedule_initial_scan(dn);
}

#if 1
/**
 *
 */
static void
dvb_network_save(dvb_network_t *dn)
{
  htsmsg_t *m = htsmsg_create_map();

  lock_assert(&global_lock);

  htsmsg_add_u32(m, "fetype",              dn->dn_fe_type);
  htsmsg_add_u32(m, "autodiscovery",       dn->dn_autodiscovery);
  htsmsg_add_u32(m, "nitoid",              dn->dn_nitoid);
  htsmsg_add_u32(m, "disable_pmt_monitor", dn->dn_disable_pmt_monitor);

  hts_settings_save(m, "dvb/networks/%s/config",
                    idnode_uuid_as_str(&dn->dn_id));
  htsmsg_destroy(m);
}
#endif



/**
 *
 */
static void
dvb_network_initial_scan(void *aux)
{
  dvb_network_t *dn = aux;
  dvb_mux_t *dm;

  while((dm = TAILQ_FIRST(&dn->dn_initial_scan_pending_queue)) != NULL) {
    assert(dm->dm_scan_status == DM_SCAN_PENDING);
    if(dvb_mux_tune(dm, "initial scan", 1))
      break;
    assert(dm->dm_scan_status == DM_SCAN_CURRENT);
  }
  gtimer_arm(&dn->dn_initial_scan_timer, dvb_network_initial_scan, dn, 10);
}

/**
 *
 */
void
dvb_network_schedule_initial_scan(dvb_network_t *dn)
{
  gtimer_arm(&dn->dn_initial_scan_timer, dvb_network_initial_scan, dn, 0);
}

/**
 *
 */
void
dvb_network_init(void)
{
  htsmsg_t *l, *c;
  htsmsg_field_t *f;

  if(0) {
    dvb_network_save(dvb_network_create(FE_QAM, NULL));
    exit(0);
  }

  if((l = hts_settings_load_r(1, "dvb/networks")) == NULL)
    return;

  HTSMSG_FOREACH(f, l) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;

    if((c = htsmsg_get_map(c, "config")) == NULL)
      continue;

    dvb_network_load(c, f->hmf_name);
  }
  htsmsg_destroy(l);
}


/**
 *
 */
idnode_t **
dvb_network_root(void)
{
  dvb_network_t *dn;
  int cnt = 1;
  LIST_FOREACH(dn, &dvb_networks, dn_global_link)
    cnt++;

  idnode_t **v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  LIST_FOREACH(dn, &dvb_networks, dn_global_link)
    v[cnt++] = &dn->dn_id;
  v[cnt] = NULL;
  return v;
}
