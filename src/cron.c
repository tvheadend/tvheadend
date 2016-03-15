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

#include "build.h"
#include "cron.h"
#include "tvheadend.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(PLATFORM_FREEBSD)
#include <alloca.h>
#endif

/*
 * Parse value
 */
static int
cron_parse_val ( const char *str, const char **key, int *v )
{
  int i = 0;
  if (!str)
    return 0;
  if (key) {
    while (key[i]) {
      if (!strncasecmp(str, key[i], strlen(key[i]))) {
        *v = i;
        return 0;
      }
      i++;
    }
  }

  return sscanf(str, "%d", v) == 1 ? 0 : 1;
}

/*
 * Parse individual field in cron spec
 */
static int
cron_parse_field 
  ( const char **istr, uint64_t *field, uint64_t mask, int bits, int off,
    const char **key )
{ 
  int sn = -1, en = -1, mn = -1;
  const char *str = *istr;
  const char *beg = str;
  uint64_t    val = 0;
  while ( 1 ) {
    if ( *str == '*' ) {
      sn     = off;
      en     = bits + off - 1;
      beg    = NULL;
    } else if ( *str == ',' || *str == ' ' || *str == '\0' ) {
      if (beg)
        if (cron_parse_val(beg, key, en == -1 ? (sn == -1 ? &sn : &en) : &mn))
          return 1;
      if ((sn - off) >= bits || (en - off) >= bits || mn > bits)
        return 1;
      if (en < 0) en = sn;
      if (mn <= 0) mn = 1;
      while (sn <= en) {
        if ( (sn % mn) == 0 )
          val |= (0x1ULL << (sn - off));
        sn++;
      }
      if (*str != ',') break;
      sn = en = mn = -1;
      beg = (str + 1);
    } else if ( *str == '/' ) {
      if (beg)
        if (en == -1 || cron_parse_val(beg, key, sn == -1 ? &sn : &en))
          return 1;
      beg = (str + 1);
    } else if ( *str == '-' ) {
      if (sn != -1 || cron_parse_val(beg, key, &sn))  
        return 1;
      beg = (str + 1);
    }
    str++;
  }
  if (*str == ' ') str++;
  *istr   = str;
  *field  = (val | ((val >> bits) & 0x1)) & mask;
  return 0;
}

/*
 * Set value
 */
int
cron_set ( cron_t *c, const char *str )
{
  uint64_t ho, mi, mo, dm, dw;
  static const char *days[] = {
    "sun", "mon", "tue", "wed", "thu", "fri", "sat"
  };
  static const char *months[] = {
    "ignore",
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec"
  };

  /* Daily (01:01) */
  if ( !strcmp(str, "@daily") ) {
    c->c_min  = 1;
    c->c_hour = 1;
    c->c_mday = CRON_MDAY_MASK;
    c->c_mon  = CRON_MON_MASK;
    c->c_wday = CRON_WDAY_MASK;

  /* Hourly (XX:02) */
  } else if ( !strcmp(str, "@hourly") ) {
    c->c_min  = 2;
    c->c_hour = CRON_HOUR_MASK;
    c->c_mday = CRON_MDAY_MASK;
    c->c_mon  = CRON_MON_MASK;
    c->c_wday = CRON_WDAY_MASK;
  
  /* Standard */
  } else {
    if (cron_parse_field(&str, &mi, CRON_MIN_MASK,  60, 0, NULL)   || !mi)
      return 1;
    if (cron_parse_field(&str, &ho, CRON_HOUR_MASK, 24, 0, NULL)   || !ho)
      return 1;
    if (cron_parse_field(&str, &dm, CRON_MDAY_MASK, 31, 1, NULL)   || !dm)
      return 1;
    if (cron_parse_field(&str, &mo, CRON_MON_MASK,  12, 1, months) || !mo)
      return 1;
    if (cron_parse_field(&str, &dw, CRON_WDAY_MASK, 7,  0, days)   || !dw)
      return 1;
    c->c_min  = mi;
    c->c_hour = ho;
    c->c_mday = dm;
    c->c_mon  = mo;
    c->c_wday = dw;
  }

  return 0;
}

