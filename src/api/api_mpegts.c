/*
 *  tvheadend - API access to MPEGTS system
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
#include "access.h"
#include "htsmsg.h"
#include "api.h"
#include "input.h"
#if ENABLE_MPEGTS_DVB
#include "input/mpegts/scanfile.h"
#endif

/*
 * Inputs
 */
static int
api_mpegts_input_network_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int i, err = EINVAL;
  const char *uuid;
  mpegts_input_t *mi;
  mpegts_network_t *mn;
  idnode_set_t *is;
  char ubuf[UUID_HEX_SIZE];
  extern const idclass_t mpegts_input_class;

  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);

  mi = mpegts_input_find(uuid);
  if (!mi)
    goto exit;

  htsmsg_t     *l  = htsmsg_create_list();
  if ((is = mi->mi_network_list(mi))) {
    for (i = 0; i < is->is_count; i++) {
      char buf[256];
      mn = (mpegts_network_t*)is->is_array[i];
      mn->mn_display_name(mn, buf, sizeof(buf));
      htsmsg_add_msg(l, NULL, htsmsg_create_key_val(idnode_uuid_as_str(&mn->mn_id, ubuf), buf));
    }
    idnode_set_free(is);
  }
  err   = 0;
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

exit:
  tvh_mutex_unlock(&global_lock);

  return err;
}

/*
 * Networks
 */
static void
api_mpegts_network_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  mpegts_network_t *mn;

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    idnode_set_add(ins, (idnode_t*)mn, &conf->filter, perm->aa_lang_ui);
  }
}

static int
api_mpegts_network_builders
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  mpegts_network_builder_t *mnb;
  htsmsg_t *l, *e;

  /* List of available builder classes */
  l = htsmsg_create_list();
  LIST_FOREACH(mnb, &mpegts_network_builders, link)
    if ((e = idclass_serialize(mnb->idc, perm->aa_lang_ui)))
      htsmsg_add_msg(l, NULL, e);

  /* Output */
  *resp = htsmsg_create_map();  
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}

static int
api_mpegts_network_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err;
  const char *class;
  htsmsg_t *conf;
  mpegts_network_t *mn;

  if (!(class = htsmsg_get_str(args, "class")))
    return EINVAL;
  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  mn = mpegts_network_build(class, conf);
  if (mn) {
    err = 0;
    api_idnode_create(resp, &mn->mn_id);
  } else {
    err = EINVAL;
  }
  tvh_mutex_unlock(&global_lock);

  return err;
}

static int
api_mpegts_network_scan
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_field_t *f;
  htsmsg_t *uuids;
  mpegts_network_t *mn;
  const char *uuid;

  if (!(f = htsmsg_field_find(args, "uuid")))
    return EINVAL;
  if ((uuids = htsmsg_field_get_list(f))) {
    HTSMSG_FOREACH(f, uuids) {
      if (!(uuid = htsmsg_field_get_str(f))) continue;
      tvh_mutex_lock(&global_lock);
      mn = mpegts_network_find(uuid);
      if (mn && mn->mn_scan)
        mn->mn_scan(mn);
      tvh_mutex_unlock(&global_lock);
    }
  } else if ((uuid = htsmsg_field_get_str(f))) {
    tvh_mutex_lock(&global_lock);
    mn = mpegts_network_find(uuid);
    if (mn && mn->mn_scan)
      mn->mn_scan(mn);
    tvh_mutex_unlock(&global_lock);
    if (!mn)
      return ENOENT;
  } else {
    return EINVAL;
  }

  return 0;
}

static int
api_mpegts_network_muxclass
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  const idclass_t *idc; 
  mpegts_network_t *mn;
  const char *uuid;

  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;
  
  tvh_mutex_lock(&global_lock);
  
  if (!(mn  = mpegts_network_find(uuid)))
    goto exit;

  if (!(idc = mn->mn_mux_class(mn)))
    goto exit;

  *resp = idclass_serialize(idc, perm->aa_lang_ui);
  err    = 0;

exit:
  tvh_mutex_unlock(&global_lock);
  return err;
}

