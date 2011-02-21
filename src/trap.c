/**
 *  Crash handling
 *  Copyright (C) 2009 Andreas Öman
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
#include "config.h"
#include "trap.h"

char tvh_binshasum[20];

#if defined(__i386__) || defined(__x86_64__)

// Only do this on x86 for now

#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <limits.h>
#if ENABLE_EXECINFO
#include <execinfo.h>
#endif
#include <stdio.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <openssl/sha.h>

#include "tvheadend.h"

#define MAXFRAMES 100

static char line1[200];
static char tmpbuf[1024];
static char libs[1024];
static char self[PATH_MAX];

static void
sappend(char *buf, size_t l, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf + strlen(buf), l - strlen(buf), fmt, ap);
  va_end(ap);
}



/**
 *
 */
static int
add2lineresolve(const char *binary, void *addr, char *buf0, size_t buflen)
{
  char *buf = buf0;
  int fd[2], r, f;
  const char *argv[5];
  pid_t p;
  char addrstr[30], *cp;

  argv[0] = "addr2line";
  argv[1] = "-e";
  argv[2] = binary;
  argv[3] = addrstr;
  argv[4] = NULL;

  snprintf(addrstr, sizeof(addrstr), "%p", (void *)((intptr_t)addr-1));

  if(pipe(fd) == -1)
    return -1;

  if((p = fork()) == -1)
    return -1;

  if(p == 0) {
    close(0);
    close(2);
    close(fd[0]);
    dup2(fd[1], 1);
    close(fd[1]);
    if((f = open("/dev/null", O_RDWR)) == -1)
      exit(1);

    dup2(f, 0);
    dup2(f, 2);
    close(f);

    execve("/usr/bin/addr2line", (char *const *) argv, environ);
    exit(2);
  }

  close(fd[1]);
  *buf = 0;
  while(buflen > 1) {
    r = read(fd[0], buf, buflen);
    if(r < 1)
      break;

    buf += r;
    buflen -= r;
    *buf = 0;
    cp = strchr(buf0, '\n');
    if(cp != NULL) {
      *cp = 0;
      break;
    }
  }
  close(fd[0]);
  return 0;
}



static void 
traphandler(int sig, siginfo_t *si, void *UC)
{
  ucontext_t *uc = UC;
  char buf[200];
#if ENABLE_EXECINFO
  static void *frames[MAXFRAMES];
  int nframes = backtrace(frames, MAXFRAMES);
  Dl_info dli;
#endif
  int i;
  const char *reason = NULL;

  tvhlog_spawn(LOG_ALERT, "CRASH", "Signal: %d in %s ", sig, line1);

  switch(sig) {
  case SIGSEGV:
    switch(si->si_code) {
    case SEGV_MAPERR:  reason = "Address not mapped"; break;
    case SEGV_ACCERR:  reason = "Access error"; break;
    }
    break;

  case SIGFPE:
    switch(si->si_code) {
    case FPE_INTDIV:  reason = "Integer division by zero"; break;
    }
    break;
  }

  tvhlog_spawn(LOG_ALERT, "CRASH", "Fault address %p (%s)",
	       si->si_addr, reason ?: "N/A");

  tvhlog_spawn(LOG_ALERT, "CRASH", "Loaded libraries: %s ", libs);
  snprintf(tmpbuf, sizeof(tmpbuf), "Register dump [%d]: ", NGREG);

  for(i = 0; i < NGREG; i++) {
#if __WORDSIZE == 64
    sappend(tmpbuf, sizeof(tmpbuf), "%016llx ", uc->uc_mcontext.gregs[i]);
#else
    sappend(tmpbuf, sizeof(tmpbuf), "%08x ", uc->uc_mcontext.gregs[i]);
#endif
  }
  tvhlog_spawn(LOG_ALERT, "CRASH", "%s", tmpbuf);

#if ENABLE_EXECINFO
  tvhlog_spawn(LOG_ALERT, "CRASH", "STACKTRACE");

  for(i = 0; i < nframes; i++) {

    
    if(dladdr(frames[i], &dli)) {

      if(dli.dli_sname != NULL && dli.dli_saddr != NULL) {
      	tvhlog_spawn(LOG_ALERT, "CRASH", "%s+0x%tx  (%s)",
		     dli.dli_sname,
		     frames[i] - dli.dli_saddr,
		     dli.dli_fname);
	continue;
      }

      if(self[0] && !add2lineresolve(self, frames[i], buf, sizeof(buf))) {
	tvhlog_spawn(LOG_ALERT, "CRASH", "%s %p", buf, frames[i]);
	continue;
      }

      if(dli.dli_fname != NULL && dli.dli_fbase != NULL) {
      	tvhlog_spawn(LOG_ALERT, "CRASH", "%s %p",
 		     dli.dli_fname,
		     frames[i]);
	continue;
      }


      tvhlog_spawn(LOG_ALERT, "CRASH", "%p", frames[i]);
    }
  }
#endif
}



static int
callback(struct dl_phdr_info *info, size_t size, void *data)
{
  if(info->dlpi_name[0])
    sappend(libs, sizeof(libs), "%s ", info->dlpi_name);
  return 0;
}


extern const char *htsversion_full;


void
trap_init(const char *ver)
{
  int r;
  uint8_t digest[20];
  struct sigaction sa, old;
  char path[256];

  SHA_CTX binsum;
  int fd;

  r = readlink("/proc/self/exe", self, sizeof(self) - 1);
  if(r == -1)
    self[0] = 0;
  else
    self[r] = 0;
  
  if((fd = open("/proc/self/exe", O_RDONLY)) != -1) {
    struct stat st;
    if(!fstat(fd, &st)) {
      char *m = malloc(st.st_size);
      if(m != NULL) {
	if(read(fd, m, st.st_size) == st.st_size) {
	  SHA_Init(&binsum);
	  SHA_Update(&binsum, (void *)m, st.st_size);
	  SHA_Final(digest, &binsum);
	}
	free(m);
      }
    }
  }
  
  snprintf(line1, sizeof(line1),
	   "PRG: %s (%s) "
	   "[%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	   "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x] "
	   "CWD: %s ", ver, htsversion_full,
	   digest[0],
	   digest[1],
	   digest[2],
	   digest[3],
	   digest[4],
	   digest[5],
	   digest[6],
	   digest[7],
	   digest[8],
	   digest[9],
	   digest[10],
	   digest[11],
	   digest[12],
	   digest[13],
	   digest[14],
	   digest[15],
	   digest[16],
	   digest[17],
	   digest[18],
	   digest[19],
	   getcwd(path, sizeof(path)));

  memcpy(tvh_binshasum, digest, 20);

  dl_iterate_phdr(callback, NULL);
  

  memset(&sa, 0, sizeof(sa));

  sigset_t m;
  sigemptyset(&m);
  sigaddset(&m, SIGSEGV);
  sigaddset(&m, SIGBUS);
  sigaddset(&m, SIGILL);
  sigaddset(&m, SIGABRT);
  sigaddset(&m, SIGFPE);

  sa.sa_sigaction = traphandler;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
  sigaction(SIGSEGV, &sa, &old);
  sigaction(SIGBUS,  &sa, &old);
  sigaction(SIGILL,  &sa, &old);
  sigaction(SIGABRT, &sa, &old);
  sigaction(SIGFPE,  &sa, &old);

  sigprocmask(SIG_UNBLOCK, &m, NULL);
}

#else

void
trap_init(const char *ver)
{

}
#endif
