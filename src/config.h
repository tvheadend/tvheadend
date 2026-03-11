/*
 *  TV headend - General configuration settings
 *  Copyright (C) 2012 Adam Sutton
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
  int auto_clear_input_counters;
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
  uint32_t default_tab;
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

#define CONFIG_DEFAULT_TAB_SYSTEM         0
#define CONFIG_DEFAULT_TAB_EPG            1
#define CONFIG_DEFAULT_TAB_DVR_UPCOMING  10
#define CONFIG_DEFAULT_TAB_DVR_FINISHED  11
#define CONFIG_DEFAULT_TAB_DVR_FAILED    12
#define CONFIG_DEFAULT_TAB_DVR_REMOVED   13
#define CONFIG_DEFAULT_TAB_DVR_AUTORECS  14
#define CONFIG_DEFAULT_TAB_DVR_TIMERS    15
#define CONFIG_DEFAULT_TAB_CFG_GENERAL   20
#define CONFIG_DEFAULT_TAB_CFG_USERS     21
#define CONFIG_DEFAULT_TAB_CFG_DVB       22
#define CONFIG_DEFAULT_TAB_CFG_CHANNEL   23
#define CONFIG_DEFAULT_TAB_CFG_STREAM    24
#define CONFIG_DEFAULT_TAB_CFG_REC       25
#define CONFIG_DEFAULT_TAB_CFG_CA        26
#define CONFIG_DEFAULT_TAB_CFG_DEBUG     27
#define CONFIG_DEFAULT_TAB_STATUS_STREAM 30
#define CONFIG_DEFAULT_TAB_STATUS_SUBS   31
#define CONFIG_DEFAULT_TAB_STATUS_CONN   32
#define CONFIG_DEFAULT_TAB_STATUS_SVC    33
#define CONFIG_DEFAULT_TAB_ABOUT         40


#endif /* __TVH_CONFIG__H__ */
