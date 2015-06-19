/*
 *  Tvheadend - SAT>IP DVB satconf
 *
 *  Copyright (C) 2014 Jaroslav Kysela
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
#include "satip_private.h"
#include "settings.h"

/* **************************************************************************
 * Frontend callbacks
 * *************************************************************************/

static satip_satconf_t *
satip_satconf_find_ele( satip_frontend_t *lfe, mpegts_mux_t *mux )
{
  satip_satconf_t *sfc;
  satip_frontend_t *lfe2;

  if (lfe->sf_master) {
    TAILQ_FOREACH(lfe2, &lfe->sf_device->sd_frontends, sf_link)
      if (lfe2->sf_number != lfe->sf_number &&
          lfe2->sf_number == lfe->sf_master &&
          lfe2->sf_master == 0) {
        lfe = lfe2;
        goto found;
      }
    return 0;
  }
found:
  TAILQ_FOREACH(sfc, &lfe->sf_satconf, sfc_link) {
    if (idnode_set_exists(sfc->sfc_networks, &mux->mm_network->mn_id))
      return sfc;
  }
  return NULL;
}

int
satip_satconf_get_priority
  ( satip_frontend_t *lfe, mpegts_mux_t *mm )
{
  satip_satconf_t *sfc = satip_satconf_find_ele(lfe, mm);
  return sfc ? sfc->sfc_priority : 0;
}

int
satip_satconf_get_grace
  ( satip_frontend_t *lfe, mpegts_mux_t *mm )
{
  satip_satconf_t *sfc = satip_satconf_find_ele(lfe, mm);
  return sfc ? sfc->sfc_grace : 0;
}

int
satip_satconf_get_position
  ( satip_frontend_t *lfe, mpegts_mux_t *mm )
{
  satip_satconf_t *sfc;
  sfc = satip_satconf_find_ele(lfe, mm);
  return sfc && sfc->sfc_enabled ? sfc->sfc_position : 0;
}

/* **************************************************************************
 * Class definition
 * *************************************************************************/

static const void *
satip_satconf_class_network_get( void *o )
{
  satip_satconf_t *sfc  = o;
  return idnode_set_as_htsmsg(sfc->sfc_networks);
}

static int
satip_satconf_class_network_set( void *o, const void *p )
{
  satip_satconf_t *sfc  = o;
  const htsmsg_t *msg = p;
  mpegts_network_t *mn;
  idnode_set_t *n = idnode_set_create(0);
  htsmsg_field_t *f;
  const char *str;
  int i, save;

  HTSMSG_FOREACH(f, msg) {
    if (!(str = htsmsg_field_get_str(f))) continue;
    if (!(mn = mpegts_network_find(str))) continue;
    idnode_set_add(n, &mn->mn_id, NULL, NULL);
  }

  save = n->is_count != sfc->sfc_networks->is_count;
  if (!save) {
    for (i = 0; i < n->is_count; i++)
      if (!idnode_set_exists(sfc->sfc_networks, n->is_array[i])) {
        save = 1;
        break;
      }
  }
  if (save) {
    /* update the local (antenna satconf) network list */
    idnode_set_free(sfc->sfc_networks);
    sfc->sfc_networks = n;
    /* update the input (frontend) network list */
    htsmsg_t *l = htsmsg_create_list();
    satip_frontend_t *lfe = sfc->sfc_lfe, *lfe2;
    satip_satconf_t *sfc2;
    TAILQ_FOREACH(sfc2, &lfe->sf_satconf, sfc_link) {
      for (i = 0; i < sfc2->sfc_networks->is_count; i++)
        htsmsg_add_str(l, NULL,
                       idnode_uuid_as_str(sfc2->sfc_networks->is_array[i]));
    }
    mpegts_input_class_network_set(lfe, l);
    /* update the slave tuners, too */
    TAILQ_FOREACH(lfe2, &lfe->sf_device->sd_frontends, sf_link)
      if (lfe2->sf_number != lfe->sf_number &&
          lfe2->sf_master == lfe->sf_number)
        mpegts_input_class_network_set(lfe2, l);
    htsmsg_destroy(l);
  } else {
    idnode_set_free(n);
  }
  return save;
}

static htsmsg_t *
satip_satconf_class_network_enum( void *o, const char *lang )
{
  htsmsg_t *m = htsmsg_create_map();
  htsmsg_t *p = htsmsg_create_map();
  htsmsg_add_str(m, "type",  "api");
  htsmsg_add_str(m, "uri",   "idnode/load");
  htsmsg_add_str(m, "event", "mpegts_network");
  htsmsg_add_u32(p, "enum",  1);
  htsmsg_add_str(p, "class", dvb_network_dvbs_class.ic_class);
  htsmsg_add_msg(m, "params", p);

  return m;
}

static char *
satip_satconf_class_network_rend( void *o, const char *lang )
{
  satip_satconf_t *sfc  = o;
  htsmsg_t               *l   = idnode_set_as_htsmsg(sfc->sfc_networks);
  char                   *str = htsmsg_list_2_csv(l);
  htsmsg_destroy(l);
  return str;
}

static const char *
satip_satconf_class_get_title ( idnode_t *o, const char *lang )
{
  return ((satip_satconf_t *)o)->sfc_name;
}

static void
satip_satconf_class_save ( idnode_t *in )
{
  satip_satconf_t *sfc = (satip_satconf_t*)in;
  satip_device_save(sfc->sfc_lfe->sf_device);
}

