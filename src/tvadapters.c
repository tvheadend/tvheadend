/*
 *  TV Adapters
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
#include "tvadapters.h"
#include "dvb/dvb.h"

/**
 *
 */
idnode_t **
tv_adapters_root(void)
{
  dvb_hardware_t *dh;
  int cnt = 1;
  TAILQ_FOREACH(dh, &dvb_adapters, dh_parent_link)
    cnt++;

  idnode_t **v = malloc(sizeof(idnode_t *) * cnt);
  cnt = 0;
  TAILQ_FOREACH(dh, &dvb_adapters, dh_parent_link)
    v[cnt++] = &dh->dh_id;
  v[cnt] = NULL;
  return v;
}
