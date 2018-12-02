#define __USE_GNU
#define TVH_THREAD_C 1
#include "tvheadend.h"
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>

#include "settings.h"
#include "htsbuf.h"

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#ifdef PLATFORM_FREEBSD
#include <pthread_np.h>
#endif

#if ENABLE_TRACE
int tvh_thread_debug;
static int tvhwatch_done;
static pthread_t thrwatch_tid;
static pthread_mutex_t thrwatch_mutex = PTHREAD_MUTEX_INITIALIZER;
static TAILQ_HEAD(, tvh_mutex) thrwatch_mutexes = TAILQ_HEAD_INITIALIZER(thrwatch_mutexes);
static int64_t tvh_thread_crash_time;
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

#if ENABLE_TRACE
static void
thread_get_name(pthread_t tid, char *buf, int len)
{
  buf[0] = '?';
  buf[1] = '\0';
#if defined(PLATFORM_LINUX)
  /* Set name */
  if (len >= 16)
    prctl(PR_GET_NAME, buf);
#elif defined(PLATFORM_FREEBSD)
  /* Get name of thread */
  //pthread_get_name_np(tid, buf); ???
#elif defined(PLATFORM_DARWIN)
  // ???
#endif
}
#endif

static void *
thread_wrapper(void *p)
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

int tvh_mutex_init(tvh_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
  memset(mutex, 0, sizeof(*mutex));
  return pthread_mutex_init(&mutex->mutex, attr);
}

int tvh_mutex_destroy(tvh_mutex_t *mutex)
{
  return pthread_mutex_destroy(&mutex->mutex);
}

#if ENABLE_TRACE
static void tvh_mutex_add_to_list(tvh_mutex_t *mutex, const char *filename, int lineno)
{
  pthread_mutex_lock(&thrwatch_mutex);
  if (filename != NULL) {
    mutex->thread = pthread_self();
    mutex->filename = filename;
    mutex->lineno = lineno;
  }
  mutex->tstamp = getfastmonoclock();
  TAILQ_SAFE_REMOVE(&thrwatch_mutexes, mutex, link);
  TAILQ_INSERT_HEAD(&thrwatch_mutexes, mutex, link);
  pthread_mutex_unlock(&thrwatch_mutex);
}
#endif

#if ENABLE_TRACE
static void tvh_mutex_check_interval(tvh_mutex_t *mutex)
{
  if (tvh_thread_debug > 10000) {
    int64_t ms = (tvh_thread_debug - 10000) * 1000;
    int64_t diff = getfastmonoclock() - mutex->tstamp;
    if (diff > ms)
      tvhdbg(LS_THREAD, "mutex %p at %s:%d took %lldms",
             mutex, mutex->filename, mutex->lineno,
             diff / (MONOCLOCK_RESOLUTION / 1000));
  }
}
#endif

#if ENABLE_TRACE
static void tvh_mutex_remove_from_list(tvh_mutex_t *mutex)
{
  pthread_mutex_lock(&thrwatch_mutex);
  TAILQ_SAFE_REMOVE(&thrwatch_mutexes, mutex, link);
  tvh_mutex_check_interval(mutex);
  mutex->filename = NULL;
  mutex->lineno = 0;
  pthread_mutex_unlock(&thrwatch_mutex);
}
#endif

#if ENABLE_TRACE
int tvh__mutex_lock(tvh_mutex_t *mutex, const char *filename, int lineno)
{
  int r = pthread_mutex_lock(&mutex->mutex);
  if (r == 0)
    tvh_mutex_add_to_list(mutex, filename, lineno);
  return r;
}
#endif

#if ENABLE_TRACE
int tvh__mutex_trylock(tvh_mutex_t *mutex, const char *filename, int lineno)
{
  int r = pthread_mutex_trylock(&mutex->mutex);
  if (r == 0)
    tvh_mutex_add_to_list(mutex, filename, lineno);
  return r;
}
#endif

#if ENABLE_TRACE
int tvh__mutex_unlock(tvh_mutex_t *mutex)
{
  int r;
  r = pthread_mutex_unlock(&mutex->mutex);
  if (r == 0)
    tvh_mutex_remove_from_list(mutex);
  return r;
}
#endif

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
  if (monotonic) {
    r = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (r) {
      fprintf(stderr, "Unable to set monotonic clocks for conditions! (%d)", r);
      abort();
    }
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
  const char *filename = mutex->filename;
  const int lineno = mutex->lineno;
  int r;
  
#if ENABLE_TRACE
  tvh_mutex_remove_from_list(mutex);
#endif
  r = pthread_cond_wait(&cond->cond, &mutex->mutex);
#if ENABLE_TRACE
  tvh_mutex_add_to_list(mutex, filename, lineno);
#endif
  return r;
}

