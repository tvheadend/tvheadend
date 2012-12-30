#include <fcntl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <unistd.h>
#include "tvheadend.h"

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
