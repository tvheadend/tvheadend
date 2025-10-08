/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2015 Jaroslav Kysela
 *
 * tvheadend, Wizard
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
