/*
 *  Tvheadend - cron routines
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

#endif /* __TVH_CRON_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
