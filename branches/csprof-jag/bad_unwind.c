#ifdef CSPROF_THREADS
#include <pthread.h>
#endif

#include "bad_unwind.h"

#ifdef CSPROF_THREADS
extern pthread_key_t k;
#include "thread_data.h"
#else
static jmp_buf bad_unwind;
#endif

_jb *get_bad_unwind(void){
#ifdef CSPROF_THREADS
  thread_data_t *td = (thread_data_t *)pthread_getspecific(k);
  return &(td->bad_unwind);
#else
  return &bad_unwind;
#endif
}
