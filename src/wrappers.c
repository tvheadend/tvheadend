#define __USE_GNU
#include "tvheadend.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#ifdef PLATFORM_FREEBSD
#include <pthread_np.h>
#endif

#include "tvhregex.h"

/*
 * filedescriptor routines
 */

int
tvh_open(const char *pathname, int flags, mode_t mode)
{
  int fd;

  pthread_mutex_lock(&fork_lock);
  fd = open(pathname, flags, mode);
  if (fd != -1)
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
  pthread_mutex_unlock(&fork_lock);
  return fd;
}

int
tvh_socket(int domain, int type, int protocol)
{
  int fd;

  pthread_mutex_lock(&fork_lock);
  fd = socket(domain, type, protocol);
  if (fd != -1)
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
  pthread_mutex_unlock(&fork_lock);
  return fd;
}

int
tvh_pipe(int flags, th_pipe_t *p)
{
  int fd[2], err;
  pthread_mutex_lock(&fork_lock);
  err = pipe(fd);
  if (err != -1) {
    fcntl(fd[0], F_SETFD, fcntl(fd[0], F_GETFD) | FD_CLOEXEC);
    fcntl(fd[1], F_SETFD, fcntl(fd[1], F_GETFD) | FD_CLOEXEC);
    fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | flags);
    fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | flags);
    p->rd = fd[0];
    p->wr = fd[1];
  }
  pthread_mutex_unlock(&fork_lock);
  return err;
}

void
tvh_pipe_close(th_pipe_t *p)
{
  close(p->rd);
  close(p->wr);
  p->rd = -1;
  p->wr = -1;
}

int
tvh_write(int fd, const void *buf, size_t len)
{
  int64_t limit = mclk() + sec2mono(25);
  ssize_t c;

  while (len) {
    c = write(fd, buf, len);
    if (c < 0) {
      if (ERRNO_AGAIN(errno)) {
        if (mclk() > limit)
          break;
        tvh_safe_usleep(100);
        continue;
      }
      break;
    }
    len -= c;
    buf += c;
  }

  return len ? 1 : 0;
}

int
tvh_nonblock_write(int fd, const void *buf, size_t len)
{
  ssize_t c;

  while (len) {
    c = write(fd, buf, len);
    if (c < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    len -= c;
    buf += c;
  }

  return len ? 1 : 0;
}

FILE *
tvh_fopen(const char *filename, const char *mode)
{
  FILE *f;
  int fd;
  pthread_mutex_lock(&fork_lock);
  f = fopen(filename, mode);
  if (f) {
    fd = fileno(f);
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
  }
  pthread_mutex_unlock(&fork_lock);
  return f;
}

/*
 * thread routines
 */

static void doquit(int sig)
{
}

struct
thread_state {
  void *(*run)(void*);
  void *arg;
  char name[17];
};

static void *
thread_wrapper ( void *p )
{
  struct thread_state *ts = p;
  sigset_t set;

#if defined(PLATFORM_LINUX)
  /* Set name */
  prctl(PR_SET_NAME, ts->name);
#elif defined(PLATFORM_FREEBSD)
  /* Set name of thread */
  pthread_set_name_np(pthread_self(), ts->name);
#elif defined(PLATFORM_DARWIN)
  pthread_setname_np(ts->name);
#endif

  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGQUIT);
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  signal(SIGTERM, doexit);
  signal(SIGQUIT, doquit);

  /* Run */
  tvhtrace(LS_THREAD, "created thread %ld [%s / %p(%p)]",
           (long)pthread_self(), ts->name, ts->run, ts->arg);
  void *r = ts->run(ts->arg);
  free(ts);

  return r;
}

int
tvhthread_create
  (pthread_t *thread, const pthread_attr_t *attr,
   void *(*start_routine) (void *), void *arg, const char *name)
{
  int r;
  struct thread_state *ts = calloc(1, sizeof(struct thread_state));
  pthread_attr_t _attr;
  if (attr == NULL) {
    pthread_attr_init(&_attr);
    pthread_attr_setstacksize(&_attr, 2*1024*1024);
    attr = &_attr;
  }
  strlcpy(ts->name, "tvh:", 5);
  strlcpy(ts->name+4, name, sizeof(ts->name)-4);
  ts->run  = start_routine;
  ts->arg  = arg;
  r = pthread_create(thread, attr, thread_wrapper, ts);
  return r;
}

/* linux style: -19 .. 20 */
int
tvhthread_renice(int value)
{
  int ret = 0;
#ifdef SYS_gettid
  pid_t tid;
  tid = syscall(SYS_gettid);
  ret = setpriority(PRIO_PROCESS, tid, value);
#elif ENABLE_ANDROID
  pid_t tid;
  tid = gettid();
  ret = setpriority(PRIO_PROCESS, tid, value);
#elif defined(PLATFORM_DARWIN)
  /* Currently not possible */
#elif defined(PLATFORM_FREEBSD)
  /* Currently not possible */
#else
#warning "Implement renice for your platform!"
#endif
  return ret;
}

