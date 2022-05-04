/*
 *  tvheadend, Wizard
 *  Copyright (C) 2015 Jaroslav Kysela
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

#ifndef __TVH_WIZARD_H__
#define __TVH_WIZARD_H__

#include "tvheadend.h"
#include "idnode.h"

typedef struct wizard_page {
  idnode_t idnode;
  const char *name;
  char *desc;
  void (*free)(struct wizard_page *);
  void *aux;
} wizard_page_t;

typedef wizard_page_t *(*wizard_build_fcn_t)(const char *lang);

wizard_page_t *wizard_hello(const char *lang);
wizard_page_t *wizard_login(const char *lang);
wizard_page_t *wizard_network(const char *lang);
wizard_page_t *wizard_muxes(const char *lang);
wizard_page_t *wizard_status(const char *lang);
wizard_page_t *wizard_mapping(const char *lang);
wizard_page_t *wizard_channels(const char *lang);

#endif /* __TVH_WIZARD_H__ */
