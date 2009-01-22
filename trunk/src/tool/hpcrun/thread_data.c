//
// $Id$
//

#include <stdlib.h>

#include "pmsg.h"
#include "mem.h"
#include "state.h"
#include "handling_sample.h"

#include "thread_data.h"

#include <monitor.h>

static thread_data_t _local_td;

static thread_data_t *
local_td(void)
{
  return &_local_td;
}

thread_data_t *(*csprof_get_thread_data)(void) = &local_td;

void
csprof_unthreaded_data(void)
{
  csprof_get_thread_data = &local_td;
}


static offset_t emergency_sz = 4 * 1024 * 1024; // 1 Meg for emergency

void
csprof_thread_data_init(int id, offset_t sz, offset_t sz_tmp)
{
  NMSG(THREAD_SPECIFIC,"init thread specific data for %d",id);
  thread_data_t *td = csprof_get_thread_data();

  // initialize thread_data with known bogus bit pattern so that missing
  // initializations will be apparent.
  memset(td, 0xfe, sizeof(thread_data_t)); 

  // sampling control variables
  //   tallent: protect against spurious signals until 'state' is fully
  //   initialized... for now rely on caller to re-enable
  td->suspend_sampling            = 1; // protect against spurious signals
  td->handling_synchronous_sample = 0;
  csprof_init_handling_sample(td,0);

  td->id                          = id;
  td->memstore                    = csprof_malloc_init(sz, sz_tmp);
  td->memstore2                   = csprof_malloc2_init(emergency_sz,0);
  td->state                       = NULL;

  // locks
  td->fnbounds_lock               = 0;
  td->splay_lock                  = 0;

  td->trace_file                  = NULL;
  td->last_us_usage               = 0;

  memset(&td->bad_unwind, 0, sizeof(td->bad_unwind));
  memset(&td->mem_error, 0, sizeof(td->mem_error));
  memset(&td->eventSet, 0, sizeof(td->eventSet));
  memset(&td->ss_state, UNINIT, sizeof(td->ss_state));

  TMSG(MALLOC," thread_data_init state");
  csprof_state_t *state = csprof_malloc(sizeof(csprof_state_t));

  csprof_set_state(state);
  csprof_state_init(state);
  csprof_state_alloc(state);

  return;
}

#ifdef CSPROF_THREADS
#include <pthread.h>

static pthread_key_t _csprof_key;

void
csprof_init_pthread_key(void)
{
  NMSG(THREAD_SPECIFIC,"creating _csprof_key");
  int bad = pthread_key_create(&_csprof_key, NULL);
  if (bad){
    EMSG("pthread_key_create returned non-zero = %d",bad);
  }
}


void
csprof_set_thread_data(thread_data_t *td)
{
  NMSG(THREAD_SPECIFIC,"setting td");
  pthread_setspecific(_csprof_key,(void *) td);
}


void
csprof_set_thread0_data(void)
{
  NMSG(THREAD_SPECIFIC,"set thread0 data");
  csprof_set_thread_data(&_local_td);
}

// FIXME: use csprof_malloc ??
thread_data_t *
csprof_allocate_thread_data(void)
{
  NMSG(THREAD_SPECIFIC,"malloc thread data");
  return malloc(sizeof(thread_data_t));
}

thread_data_t *
thread_specific_td(void)
{
  thread_data_t *ret = (thread_data_t *) pthread_getspecific(_csprof_key);
  if (!ret){
    monitor_real_abort();
  }
  return ret;
}

void
csprof_threaded_data(void)
{
  assert(csprof_get_thread_data == &local_td);
  csprof_get_thread_data = &thread_specific_td;
}
#endif // defined(CSPROF_THREADS)
