#include <setjmp.h>
#include "bad_unwind.h"
typedef struct _td_t {
  int id;
  sigjmp_buf_t bad_unwind;
} thread_data_t;
