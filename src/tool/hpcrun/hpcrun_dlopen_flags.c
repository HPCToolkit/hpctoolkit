#include "hpcrun_dlopen_flags.h"

#include <assert.h>
#include <stdbool.h>

static __thread int dlopen_flags = 0;
static __thread int dlopen_flag_index = 0;


void
hpcrun_dlopen_flags_push(bool flag)
{
  if (flag) {
    dlopen_flags |= (0x1 << (dlopen_flag_index++));
  } else {
    dlopen_flags &= ~(0x1 << (dlopen_flag_index++));
  }
  printf("flag %d dlopen_flags %d\n", flag, dlopen_flags);
  assert(dlopen_flag_index <= 32);
}


bool
hpcrun_dlopen_flags_pop()
{
  assert(dlopen_flag_index >= 1);
  bool ret = dlopen_flags & (0x1 << (--dlopen_flag_index));
  printf("ret %d dlopen_flags %d\n", ret, dlopen_flags);
  return ret;
}
