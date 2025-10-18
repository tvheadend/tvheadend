/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Adam Sutton
 *
 * Tvheadend - File/Directory monitoring
 */

#ifndef __FS_MONITOR_H__
#define __FS_MONITOR_H__

typedef struct fsmonitor      fsmonitor_t;
typedef struct fsmonitor_path fsmonitor_path_t;
typedef struct fsmonitor_link fsmonitor_link_t;

struct fsmonitor_path
{
  char                        *fmp_path;
  int                          fmp_fd;
  RB_ENTRY(fsmonitor_path)     fmp_link;
  LIST_HEAD(,fsmonitor_link)   fmp_monitors;
};

struct fsmonitor_link
{
  LIST_ENTRY(fsmonitor_link)   fml_plink;
  LIST_ENTRY(fsmonitor_link)   fml_mlink;
  fsmonitor_path_t            *fml_path;
  fsmonitor_t                 *fml_monitor;
};

struct fsmonitor {
  LIST_HEAD(,fsmonitor_link) fsm_paths;
  void (*fsm_create) ( struct fsmonitor *fsm, const char *path );
  void (*fsm_delete) ( struct fsmonitor *fsm, const char *path );
};

void fsmonitor_init ( void );
void fsmonitor_done ( void );
int  fsmonitor_add  ( const char *path, fsmonitor_t *fsm );
void fsmonitor_del  ( const char *path, fsmonitor_t *fsm );

#endif /* __FS_MONITOR_H__ */
