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
#include "file.h"
#include "spawn.h"

extern char **environ;

pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;

static LIST_HEAD(, spawn) spawns;

typedef struct spawn {
  LIST_ENTRY(spawn) link;
  pid_t pid;
  const char *name;
} spawn_t;

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
 * The reaper is called once a second to finish of any pending spawns
 */
void 
spawn_reaper(void)
{
  pid_t pid;
  int status;
  char txt[100];
  spawn_t *s;

  while(1) {
    pid = waitpid(-1, &status, WNOHANG);
    if(pid < 1)
      break;

    pthread_mutex_lock(&spawn_mutex);
    LIST_FOREACH(s, &spawns, link)
      if(s->pid == pid)
	break;

    if (WIFEXITED(status)) {
      snprintf(txt, sizeof(txt), 
	       "exited, status=%d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      snprintf(txt, sizeof(txt), 
	       "killed by signal %d", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      snprintf(txt, sizeof(txt), 
	       "stopped by signal %d", WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
      snprintf(txt, sizeof(txt), 
	       "continued");
    } else {
      snprintf(txt, sizeof(txt), 
	       "unknown status");
    }

    if(s != NULL) {
      LIST_REMOVE(s, link);
      free((void *)s->name);
      free(s);
    }
    pthread_mutex_unlock(&spawn_mutex);
  }
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
 * Execute the given program and return its output in a malloc()ed buffer
 * 
 * *outp will point to the allocated buffer
 * The function will return the size of the buffer
 */

int
spawn_and_store_stdout(const char *prog, char *argv[], char **outp)
{
  pid_t p;
  int fd[2], f;
  char bin[256];
  const char *local_argv[2] = { NULL, NULL };

  if (*prog != '/' && *prog != '.') {
    if (!find_exec(prog, bin, sizeof(bin))) return -1;
    prog = bin;
  }

  if(!argv) argv = (void *)local_argv;
  if (!argv[0]) argv[0] = (char*)prog;

  pthread_mutex_lock(&fork_lock);

  if(pipe(fd) == -1) {
    pthread_mutex_unlock(&fork_lock);
    return -1;
  }

  p = fork();

  if(p == -1) {
    pthread_mutex_unlock(&fork_lock);
    syslog(LOG_ERR, "spawn: Unable to fork() for \"%s\" -- %s",
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
      syslog(LOG_ERR, 
	     "spawn: pid %d cannot open /dev/null for redirect %s -- %s",
	     getpid(), prog, strerror(errno));
      exit(1);
    }

    dup2(f, 0);
    dup2(f, 2);
    close(f);

    execve(prog, argv, environ);
    syslog(LOG_ERR, "spawn: pid %d cannot execute %s -- %s",
	   getpid(), prog, strerror(errno));
    exit(1);
  }

  pthread_mutex_unlock(&fork_lock);

  spawn_enq(prog, p);

  close(fd[1]);

  return file_readall(fd[0], outp);
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

  p = fork();

  if(p == -1) {
    syslog(LOG_ERR, "spawn: Unable to fork() for \"%s\" -- %s",
	   prog, strerror(errno));
    return -1;
  }

  if(p == 0) {
    close(0);
    close(2);
    syslog(LOG_INFO, "spawn: Executing \"%s\"", prog);
    execve(prog, argv, environ);
    syslog(LOG_ERR, "spawn: pid %d cannot execute %s -- %s",
	   getpid(), prog, strerror(errno));
    close(1);
    exit(1);
  }

  spawn_enq(prog, p);
  return 0;
}
