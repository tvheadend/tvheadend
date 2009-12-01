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

#define _GNU_SOURCE
#include <link.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdarg.h>

#include <libavutil/sha1.h>

#include "trap.h"
#include "tvhead.h"


static char traceline[4096];
static int tracepos = 0;

#define MAXFRAMES 100

static void
traceappend(const char *fmt, ...)
{
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(traceline + tracepos, sizeof(traceline) - tracepos, fmt, ap);
  va_end(ap);

  if(n < 1)
    return;
  tracepos += n;
}



static void 
traphandler(int sig, siginfo_t *si, void *UC)
{
  ucontext_t *uc = UC;
  static void *frames[MAXFRAMES];
  int nframes = backtrace(frames, MAXFRAMES);

  int i;

  traceappend("SIGNAL: %d ", sig); 
  traceappend("REGDUMP[%d]: ", NGREG);
  for(i = 0; i < NGREG; i++) {
#if __WORDSIZE == 64
    traceappend("%016llx ", uc->uc_mcontext.gregs[i]);
#else
    traceappend("%08x ", uc->uc_mcontext.gregs[i]);
#endif
  }

  traceappend("STACKTRACE[%d]: ", nframes);
  for(i = 0; i < nframes; i++)
    traceappend("%p ", frames[i]);

  tvhlog_spawn(LOG_ALERT, "CRASH", "%s", traceline);
}

static struct AVSHA1 *binsum;


static int
callback(struct dl_phdr_info *info, size_t size, void *data)
{
  int j;

  // Other lib, just record its presence
  if(info->dlpi_name[0]) {
    traceappend("%s ", info->dlpi_name);
    return 0;
  }

  // Our self, hash huge R-X segments which in practice is the main text seg.

  for(j = 0; j < info->dlpi_phnum; j++) {
    if(info->dlpi_phdr[j].p_flags & PF_W)
      continue;

    if(!(info->dlpi_phdr[j].p_flags & PF_X))
      continue;

    if(info->dlpi_phdr[j].p_memsz < 65536)
      continue;

    av_sha1_update(binsum, 
		   (void *)(info->dlpi_addr + info->dlpi_phdr[j].p_vaddr),
		   info->dlpi_phdr[j].p_memsz);
  }
  return 0;
}


extern const char *htsversion_full;


void
trap_init(const char *ver)
{
  uint8_t digest[20];
  struct sigaction sa, old;
  char path[256];

  traceappend("PRG: %s [%s] CWD: %s ", ver, htsversion_full,
	      getcwd(path, sizeof(path)));
   

  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = traphandler;
  sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

  sigaction(SIGSEGV, &sa, &old);
  sigaction(SIGBUS,  &sa, &old);
  sigaction(SIGILL,  &sa, &old);
  sigaction(SIGABRT, &sa, &old);
  sigaction(SIGFPE,  &sa, &old);
 

  traceappend("OBJS: ");
  binsum = malloc(av_sha1_size);

  av_sha1_init(binsum);
  dl_iterate_phdr(callback, NULL);

  av_sha1_final(binsum, digest);

  traceappend("SHA1: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
	      "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ",
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
	      digest[19]);

  free(binsum);
}