const idclass_t satip_satconf_class =
{
  .ic_class      = "satip_satconf",
  .ic_caption    = N_("Satconf"),
  .ic_event      = "satip_satconf",
  .ic_get_title  = satip_satconf_class_get_title,
  .ic_save       = satip_satconf_class_save,
  .ic_properties = (const property_t[]) {
    {
      .type     = PT_BOOL,
      .id       = "enabled",
      .name     = N_("Enabled"),
      .off      = offsetof(satip_satconf_t, sfc_enabled),
    },
    {
      .type     = PT_STR,
      .id       = "displayname",
      .name     = N_("Name"),
      .off      = offsetof(satip_satconf_t, sfc_name),
      .notify   = idnode_notify_title_changed,
    },
    {
      .type     = PT_INT,
      .id       = "priority",
      .name     = N_("Priority"),
      .off      = offsetof(satip_satconf_t, sfc_priority),
      .opts     = PO_ADVANCED,
    },
    {
      .type     = PT_INT,
      .id       = "timeout",
      .name     = N_("Timeout (seconds)"),
      .off      = offsetof(satip_satconf_t, sfc_grace),
      .opts     = PO_ADVANCED,
      .def.i    = 10
    },
    {
      .type     = PT_INT,
      .id       = "position",
      .name     = N_("Position"),
      .off      = offsetof(satip_satconf_t, sfc_position),
      .def.i    = 1,
      .opts     = PO_RDONLY | PO_ADVANCED,
    },
    {
      .type     = PT_STR,
      .id       = "networks",
      .name     = N_("Networks"),
      .islist   = 1,
      .set      = satip_satconf_class_network_set,
      .get      = satip_satconf_class_network_get,
      .list     = satip_satconf_class_network_enum,
      .rend     = satip_satconf_class_network_rend,
    },
    {}
  }
};

/* **************************************************************************
 * Creation/Config
 * *************************************************************************/

static satip_satconf_t *
satip_satconf_create0
  ( satip_frontend_t *lfe, htsmsg_t *conf, int position )
{
  static const char *tbl[] = {" (AA)", " (AB)", " (BA)", " (BB)"};
  const char *uuid = NULL;
  satip_satconf_t *sfc = calloc(1, sizeof(*sfc));
  char buf[32];
  const char *s;

  /* defaults */
  sfc->sfc_priority = 1;

  if (conf)
    uuid = htsmsg_get_str(conf, "uuid");
  if (idnode_insert(&sfc->sfc_id, uuid, &satip_satconf_class, 0)) {
    free(sfc);
    return NULL;
  }
  sfc->sfc_networks = idnode_set_create(0);
  sfc->sfc_lfe      = lfe;
  sfc->sfc_position = position + 1;
  sfc->sfc_grace    = 10;
  TAILQ_INSERT_TAIL(&lfe->sf_satconf, sfc, sfc_link);
  if (conf)
    idnode_load(&sfc->sfc_id, conf);
  if (sfc->sfc_name == NULL || sfc->sfc_name[0] == '\0') {
    free(sfc->sfc_name);
    s = position < 4 ? tbl[position] : "";
    snprintf(buf, sizeof(buf), "Position #%i%s", position + 1, s);
    sfc->sfc_name = strdup(buf);
  }

  return sfc;
}

void
satip_satconf_create
  ( satip_frontend_t *lfe, htsmsg_t *conf, int def_positions )
{
  htsmsg_t *l, *e;
  htsmsg_field_t *f;
  int pos = 0;

  if (conf && (l = htsmsg_get_list(conf, "satconf"))) {
    satip_satconf_destroy(lfe);
    HTSMSG_FOREACH(f, l) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      if (satip_satconf_create0(lfe, e, pos++))
        lfe->sf_positions++;
    }
  }

  if (lfe->sf_positions == 0)
    for ( ; lfe->sf_positions < def_positions; lfe->sf_positions++)
      satip_satconf_create0(lfe, NULL, lfe->sf_positions);
}

static void
satip_satconf_destroy0
  ( satip_satconf_t *sfc )
{
  satip_frontend_t *lfe = sfc->sfc_lfe;
  TAILQ_REMOVE(&lfe->sf_satconf, sfc, sfc_link);
  idnode_unlink(&sfc->sfc_id);
  idnode_set_free(sfc->sfc_networks);
  free(sfc->sfc_name);
  free(sfc);
}

void
satip_satconf_updated_positions
  ( satip_frontend_t *lfe )
{
  satip_satconf_t *sfc, *sfc_old;
  int i;

  sfc = TAILQ_FIRST(&lfe->sf_satconf);
  for (i = 0; i < lfe->sf_positions; i++) {
    if (sfc == NULL)
      satip_satconf_create0(lfe, NULL, i);
    sfc = sfc ? TAILQ_NEXT(sfc, sfc_link) : NULL;
  }
  while (sfc) {   
    sfc_old = sfc;
    sfc = TAILQ_NEXT(sfc, sfc_link);
    satip_satconf_destroy0(sfc_old);
  }
}

void
satip_satconf_destroy ( satip_frontend_t *lfe )
{
  satip_satconf_t *sfc;

  while ((sfc = TAILQ_FIRST(&lfe->sf_satconf)) != NULL)
    satip_satconf_destroy0(sfc);
  lfe->sf_positions = 0;
}

void
satip_satconf_save ( satip_frontend_t *lfe, htsmsg_t *m )
{
  satip_satconf_t *sfc;
  htsmsg_t *l, *e;

  l = htsmsg_create_list();
  TAILQ_FOREACH(sfc, &lfe->sf_satconf, sfc_link) { 
    e = htsmsg_create_map();
    idnode_save(&sfc->sfc_id, e);
    htsmsg_add_msg(l, NULL, e);
  }
  htsmsg_add_msg(m, "satconf", l);
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
