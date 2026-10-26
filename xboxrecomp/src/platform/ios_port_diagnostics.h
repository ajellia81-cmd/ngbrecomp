#pragma once
#include <stdio.h>
/* Only messages introduced by this iOS port use this macro. Standard streams,
 * original logger/profiler code and original on-demand captures stay untouched. */
#ifdef NGB_IOS_PORT_RELEASE
#define NGB_IOS_PORT_DIAG(...) ((void)0)
#else
#define NGB_IOS_PORT_DIAG(...) fprintf(stderr, __VA_ARGS__)
#endif
