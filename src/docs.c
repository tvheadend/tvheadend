#include <unistd.h>
#include "docs.h"

#define LANGPREF     "\xff\x01"
#define DOCINCPREF   "\xff\x02"
#define ITEMSINCPREF "\xff\x03"
#define MDINCLUDE    "\xff\x04"
#define N_(s) s

#include "docs_inc.c"