int
tvh_mutex_timedlock
  ( pthread_mutex_t *mutex, int64_t usec )
{
  int64_t finish = getfastmonoclock() + usec;
  int retcode;

  while ((retcode = pthread_mutex_trylock (mutex)) == EBUSY) {
    if (getfastmonoclock() >= finish)
      return ETIMEDOUT;

    tvh_safe_usleep(10000);
  }

  return retcode;
}

/*
 * thread condition variables - monotonic clocks
 */

int
tvh_cond_init
  ( tvh_cond_t *cond )
{
  int r;

  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
#if defined(PLATFORM_DARWIN)
  /*
   * pthread_condattr_setclock() not supported on platform Darwin.
   * We use pthread_cond_timedwait_relative_np() which doesn't
   * need it.
   */
   r = 0;
#else
  r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  if (r) {
    fprintf(stderr, "Unable to set monotonic clocks for conditions! (%d)", r);
    abort();
  }
#endif
  return pthread_cond_init(&cond->cond, &attr);
}

int
tvh_cond_destroy
  ( tvh_cond_t *cond )
{
  return pthread_cond_destroy(&cond->cond);
}

int
tvh_cond_signal
  ( tvh_cond_t *cond, int broadcast )
{
  if (broadcast)
    return pthread_cond_broadcast(&cond->cond);
  else
    return pthread_cond_signal(&cond->cond);
}

int
tvh_cond_wait
  ( tvh_cond_t *cond, pthread_mutex_t *mutex)
{
  return pthread_cond_wait(&cond->cond, mutex);
}

int
tvh_cond_timedwait
  ( tvh_cond_t *cond, pthread_mutex_t *mutex, int64_t monoclock )
{
#if defined(PLATFORM_DARWIN)
  /* Use a relative timedwait implementation */
  int64_t now = getmonoclock();
  int64_t relative = monoclock - now;

  struct timespec ts;
  ts.tv_sec  = relative / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (relative % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);

  return pthread_cond_timedwait_relative_np(&cond->cond, mutex, &ts);
#else
  struct timespec ts;
  ts.tv_sec = monoclock / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (monoclock % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);
  return pthread_cond_timedwait(&cond->cond, mutex, &ts);
#endif
}

/*
 * clocks
 */

void
tvh_safe_usleep(int64_t us)
{
  int64_t r;
  if (us <= 0)
    return;
  do {
    r = tvh_usleep(us);
    if (r < 0) {
      if (ERRNO_AGAIN(-r))
        continue;
      break;
    }
    us = r;
  } while (r > 0);
}

int64_t
tvh_usleep(int64_t us)
{
#if defined(PLATFORM_DARWIN)
  return usleep(us);
#else
  struct timespec ts;
  int64_t val;
  int r;
  if (us <= 0)
    return 0;
  ts.tv_sec = us / 1000000LL;
  ts.tv_nsec = (us % 1000000LL) * 1000LL;
  r = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts);
  val = (ts.tv_sec * 1000000LL) + ((ts.tv_nsec + 500LL) / 1000LL);
  if (ERRNO_AGAIN(r))
    return val;
  return r ? -r : 0;
#endif
}

int64_t
tvh_usleep_abs(int64_t us)
{
#if defined(PLATFORM_DARWIN)
  /* Convert to relative wait */
  int64_t now = getmonoclock();
  int64_t relative = us - now;
  return tvh_usleep(relative);
#else
  struct timespec ts;
  int64_t val;
  int r;
  if (us <= 0)
    return 0;
  ts.tv_sec = us / 1000000LL;
  ts.tv_nsec = (us % 1000000LL) * 1000LL;
  r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, &ts);
  val = (ts.tv_sec * 1000000LL) + ((ts.tv_nsec + 500LL) / 1000LL);
  if (ERRNO_AGAIN(r))
    return val;
  return r ? -r : 0;
#endif
}

/*
 * qsort
 */

#if ! ENABLE_QSORT_R
/*
 * qsort_r wrapper for pre GLIBC 2.8
 */

static __thread struct {
  int (*cmp) ( const void *a, const void *b, void *p );
  void *aux;
} qsort_r_data;

static int
qsort_r_wrap ( const void *a, const void *b )
{
  return qsort_r_data.cmp(a, b, qsort_r_data.aux);
}

void
qsort_r(void *base, size_t nmemb, size_t size,
       int (*cmp)(const void *, const void *, void *), void *aux)
{
  qsort_r_data.cmp = cmp;
  qsort_r_data.aux = aux;
  qsort(base, nmemb, size, qsort_r_wrap);
}
#endif /* ENABLE_QSORT_R */


