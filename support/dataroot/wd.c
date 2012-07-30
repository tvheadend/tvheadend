#include <string.h>
#include <unistd.h>
#include <assert.h>

const char *tvheadend_dataroot(void)
{
  static char cwd[256] = { 0 };
  if (!*cwd) {
    assert(getcwd(cwd, 254));
    strcat(cwd, "/");
  }
  return cwd;
}
