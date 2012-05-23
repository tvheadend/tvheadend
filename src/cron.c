#include "cron.h"
#include <time.h>

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
