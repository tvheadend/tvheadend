#ifndef __CRON_H__
#define __CRON_H__

#include <stdint.h>
#include <sys/types.h>
#include "htsmsg.h"

#define CRON_MIN_MASK  0x0FFFFFFFFFFFFFFFLL // 60 bits
#define CRON_HOUR_MASK 0x00FFFFFF           // 24 bits
#define CRON_DOM_MASK  0x7FFFFFFF           // 31 bits
#define CRON_MON_MASK  0x0FFF               // 12 bits
#define CRON_DOW_MASK  0xFF                 //  8 bits (allow 0/7 for sunday)

typedef struct cron
{
  uint64_t min;  ///< Minutes
  uint32_t hour; ///< Hours
  uint32_t dom;  ///< Day of month
  uint16_t mon;  ///< Month
  uint8_t  dow;  ///< Day of week
} cron_t;

void cron_to_string   ( cron_t *cron, char *str );
void cron_from_string ( cron_t *cron, const char *str );
int  cron_is_time     ( cron_t *cron );
void cron_pack        ( cron_t *cron, htsmsg_t *msg );
void cron_unpack      ( cron_t *cron, htsmsg_t *msg );

#endif /* __CRON_H__ */
