// This is called "thread data", but it applies to process as well
// (there is just 1 thread).

#include <setjmp.h>
#include "bad_unwind.h"
#include "state.h"
#include "mem.h"

typedef struct _td_t {
  int id;
  csprof_pstate_t *state;
  csprof_mem_t    *memstore;
  sigjmp_buf_t bad_unwind;
  int eventSet; // for PAPI
  int handling_sample;
} thread_data_t;
