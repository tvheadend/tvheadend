#include "config.h"
#include "filebundle.h"

filebundle_entry_t *filebundle_root = NULL;

const char *tvheadend_dataroot(void)
{
  return TVHEADEND_DATADIR;
}

