/*
 *  API - elementary stream filter related calls
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
#include "esfilter.h"
#include "access.h"
#include "api.h"

static void
api_esfilter_grid
  ( access_t *perm, idnode_set_t *ins, api_idnode_grid_conf_t *conf, htsmsg_t *args,
    esfilter_class_t cls )
{
  esfilter_t *esf;

  TAILQ_FOREACH(esf, &esfilters[cls], esf_link) {
    idnode_set_add(ins, (idnode_t*)esf, &conf->filter, perm->aa_lang);
  }
}

static int
api_esfilter_create
  ( access_t *perm, void *opaque, const char *op, htsmsg_t *args, htsmsg_t **resp,
    esfilter_class_t cls )
{
  htsmsg_t *conf;

  if (!(conf  = htsmsg_get_map(args, "conf")))
    return EINVAL;

  pthread_mutex_lock(&global_lock);
  esfilter_create(cls, NULL, conf, 1);
  pthread_mutex_unlock(&global_lock);

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
