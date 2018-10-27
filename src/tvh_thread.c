#define __USE_GNU
#include "tvheadend.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#define TVH_THREAD_C 1
#include "tvh_thread.h"

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#ifdef PLATFORM_FREEBSD
#include <pthread_np.h>
#endif

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
tvh_thread_create
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

int tvh_thread_kill(pthread_t thread, int sig)
{
  return pthread_kill(thread, sig);
}

/* linux style: -19 .. 20 */
int
tvh_thread_renice(int value)
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

int tvh_mutex_init(tvh_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr)
{
  return pthread_mutex_init(&mutex->mutex, attr);
}

int tvh_mutex_destroy(tvh_mutex_t *mutex)
{
  return pthread_mutex_destroy(&mutex->mutex);
}

int tvh_mutex_lock(tvh_mutex_t *mutex)
{
  return pthread_mutex_lock(&mutex->mutex);
}

int tvh_mutex_trylock(tvh_mutex_t *mutex)
{
  return pthread_mutex_trylock(&mutex->mutex);
}

int tvh_mutex_unlock(tvh_mutex_t *mutex)
{
  return pthread_mutex_unlock(&mutex->mutex);
}


int
tvh_mutex_timedlock
  ( tvh_mutex_t *mutex, int64_t usec )
{
  int64_t finish = getfastmonoclock() + usec;
  int retcode;

  while ((retcode = pthread_mutex_trylock (&mutex->mutex)) == EBUSY) {
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
  ( tvh_cond_t *cond, int monotonic )
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
  ( tvh_cond_t *cond, tvh_mutex_t *mutex)
{
  return pthread_cond_wait(&cond->cond, &mutex->mutex);
}

int
tvh_cond_timedwait
  ( tvh_cond_t *cond, tvh_mutex_t *mutex, int64_t monoclock )
{
#if defined(PLATFORM_DARWIN)
  /* Use a relative timedwait implementation */
  int64_t now = getmonoclock();
  int64_t relative = monoclock - now;
  struct timespec ts;

  if (relative < 0)
    return 0;

  ts.tv_sec  = relative / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (relative % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);

  return pthread_cond_timedwait_relative_np(&cond->cond, &mutex->mutex, &ts);
#else
  struct timespec ts;
  ts.tv_sec = monoclock / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (monoclock % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);
  return pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
#endif
}

int tvh_cond_timedwait_ts(tvh_cond_t *cond, tvh_mutex_t *mutex, struct timespec *ts)
{
  return pthread_cond_timedwait(&cond->cond, &mutex->mutex, ts);
}

void
tvh_mutex_not_held(const char *file, int line)
{
  fprintf(stderr, "Mutex not held at %s:%d\n", file, line);
  abort();
}
