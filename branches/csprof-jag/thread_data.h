#include <setjmp.h>
typedef struct _td_t {
  int id;
  sigjmp_buf bad_unwind;
#ifdef MPI_SPECIAL
  int csprof_rank;
#endif
} thread_data_t;
