#ifndef __CRON_H__
#define __CRON_H__

#include <stdint.h>
#include <sys/types.h>
#include "htsmsg.h"

typedef struct cron
{
  uint64_t min;  ///< Minutes
  uint32_t hour; ///< Hours
  uint32_t dom;  ///< Day of month
  uint16_t mon;  ///< Month
  uint8_t  dow;  ///< Day of week
} cron_t;

void cron_set_string ( cron_t *cron, const char *str );
int  cron_is_time    ( cron_t *cron );
void cron_pack       ( cron_t *cron, htsmsg_t *msg );
void cron_unpack     ( cron_t *cron, htsmsg_t *msg );

#endif /* __CRON_H__ */