static int
api_mpegts_network_muxcreate
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err = EINVAL;
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  htsmsg_t *conf;
  const char *uuid;

  if (!(uuid = htsmsg_get_str(args, "uuid")))
    return EINVAL;
  if (!(conf = htsmsg_get_map(args, "conf")))
    return EINVAL;
  
  tvh_mutex_lock(&global_lock);
  
  if (!(mn  = mpegts_network_find(uuid)))
    goto exit;
  
  if (!(mm = mn->mn_mux_create2(mn, conf)))
    goto exit;

  api_idnode_create(resp, &mm->mm_id);
  err = 0;

exit:
  tvh_mutex_unlock(&global_lock);
  return err;
}

/*
 * Muxes
 */
static void
api_mpegts_mux_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  int hide = 1;
  const char *s = htsmsg_get_str(args, "hidemode");
  if (s) {
    if (!strcmp(s, "all"))
      hide = 2;
    else if (!strcmp(s, "none"))
      hide = 0;
  }

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (hide && !mn->mn_enabled) continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      if (hide == 2 && !mm->mm_is_enabled(mm)) continue;
      idnode_set_add(ins, (idnode_t*)mm, &conf->filter, perm->aa_lang_ui);
    }
  }
}

/*
 * Services
 */
static void
api_mpegts_service_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  mpegts_network_t *mn;
  mpegts_mux_t *mm;
  mpegts_service_t *ms;
  int hide = 1;
  const char *s = htsmsg_get_str(args, "hidemode");
  if (s) {
    if (!strcmp(s, "all"))
      hide = 2;
    else if (!strcmp(s, "none"))
      hide = 0;
  }

  LIST_FOREACH(mn, &mpegts_network_all, mn_global_link) {
    if (hide && !mn->mn_enabled) continue;
    LIST_FOREACH(mm, &mn->mn_muxes, mm_network_link) {
      if (hide && !mm->mm_is_enabled(mm)) continue;
      LIST_FOREACH(ms, &mm->mm_services, s_dvb_mux_link) {
        if (hide && !ms->s_verified) continue;
        if (hide == 2 && !ms->s_is_enabled((service_t*)ms, 0)) continue;
        idnode_set_add(ins, (idnode_t*)ms, &conf->filter, perm->aa_lang_ui);
      }
    }
  }
}

/*
 * Mux scheduler
 */
static void
api_mpegts_mux_sched_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args )
{
  mpegts_mux_sched_t *mms;
  LIST_FOREACH(mms, &mpegts_mux_sched_all, mms_link)
    idnode_set_add(ins, (idnode_t*)mms, &conf->filter, perm->aa_lang_ui);
}

static int
api_mpegts_mux_sched_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  int err;
  htsmsg_t *conf;
  mpegts_mux_sched_t *mms;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  mms = mpegts_mux_sched_create(NULL, conf);
  if (mms) {
    err = 0;
    api_idnode_create(resp, &mms->mms_id);
  } else {
    err = EINVAL;
  }
  tvh_mutex_unlock(&global_lock);

  return err;
}

#if ENABLE_MPEGTS_DVB
static int
api_dvb_orbitalpos_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  htsmsg_t *l, *e, *c;
  htsmsg_field_t *f;
  const char *s;
  int satpos, i;
  char buf[128];

  if (!satellites)
    return 0;

  l = htsmsg_create_list();
  HTSMSG_FOREACH(f, satellites) {
    if((c = htsmsg_get_map_by_field(f)) == NULL)
      continue;
    if(htsmsg_get_s32(c, "pos", &satpos))
      continue;
    if((s = htsmsg_get_str(c, "name")) == NULL)
      continue;
    e = htsmsg_create_map();
    dvb_sat_position_to_str(satpos, buf, sizeof(buf));
    htsmsg_add_str(e, "key", buf);
    i = strlen(buf);
    snprintf(buf + i, sizeof(buf) - i, " : %s", s);
    htsmsg_add_str(e, "val", buf);
    htsmsg_add_msg(l, NULL, e);
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}
#endif

