#include <time.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include "tvhtime.h"
#include "tvheadend.h"
#include "config.h"

#if !ENABLE_ANDROID
#include <sys/shm.h>
#endif

/*
 * NTP processing
 */
#define NTPD_BASE	0x4e545030	/* "NTP0" */
#define NTPD_UNIT	2

#if !ENABLE_ANDROID
typedef struct
{
  int    mode; /* 0 - if valid set
				        *       use values, 
				        *       clear valid
				        * 1 - if valid set 
				        *       if count before and after read of values is equal,
				        *         use values 
				        *       clear valid
				        */
  int    count;
  time_t clockTimeStampSec;
  int    clockTimeStampUSec;
  time_t receiveTimeStampSec;
  int    receiveTimeStampUSec;
  int    leap;
  int    precision;
  int    nsamples;
  int    valid;
  int    pad[10];
} ntp_shm_t;

static ntp_shm_t *
ntp_shm_init ( void )
{
  int shmid, unit, mode;
  static ntp_shm_t *shmptr = NULL;

  if (shmptr != NULL)
    return shmptr;

  unit = getuid() ? 2 : 0;
  mode = getuid() ? 0666 : 0600;

  shmid = shmget((key_t)NTPD_BASE + unit, sizeof(ntp_shm_t), IPC_CREAT | mode);
  if (shmid == -1)
    return NULL;

  shmptr = shmat(shmid, 0, 0);

  if (shmptr) {
    memset(shmptr, 0, sizeof(ntp_shm_t));
    shmptr->mode      = 1;
    shmptr->precision = -1;
    shmptr->nsamples  = 1;
  }

  return shmptr;
}
#endif

/*
 * Update time
 */
void
tvhtime_update ( time_t utc, const char *srcname )
{
#if !ENABLE_ANDROID
  struct tm tm;
  struct timeval tv;
  ntp_shm_t *ntp_shm;
  int64_t t1, t2, diff;

  localtime_r(&utc, &tm);

  tvhtrace(LS_TIME, "%s - current time is %04d/%02d/%02d %02d:%02d:%02d",
           srcname,
           tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec);

  gettimeofday(&tv, NULL);

  /* Delta */
  t1 = utc * 1000000;
  t2 = tv.tv_sec * 1000000 + tv.tv_usec;

  /* Update local clock */
  if (config.tvhtime_update_enabled) {
    if ((diff = llabs(t2 - t1)) > config.tvhtime_tolerance) {
#if ENABLE_STIME
      if (stime(&utc) < 0)
        tvherror(LS_TIME, "%s - unable to update system time: %s", srcname, strerror(errno));
      else
        tvhdebug(LS_TIME, "%s - updated system clock", srcname);
#else
      tvhnotice(LS_TIME, "%s - stime(2) not implemented", srcname);
#endif
    } else {
      tvhwarn(LS_TIME, "%s - time not updated (diff %"PRId64")", srcname, diff);
    }
  }

  /* NTP */
  if (config.tvhtime_ntp_enabled) {
    if (!(ntp_shm = ntp_shm_init()))
      return;

    tvhtrace(LS_TIME, "ntp delta = %"PRId64" us\n", t2 - t1);
    ntp_shm->valid = 0;
    ntp_shm->count++;
    ntp_shm->clockTimeStampSec    = utc;
    ntp_shm->clockTimeStampUSec   = 0;
    ntp_shm->receiveTimeStampSec  = tv.tv_sec;
    ntp_shm->receiveTimeStampUSec = (int)tv.tv_usec;
    ntp_shm->count++;
    ntp_shm->valid = 1;
  }
#endif
}

/* Initialise */
void tvhtime_init ( void )
{
  if (config.tvhtime_tolerance == 0)
    config.tvhtime_tolerance = 5000;
}
