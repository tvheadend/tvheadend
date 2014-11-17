/*
 *  Process spawn functions
 *  Copyright (C) 2008 Andreas Öman
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <syslog.h>
#include <fcntl.h>
#include <dirent.h>

#include "tvheadend.h"
#include "tvhpoll.h"
#include "file.h"
#include "spawn.h"

#if ENABLE_ANDROID
#define WIFCONTINUED(s) ((s) == 0xffff)
#endif

extern char **environ;

pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;

static LIST_HEAD(, spawn) spawns;

static char *spawn_info_buf = NULL;
static char *spawn_error_buf = NULL;

static th_pipe_t spawn_pipe_info;
static th_pipe_t spawn_pipe_error;

static pthread_t spawn_pipe_tid;

static int spawn_pipe_running;

typedef struct spawn {
  LIST_ENTRY(spawn) link;
  pid_t pid;
  const char *name;
} spawn_t;

/*
 *
 */
#define SPAWN_PIPE_READ_SIZE 4096

static void
spawn_pipe_read( th_pipe_t *p, char **_buf, int level )
{
  char *buf = *_buf, *s;
  size_t len;
  int r;

  if (buf == NULL) {
    buf = malloc(SPAWN_PIPE_READ_SIZE);
    buf[0] = '\0';
    buf[SPAWN_PIPE_READ_SIZE - 1] = 0;
    *_buf = buf;
  }
  while (1) {
    len = strlen(buf);
    r = read(p->rd, buf + len, SPAWN_PIPE_READ_SIZE - 1 - len);
    if (r < 1) {
      if (errno == EAGAIN)
        break;
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    buf[len + r] = '\0';
    tvhlog_hexdump("spawn", buf + len, r);
    while ((s = strchr(buf, '\n')) != NULL) {
      *s++ = '\0';
      tvhlog(level, "spawn", "%s", buf);
      memmove(buf, s, strlen(s) + 1);
    }
    if (strlen(buf) == SPAWN_PIPE_READ_SIZE - 1) {
      tvherror("spawn", "pipe buffer full");
      buf[0] = '\0';
    }
  }
}

static void *
spawn_pipe_thread(void *aux)
{
  tvhpoll_event_t ev[2];
  tvhpoll_t *efd = tvhpoll_create(2);
  int nfds;

  memset(ev, 0, sizeof(ev));
  ev[0].events   = TVHPOLL_IN;
  ev[0].fd       = spawn_pipe_info.rd;
  ev[0].data.ptr = &spawn_pipe_info;
  ev[1].events   = TVHPOLL_IN;
  ev[1].fd       = spawn_pipe_error.rd;
  ev[1].data.ptr = &spawn_pipe_error;
  tvhpoll_add(efd, ev, 2);

  while (spawn_pipe_running) {

    nfds = tvhpoll_wait(efd, ev, 2, 500);

    if (nfds > 0) {
      spawn_pipe_read(&spawn_pipe_info, &spawn_info_buf, LOG_INFO);
      spawn_pipe_read(&spawn_pipe_error, &spawn_error_buf, LOG_ERR);
    }
    spawn_reaper();

  }

  tvhpoll_destroy(efd);
  return NULL;
}

static void
spawn_pipe_write( th_pipe_t *p, const char *fmt, va_list ap )
{
  char buf[512], *s = buf;
  int r;

  vsnprintf(buf, sizeof(buf), fmt, ap);
  while (*s) {
    r = write(p->wr, s, strlen(s));
    if (r < 0) {
      if (errno == EAGAIN)
        break;
      if (ERRNO_AGAIN(errno))
        continue;
      break;
    }
    if (!r)
      break;
    s += r;
  }
}

void
spawn_info( const char *fmt, ... )
{
  va_list ap;

  va_start(ap, fmt);
  spawn_pipe_write(&spawn_pipe_info, fmt, ap);
  va_end(ap);
}

void
spawn_error( const char *fmt, ... )
{
  va_list ap;

  va_start(ap, fmt);
  spawn_pipe_write(&spawn_pipe_error, fmt, ap);
  va_end(ap);
}

/*
 * Search PATH for executable
 */
int
find_exec ( const char *name, char *out, size_t len )
{
  int ret = 0;
  char bin[512];
  char *path, *tmp, *tmp2 = NULL;
  DIR *dir;
  struct dirent *de;
  struct stat st;
  if (name[0] == '/') {
    if (lstat(name, &st)) return 0;
    if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IEXEC)) return 0;
    strncpy(out, name, len);
    out[len-1] = '\0';
    return 1;
  }
  if (!(path = getenv("PATH"))) return 0;
  path = strdup(path);
  tmp  = strtok_r(path, ":", &tmp2);
  while (tmp && !ret) {
    if ((dir = opendir(tmp))) {
      while ((de = readdir(dir))) {
        if (strstr(de->d_name, name) != de->d_name) continue;
        snprintf(bin, sizeof(bin), "%s/%s", tmp, de->d_name);
        if (lstat(bin, &st)) continue;
        if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IEXEC)) continue;
        strncpy(out, bin, len);
        out[len-1] = '\0';
        ret = 1;
        break;
      }
      closedir(dir);
    }
    tmp = strtok_r(NULL, ":", &tmp2);
  }
  free(path);
  return ret;
}

