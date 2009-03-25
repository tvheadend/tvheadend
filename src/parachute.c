/*
 *  Crash Parachute
 *  Copyright (C) 2007-2008 Andreas Öman
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

#include "parachute.h"
#include <assert.h>

static void (*htsparachute_userhandler)(int);

static void
htsparachute_sighandler (int sig)
{
  signal(SIGSEGV, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  signal(SIGILL, SIG_DFL);
  signal(SIGABRT, SIG_DFL);
  signal(SIGFPE, SIG_DFL);

  if (htsparachute_userhandler)
    htsparachute_userhandler(sig);

  assert(!*"parachute");
}

void
htsparachute_init (void (*handler) (int))
{
  signal(SIGSEGV, htsparachute_sighandler);
  signal(SIGBUS, htsparachute_sighandler);
  signal(SIGILL, htsparachute_sighandler);
  signal(SIGABRT, htsparachute_sighandler);
  signal(SIGFPE, htsparachute_sighandler);
  htsparachute_userhandler = handler;
}