#if ENABLE_MPEGTS_DVB
static int
api_dvb_scanfile_list
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp )
{
  char buf[512];
  const char *type = htsmsg_get_str(args, "type");
  int satpos = htsmsg_get_s32_or_default(args, "satpos", INT_MAX);
  scanfile_region_list_t *list = NULL;
  htsmsg_t *l, *e;
  scanfile_region_t *r;
  scanfile_network_t *n;

  if (!type)
    return EINVAL;

  list = scanfile_find_region_list(type);
  if (!list)
    return EINVAL;
  
  l = htsmsg_create_list();
  LIST_FOREACH(r, &list->srl_regions, sfr_link) {
    LIST_FOREACH(n, &r->sfr_networks, sfn_link) {
      if (satpos != INT_MAX &&
          abs(n->sfn_satpos - satpos) > 2 &&
          abs(satpos - n->sfn_satpos) > 2) continue;
      e = htsmsg_create_map();
      sprintf(buf, "%s/%s/%s", type, r->sfr_id, n->sfn_id);
      htsmsg_add_str(e, "key", buf);
      if (strcmp(list->srl_type, "dvb-s")) {
        sprintf(buf, "%s: %s", r->sfr_name, n->sfn_name);
        htsmsg_add_str(e, "val", buf);
      } else {
        htsmsg_add_str(e, "val", n->sfn_name);
      }
      htsmsg_add_msg(l, NULL, e);
    }
  }
  *resp = htsmsg_create_map();
  htsmsg_add_msg(*resp, "entries", l);

  return 0;
}
#endif

/*
 * Init
 */
void
api_mpegts_init ( void )
{
  extern const idclass_t mpegts_network_class;
  extern const idclass_t mpegts_mux_class;
  extern const idclass_t mpegts_service_class;

  static api_hook_t ah[] = {
    { "mpegts/input/network_list", ACCESS_ADMIN, api_mpegts_input_network_list, NULL },
    { "mpegts/network/grid",       ACCESS_ADMIN, api_idnode_grid,  api_mpegts_network_grid },
    { "mpegts/network/class",      ACCESS_ADMIN, api_idnode_class, (void*)&mpegts_network_class },
    { "mpegts/network/builders",   ACCESS_ADMIN, api_mpegts_network_builders, NULL },
    { "mpegts/network/create",     ACCESS_ADMIN, api_mpegts_network_create,   NULL },
    { "mpegts/network/mux_class",  ACCESS_ADMIN, api_mpegts_network_muxclass, NULL },
    { "mpegts/network/mux_create", ACCESS_ADMIN, api_mpegts_network_muxcreate, NULL },
    { "mpegts/network/scan",       ACCESS_ADMIN, api_mpegts_network_scan, NULL },
    { "mpegts/mux/grid",           ACCESS_ADMIN, api_idnode_grid,  api_mpegts_mux_grid },
    { "mpegts/mux/class",          ACCESS_ADMIN, api_idnode_class, (void*)&mpegts_mux_class },
    { "mpegts/service/grid",       ACCESS_ADMIN, api_idnode_grid,  api_mpegts_service_grid },
    { "mpegts/service/class",      ACCESS_ADMIN, api_idnode_class, (void*)&mpegts_service_class },
    { "mpegts/mux_sched/class",    ACCESS_ADMIN, api_idnode_class, (void*)&mpegts_mux_sched_class },
    { "mpegts/mux_sched/grid",     ACCESS_ADMIN, api_idnode_grid, api_mpegts_mux_sched_grid },
    { "mpegts/mux_sched/create",   ACCESS_ADMIN, api_mpegts_mux_sched_create, NULL },
#if ENABLE_MPEGTS_DVB
    { "dvb/orbitalpos/list",       ACCESS_ADMIN, api_dvb_orbitalpos_list, NULL },
    { "dvb/scanfile/list",         ACCESS_ADMIN, api_dvb_scanfile_list, NULL },
#endif
    { NULL },
  };

  api_register_all(ah);
}
