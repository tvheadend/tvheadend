#include "cron.h"
#include <time.h>
#include <stdio.h>

static int _field_to_string 
  ( char *out, uint64_t field, uint64_t mask, int offset )
{
  int i, cnt = 0, first = 1, start = -1;
  field &= mask;

  /* Star */
  if ( field == mask )
    return sprintf(out, "*");

  for ( i = 0; i < 64; i++ ) {
    int b = (field >> i) & 0x1;
    if ( !b & start != -1 ) {
      if (!first)
        cnt += sprintf(out+cnt, ",");
      first = 0;
      cnt += sprintf(out+cnt, "%d", start + offset);
      if (i != (start+1))
        cnt += sprintf(out+cnt, "-%d", (i - 1) + offset);
      start = -1;
    } else if ( b & start == -1 )
      start = i;
  }
  
  return cnt;
}

static int _field_from_string 
  ( const char *str, uint64_t *field, uint64_t mask, int offset )
{ 
  int i = 0, j = 0, sn = -1, en = -1;
  *field = 0;
  while ( str[i] != '\0' ) {
    if ( str[i] == '*' ) *field = mask;
    else if ( str[i] == ',' || str[i] == ' ' ) {
      sscanf(str+j, "%d", sn == -1 ? &sn : &en);
      if (en == -1) en = sn;
      while (sn <= en) {
        *field |= (0x1LL << (sn - offset));
        sn++;
      }
      j  = i+1;
      sn = -1;
      en = -1;
      if (str[i] == ' ') break;
    } else if ( str[i] == '-' ) {
      sscanf(str+j, "%d", &sn);
      j  = i+1;
      en = 64;
    }
    i++;
  }
  if (str[i] == ' ') i++;
  *field &= mask;
  return i;
}

void cron_to_string ( cron_t *cron, char *str )
{
  int i = 0;
  i += _field_to_string(str+i, cron->min,  CRON_MIN_MASK, 0);
  i += sprintf(str+i, " ");
  i += _field_to_string(str+i, cron->hour, CRON_HOUR_MASK, 0);
  i += sprintf(str+i, " ");
  i += _field_to_string(str+i, cron->dom,  CRON_DOM_MASK, 1);
  i += sprintf(str+i, " ");
  i += _field_to_string(str+i, cron->mon,  CRON_MON_MASK, 1);
  i += sprintf(str+i, " ");
  i += _field_to_string(str+i, cron->dow,  CRON_DOW_MASK, 0);
}

void cron_from_string ( cron_t *cron, const char *str )
{
  int i = 0;
  uint64_t u64;
  i += _field_from_string(str+i, &u64, CRON_MIN_MASK, 0);
  cron->min = u64;
  i += _field_from_string(str+i, &u64, CRON_HOUR_MASK, 0);
  cron->hour = (uint32_t)u64;
  i += _field_from_string(str+i, &u64, CRON_DOM_MASK, 1);
  cron->dom = (uint32_t)u64;
  i += _field_from_string(str+i, &u64, CRON_MON_MASK, 1);
  cron->mon = (uint16_t)u64;
  i += _field_from_string(str+i, &u64, CRON_DOW_MASK, 0);
  cron->dow = (uint8_t)u64;
}

int cron_is_time ( cron_t *cron )
{
  int      ret = 1;
  uint64_t b   = 0x1;

  /* Get the current time */
  time_t t = time(NULL);
  struct tm now;
  localtime_r(&t, &now);
  
  /* Check */
  if ( ret && !((b << now.tm_min)  & cron->min)  ) ret = 0;
  if ( ret && !((b << now.tm_hour) & cron->hour) ) ret = 0;
  if ( ret && !((b << now.tm_mday) & cron->dom)  ) ret = 0;
  if ( ret && !((b << now.tm_mon)  & cron->mon)  ) ret = 0;
  if ( ret && !((b << now.tm_wday) & cron->dow)  ) ret = 0;

  return ret;
}

void cron_pack ( cron_t *cron, htsmsg_t *msg )
{
  htsmsg_add_u64(msg, "min",  cron->min);
  htsmsg_add_u32(msg, "hour", cron->hour);
  htsmsg_add_u32(msg, "dom",  cron->dom);
  htsmsg_add_u32(msg, "mon",  cron->mon);
  htsmsg_add_u32(msg, "dow",  cron->dow);
}

void cron_unpack ( cron_t *cron, htsmsg_t *msg )
{
  uint32_t u32 = 0;
  htsmsg_get_u64(msg, "min", &cron->min);
  htsmsg_get_u32(msg, "hour", &cron->hour);
  htsmsg_get_u32(msg, "dom",  &cron->dom);
  htsmsg_get_u32(msg, "mon",  &u32);
  cron->mon = (uint16_t)u32;
  htsmsg_get_u32(msg, "dow",  &u32);
  cron->dow = (uint8_t)u32;
}