int
tvh_cond_timedwait
  ( tvh_cond_t *cond, tvh_mutex_t *mutex, int64_t monoclock )
{
  const char *filename = mutex->filename;
  const int lineno = mutex->lineno;
  int r;

#if ENABLE_TRACE
  tvh_mutex_remove_from_list(mutex);
#endif
  
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

  r = pthread_cond_timedwait_relative_np(&cond->cond, &mutex->mutex, &ts);
#else
  struct timespec ts;
  ts.tv_sec = monoclock / MONOCLOCK_RESOLUTION;
  ts.tv_nsec = (monoclock % MONOCLOCK_RESOLUTION) *
               (1000000000ULL/MONOCLOCK_RESOLUTION);
  r = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
#endif

#if ENABLE_TRACE
  tvh_mutex_add_to_list(mutex, filename, lineno);
#endif
  return r;
}

int tvh_cond_timedwait_ts(tvh_cond_t *cond, tvh_mutex_t *mutex, struct timespec *ts)
{
  const char *filename = mutex->filename;
  const int lineno = mutex->lineno;
  int r;
  
#if ENABLE_TRACE
  tvh_mutex_remove_from_list(mutex);
#endif
  r = pthread_cond_timedwait(&cond->cond, &mutex->mutex, ts);
#if ENABLE_TRACE
  tvh_mutex_add_to_list(mutex, filename, lineno);
#endif
  return r;
}

void
tvh_mutex_not_held(const char *file, int line)
{
  fprintf(stderr, "Mutex not held at %s:%d\n", file, line);
  abort();
}

#if ENABLE_TRACE
static void tvh_thread_mutex_deadlock(tvh_mutex_t *mutex)
{
  int fd = hts_settings_open_file(HTS_SETTINGS_OPEN_WRITE | HTS_SETTINGS_OPEN_DIRECT, "mutex-deadlock.txt");
  if (fd < 0) fd = fileno(stderr);
  int sid = mutex->mutex.__data.__owner; /* unportable */
  char name[256], *s;
  htsbuf_queue_t q;
  size_t l;

  thread_get_name(mutex->thread, name, sizeof(name));

  htsbuf_queue_init(&q, 0);
  htsbuf_qprintf(&q, "Thread %i: %s\n", sid, name);
  htsbuf_qprintf(&q, "  locked in: %s:%i\n", mutex->filename, mutex->lineno);

  s = htsbuf_to_string(&q);
  l = s ? strlen(s) : 0;
  if (l > 0) {
    tvh_write(fd, s, l);
    tvh_write(STDERR_FILENO, s, l);
  }

  abort();
}
#endif

#if ENABLE_TRACE
static void *tvh_thread_watch_thread(void *aux)
{
  int64_t now;
  tvh_mutex_t *mutex, dmutex;

  while (!tvhwatch_done) {
    pthread_mutex_lock(&thrwatch_mutex);
    now = getfastmonoclock();
    mutex = TAILQ_LAST(&thrwatch_mutexes, tvh_mutex_queue);
    if (mutex && mutex->tstamp + sec2mono(5) < now) {
      pthread_mutex_unlock(&thrwatch_mutex);
      tvh_thread_mutex_deadlock(mutex);
    }
    pthread_mutex_unlock(&thrwatch_mutex);
    if (tvh_thread_debug == 12345678 && tvh_thread_crash_time < getfastmonoclock()) {
      tvh_thread_debug--;
      tvh_mutex_init(&dmutex, NULL);
      tvh_mutex_lock(&dmutex);
    }
    tvh_usleep(1000000);
  }
  return NULL;
}
#endif

void tvh_thread_init(int debug_level)
{
#if ENABLE_TRACE
  tvh_thread_debug = debug_level;
  tvh_thread_crash_time = getfastmonoclock() + sec2mono(15);
  if (debug_level > 0) {
    tvhwatch_done = 0;
    tvh_thread_create(&thrwatch_tid, NULL, tvh_thread_watch_thread, NULL, "thrwatch");
  }
#endif
}

void tvh_thread_done(void)
{
#if ENABLE_TRACE
  if (tvh_thread_debug > 0) {
    tvhwatch_done = 1;
    pthread_join(thrwatch_tid, NULL);
  }
#endif
}
