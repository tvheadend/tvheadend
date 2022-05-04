/*
 *  Tvheadend - structures
 *  Copyright (C) 2007 Andreas Öman
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
#ifndef TVHEADEND_H
#define TVHEADEND_H

#include "build.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <libgen.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include "queue.h"
#include "tvh_thread.h"
#include "tvh_string.h"
#include "hts_strtab.h"
#include "htsmsg.h"
#include "tprofile.h"
#include "tvhlog.h"

#include "redblack.h"

#include "tvh_locale.h"

#define ERRNO_AGAIN(e) ((e) == EAGAIN || (e) == EINTR || (e) == EWOULDBLOCK)

#include "compat.h"

typedef struct {
  const char     *name;
  const uint32_t *enabled;
} tvh_caps_t;

extern int              tvheadend_running;
extern const char      *tvheadend_version;
extern const char      *tvheadend_cwd;
extern const char      *tvheadend_webroot;
extern const tvh_caps_t tvheadend_capabilities[];

static inline int tvheadend_is_running(void)
{
  return atomic_get(&tvheadend_running);
}

htsmsg_t *tvheadend_capabilities_list(int check);

typedef struct str_list
{
  int max;
  int num;
  char **str;
} str_list_t;

#define PTS_UNSET INT64_C(0x8000000000000000)
#define PTS_MASK  INT64_C(0x00000001ffffffff)

extern tvh_mutex_t global_lock;
extern tvh_mutex_t tasklet_lock;
extern tvh_mutex_t fork_lock;

extern int tvheadend_webui_port;
extern int tvheadend_webui_debug;
extern int tvheadend_htsp_port;

#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define CLANG_SANITIZER 1
#endif
#endif

/*
 *
 */
#define UILEVEL_DEFAULT  (-1)
#define UILEVEL_BASIC    0
#define UILEVEL_ADVANCED 1
#define UILEVEL_EXPERT   2

/*
 *
 */
#define CHICON_NONE      0
#define CHICON_LOWERCASE 1
#define CHICON_SVCNAME   2

/*
 *
 */
#define PICON_STANDARD   0
#define PICON_ISVCTYPE   1

/*
 * timer support functions
 */

#if ENABLE_GTIMER_CHECK
#define GTIMER_TRACEID_ const char *id,
#define GTIMER_FCN(n) check_##n
#else
#define GTIMER_TRACEID_
#define GTIMER_FCN(n) n
#endif

/*
 * global timer - monotonic
 */

typedef void (mti_callback_t)(void *opaque);

#define MTIMER_MAGIC1 0x0d62a9de

typedef struct mtimer {
  LIST_ENTRY(mtimer) mti_link;
#if ENABLE_TRACE
  uint32_t mti_magic1;
#endif
  mti_callback_t *mti_callback;
  void *mti_opaque;
  int64_t mti_expire;
#if ENABLE_GTIMER_CHECK
  const char *mti_id;
#endif
} mtimer_t;

void GTIMER_FCN(mtimer_arm_rel)
  (GTIMER_TRACEID_ mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t delta);
void GTIMER_FCN(mtimer_arm_abs)
  (GTIMER_TRACEID_ mtimer_t *mti, mti_callback_t *callback, void *opaque, int64_t when);

#if ENABLE_GTIMER_CHECK
#define mtimer_arm_rel(a, b, c, d) GTIMER_FCN(mtimer_arm_rel)(SRCLINEID(), a, b, c, d)
#define mtimer_arm_abs(a, b, c, d) GTIMER_FCN(mtimer_arm_abs)(SRCLINEID(), a, b, c, d)
#endif

void mtimer_disarm(mtimer_t *mti);



/*
 * global timer (based on the current system time - time())
 */

#define GTIMER_MAGIC1 0x8a6f238f

typedef void (gti_callback_t)(void *opaque);

typedef struct gtimer {
  LIST_ENTRY(gtimer) gti_link;
#if ENABLE_TRACE
  uint32_t gti_magic1;
#endif
  gti_callback_t *gti_callback;
  void *gti_opaque;
  time_t gti_expire;
#if ENABLE_GTIMER_CHECK
  const char *gti_id;
#endif
} gtimer_t;

void GTIMER_FCN(gtimer_arm_rel)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t delta);
void GTIMER_FCN(gtimer_arm_absn)
  (GTIMER_TRACEID_ gtimer_t *gti, gti_callback_t *callback, void *opaque, time_t when);

#if ENABLE_GTIMER_CHECK
#define gtimer_arm_rel(a, b, c, d) GTIMER_FCN(gtimer_arm_rel)(SRCLINEID(), a, b, c, d)
#define gtimer_arm_absn(a, b, c, d) GTIMER_FCN(gtimer_arm_absn)(SRCLINEID(), a, b, c, d)
#endif

void gtimer_disarm(gtimer_t *gti);


/*
 * tasklet
 */

typedef void (tsk_callback_t)(void *opaque, int disarmed);

typedef struct tasklet {
  TAILQ_ENTRY(tasklet) tsk_link;
  tsk_callback_t *tsk_callback;
  void *tsk_opaque;
  void (*tsk_free)(void *);
} tasklet_t;

