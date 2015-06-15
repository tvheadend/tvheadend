/*
 *  Functions for transport probing
 *  Copyright (C) 2007 Andreas Öman
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

#ifndef __TVH_SERVICE_MAPPER_H__
#define __TVH_SERVICE_MAPPER_H__

struct bouquet;

typedef struct service_mapper_conf
{
  int check_availability; ///< Check service is receivable
  int encrypted;          ///< Include encrypted services
  int merge_same_name;    ///< Merge entries with the same name
  int provider_tags;      ///< Create tags based on provider name
} service_mapper_conf_t;

typedef struct service_mapper_status
{
  int       total;
  int       ok;
  int       fail;
  int       ignore;
  service_t *active;
} service_mapper_status_t;

void service_mapper_init   ( void );
void service_mapper_done   ( void );

// Start new mapping
void service_mapper_start  
  ( const service_mapper_conf_t *conf, htsmsg_t *uuids );

// Stop pending services (remove from Q)
void service_mapper_stop   ( void );

// Remove service (deleted?) from Q
void service_mapper_remove ( struct service *t );

// Get current Q size
service_mapper_status_t service_mapper_status ( void );

// Link service to channel
int  service_mapper_link   ( struct service *s, struct channel *c, void *origin );

// Create new link
int service_mapper_create ( idnode_t *s, idnode_t *c, void *origin );

// Process one service
struct channel *service_mapper_process ( struct service *s, struct bouquet *bq );

// Resets the stat counters
void service_mapper_reset_stats ( void );

#endif /* __TVH_SERVICE_MAPPER_H__ */
