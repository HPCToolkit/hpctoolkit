#ifndef UCONTEXT_MANIP
#define UCONTEXT_MANIP

#if defined(HOST_CPU_x86) || defined(HOST_CPU_x86_64)
#include "x86-family/specific-ucontext-manip.h"
#elif defined(HOST_CPU_PPC)
#include "ppc64/specific-ucontext-manip.h"
#elif defined(HOST_CPU_IA64)
#error ia64 has no specific ucontext-manip.h
#elif defined(HOST_CPU_ARM64)
#error aarch64 has no specific ucontext-manip.h
#else
#error No valid HOST_CPU_* defined
#endif

#endif // UCONTEXT_MANIP
