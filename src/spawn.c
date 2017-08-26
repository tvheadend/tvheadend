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
#include <signal.h>

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
  int64_t killed;
} spawn_t;

static void spawn_reaper(void);

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
    tvhlog_hexdump(LS_SPAWN, buf + len, r);
    while (1) {
      s = buf;
      while (*s && *s != '\n' && *s != '\r')
        s++;
      if (*s == '\0')
        break;
      *s++ = '\0';
      if (buf[0])
        tvhlog(level, LS_SPAWN, "%s", buf);
      memmove(buf, s, strlen(s) + 1);
    }
    if (strlen(buf) == SPAWN_PIPE_READ_SIZE - 1) {
      tvherror(LS_SPAWN, "pipe buffer full");
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

  while (atomic_get(&spawn_pipe_running)) {

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
spawn_reap(pid_t wpid, char *stxt, size_t stxtlen)
{
  pid_t pid;
  int status, res;
  spawn_t *s;

  pid = waitpid(wpid, &status, WNOHANG);
  if(pid < 0 && ERRNO_AGAIN(errno))
    return -EAGAIN;
  if(pid < 0)
    return -errno;
  if(pid < 1) {
    if (wpid > 0 && pid != wpid)
      return -EAGAIN;
    return 0;
  }

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
static void
spawn_reaper(void)
{
  int r;
  spawn_t *s;

  do {
    r = spawn_reap(-1, NULL, 0);
    if (r == -EAGAIN)
      continue;
    if (r <= 0)
      break;
  } while (1);

  /* forced kill for expired PIDs */
  pthread_mutex_lock(&spawn_mutex);
  LIST_FOREACH(s, &spawns, link)
    if (s->killed && s->killed < mclk()) {
      /* kill the whole process group */
      kill(-(s->pid), SIGKILL);
    }
  pthread_mutex_unlock(&spawn_mutex);
}

/**
 * Kill the pid (only if waiting)
 */
int
spawn_kill(pid_t pid, int sig, int timeout)
{
  int r = -ESRCH;
  spawn_t *s;

  if (pid > 0) {
    spawn_reaper();

    pthread_mutex_lock(&spawn_mutex);
    LIST_FOREACH(s, &spawns, link)
      if(s->pid == pid)
        break;
    if (s) {
      if (!s->killed)
        s->killed = mclk() + sec2mono(MINMAX(timeout, 5, 3600));
      /* kill the whole process group */
      r = kill(-pid, sig);
      if (r < 0)
        r = -errno;
    }
    pthread_mutex_unlock(&spawn_mutex);
  }
  return r;
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
 *
 */
int
spawn_parse_args(char ***argv, int argc, const char *cmd, const char **replace)
{
  char *s, *f, *p, *a;
  const char **r;
  int i = 0, l, eow = 0;

  if (!argv || !cmd)
    return -1;

  s = tvh_strdupa(cmd);
  *argv = calloc(argc, sizeof(char *));

  while (*s && i < argc - 1) {
    while (*s == ' ')
      s++;
    f = s;
    eow = *s == '\'' || *s == '"' ? *s++ : ' ';
    while (*s && *s != eow) {
      if (*s == '\\') {
        l = *(s + 1);
        if (l == 'b')
          l = '\b';
        else if (l == 'f')
          l = '\f';
        else if (l == 'n')
          l = '\n';
        else if (l == 'r')
          l = '\r';
        else if (l == 't')
          l = '\t';
        else
          l = 0;
        if (l) {
          *s++ = l;
          memmove(s, s + 1, strlen(s));
        } else {
          memmove(s, s + 1, strlen(s));
          if (*s)
            s++;
        }
      } else {
        s++;
      }
    }
    if (f != s) {
      if (*s) {
        *(char *)s = '\0';
        s++;
      }
      for (r = replace; r && *r; r += 2) {
        p = strstr(f, *r);
        if (p) {
          l = strlen(*r);
          a = malloc(strlen(f) + strlen(r[1]) + 1);
          *p = '\0';
          strcpy(a, f);
          strcat(a, r[1]);
          strcat(a, p + l);
          (*argv)[i++] = a;
          break;
        }
      }
      if (r && *r)
        continue;
      (*argv)[i++] = strdup(f);
    }
  }
  (*argv)[i] = NULL;
  return 0;
}

/**
 *
 */
void
spawn_free_args(char **argv)
{
  char **a = argv;
  for (; *a; a++)
    free(*a);
  free(argv);
}

/**
 * Execute the given program and return its standard output as file-descriptor (pipe).
 */
int
spawn_and_give_stdout(const char *prog, char *argv[], char *envp[],
                      int *rd, pid_t *pid, int redir_stderr)
{
  pid_t p;
  int fd[2], f, i, maxfd;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };
  char **e, **e0, **e2, **e3, *p1, *p2;

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if (!argv) argv = (void *)local_argv;
  if (!argv[0]) {
    if (argv != (void *)local_argv) {
      for (i = 1, e = argv + 1; *e; i++, e++);
      i = (i + 1) * sizeof(char *);
      e = alloca(i);
      memcpy(e, argv, i);
      argv = e;
    }
    argv[0] = (char *)prog;
  }

  if (!envp || !envp[0]) {
    e = environ;
  } else {
    for (i = 0, e2 = environ; *e2; i++, e2++);
    for (f = 0, e2 = envp; *e2; f++, e2++);
    e = alloca((i + f + 1) * sizeof(char *));
    memcpy(e, environ, i * sizeof(char *));
    e0 = e + i;
    *e0 = NULL;
    for (e2 = envp; *e2; e2++) {
      for (e3 = e; *e3; e3++) {
        p1 = strchr(*e2, '=');
        p2 = strchr(*e3, '=');
        if (p1 - *e2 == p2 - *e3 && !strncmp(*e2, *e3, p1 - *e2)) {
          *e3 = *e2;
          break;
        }
      }
      if (!*e3) {
        *e0++ = *e2;
        *e0 = NULL;
      }
    }
    *e0 = NULL;
  }

  maxfd = sysconf(_SC_OPEN_MAX);

  pthread_mutex_lock(&fork_lock);

  if(pipe(fd) == -1) {
    pthread_mutex_unlock(&fork_lock);
    return -1;
  }

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    tvherror(LS_SPAWN, "Unable to fork() for \"%s\" -- %s",
             prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    f = open("/dev/null", O_RDWR);
    if(f == -1) {
      spawn_error("pid %d cannot open /dev/null for redirect %s -- %s",
                  getpid(), prog, strerror(errno));
      exit(1);
    }

    close(0);
    close(1);
    close(2);

    dup2(f, 0);
    dup2(fd[1], 1);
    dup2(redir_stderr ? spawn_pipe_error.wr : f, 2);

    close(fd[0]);
    close(fd[1]);
    close(f);

    spawn_info("Executing \"%s\"\n", prog);

    for (f = 3; f < maxfd; f++)
      close(f);

    execve(prog, argv, e);
    spawn_error("pid %d cannot execute %s -- %s\n",
                getpid(), prog, strerror(errno));
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  close(fd[1]);

  *rd = fd[0];
  if (pid) {
    *pid = p;

    // make the spawned process a session leader so killing the
    // process group recursively kills any child process that
    // might have been spawned
    setpgid(p, p);
  }
  return 0;
}

/**
 * Execute the given program and return its standard input as file-descriptor (pipe).
 * The standard output file-decriptor (od) must be valid, too.
 */
int
spawn_with_passthrough(const char *prog, char *argv[], char *envp[],
                       int od, int *wd, pid_t *pid, int redir_stderr)
{
  pid_t p;
  int fd[2], f, i, maxfd;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };
  char **e, **e0, **e2, **e3, *p1, *p2;

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if (!argv) argv = (void *)local_argv;
  if (!argv[0]) {
    if (argv != (void *)local_argv) {
      for (i = 1, e = argv + 1; *e; i++, e++);
      i = (i + 1) * sizeof(char *);
      e = alloca(i);
      memcpy(e, argv, i);
      argv = e;
    }
    argv[0] = (char *)prog;
  }

  if (!envp || !envp[0]) {
    e = environ;
  } else {
    for (i = 0, e2 = environ; *e2; i++, e2++);
    for (f = 0, e2 = envp; *e2; f++, e2++);
    e = alloca((i + f + 1) * sizeof(char *));
    memcpy(e, environ, i * sizeof(char *));
    e0 = e + i;
    *e0 = NULL;
    for (e2 = envp; *e2; e2++) {
      for (e3 = e; *e3; e3++) {
        p1 = strchr(*e2, '=');
        p2 = strchr(*e3, '=');
        if (p1 - *e2 == p2 - *e3 && !strncmp(*e2, *e3, p1 - *e2)) {
          *e3 = *e2;
          break;
        }
      }
      if (!*e3) {
        *e0++ = *e2;
        *e0 = NULL;
      }
    }
    *e0 = NULL;
  }

  maxfd = sysconf(_SC_OPEN_MAX);

  pthread_mutex_lock(&fork_lock);

  if(pipe(fd) == -1) {
    pthread_mutex_unlock(&fork_lock);
    return -1;
  }

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    tvherror(LS_SPAWN, "Unable to fork() for \"%s\" -- %s",
             prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    if (redir_stderr) {
      f = spawn_pipe_error.wr;
    } else {
      f = open("/dev/null", O_RDWR);
      if(f == -1) {
        spawn_error("pid %d cannot open /dev/null for redirect %s -- %s",
                    getpid(), prog, strerror(errno));
        exit(1);
      }
    }

    close(0);
    close(1);
    close(2);

    dup2(fd[0], 0);
    dup2(od, 1);
    dup2(f, 2);

    close(fd[0]);
    close(fd[1]);
    close(f);

    spawn_info("Executing \"%s\"\n", prog);

    for (f = 3; f < maxfd; f++)
      close(f);

    execve(prog, argv, e);
    spawn_error("pid %d cannot execute %s -- %s\n",
                getpid(), prog, strerror(errno));
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  close(fd[0]);

  *wd = fd[1];
  if (pid) {
    *pid = p;

    // make the spawned process a session leader so killing the
    // process group recursively kills any child process that
    // might have been spawned
    setpgid(p, p);
  }
  return 0;
}

/**
 * Execute the given program with arguments
 * 
 * *outp will point to the allocated buffer
 */
int
spawnv(const char *prog, char *argv[], pid_t *pid, int redir_stdout, int redir_stderr)
{
  pid_t p, f, maxfd;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if(!argv) argv = (void *)local_argv;
  if (!argv[0]) argv[0] = (char*)prog;

  maxfd = sysconf(_SC_OPEN_MAX);

  pthread_mutex_lock(&fork_lock);

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    tvherror(LS_SPAWN, "Unable to fork() for \"%s\" -- %s",
	     prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    f = open("/dev/null", O_RDWR);
    if(f == -1) {
      spawn_error("pid %d cannot open /dev/null for redirect %s -- %s",
                  getpid(), prog, strerror(errno));
      exit(1);
    }

    close(0);
    close(1);
    close(2);

    dup2(f, 0);
    dup2(redir_stdout ? spawn_pipe_info.wr : f, 1);
    dup2(redir_stderr ? spawn_pipe_error.wr : f, 2);

    close(f);

    spawn_info("Executing \"%s\"\n", prog);

    for (f = 3; f < maxfd; f++)
      close(f);

    execve(prog, argv, environ);
    spawn_error("pid %d cannot execute %s -- %s\n",
	        getpid(), prog, strerror(errno));
    close(1);
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  if (pid)
    *pid = p;

  return 0;
}

/*
 *
 */
void spawn_init(void)
{
  tvh_pipe(O_NONBLOCK, &spawn_pipe_info);
  tvh_pipe(O_NONBLOCK, &spawn_pipe_error);
  atomic_set(&spawn_pipe_running, 1);
  pthread_create(&spawn_pipe_tid, NULL, spawn_pipe_thread, NULL);
}

void spawn_done(void)
{
  spawn_t *s;

  atomic_set(&spawn_pipe_running, 0);
  pthread_kill(spawn_pipe_tid, SIGTERM);
  pthread_join(spawn_pipe_tid, NULL);
  tvh_pipe_close(&spawn_pipe_error);
  tvh_pipe_close(&spawn_pipe_info);
  free(spawn_error_buf);
  free(spawn_info_buf);
  while ((s = LIST_FIRST(&spawns)) != NULL) {
    LIST_REMOVE(s, link);
    free((char *)s->name);
    free(s);
  }
}
