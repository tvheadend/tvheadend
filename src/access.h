/*
 *  TV headend - Access control
 *  Copyright (C) 2008 Andreas Öman
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

#ifndef ACCESS_H_
#define ACCESS_H_

#include "idnode.h"
#include "htsmsg.h"

#define ACCESS_DEFAULT_COMMENT "Default access entry"

struct profile;
struct dvr_config;
struct channel_tag;

TAILQ_HEAD(access_ipmask_queue, access_ipmask);

TAILQ_HEAD(ipblock_entry_queue, ipblock_entry);

extern struct ipblock_entry_queue ipblock_entries;

typedef struct ipblock_entry {
  idnode_t ib_id;

  TAILQ_ENTRY(ipblock_entry) ib_link;

  int   ib_enabled;
  struct access_ipmask_queue ib_ipmasks;
  char *ib_comment;
} ipblock_entry_t;

extern const idclass_t ipblock_entry_class;

TAILQ_HEAD(passwd_entry_queue, passwd_entry);

extern struct passwd_entry_queue passwd_entries;

typedef struct passwd_entry {
  idnode_t pw_id;

  TAILQ_ENTRY(passwd_entry) pw_link;

  char *pw_username;
  char *pw_password;
  char *pw_password2;

  int   pw_enabled;
  int   pw_wizard;

  char *pw_comment;
} passwd_entry_t;

extern const idclass_t passwd_entry_class;

typedef struct access_ipmask {
  TAILQ_ENTRY(access_ipmask) ai_link;

  int ai_family;

  struct in6_addr ai_ip6;

  int ai_prefixlen;

  uint32_t ai_netmask;
  uint32_t ai_network;
} access_ipmask_t;

TAILQ_HEAD(access_entry_queue, access_entry);

extern struct access_entry_queue access_entries;

enum {
  ACCESS_CONN_LIMIT_TYPE_ALL = 0,
  ACCESS_CONN_LIMIT_TYPE_STREAMING,
  ACCESS_CONN_LIMIT_TYPE_DVR,
};

typedef struct access_entry {
  idnode_t ae_id;

  TAILQ_ENTRY(access_entry) ae_link;
  char *ae_username;
  char *ae_comment;
  char *ae_lang;
  char *ae_lang_ui;
  char *ae_theme;

  int ae_index;
  int ae_wizard;
  int ae_enabled;
  int ae_uilevel;
  int ae_uilevel_nochange;

  int ae_streaming;
  int ae_adv_streaming;
  int ae_htsp_streaming;

  idnode_list_head_t ae_profiles;

  int ae_conn_limit_type;
  uint32_t ae_conn_limit;

  int ae_dvr;
  int ae_htsp_dvr;
  int ae_all_dvr;
  int ae_all_rw_dvr;
  int ae_failed_dvr;

  int ae_htsp_anonymize;

  idnode_list_head_t ae_dvr_configs;

  int ae_webui;
  int ae_admin;

  uint64_t ae_chmin;
  uint64_t ae_chmax;

  int ae_chtags_exclude;
  idnode_list_head_t ae_chtags;

  uint32_t ae_rights;

  struct access_ipmask_queue ae_ipmasks;
} access_entry_t;

extern const idclass_t access_entry_class;

typedef struct access {
  char     *aa_username;
  char     *aa_representative;
  char     *aa_lang;
  char     *aa_lang_ui;
  uint32_t  aa_rights;
  htsmsg_t *aa_profiles;
  htsmsg_t *aa_dvrcfgs;
  uint64_t *aa_chrange;
  int       aa_chrange_count;
  htsmsg_t *aa_chtags;
  int       aa_match;
  uint32_t  aa_conn_limit;
  uint32_t  aa_conn_limit_streaming;
  uint32_t  aa_conn_limit_dvr;
  uint32_t  aa_conn_streaming;
  uint32_t  aa_conn_dvr;
  int       aa_uilevel;
  int       aa_uilevel_nochange;
  char     *aa_theme;
} access_t;

TAILQ_HEAD(access_ticket_queue, access_ticket);

extern struct access_ticket_queue access_tickets;

typedef struct access_ticket {
  char *at_id;

  TAILQ_ENTRY(access_ticket) at_link;

  mtimer_t at_timer;
  char *at_resource;
  access_t *at_access;
} access_ticket_t;

#define ACCESS_ANONYMOUS          0
#define ACCESS_STREAMING          (1<<0)
#define ACCESS_ADVANCED_STREAMING (1<<1)
#define ACCESS_HTSP_STREAMING     (1<<2)
#define ACCESS_WEB_INTERFACE      (1<<3)
#define ACCESS_HTSP_INTERFACE     (1<<4)
#define ACCESS_RECORDER           (1<<5)
#define ACCESS_HTSP_RECORDER      (1<<6)
#define ACCESS_ALL_RECORDER       (1<<7)
#define ACCESS_ALL_RW_RECORDER    (1<<8)
#define ACCESS_FAILED_RECORDER    (1<<9)
#define ACCESS_HTSP_ANONYMIZE     (1<<10)
#define ACCESS_ADMIN              (1<<11)
#define ACCESS_OR                 (1<<30)

#define ACCESS_FULL \
  (ACCESS_STREAMING | ACCESS_ADVANCED_STREAMING | \
   ACCESS_HTSP_STREAMING | ACCESS_WEB_INTERFACE | \
   ACCESS_RECORDER | ACCESS_HTSP_RECORDER | \
   ACCESS_ALL_RECORDER | ACCESS_ALL_RW_RECORDER | \
   ACCESS_FAILED_RECORDER | ACCESS_ADMIN)

/**
 * Create a new ticket for the requested resource and generate a id for it
 */
