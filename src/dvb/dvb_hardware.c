/*
 *  Generic DVB hardware stuff
 *  Copyright (C) 2013 Andreas Öman
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
#include "dvb.h"


struct dvb_hardware_queue dvb_adapters;


/**
 *
 */
idnode_t **
dvb_hardware_get_childs(struct idnode *self)
{
  dvb_hardware_t *dh = (dvb_hardware_t *)self;
  dvb_hardware_t *c;
  int cnt = 1;

  TAILQ_FOREACH(c, &dh->dh_childs, dh_parent_link)
    cnt++;

  idnode_t **v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  TAILQ_FOREACH(c, &dh->dh_childs, dh_parent_link)
    v[cnt++] = &c->dh_id;
  //  qsort(v, cnt, sizeof(idnode_t *), svcsortcmp);
  v[cnt] = NULL;
  return v;

}


/**
 *
 */
const char *
dvb_hardware_get_title(struct idnode *self)
{
  dvb_hardware_t *dh = (dvb_hardware_t *)self;
  return dh->dh_name;
}


/**
 *
 */
void *
dvb_hardware_create(const idclass_t *class, size_t size,
                    dvb_hardware_t *parent, const char *uuid,
                    const char *name)
{
  dvb_hardware_t *dh = calloc(1, size);
  idnode_insert(&dh->dh_id, uuid, class);

  TAILQ_INIT(&dh->dh_childs);

  if(parent == NULL) {
    TAILQ_INSERT_TAIL(&dvb_adapters, dh, dh_parent_link);
  } else {
    TAILQ_INSERT_TAIL(&parent->dh_childs, dh, dh_parent_link);
  }
  dh->dh_name = strdup(name);
  return dh;
}
