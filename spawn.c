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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "tvhead.h"
#include "dispatch.h"
#include "spawn.h"

extern char **environ;

static dtimer_t reaper_timer;

pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;

static LIST_HEAD(, spawn) spawns;

typedef struct spawn {
  LIST_ENTRY(spawn) link;
  pid_t pid;
  const char *name;
} spawn_t;


/**
 * Structs for reading back output from a spawn via a pipe
 */
TAILQ_HEAD(spawn_output_buf_queue, spawn_output_buf);

#define MAX_SOB_SIZE 4000

typedef struct spawn_output_buf {
  TAILQ_ENTRY(spawn_output_buf) sob_link;
  int sob_size;
  char sob_buf[MAX_SOB_SIZE];
} spawn_output_buf_t;



/**
 * The reaper is called once a second to finish of any pending spawns
 */
static void 
reaper(void *opaque, int64_t now)
{
  pid_t pid;
  int status;
  char txt[100];
  spawn_t *s;

  dtimer_arm(&reaper_timer, reaper, NULL, 1);

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
	       "exited, status=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      snprintf(txt, sizeof(txt), 
	       "killed by signal %d\n", WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      snprintf(txt, sizeof(txt), 
	       "stopped by signal %d\n", WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
      snprintf(txt, sizeof(txt), 
	       "continued\n");
    } else {
      snprintf(txt, sizeof(txt), 
	       "unknown status\n");
    }

    syslog(LOG_INFO, "spawn: \"%s\" %s", s ? s->name : "<unknwon spawn>", txt);
    if(s != NULL) {
      LIST_REMOVE(s, link);
      free((void *)s->name);
      free(s);
    }
    pthread_mutex_unlock(&spawn_mutex);
  }
}


void
spawn_init(void)
{
  dtimer_arm(&reaper_timer, reaper, NULL, 1);
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
spawn_and_store_stdout(const char *prog, char *const argv[], char **outp)
{
  pid_t p;
  int fd[2], r, totalsize = 0;
  char *outbuf;
  struct spawn_output_buf_queue bufs;
  spawn_output_buf_t *b = NULL;
  const char *local_argv[2];

  if(argv == NULL) {
    local_argv[0] = prog;
    local_argv[1] = NULL;
    argv = (void *)local_argv;
  }

  if(pipe(fd) == -1)
    return -1;

  p = fork();

  if(p == -1) {
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
    syslog(LOG_INFO, "spawn: Executing \"%s\"", prog);
    execve(prog, argv, environ);
    syslog(LOG_ERR, "spawn: pid %d cannot execute %s -- %s",
	   getpid(), prog, strerror(errno));
    close(1);
    exit(1);
  }

  spawn_enq(prog, p);

  close(fd[1]);

  TAILQ_INIT(&bufs);
  while(1) {
    if(b == NULL) {
      b = malloc(sizeof(spawn_output_buf_t));
      b->sob_size = 0;
      TAILQ_INSERT_TAIL(&bufs, b, sob_link);
    }

    r = read(fd[0], b->sob_buf + b->sob_size, MAX_SOB_SIZE - b->sob_size);
    if(r < 1)
      break;
    b->sob_size += r;
    totalsize += r;
    if(b->sob_size == MAX_SOB_SIZE)
      b = NULL;
  } 

  if(totalsize == 0) {
    free(b);
    *outp = NULL;
    return 0;
  }

  outbuf = malloc(totalsize + 1);
  r = 0;
  while((b = TAILQ_FIRST(&bufs)) != NULL) {
    memcpy(outbuf + r, b->sob_buf, b->sob_size);
    r+= b->sob_size;
    TAILQ_REMOVE(&bufs, b, sob_link);
    free(b);
  }
  assert(r == totalsize);
  *outp = outbuf;
  return totalsize;
}


/**
 * Execute the given program with arguments
 * 
 * *outp will point to the allocated buffer
 * The function will return the size of the buffer
 */
int
spawnv(const char *prog, char *const argv[])
{
  pid_t p;

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
