/*
 *  Tvheadend - File/Directory monitoring
 *
 *  Copyright (C) 2014 Adam Sutton
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

#ifndef __FS_MONITOR_H__
#define __FS_MONITOR_H__

typedef struct fsmonitor      fsmonitor_t;
typedef struct fsmonitor_path fsmonitor_path_t;
typedef struct fsmonitor_link fsmonitor_link_t;

typedef struct fsmonitor_path
{
  char                        *fmp_path;
  int                          fmp_fd;
  RB_ENTRY(fsmonitor_path)     fmp_link;
  LIST_HEAD(,fsmonitor_link)   fmp_monitors;
} fsmonitor_path_t;

typedef struct fsmonitor_link
{
  LIST_ENTRY(fsmonitor_link)   fml_plink;
  LIST_ENTRY(fsmonitor_link)   fml_mlink;
  fsmonitor_path_t            *fml_path;
  fsmonitor_t                 *fml_monitor;
} fsmonitor_link_t;

typedef struct fsmonitor {
  LIST_HEAD(,fsmonitor_link) fsm_paths;
  void (*fsm_create) ( struct fsmonitor *fsm, const char *path );
  void (*fsm_delete) ( struct fsmonitor *fsm, const char *path );
} fsmonitor_t;

void fsmonitor_init ( void );
int  fsmonitor_add  ( const char *path, fsmonitor_t *fsm );
void fsmonitor_del  ( const char *path, fsmonitor_t *fsm );

#endif /* __FS_MONITOR_H__ */