/*
 * Set value
 */
cron_multi_t *
cron_multi_set ( const char *str )
{
  char *s = str ? alloca(strlen(str) + 1) : NULL;
  char *line, *sptr = NULL;
  cron_t cron;
  cron_multi_t *cm = NULL, *cm2;
  int count = 0;

  if (s == NULL)
    return NULL;
  strcpy(s, str);
  line = strtok_r(s, "\n", &sptr);
  while (line) {
    while (*line && *line <= ' ')
      line++;
    if (line[0] != '#')
      if (!cron_set(&cron, line)) {
        count++;
        cm2 = realloc(cm, sizeof(*cm) + sizeof(cron) * count);
        if (cm2 == NULL) {
          free(cm);
          return NULL;
        }
        cm = cm2;
        cm->cm_crons[count - 1] = cron;
      }
    line = strtok_r(NULL, "\n", &sptr);
  }
  if (count)
    cm->cm_count = count;
  return cm;
}

/*
 * Check for leap year
 */
static int
is_leep_year ( int year )
{
  if (!(year % 400))
    return 1;
  if (!(year % 100))
    return 0;
  return (year % 4) ? 0 : 1;
}

/*
 * Check for days in month
 */
static int
days_in_month ( int year, int mon )
{
  int d;
  if (mon == 2)
    d = 28 + is_leep_year(year);
  else
    d = 30 + ((0x15AA >> mon) & 0x1);
  return d;
}

/*
 * Find the next time (starting from now) that the cron should fire
 */
int
cron_next ( cron_t *c, const time_t now, time_t *ret )
{
  struct tm nxt, tmp;
  int endyear, loops = 1000;
  localtime_r(&now, &nxt);
  endyear = nxt.tm_year + 10;

  /* Clear seconds */
  nxt.tm_sec = 0;

  /* Invalid day */
  if (!(c->c_mday & (0x1LL << (nxt.tm_mday-1))) ||
      !(c->c_wday & (0x1LL << (nxt.tm_wday)))   ||
      !(c->c_mon & (0x1LL << (nxt.tm_mon))) ) {
    nxt.tm_min  = 0;
    nxt.tm_hour = 0;

  /* Invalid hour */
  } else if (!(c->c_hour & (0x1LL << nxt.tm_hour))) {
    nxt.tm_min  = 0;

  /* Increment */
  } else {
    ++nxt.tm_min;
  }

  /* Minute */
  while (!(c->c_min & (0x1LL << nxt.tm_min))) {
    if (nxt.tm_min == 60) {
      ++nxt.tm_hour;
      nxt.tm_min = 0;
    } else
      nxt.tm_min++;
  }

  /* Hour */
  while (!(c->c_hour & (0x1LL << nxt.tm_hour))) {
    if (nxt.tm_hour == 24) {
      ++nxt.tm_mday;
      ++nxt.tm_wday;
      nxt.tm_hour = 0;
    } else
      ++nxt.tm_hour;
  }

  /* Date */
  if (nxt.tm_wday == 7)
    nxt.tm_wday = 0;
  if (nxt.tm_mday > days_in_month(nxt.tm_year+1900, nxt.tm_mon+1)) {
    nxt.tm_mday = 1;
    nxt.tm_mon++;
    if (nxt.tm_mon == 12) {
      nxt.tm_mon = 0;
      ++nxt.tm_year;
    }
  }
  while (!(c->c_mday & (0x1LL << (nxt.tm_mday-1))) ||
         !(c->c_wday & (0x1LL << (nxt.tm_wday)))   ||
         !(c->c_mon & (0x1LL << (nxt.tm_mon))) ) {

    /* Endless loop protection */
    if (loops-- == 0)
      return -1;

    /* Stop possible infinite loop on invalid request */
    if (nxt.tm_year >= endyear)
      return -1;

    /* Increment day of week */
    if (++nxt.tm_wday == 7)
      nxt.tm_wday = 0;

    /* Increment day */
    if (++nxt.tm_mday > days_in_month(nxt.tm_year+1900, nxt.tm_mon+1)) {
      nxt.tm_mday = 1;
      if (++nxt.tm_mon == 12) {
        nxt.tm_mon = 0;
        ++nxt.tm_year;
      }
    }

    /* Shortcut the month */
    while (!(c->c_mon & (0x1LL << nxt.tm_mon))) {
      nxt.tm_wday
        += 1 + (days_in_month(nxt.tm_year+1900, nxt.tm_mon+1) - nxt.tm_mday);
      nxt.tm_mday = 1;
      if (++nxt.tm_mon >= 12) {
        nxt.tm_mon = 0;
        ++nxt.tm_year;
      }
    }
    nxt.tm_wday %= 7;
  }

  /* Create time */
  memcpy(&tmp, &nxt, sizeof(tmp));
  mktime(&tmp);
  nxt.tm_isdst = tmp.tm_isdst;
  *ret         = mktime(&nxt);
  if (*ret <= now)
    *ret = mktime(&tmp);
  if (*ret <= now) {
#ifndef CRON_TEST
    tvherror("cron", "invalid time, now %"PRItime_t", result %"PRItime_t, now, *ret);
#else
    printf("ERROR: invalid time, now %"PRItime_t", result %"PRItime_t"\n", now, *ret);
#endif
    *ret = now + 600;
  }
  return 0;
}

