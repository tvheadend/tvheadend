/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * API - elementary stream filter related calls
 */

#include "tvheadend.h"
#include "esfilter.h"
#include "access.h"
#include "api.h"

static void
api_esfilter_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args,
    esfilter_class_t cls )
{
  esfilter_t *esf;

  TAILQ_FOREACH(esf, &esfilters[cls], esf_link)
    idnode_set_add(ins, (idnode_t*)esf, &conf->filter, perm->aa_lang_ui);
}

static int
api_esfilter_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp,
    esfilter_class_t cls )
{
  htsmsg_t *conf;
  esfilter_t *esf;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  tvh_mutex_lock(&global_lock);
  esf = esfilter_create(cls, NULL, conf, 1);
  if (esf)
    api_idnode_create(resp, &esf->esf_id);
  tvh_mutex_unlock(&global_lock);

  return 0;
}

#define ESFILTER(func, t) \
static void api_esfilter_grid_##func \
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args ) \
{ return api_esfilter_grid(perm, ins, conf, args, (t)); } \
static int api_esfilter_create_##func \
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp ) \
{ return api_esfilter_create(perm, opaque, op, args, resp, (t)); }

ESFILTER(video, ESF_CLASS_VIDEO);
ESFILTER(audio, ESF_CLASS_AUDIO);
ESFILTER(teletext, ESF_CLASS_TELETEXT);
ESFILTER(subtit, ESF_CLASS_SUBTIT);
ESFILTER(ca, ESF_CLASS_CA);
ESFILTER(other, ESF_CLASS_OTHER);

void api_esfilter_init ( void )
{
  static api_hook_t ah[] = {
    { "esfilter/video/class",    ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_video },
    { "esfilter/video/grid",     ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_video },
    { "esfilter/video/create",   ACCESS_ADMIN,     api_esfilter_create_video, NULL },

    { "esfilter/audio/class",    ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_audio },
    { "esfilter/audio/grid",     ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_audio },
    { "esfilter/audio/create",   ACCESS_ADMIN,     api_esfilter_create_audio, NULL },

    { "esfilter/teletext/class", ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_teletext },
    { "esfilter/teletext/grid",  ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_teletext },
    { "esfilter/teletext/create",ACCESS_ADMIN,     api_esfilter_create_teletext, NULL },

    { "esfilter/subtit/class",   ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_subtit },
    { "esfilter/subtit/grid",    ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_subtit },
    { "esfilter/subtit/create",  ACCESS_ADMIN,     api_esfilter_create_subtit, NULL },

    { "esfilter/ca/class",       ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_ca },
    { "esfilter/ca/grid",        ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_ca },
    { "esfilter/ca/create",      ACCESS_ADMIN,     api_esfilter_create_ca, NULL },

    { "esfilter/other/class",    ACCESS_ANONYMOUS, api_idnode_class, (void*)&esfilter_class_other },
    { "esfilter/other/grid",     ACCESS_ANONYMOUS, api_idnode_grid,  api_esfilter_grid_other },
    { "esfilter/other/create",   ACCESS_ADMIN,     api_esfilter_create_other, NULL },

    { NULL },
  };

  api_register_all(ah);
}
