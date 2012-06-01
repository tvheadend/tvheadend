#include "cron.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cron
{
  char     *str; ///< String representation
  uint64_t min;  ///< Minutes
  uint32_t hour; ///< Hours
  uint32_t dom;  ///< Day of month
  uint16_t mon;  ///< Month
  uint8_t  dow;  ///< Day of week
} cron_t;

static int _cron_parse_field 
  ( const char *str, uint64_t *field, uint64_t mask, int offset )
{ 
  int i = 0, j = 0, sn = -1, en = -1, mn = -1;
  *field = 0;
  while ( str[i] != '\0' ) {
    if ( str[i] == '*' ) {
      sn = 0;
      en = 64;
      j  = -1;
    } else if ( str[i] == ',' || str[i] == ' ' ) {
      if (j >= 0) 
        sscanf(str+j, "%d", en == -1 ? (sn == -1 ? &sn : &en) : &mn);
      if (en == -1) en = sn;
      if (mn == -1) mn = 1;
      while (sn <= en) {
        if ( (sn % mn) == 0 )
          *field |= (0x1LL << (sn - offset));
        sn++;
      }
      j  = i+1;
      sn = en = mn = -1;
      if (str[i] == ' ') break;
    } else if ( str[i] == '/' ) {
      if (j >= 0) sscanf(str+j, "%d", sn == -1 ? &sn : &en);
      j  = i+1;
      mn = 1;
    } else if ( str[i] == '-' ) {
      if (j >= 0) sscanf(str+j, "%d", &sn);
      j  = i+1;
      en = 64;
    }
    i++;
  }
  if (str[i] == ' ') i++;
  *field &= mask;
  return i;
}

static int _cron_parse ( cron_t *cron, const char *str )
{
  int i = 0;
  uint64_t u64;

  /* daily */
  if ( !strcmp(str, "@daily") ) {
    cron->min  = 1;
    cron->hour = 1;
    cron->dom  = CRON_DOM_MASK;
    cron->mon  = CRON_MON_MASK;
    cron->dow  = CRON_DOW_MASK;

  /* hourly */
  } else if ( !strcmp(str, "@hourly") ) {
    cron->min  = 1;
    cron->hour = CRON_HOUR_MASK;
    cron->dom  = CRON_DOM_MASK;
    cron->mon  = CRON_MON_MASK;
    cron->dow  = CRON_DOW_MASK;
  
  /* standard */
  } else {
    i += _cron_parse_field(str+i, &u64, CRON_MIN_MASK, 0);
    cron->min = u64;
    i += _cron_parse_field(str+i, &u64, CRON_HOUR_MASK, 0);
    cron->hour = (uint32_t)u64;
    i += _cron_parse_field(str+i, &u64, CRON_DOM_MASK, 1);
    cron->dom = (uint32_t)u64;
    i += _cron_parse_field(str+i, &u64, CRON_MON_MASK, 1);
    cron->mon = (uint16_t)u64;
    i += _cron_parse_field(str+i, &u64, CRON_DOW_MASK, 0);
    cron->dow = (uint8_t)u64;
  }

  return 1; // TODO: do proper validation
}

cron_t *cron_create ( const char *str )
{
  cron_t *c = calloc(1, sizeof(cron_t));
  if (str) cron_set_string(c, str);
  return c;
}

void cron_destroy ( cron_t *cron )
{
  if (cron->str) free(cron->str);
  free(cron);
}

const char *cron_get_string ( cron_t *cron )
{
  return cron->str;
}

int cron_set_string ( cron_t *cron, const char *str )
{
  int save = 0;
  cron_t tmp;
  if (!cron->str || !strcmp(cron->str, str)) {
    tmp.str = (char*)str;
    if (_cron_parse(&tmp, str)) {
      if (cron->str) free(cron->str);
      *cron     = tmp;
      cron->str = strdup(str);
      save      = 1;
    }
  }
  return save;
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

// TODO: do proper search for next time
time_t cron_next ( cron_t *cron )
{
  time_t now;
  time(&now);
  now += 60;
  return (now / 60) * 60; // round to start of minute
}


void cron_serialize ( cron_t *cron, htsmsg_t *msg )
{
  htsmsg_add_str(msg, "cron", cron->str);
}

cron_t *cron_deserialize ( htsmsg_t *msg )
{
  return cron_create(htsmsg_get_str(msg, "cron"));
}

#if 0
int main ( int argc, char **argv )
{
  cron_t *c = cron_create();
  cron_set_string(c, argv[1]);
  printf("min  = 0x%016lx\n", c->min);
  printf("hour = 0x%06x\n",   c->hour);
  printf("dom  = 0x%08x\n",   c->dom);
  printf("mon  = 0x%03hx\n",  c->mon);
  printf("dow  = 0x%0hhx\n",  c->dow);
  cron_destroy(c);
}
#endif
