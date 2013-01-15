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


TAILQ_HEAD(access_entry_queue, access_entry);

extern struct access_entry_queue access_entries;

typedef struct access_entry {
  char *ae_id;

  TAILQ_ENTRY(access_entry) ae_link;
  char *ae_username;
  char *ae_password;
  char *ae_comment;
  int ae_ipv6;
  struct in_addr ae_ip;
  struct in6_addr ae_ip6;
  int ae_prefixlen;
  int ae_enabled;
  

  uint32_t ae_rights;

  uint32_t ae_network; /* derived from ae_ip */
  uint32_t ae_netmask; /* derived from ae_prefixlen */
} access_entry_t;

TAILQ_HEAD(access_ticket_queue, access_ticket);

extern struct access_ticket_queue access_tickets;

typedef struct access_ticket {
  char *at_id;

  TAILQ_ENTRY(access_ticket) at_link;

  gtimer_t at_timer;
  char *at_resource;
} access_ticket_t;

#define ACCESS_ANONYMOUS       0x0
#define ACCESS_STREAMING       0x1
#define ACCESS_WEB_INTERFACE   0x2
#define ACCESS_RECORDER        0x4
#define ACCESS_RECORDER_ALL    0x8
#define ACCESS_ADMIN           0x10
#define ACCESS_FULL 0x3f

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
 * Verifies that the given user in combination with the source ip
 * complies with the requested mask
 *
 * Return 0 if access is granted, -1 otherwise
 */
int access_verify(const char *username, const char *password,
		  struct sockaddr *src, uint32_t mask);

/**
 *
 */
uint32_t access_get_hashed(const char *username, const uint8_t digest[20],
			   const uint8_t *challenge, struct sockaddr *src,
			   int *entrymatch);

/**
 *
 */
uint32_t access_get_by_addr(struct sockaddr *src);


/**
 *
 */
void access_init(int createdefault);

#endif /* ACCESS_H_ */
