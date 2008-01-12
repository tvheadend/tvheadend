#define _XOPEN_SOURCE
#include <unistd.h>

char *
cwc_krypt(const char *key, const char *salt)
{
  return crypt(key, salt);
}
