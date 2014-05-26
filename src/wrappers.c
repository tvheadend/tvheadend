#define __USE_GNU
#include "tvheadend.h"
#include <fcntl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#ifdef PLATFORM_LINUX
#include <sys/prctl.h>
#endif

#ifdef PLATFORM_FREEBSD
#include <pthread_np.h>
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
  ssize_t c;

  while (len) {
    c = write(fd, buf, len);
    if (c < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        usleep(100);
        continue;
      }
      break;
    }
    len -= c;
    buf += c;
  }

  return len ? 1 : 0;
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
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  signal(SIGTERM, doexit);

  /* Run */
  tvhtrace("thread", "created thread %ld [%s / %p(%p)]",
           (long)pthread_self(), ts->name, ts->run, ts->arg);
  void *r = ts->run(ts->arg);
  free(ts);

  return r;
}

int
tvhthread_create0
  (pthread_t *thread, const pthread_attr_t *attr,
   void *(*start_routine) (void *), void *arg, const char *name,
   int detach)
{
  int r;
  struct thread_state *ts = calloc(1, sizeof(struct thread_state));
  strncpy(ts->name, name, sizeof(ts->name));
  ts->run  = start_routine;
  ts->arg  = arg;
  r = pthread_create(thread, attr, thread_wrapper, ts);
  if (detach)
    pthread_detach(*thread);
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


#if defined(PLATFORM_FREEBSD)
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
#endif /* PLATFORM_FREEBSD */


void
tvh_qsort_r(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)
{
#if defined(PLATFORM_FREEBSD)
    struct tvh_qsort_data swap_arg = {arg, compar};
    qsort_r(base, nmemb, size, &swap_arg, tvh_qsort_swap);
#else
    qsort_r(base, nmemb, size, compar, arg);
#endif
}