const char* access_ticket_create(const char *resource, access_t *a);

/**
 * Verifies that a given ticket id matches a resource
 */
access_t *access_ticket_verify2(const char *id, const char *resource);

int access_ticket_delete(const char *ticket_id);

/**
 * Free the access structure
 */
void access_destroy(access_t *a);

/**
 * Copy the access structure
 */
access_t *access_copy(access_t *src);

/**
 *
 */
char *
access_get_lang(access_t *a, const char *lang);

/**
 *
 */
const char *
access_get_theme(access_t *a);

/**
 * Verifies that the given user in combination with the source ip
 * complies with the requested mask
 *
 * Return 0 if access is granted, -1 otherwise
 */
static inline int access_verify2(access_t *a, uint32_t mask)
  { return (mask & ACCESS_OR) ?
      ((a->aa_rights & mask) ? 0 : -1) :
      ((a->aa_rights & mask) == mask ? 0 : -1); }

int access_verify_list(htsmsg_t *list, const char *item);

/**
 * Get the access structure
 */
typedef int (*verify_callback_t)(void *aux, const char *passwd);

access_t *access_get(struct sockaddr *src, const char *username,
                     verify_callback_t verify, void *aux);

/**
 *
 */
access_t *
access_get_by_username(const char *username);

/**
 *
 */
access_t *
access_get_by_addr(struct sockaddr *src);

/**
 *
 */
access_entry_t *
access_entry_create(const char *uuid, htsmsg_t *conf);

/**
 *
 */
void
access_entry_destroy(access_entry_t *ae, int delconf);

/**
 *
 */
void
access_destroy_by_profile(struct profile *pro, int delconf);
void
access_destroy_by_dvr_config(struct dvr_config *cfg, int delconf);
void
access_destroy_by_channel_tag(struct channel_tag *ct, int delconf);

/**
 *
 */
passwd_entry_t *
passwd_entry_create(const char *uuid, htsmsg_t *conf);
void
passwd_entry_destroy(passwd_entry_t *ae, int delconf);

/**
 *
 */
ipblock_entry_t *
ipblock_entry_create(const char *uuid, htsmsg_t *conf);

/**
 *
 */
void access_init(int createdefault, int noacl);
void access_done(void);

/**
 *
 */
htsmsg_t *language_get_list ( void *obj, const char *lang );
htsmsg_t *language_get_ui_list ( void *obj, const char *lang );
htsmsg_t *theme_get_ui_list ( void *obj, const char *lang );
htsmsg_t *user_get_userlist ( void *obj, const char *lang );

#endif /* ACCESS_H_ */