/*
 * Find the next time (starting from now) that the cron should fire
 */
int
cron_multi_next ( cron_multi_t *cm, const time_t now, time_t *ret )
{
  uint32_t i;
  time_t r = (time_t)-1, t;

  if (cm == NULL)
    return -1;
  for (i = 0; i < cm->cm_count; i++)
    if (!cron_next(&cm->cm_crons[i], now, &t))
      if (r == (time_t)-1 || t < r)
        r = t;
  if (r == (time_t)-1)
    return -1;
  *ret = r;
  return 0;
}

/*
 * Testing
 *
 *   gcc -g -DCRON_TEST -I./build.linux src/cron.c
 */
#ifdef CRON_TEST
static
void print_bits ( uint64_t b, int n )
{
  while (n) {
    printf("%d", (int)(b & 0x1));
    b >>= 1;
    n--;
  }
}

int
main ( int argc, char **argv )
{
  cron_t c;
  time_t n;
  struct tm tm;
  char buf[128];

  if (argc < 2) {
    printf("Specify: CRON [NOW]\n");
    return 1;
  }
  if (argc > 2)
    n = atol(argv[2]);
  else
    time(&n);
  if (cron_set(&c, argv[1]))
    printf("INVALID CRON: %s\n", argv[1]);
  else {
    printf("min  = "); print_bits(c.c_min,  60); printf("\n");
    printf("hour = "); print_bits(c.c_hour, 24); printf("\n");
    printf("mday = "); print_bits(c.c_mday, 31); printf("\n");
    printf("mon  = "); print_bits(c.c_mon,  12); printf("\n");
    printf("wday = "); print_bits(c.c_wday,  7); printf("\n");

    localtime_r(&n, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    printf("NOW: %ld - %s (DST %d) (ZONE %s)\n", (long)n, buf, tm.tm_isdst, tm.tm_zone);

    if (cron_next(&c, n, &n)) {
      printf("FAILED to find NEXT\n");
      return 1;
    }
    localtime_r(&n, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    printf("NXT: %ld - %s (DST %d) (ZONE %s)\n", (long)n, buf, tm.tm_isdst, tm.tm_zone);
    
  }
  return 0;
}
#endif

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
