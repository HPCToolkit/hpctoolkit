#include "bad_unwind.h"

#ifdef CSPROF_THREADS
#include <pthread.h>
#include "thread_data.h"
#include "thread_use.h"
#endif

static _jb bad_unwind;


#if JOHNFIX
_jb *get_bad_unwind(void){
#if 0
#ifdef CSPROF_THREADS
  thread_data_t *td = (thread_data_t *)pthread_getspecific(k);
  return &(td->bad_unwind);
#else
  return &bad_unwind;
#endif
#else
static _jb bad_unwind;
  return &bad_unwind;
#endif
}
#endif

_jb *get_bad_unwind(void){
#ifdef CSPROF_THREADS
  if (csprof_using_threads){
    thread_data_t *td = (thread_data_t *)pthread_getspecific(k);
    return &(td->bad_unwind);
  }
  else {
    return &bad_unwind;
  }
#else
  return &bad_unwind;
#endif
}