/**
 * Reap one child
 */
int
spawn_reap(char *stxt, size_t stxtlen)
{
  pid_t pid;
  int status, res;
  spawn_t *s;

  pid = waitpid(-1, &status, WNOHANG);
  if(pid < 1)
    return -EAGAIN;

  pthread_mutex_lock(&spawn_mutex);
  LIST_FOREACH(s, &spawns, link)
    if(s->pid == pid)
      break;

  res = -EIO;
  if (WIFEXITED(status)) {
    res = WEXITSTATUS(status);
    if (stxt)
      snprintf(stxt, stxtlen, "exited, status=%d", WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    if (stxt)
      snprintf(stxt, stxtlen, "killed by signal %d, "
                              "stopped by signal %d",
                              WTERMSIG(status),
                              WSTOPSIG(status));
  } else if (WIFCONTINUED(status)) {
    if (stxt)
      snprintf(stxt, stxtlen, "continued");
  } else {
    if (stxt)
      snprintf(stxt, stxtlen, "unknown status");
  }

  if(s != NULL) {
    LIST_REMOVE(s, link);
    free((void *)s->name);
    free(s);
  }
  pthread_mutex_unlock(&spawn_mutex);
  return res;
}

/**
 * The reaper is called once a second to finish of any pending spawns
 */
void
spawn_reaper(void)
{
  while (spawn_reap(NULL, 0) != -EAGAIN) ;
}


/**
 * Enqueue a spawn on the pending spawn list
 */
static spawn_t *
spawn_enq(const char *name, int pid)
{
  spawn_t *s = calloc(1, sizeof(spawn_t));
  s->name = strdup(name);
  s->pid = pid;
  pthread_mutex_lock(&spawn_mutex);
  LIST_INSERT_HEAD(&spawns, s, link);
  pthread_mutex_unlock(&spawn_mutex);
  return s;
}

/**
 * Execute the given program and return its standard output as file-descriptor (pipe).
 */

int
spawn_and_give_stdout(const char *prog, char *argv[], int *rd, int redir_stderr)
{
  pid_t p;
  int fd[2], f;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if (!argv) argv = (void *)local_argv;
  if (!argv[0]) argv[0] = (char*)prog;

  pthread_mutex_lock(&fork_lock);

  if(pipe(fd) == -1) {
    pthread_mutex_unlock(&fork_lock);
    return -1;
  }

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    tvherror("spawn", "Unable to fork() for \"%s\" -- %s",
             prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    close(0);
    close(2);
    close(fd[0]);
    dup2(fd[1], 1);
    close(fd[1]);

    f = open("/dev/null", O_RDWR);
    if(f == -1) {
      spawn_error("pid %d cannot open /dev/null for redirect %s -- %s",
                  getpid(), prog, strerror(errno));
      exit(1);
    }

    dup2(f, 0);
    dup2(redir_stderr ? spawn_pipe_error.wr : f, 2);
    close(f);

    spawn_info("Executing \"%s\"\n", prog);

    execve(prog, argv, environ);
    spawn_error("pid %d cannot execute %s -- %s\n",
                getpid(), prog, strerror(errno));
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  close(fd[1]);

  *rd = fd[0];
  return 0;
}

/**
 * Execute the given program with arguments
 * 
 * *outp will point to the allocated buffer
 * The function will return the size of the buffer
 */
int
spawnv(const char *prog, char *argv[])
{
  pid_t p;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if(!argv) argv = (void *)local_argv;
  if (!argv[0]) argv[0] = (char*)prog;

  pthread_mutex_lock(&fork_lock);

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    tvherror("spawn", "Unable to fork() for \"%s\" -- %s",
	     prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    close(0);
    close(2);
    spawn_info("Executing \"%s\"\n", prog);
    execve(prog, argv, environ);
    spawn_error("pid %d cannot execute %s -- %s\n",
	        getpid(), prog, strerror(errno));
    close(1);
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  return 0;
}

/*
 *
 */
void spawn_init(void)
{
  tvh_pipe(O_NONBLOCK, &spawn_pipe_info);
  tvh_pipe(O_NONBLOCK, &spawn_pipe_error);
  spawn_pipe_running = 1;
  pthread_create(&spawn_pipe_tid, NULL, spawn_pipe_thread, NULL);
}

void spawn_done(void)
{
  spawn_pipe_running = 0;
  pthread_kill(spawn_pipe_tid, SIGTERM);
  pthread_join(spawn_pipe_tid, NULL);
  tvh_pipe_close(&spawn_pipe_error);
  tvh_pipe_close(&spawn_pipe_info);
  free(spawn_error_buf);
  free(spawn_info_buf);
}
