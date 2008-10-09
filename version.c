#include "version.h"
#include "config.h"

#ifdef HTS_RELEASE_TAG
const char *htsversion=HTS_RELEASE_TAG;
const char *htsversion_full=HTS_RELEASE_TAG " (" HTS_VERSION ")";
#else
const char *htsversion=HTS_VERSION;
const char *htsversion_full=HTS_VERSION;
#endif
