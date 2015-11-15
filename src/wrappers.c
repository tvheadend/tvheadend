#define __USE_GNU
#include "tvheadend.h"
#include <fcntl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#endif

#ifdef PLATFORM_FREEBSD
#include <pthread_np.h>
#endif

#if ENABLE_ANDROID
int
pthread_mutex_timedlock
  ( pthread_mutex_t *mutex, struct timespec *timeout )
{
  struct timeval timenow;
  struct timespec sleepytime;
  int retcode;

  /* This is just to avoid a completely busy wait */
  sleepytime.tv_sec = 0;
  sleepytime.tv_nsec = 10000000; /* 10ms */

  while ((retcode = pthread_mutex_trylock (mutex)) == EBUSY) {
    gettimeofday (&timenow, NULL);

    if (timenow.tv_sec >= timeout->tv_sec &&
       (timenow.tv_usec * 1000) >= timeout->tv_nsec)
      return ETIMEDOUT;

    nanosleep (&sleepytime, NULL);
  }

  return retcode;
}
#endif

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
  time_t next = dispatch_clock + 25;
  ssize_t c;

  while (len) {
    c = write(fd, buf, len);
    if (c < 0) {
      if (ERRNO_AGAIN(errno)) {
        if (dispatch_clock > next)
          break;
        usleep(100);
        dispatch_clock_update(NULL);
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
  tvhtrace("thread", "created thread %ld [%s / %p(%p)]",
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
  strncpy(ts->name, "tvh:", 4);
  strncpy(ts->name+4, name, sizeof(ts->name)-4);
  ts->name[sizeof(ts->name)-1] = '\0';
  ts->run  = start_routine;
  ts->arg  = arg;
  r = pthread_create(thread, attr, thread_wrapper, ts);
  return r;
}

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
