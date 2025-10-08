/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Jaroslav Kysela
 *
 * TV headend - Bouquets
 */

#ifndef BOUQUET_H_
#define BOUQUET_H_

#include "idnode.h"
#include "htsmsg.h"
#include "service.h"
#include "channels.h"

typedef struct bouquet {
  idnode_t bq_id;
  RB_ENTRY(bouquet) bq_link;

  int           bq_saveflag;
  int           bq_in_load;

  int           bq_shield;
  int           bq_enabled;
  int           bq_rescan;
  int           bq_maptoch;
  int           bq_mapnolcn;
  int           bq_mapnoname;
  int           bq_mapradio;
  int           bq_mapencrypted;
  int           bq_mapmergename;
  int           bq_mapmergefuzzy;
  int           bq_tidychannelname;
  int           bq_chtag;
  int           bq_chtag_type_tags;
  int           bq_chtag_provider_tags;
  int           bq_chtag_network_tags;
  channel_tag_t*bq_chtag_ptr;
  const char   *bq_chtag_waiting;
  char         *bq_name;
  char         *bq_ext_url;
  int           bq_ssl_peer_verify;
  int           bq_ext_url_period;
  char         *bq_src;
  char         *bq_comment;
  idnode_set_t *bq_services;
  idnode_set_t *bq_active_services;
  htsmsg_t     *bq_services_waiting;
  uint32_t      bq_services_seen;
  uint32_t      bq_lcn_offset;

  /* fastscan bouquet helpers */
  int           bq_fastscan_nit;
  int           bq_fastscan_sdt;
  void         *bq_fastscan_bi;

  void         *bq_download;

} bouquet_t;

typedef RB_HEAD(,bouquet) bouquet_tree_t;

extern bouquet_tree_t bouquets;

extern const idclass_t bouquet_class;

/**
 *
 */

htsmsg_t * bouquet_class_get_list(void *o, const char *lang);

bouquet_t * bouquet_create(const char *uuid, htsmsg_t *conf,
                           const char *name, const char *src);

void bouquet_delete(bouquet_t *bq);

void bouquet_destroy_by_service(service_t *t, int delconf);
void bouquet_destroy_by_channel_tag(channel_tag_t *ct);

void bouquet_notify_service_enabled(service_t *t);

static inline bouquet_t *
bouquet_find_by_uuid(const char *uuid)
  { return (bouquet_t *)idnode_find(uuid, &bouquet_class, NULL); }

bouquet_t * bouquet_find_by_source(const char *name, const char *src, int create);

void bouquet_map_to_channels(bouquet_t *bq);
void bouquet_notify_channels(bouquet_t *bq);
void bouquet_add_service(bouquet_t *bq, service_t *s, uint64_t lcn, const char *tag);
void bouquet_completed(bouquet_t *bq, uint32_t seen);
void bouquet_change_comment(bouquet_t *bq, const char *comment, int replace);
void bouquet_scan(bouquet_t *bq);
void bouquet_detach(channel_t *ch);

uint64_t bouquet_get_channel_number(bouquet_t *bq, service_t *t);

/**
 *
 */

void bouquet_init(void);
void bouquet_service_resolve(void);
void bouquet_done(void);

#endif /* BOUQUET_H_ */
