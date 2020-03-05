#ifndef _atomics_h_
#define _atomics_h

#if defined(__powerpc__)

#include "atomic-powerpc.h"

#endif

#if defined(__x86_64__)

#include "atomic-op-gcc.h"

#endif

#endif

