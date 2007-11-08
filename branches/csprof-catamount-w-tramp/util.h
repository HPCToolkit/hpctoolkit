#ifndef CSPROF_UTIL_H
#define CSPROF_UTIL_H

#include "structs.h"
#include "mem.h"
#ifdef CSPROF_THREADS
#include "xpthread.h"
#else
#include "libc.h"
#endif

/* fetches the state of the profiler */
extern csprof_state_t *csprof_get_state();
/* sets the state of the profiler (at epoch turns, e.g.) */
extern void csprof_set_state(csprof_state_t *);
/* fetches private memory for the profiler */

#ifdef FIXME
extern csprof_mem_t *csprof_get_memstore();
#endif
extern get_memstore_f *csprof_get_memstore;

/* these are used to call into libc/libexc/libpthread and so forth, since
   we can't be assured that we'll have dlsym'd the appropriate addresses
   at the point these functions are called.  this is a HUGE HACK, since it
   totally overrides any shared-library handling which might take place
   and it is incredibly fragile--if the shared library changes out from
   under us, we'll break, because our hardcoded addresses will no longer
   be valid.  the last part could be fixed fairly easily.

   also note that these could be redefined as macros in case we find a
   good and proper solution for the issues we're having. */
/* it'd also be great if the compiler paid no attention to these things
   whatsoever...since they're only here to say "yes, I know about this
   particular function." */
#ifdef CSPROF_FIXED_LIBCALLS
extern void *libcall1(void *func_addr, void *);
extern void *libcall2(void *func_addr, void *, void *);
extern void *libcall3(void *func_addr, void *, void *, void *);
extern void *libcall4(void *func_addr, void *, void *, void *, void *);
#else
/* macroize to redirect properly */
#define libcall1(func_addr, arg0) func_addr(arg0)
#define libcall2(func_addr, arg0, arg1) func_addr(arg0, arg1)
#define libcall3(func_addr, arg0, arg1, arg2) func_addr(arg0, arg1, arg2)
#define libcall4(func_addr, arg0, arg1, arg2, arg3) func_addr(arg0, arg1, arg2, arg3)
#endif

/* calls the correct sigmask'ing function depending on which build we're
   currently running.  this is inline for various reasons, most notably
   because we bail out of the signal handler if we're sigmask'ing (since
   we might be doing it from the trampoline) and blocking this function
   too seems silly.  especially when the code is so short anyway. */
static inline int
csprof_sigmask(int mode, const sigset_t *inset, sigset_t *outset)
{
#ifdef CSPROF_THREADS
    return libcall3(csprof_pthread_sigmask, mode, inset, outset);
#else
    return libcall3(csprof_sigprocmask, mode, inset, outset);
#endif
}

#endif
