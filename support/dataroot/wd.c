#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "filebundle.h"

filebundle_entry_t *filebundle_root = NULL;

const char *tvheadend_dataroot(void)
{
  static char cwd[256] = { 0 };
  if (!*cwd) {
    assert(getcwd(cwd, 254));
  }
  return cwd;
}