#if defined(PLATFORM_FREEBSD) || defined(PLATFORM_DARWIN)
struct tvh_qsort_data {
    void *arg;
    int (*compar)(const void *, const void *, void *);
};


static int
tvh_qsort_swap(void *arg, const void *a, const void *b)
{
    struct tvh_qsort_data *data = arg;
    return data->compar(a, b, data->arg);
}
#endif /* PLATFORM_FREEBSD || PLATFORM_DARWIN */


void
tvh_qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)
{
#if defined(PLATFORM_FREEBSD) || defined(PLATFORM_DARWIN)
    struct tvh_qsort_data swap_arg = {arg, compar};
    qsort_r(base, nmemb, size, &swap_arg, tvh_qsort_swap);
#else
    qsort_r(base, nmemb, size, compar, arg);
#endif
}

/*
 * Regex stuff
 */
void regex_free(tvh_regex_t *regex)
{
#if ENABLE_PCRE || ENABLE_PCRE2
  if (regex->is_posix) {
#endif
    regfree(&regex->re_posix_code);
    regex->re_posix_text = NULL;
#if ENABLE_PCRE || ENABLE_PCRE2
  } else {
#if ENABLE_PCRE
#ifdef PCRE_CONFIG_JIT
#if PCRE_STUDY_JIT_COMPILE
    if (regex->re_jit_stack) {
      pcre_jit_stack_free(regex->re_jit_stack);
      regex->re_jit_stack = NULL;
    }
#endif
    pcre_free_study(regex->re_extra);
#else
    pcre_free(regex->re_extra);
#endif
    pcre_free(regex->re_code);
    regex->re_extra = NULL;
    regex->re_code = NULL;
    regex->re_text = NULL;
#elif ENABLE_PCRE2
    pcre2_jit_stack_free(regex->re_jit_stack);
    pcre2_match_data_free(regex->re_match);
    pcre2_code_free(regex->re_code);
    pcre2_match_context_free(regex->re_mcontext);
    regex->re_match = NULL;
    regex->re_code = NULL;
    regex->re_mcontext = NULL;
    regex->re_jit_stack = NULL;
#endif
  }
#endif
}

int regex_compile(tvh_regex_t *regex, const char *re_str, int flags, int subsys)
{
#if ENABLE_PCRE || ENABLE_PCRE2
  regex->is_posix = 0;
  if (flags & TVHREGEX_POSIX) {
    regex->is_posix = 1;
#endif
    int options = REG_EXTENDED;
    if (flags & TVHREGEX_CASELESS)
      options |= REG_ICASE;
    if (!regcomp(&regex->re_posix_code, re_str, options))
      return 0;
    tvherror(subsys, "Unable to compile regex '%s'", re_str);
    return -1;
#if ENABLE_PCRE || ENABLE_PCRE2
  } else {
#if ENABLE_PCRE
    const char *estr;
    int eoff;
    int options = PCRE_UTF8;
    if (flags & TVHREGEX_CASELESS)
      options |= PCRE_CASELESS;
#if PCRE_STUDY_JIT_COMPILE
    regex->re_jit_stack = NULL;
#endif
    regex->re_extra = NULL;
    regex->re_code = pcre_compile(re_str, options, &estr, &eoff, NULL);
    if (regex->re_code == NULL) {
      tvherror(subsys, "Unable to compile PCRE '%s': %s", re_str, estr);
    } else {
      regex->re_extra = pcre_study(regex->re_code,
                                   PCRE_STUDY_JIT_COMPILE, &estr);
      if (regex->re_extra == NULL && estr)
        tvherror(subsys, "Unable to study PCRE '%s': %s", re_str, estr);
      else {
#if PCRE_STUDY_JIT_COMPILE
        regex->re_jit_stack = pcre_jit_stack_alloc(32*1024, 512*1024);
        if (regex->re_jit_stack)
          pcre_assign_jit_stack(regex->re_extra, NULL, regex->re_jit_stack);
#endif
        return 0;
      }
    }
    return -1;
#elif ENABLE_PCRE2
    PCRE2_UCHAR8 ebuf[128];
    int ecode;
    PCRE2_SIZE eoff;
    size_t jsz;
    uint32_t options;
    assert(regex->re_jit_stack == NULL);
    regex->re_jit_stack = NULL;
    regex->re_match = NULL;
    regex->re_mcontext = pcre2_match_context_create(NULL);
    options = PCRE2_UTF;
    if (flags & TVHREGEX_CASELESS)
      options |= PCRE2_CASELESS;
    regex->re_code = pcre2_compile((PCRE2_SPTR8)re_str, -1, options,
                                   &ecode, &eoff, NULL);
    if (regex->re_code == NULL) {
      (void)pcre2_get_error_message(ecode, ebuf, 120);
      tvherror(subsys, "Unable to compile PCRE2 '%s': %s", re_str, ebuf);
    } else {
      regex->re_match = pcre2_match_data_create(TVHREGEX_MAX_MATCHES, NULL);
      if (re_str[0] && pcre2_jit_compile(regex->re_code, PCRE2_JIT_COMPLETE) >= 0) {
        jsz = 0;
        if (pcre2_pattern_info(regex->re_code, PCRE2_INFO_JITSIZE, &jsz) >= 0 && jsz > 0) {
          regex->re_jit_stack = pcre2_jit_stack_create(32 * 1024, 512 * 1024, NULL);
          if (regex->re_jit_stack)
            pcre2_jit_stack_assign(regex->re_mcontext, NULL, regex->re_jit_stack);
        }
      }
      return 0;
    }
    return -1;
#endif
  }
#endif
}

