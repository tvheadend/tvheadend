/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2012 Adam Sutton
 *
 * TV headend - General configuration settings
 */

// TODO: expand this, possibly integrate command line

#ifndef __TVH_CONFIG__H__
#define __TVH_CONFIG__H__

#include <unistd.h>

#include "build.h"
#include "htsmsg.h"
#include "idnode.h"

typedef struct config {
  idnode_t idnode;
  uint32_t version;
  char *confdir;
  int hbbtv;
  int uilevel;
  int uilevel_nochange;
  int ui_quicktips;
  int http_auth;
  int http_auth_algo;
  int proxy;
  char *realm;
  char *wizard;
  char *full_version;
  char *server_name;
  char *http_server_name;
  char *http_user_agent;
  char *language;
  char *info_area;
  int chname_num;
  int chname_src;
  char *language_ui;
  char *theme_ui;
  char *muxconf_path;
  int prefer_picon;
  char *chicon_path;
  int chicon_scheme;
  char *picon_path;
  int picon_scheme;
  int tvhtime_update_enabled;
  int tvhtime_ntp_enabled;
  uint32_t tvhtime_tolerance;
  char *cors_origin;
  uint32_t cookie_expires;
  int dscp;
  uint32_t descrambler_buffer;
  int caclient_ui;
  int parser_backlog;
  int epg_compress;
  uint32_t epg_cut_window;
  uint32_t epg_update_window;
  int iptv_tpool_count;
  char *date_mask;
  int label_formatting;
  uint32_t ticket_expires;
  char *hdhomerun_ip;
  char *local_ip;
  int local_port;
  int rtsp_udp_min_port;
  int rtsp_udp_max_port;
  uint32_t hdhomerun_server_tuner_count;
  char *hdhomerun_server_model_name;
  int hdhomerun_server_enable;
#if ENABLE_VAAPI
  int enable_vainfo;
  #endif
  uint32_t page_size_ui;
} config_t;

extern const idclass_t config_class;
extern config_t config;

void config_boot
  ( const char *path, gid_t gid, uid_t uid, const char *http_user_agent );
void config_init( int backup );
void config_done( void );

const char *config_get_server_name ( void );
const char *config_get_http_server_name ( void );
const char *config_get_language    ( void );
const char *config_get_language_ui ( void );

#endif /* __TVH_CONFIG__H__ */
