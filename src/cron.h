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

typedef struct cron cron_t;

cron_t     *cron_create        ( void );
void        cron_destroy       ( cron_t *cron );
const char *cron_get_string    ( cron_t *cron );
int         cron_set_string    ( cron_t *cron, const char *str );
int         cron_is_time       ( cron_t *cron );
time_t      cron_next          ( cron_t *cron );
void        cron_serialize     ( cron_t *cron, htsmsg_t *msg );
cron_t     *cron_deserialize   ( htsmsg_t *msg );

#endif /* __CRON_H__ */
