#ifndef _HPCTOOLKIT_DLOPEN_FLAGS_H_
#define _HPCTOOLKIT_DLOPEN_FLAGS_H_

#include <stdbool.h>

typedef struct dlopen_flag_entry {
  struct dlopen_flag_entry* next;
  struct dlopen_flag_entry* prev;
  bool flag;
} dlopen_flag_entry_t;


extern void hpcrun_dlopen_flags_push(bool flag);
extern bool hpcrun_dlopen_flags_pop();

#endif

