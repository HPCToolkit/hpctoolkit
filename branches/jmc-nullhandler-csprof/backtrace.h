/* backtrace.h -- an interface for performing stack unwinding */

#ifndef CSPROF_BACKTRACE_H
#define CSPROF_BACKTRACE_H

#include <stddef.h>

#include "state.h"

// Amount to chop off call stack samples b/c of the profiler.
// Beginning from top:
//   csprof_backtrace()
//   csprof_sample_callstack()
//   csprof_take_profile_sample()
//   csprof_signal_handler()
//   libc_signal_trampoline()
/* FIXME: depending on how the whole trampoline_backend thing falls out,
   this will probably need to be changed for *both* backends */
#if defined(CSPROF_SETJMP_SUPPORT)
#define CSPROF_CS_CHOP 5
#else
#define CSPROF_CS_CHOP 4
#endif

int csprof_sample_callstack(csprof_state_t *, int, size_t, void *);

#endif
