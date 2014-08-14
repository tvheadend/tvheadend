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

#include "htsmsg.h"

typedef struct access_ipmask {
  TAILQ_ENTRY(access_ipmask) ai_link;

  int ai_ipv6;

  struct in_addr ai_ip;
  struct in6_addr ai_ip6;

  int ai_prefixlen;

  uint32_t ai_network;
  uint32_t ai_netmask;
} access_ipmask_t;

TAILQ_HEAD(access_entry_queue, access_entry);

extern struct access_entry_queue access_entries;

typedef struct access_entry {
  char *ae_id;

  TAILQ_ENTRY(access_entry) ae_link;
  char *ae_username;
  char *ae_password;
  char *ae_comment;
  int ae_enabled;
  int ae_tagonly;
  uint32_t ae_chmin;
  uint32_t ae_chmax;
  char *ae_chtag;

  uint32_t ae_rights;

  TAILQ_HEAD(, access_ipmask) ae_ipmasks;
} access_entry_t;

TAILQ_HEAD(access_ticket_queue, access_ticket);

extern struct access_ticket_queue access_tickets;

typedef struct access_ticket {
  char *at_id;

  TAILQ_ENTRY(access_ticket) at_link;

  gtimer_t at_timer;
  char *at_resource;
} access_ticket_t;

typedef struct access {
  uint32_t  aa_rights;
  uint32_t  aa_chmin;
  uint32_t  aa_chmax;
  htsmsg_t *aa_chtags;
  int       aa_match;
} access_t;

#define ACCESS_ANONYMOUS          0
#define ACCESS_STREAMING          (1<<0)
#define ACCESS_ADVANCED_STREAMING (1<<1)
#define ACCESS_WEB_INTERFACE      (1<<2)
#define ACCESS_RECORDER           (1<<3)
#define ACCESS_RECORDER_ALL       (1<<4)
#define ACCESS_TAG_ONLY           (1<<5)
#define ACCESS_ADMIN              (1<<6)

#define ACCESS_FULL \
  (ACCESS_STREAMING | ACCESS_ADVANCED_STREAMING | \
   ACCESS_WEB_INTERFACE | ACCESS_RECORDER | ACCESS_ADMIN)

/**
 * Create a new ticket for the requested resource and generate a id for it
 */
const char* access_ticket_create(const char *resource);

/**
 * Verifies that a given ticket id matches a resource
 */
int access_ticket_verify(const char *id, const char *resource);

int access_ticket_delete(const char *ticket_id);

/**
 * Free the access structure
 */
void access_destroy(access_t *a);

/**
 * Verifies that the given user in combination with the source ip
 * complies with the requested mask
 *
 * Return 0 if access is granted, -1 otherwise
 */
int access_verify(const char *username, const char *password,
		  struct sockaddr *src, uint32_t mask);

/**
 * Get the access structure
 */
access_t *access_get(const char *username, const char *password,
                     struct sockaddr *src);

/**
 *
 */
access_t *
access_get_hashed(const char *username, const uint8_t digest[20],
		  const uint8_t *challenge, struct sockaddr *src);

/**
 *
 */
access_t *
access_get_by_addr(struct sockaddr *src);

/**
 *
 */
void access_init(int createdefault, int noacl);
void access_done(void);

#endif /* ACCESS_H_ */
