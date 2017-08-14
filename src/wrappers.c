#define __USE_GNU
#include "tvheadend.h"
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
  strncpy(ts->name, "tvh:", 4);
  strncpy(ts->name+4, name, sizeof(ts->name)-4);
  ts->name[sizeof(ts->name)-1] = '\0';
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
  r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  if (r) {
    fprintf(stderr, "Unable to set monotonic clocks for conditions! (%d)", r);
    abort();
  }
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
  struct timespec ts;
  ts.tv_sec = monoclock / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (monoclock % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);
  return pthread_cond_timedwait(&cond->cond, mutex, &ts);
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
}

int64_t
tvh_usleep_abs(int64_t us)
{
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
#elif ENABLE_PCRE2
  pcre2_jit_stack_free(regex->re_jit_stack);
  pcre2_match_data_free(regex->re_match);
  pcre2_code_free(regex->re_code);
  pcre2_match_context_free(regex->re_mcontext);
  regex->re_match = NULL;
  regex->re_code = NULL;
  regex->re_mcontext = NULL;
  regex->re_jit_stack = NULL;
#else
  regfree(&regex->re_code);
#endif
}

int regex_compile(tvh_regex_t *regex, const char *re_str, int subsys)
{
#if ENABLE_PCRE
  const char *estr;
  int eoff;
#if PCRE_STUDY_JIT_COMPILE
  regex->re_jit_stack = NULL;
#endif
  regex->re_extra = NULL;
  regex->re_code = pcre_compile(re_str, PCRE_CASELESS | PCRE_UTF8,
                                &estr, &eoff, NULL);
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
  assert(regex->re_jit_stack == NULL);
  regex->re_jit_stack = NULL;
  regex->re_match = NULL;
  regex->re_mcontext = pcre2_match_context_create(NULL);
  regex->re_code = pcre2_compile((PCRE2_SPTR8)re_str, -1,
                                 PCRE2_CASELESS | PCRE2_UTF,
                                 &ecode, &eoff, NULL);
  if (regex->re_code == NULL) {
    (void)pcre2_get_error_message(ecode, ebuf, 120);
    tvherror(subsys, "Unable to compile PCRE2 '%s': %s", re_str, ebuf);
  } else {
    regex->re_match = pcre2_match_data_create(20, NULL);
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
#else
  if (!regcomp(&regex->re_code, re_str,
               REG_ICASE | REG_EXTENDED | REG_NOSUB))
    return 0;
  tvherror(subsys, "Unable to compile regex '%s'", re_str);
  return -1;
#endif
}