tasklet_t *tasklet_arm_alloc(tsk_callback_t *callback, void *opaque);
void tasklet_arm(tasklet_t *tsk, tsk_callback_t *callback, void *opaque);
void tasklet_disarm(tasklet_t *gti);

/**
 *
 */
#define TVH_KILL_KILL   0
#define TVH_KILL_TERM   1
#define TVH_KILL_INT    2
#define TVH_KILL_HUP    3
#define TVH_KILL_USR1   4
#define TVH_KILL_USR2   5

int tvh_kill_to_sig(int tvh_kill);

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MINMAX(a,mi,ma) MAX(mi, MIN(ma, a))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

int sri_to_rate(int sri);
int rate_to_sri(int rate);

typedef struct th_pipe
{
  int rd;
  int wr;
} th_pipe_t;

void doexit(int x);

int tvh_open(const char *pathname, int flags, mode_t mode);

int tvh_socket(int domain, int type, int protocol);

int tvh_pipe(int flags, th_pipe_t *pipe);

void tvh_pipe_close(th_pipe_t *pipe);

int tvh_write(int fd, const void *buf, size_t len);

int tvh_nonblock_write(int fd, const void *buf, size_t len);

FILE *tvh_fopen(const char *filename, const char *mode);

void hexdump(const char *pfx, const uint8_t *data, int len);

uint32_t tvh_crc32(const uint8_t *data, size_t datalen, uint32_t crc);

int base64_decode(uint8_t *out, const char *in, int out_size);

char *base64_encode(char *out, int out_size, const uint8_t *in, int in_size);

/* Calculate the output size needed to base64-encode x bytes. */
#define BASE64_SIZE(x) (((x)+2) / 3 * 4 + 1)

static inline int64_t ts_rescale(int64_t ts, int tb)
{
  //  return (ts * tb + (tb / 2)) / 90000LL;
  return (ts * tb ) / 90000LL;
}

static inline int64_t ts_rescale_inv(int64_t ts, int tb)
{
  return (ts * 90000LL) / tb;
}

char *md5sum ( const char *str, int lowercase );
char *sha256sum ( const char *str, int lowercase );
char *sha512sum256 ( const char *str, int lowercase );
char *sha256sum_base64 ( const char *str );

int makedirs ( int subsys, const char *path, int mode, int mstrict, gid_t gid, uid_t uid );

int rmtree ( const char *path );

char *regexp_escape ( const char *str );

#if ENABLE_ZLIB
uint8_t *tvh_gzip_inflate ( const uint8_t *data, size_t size, size_t orig );
uint8_t *tvh_gzip_deflate ( const uint8_t *data, size_t orig, size_t *size );
int      tvh_gzip_deflate_fd ( int fd, const uint8_t *data, size_t orig, size_t *size, int speed );
int      tvh_gzip_deflate_fd_header ( int fd, const uint8_t *data, size_t orig, size_t *size, int speed , const char *signature);
#endif

/* URL decoding */
char to_hex(char code);
char *url_encode(const char *str);
void http_deescape(char *str);

int mpegts_word_count(const uint8_t *tsb, int len, uint32_t mask);

int deferred_unlink(const char *filename, const char *rootdir);
void dvr_cutpoint_delete_files (const char *s);

void sha1_calc(uint8_t *dst, const uint8_t *d1, size_t d1_len, const uint8_t *d2, size_t d2_len);

uint32_t gcdU32(uint32_t a, uint32_t b);
static inline int32_t deltaI32(int32_t a, int32_t b) { return (a > b) ? (a - b) : (b - a); }
static inline uint32_t deltaU32(uint32_t a, uint32_t b) { return (a > b) ? (a - b) : (b - a); }

#define SKEL_DECLARE(name, type) type *name;
#define SKEL_ALLOC(name) do { if (!name) name = calloc(1, sizeof(*name)); } while (0)
#define SKEL_USED(name) do { name = NULL; } while (0)
#define SKEL_FREE(name) do { free(name); name = NULL; } while (0)

htsmsg_t *network_interfaces_enum(void *obj, const char *lang);

const char *gmtime2local(time_t gmt, char *buf, size_t buflen);

/* glibc wrapper */
#if ! ENABLE_QSORT_R
void
qsort_r(void *base, size_t nmemb, size_t size,
       int (*cmp)(const void *, const void *, void *), void *aux);
#endif /* ENABLE_QSORT_R */

void tvh_qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg);

/* printing */
#ifndef __WORDSIZE
# if ULONG_MAX == 0xffffffffffffffff
#  define __WORDSIZE 64
# elif ULONG_MAX == 0xffffffff
#  define __WORDSIZE 32
# endif /* ULONG_MAX */
#endif /* __WORDSIZE */

#if __WORDSIZE == 32 && defined(PLATFORM_FREEBSD)
#define PRItime_t       "d"
#else
#define PRItime_t       "ld"
#endif

/* transcoding */
#define TVH_NAME_LEN 32
#define TVH_TITLE_LEN 256

/* sanitizer helpers */
#if ENABLE_CCLANG_THREADSAN
void *blacklisted_memcpy(void *dest, const void *src, size_t n);
int blacklisted_close(int fildes);
#else
#define blacklisted_memcpy memcpy
#define blacklisted_close close
#endif

#endif /* TVHEADEND_H */
