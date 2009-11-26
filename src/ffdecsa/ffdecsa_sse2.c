#if defined(__i386__) || defined(__x86_64__)
#define PARALLEL_MODE PARALLEL_128_SSE2
#include "FFdecsa.c"
#endif
