#ifndef ARCH_LIBC_H
#define ARCH_LIBC_H

#include <excpt.h>

extern unsigned long (*csprof_exc_virtual_unwind)(pdsc_crd *, CONTEXT *);

#endif