int regex_match(tvh_regex_t *regex, const char *str)
{
#if ENABLE_PCRE || ENABLE_PCRE2
  if (regex->is_posix) {
#endif
    regex->re_posix_text = str;
    return regexec(&regex->re_posix_code, str, TVHREGEX_MAX_MATCHES, regex->re_posix_match, 0);
#if ENABLE_PCRE || ENABLE_PCRE2
  } else {
#if ENABLE_PCRE
    regex->re_text = str;
    regex->re_matches =
      pcre_exec(regex->re_code, regex->re_extra,
                str, strlen(str), 0, 0, regex->re_match, TVHREGEX_MAX_MATCHES * 3);
    return regex->re_matches < 0;
#elif ENABLE_PCRE2
    return pcre2_match(regex->re_code, (PCRE2_SPTR8)str, -1, 0, 0,
                       regex->re_match, regex->re_mcontext) <= 0;
#endif
  }
#endif
}

int regex_match_substring(tvh_regex_t *regex, unsigned number, char *buf, size_t size_buf)
{
  assert(buf);
  if (number >= TVHREGEX_MAX_MATCHES)
    return -2;
#if ENABLE_PCRE || ENABLE_PCRE2
  if (regex->is_posix) {
#endif
    if (regex->re_posix_match[number].rm_so == -1)
      return -1;
    ssize_t size = regex->re_posix_match[number].rm_eo - regex->re_posix_match[number].rm_so;
    if (size < 0 || size > (size_buf - 1))
      return -1;
    strlcpy(buf, regex->re_posix_text + regex->re_posix_match[number].rm_so, size + 1);
    return 0;
#if ENABLE_PCRE || ENABLE_PCRE2
  } else {
#if ENABLE_PCRE
    return pcre_copy_substring(regex->re_text, regex->re_match,
                               (regex->re_matches == 0)
                               ? TVHREGEX_MAX_MATCHES
                               : regex->re_matches,
                               number, buf, size_buf) < 0;
#elif ENABLE_PCRE2
    PCRE2_SIZE psiz = size_buf;
    return pcre2_substring_copy_bynumber(regex->re_match, number, (PCRE2_UCHAR8*)buf, &psiz);
#endif
  }
#endif
}

int regex_match_substring_length(tvh_regex_t *regex, unsigned number)
{
  if (number >= TVHREGEX_MAX_MATCHES)
    return -2;
#if ENABLE_PCRE || ENABLE_PCRE2
  if (regex->is_posix) {
#endif
    if (regex->re_posix_match[number].rm_so == -1)
      return -1;
    return regex->re_posix_match[number].rm_eo - regex->re_posix_match[number].rm_so;
#if ENABLE_PCRE || ENABLE_PCRE2
  } else {
#if ENABLE_PCRE
  if (number >= regex->re_matches)
    return -1;
  if (regex->re_match[number * 2] == -1)
    return -1;
  return regex->re_match[number * 2 + 1] - regex->re_match[number * 2];
#elif ENABLE_PCRE2
  PCRE2_SIZE len;
  int rc = pcre2_substring_length_bynumber(regex->re_match, number, &len);
  return (!rc) ? len : -1;
#endif
  }
#endif
}

/*
 * Sanitizer helpers to avoid false positives
 */
#if ENABLE_CCLANG_THREADSAN
void *blacklisted_memcpy(void *dest, const void *src, size_t n)
  __attribute__((no_sanitize("thread")))
{
  uint8_t *d = dest;
  const uint8_t *s = src;
  while (n-- > 0) *d++ = *s++;
  return dest;
}

void *dlsym(void *handle, const char *symbol);

int blacklisted_close(int fd)
  __attribute__((no_sanitize("thread")))
{
  // close(fd); // sanitizer reports errors in close()
  return 0;
}
#endif
