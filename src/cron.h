/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2014 Adam Sutton
 *
 * Tvheadend - cron routines
 */

#ifndef __TVH_CRON_H__
#define __TVH_CRON_H__

#include <stdint.h>
#include <sys/types.h>

#define CRON_MIN_MASK   (0x0FFFFFFFFFFFFFFFLL) // 60 bits
#define CRON_HOUR_MASK  (0x00FFFFFF)           // 24 bits
#define CRON_MDAY_MASK  (0x7FFFFFFF)           // 31 bits
#define CRON_MON_MASK   (0x0FFF)               // 12 bits
#define CRON_WDAY_MASK  (0x7F)                 //  7 bits

typedef struct cron
{
  uint64_t c_min;     ///< Minute mask
  uint32_t c_hour;    ///< Hour mask
  uint32_t c_mday;    ///< Day of the Month mask
  uint16_t c_mon;     ///< Month mask
  uint8_t  c_wday;    ///< Day of the Week mask
} cron_t;

typedef struct cron_multi
{
  uint32_t cm_count;  ///< Count of multiple crons
  cron_t   cm_crons[0]; ///< Allocated cron structures
} cron_multi_t;

/**
 * Initialise from a string
 *
 * @param c   The cron instance to update
 * @param str String representation of the cron
 *
 * @return 0 if OK, 1 if failed to parse
 */
int cron_set ( cron_t *c, const char *str );

/**
 * Determine the next time a cron will run (from cur)
 *
 * @param c   The cron to check
 * @param now The current time
 * @param nxt The next time to execute
 *
 * @return 0 if next time was found
 */
int cron_next ( cron_t *c, const time_t cur, time_t *nxt );

/**
 * Initialise from a string
 *
 * @param str String representation of the mutiple cron entries ('\n' delimiter)
 *
 * @return cron_multi_t pointer if OK, NULL if failed to parse
 */
cron_multi_t *cron_multi_set ( const char *str );

/**
 * Determine the next time a cron will run (from cur)
 *
 * @param c   The cron to check
 * @param now The current time
 * @param nxt The next time to execute
 *
 * @return 0 if next time was found
 */
int cron_multi_next ( cron_multi_t *cm, const time_t cur, time_t *nxt );

#endif /* __TVH_CRON_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
