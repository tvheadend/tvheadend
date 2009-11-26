#define _GNU_SOURCE
#include <fcntl.h>
#include "tvhead.h"

int
tvh_open(const char *pathname, int flags, mode_t mode)
{
  return open(pathname, flags | O_CLOEXEC, mode);
}
