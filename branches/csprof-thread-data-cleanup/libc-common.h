#ifndef CSPROF_LIBC_COMMON_H
#define CSPROF_LIBC_COMMON_H

#include <sys/time.h>
#include <signal.h>

extern sigset_t prof_sigset;

extern void csprof_libc_init();

#if defined(__osf__)
extern int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#else
extern int (*csprof_setitimer)(int, const struct itimerval *, struct itimerval *);
#endif

extern int (*csprof_sigprocmask)(int, const sigset_t *, sigset_t *);

#endif
