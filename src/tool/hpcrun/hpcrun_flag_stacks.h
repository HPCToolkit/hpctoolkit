#ifndef _HPCTOOLKIT_FLAG_STACKS_H_
#define _HPCTOOLKIT_FLAG_STACKS_H_

#include <stdbool.h>

extern void hpcrun_dlopen_flags_push(bool flag);
extern bool hpcrun_dlopen_flags_pop();
extern void hpcrun_dlclose_flags_push(bool flag);
extern bool hpcrun_dlclose_flags_pop();

#endif

